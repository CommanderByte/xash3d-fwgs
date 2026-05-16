// xash3dpp — Filesystem implementation  (internal)
// Legacy reference: filesystem/filesystem.c

#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/utilities/gameinfo_parser.hpp>
#include <xash3dpp/private/filesystem/search_path.hpp>
#include <xash3dpp/private/filesystem/archive_registry.hpp>
#include <xash3dpp/private/filesystem/ci_directory.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/backends/dir_backend.hpp>
#include <xash3dpp/private/filesystem/os_file_factory.hpp>
#include <xash3dpp/utilities/hash.hpp>
#include <xash3dpp/utilities/path.hpp>

#include <algorithm>
#include <atomic>
#include <deque>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace xash::filesystem {

namespace platform = ::xash::platform;
using ::xash::platform::OsFd;

using GameInfo = ::xash::GameInfo;

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------

// Returns the parent sub-directory of `path` (relative, no trailing slash).
// "hello.txt" → "", "sub/dir/file.txt" → "sub/dir".
static std::string_view parent_dir_of(std::string_view path) noexcept
{
    auto pos = path.rfind('/');
    if (pos == std::string_view::npos) pos = path.rfind('\\');
    return pos == std::string_view::npos ? std::string_view{} : path.substr(0, pos);
}

// Reinterpret a byte buffer as a string view without copying.
// std::byte* → char* aliasing is permitted by [basic.lval].
static std::string bytes_as_string( std::span<const std::byte> s ) noexcept
{
    return { reinterpret_cast<const char*>( s.data() ), s.size() };
}

// ---------------------------------------------------------------------------
// File-local: search-path collection helpers (no locking)
// ---------------------------------------------------------------------------

// Fill `out` with SearchPath entries for all archives + the plain directory
// found under `dir`.  No mutex is held during this call.
static void collect_paths_for_dir( xash::memory::PoolHandle pool,
                                   std::string_view dir, SearchPathFlags flags,
                                   std::vector<SearchPath>& out )
{
    auto entries = platform::list_directory( dir );
    std::sort( entries.begin(), entries.end() );  // alphabetical: pak0 before pak1

    for ( const auto& at : k_archive_types ) {
        for ( const auto& entry : entries ) {
            const auto ext = xash::utilities::file_extension( entry );
            if ( ext.size() < 2 || ext.substr(1) != at.extension ) continue;
            const std::string full = xash::utilities::path_join( dir, entry );
            auto backend = at.factory( pool, full, flags | at.default_flags );
            if ( backend )
                out.push_back( { std::move(backend), full, flags | at.default_flags } );
        }
    }

    // Plain directory backend — highest priority, added last.
    out.push_back( {
        backends::DirBackend::create( pool, dir, flags ),
        std::string{dir},
        flags
    } );
}

// Fill `out` with paths for the full game hierarchy rooted at `dir`
// (including _hd and _lv variants when the corresponding flags are set).
static void collect_hierarchy( xash::memory::PoolHandle pool,
                               std::string_view dir, SearchPathFlags flags,
                               std::vector<SearchPath>& out )
{
    collect_paths_for_dir( pool, dir, flags, out );
    if ( any( flags & SearchPathFlags::MountHD ) )
        collect_paths_for_dir( pool, std::string{dir} + "_hd", flags, out );
    if ( any( flags & SearchPathFlags::MountLV ) )
        collect_paths_for_dir( pool, std::string{dir} + "_lv", flags, out );
    // TODO: MountAddon, MountL10n
}

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct Filesystem::Impl {
    // Written once in init(), never mutated after — Safe-RO.
    std::string                 rootdir;
    std::string                 basedir;
    std::string                 rodir;

    // Game-selection state — protected by game_mutex.
    mutable std::shared_mutex   game_mutex;
    std::string                 gamedir;
    GameInfo                    active_game;
    bool                        game_loaded = false;

    // search-path list — protected by paths_mutex.
    std::deque<SearchPath>      search_paths;  // @pre-reserved: filesystem_search_path_max (std::deque: reserve N/A; informally bounded)
    mutable std::shared_mutex   paths_mutex;

    // Memory pool — created in init(), destroyed in shutdown().
    xash::memory::PoolHandle    pool_;

    // Toggle for absolute/traversal paths — atomic for lock-free access.
    std::atomic<bool>           allow_direct_paths{false};

    // Always-on observability snapshot.
    FilesystemStats             stats_ {};
};

// ---------------------------------------------------------------------------
// Filesystem public methods
// ---------------------------------------------------------------------------

Filesystem::Filesystem()  : impl_{std::make_unique<Impl>()} {}
Filesystem::~Filesystem()              = default;
Filesystem::Filesystem(Filesystem&&) noexcept            = default;
Filesystem& Filesystem::operator=(Filesystem&&) noexcept = default;

bool Filesystem::init(std::string_view rootdir,
                      std::string_view basedir,
                      std::string_view gamedir,
                      std::string_view rodir)
{
    impl_->rootdir = rootdir;
    impl_->basedir = basedir;
    impl_->gamedir = gamedir;
    impl_->rodir   = rodir;
    impl_->pool_   = xash::memory::create_pool("filesystem");
    impl_->stats_  = {};
    return static_cast<bool>(impl_->pool_);
}

void Filesystem::shutdown() {
    {
        std::unique_lock lock{impl_->paths_mutex};
        impl_->search_paths.clear();
        impl_->stats_.search_path_count = 0;
    }
    {
        std::unique_lock lock{impl_->game_mutex};
        impl_->active_game = {};
        impl_->game_loaded  = false;
        impl_->stats_.game_loaded = false;
    }
    if (impl_->pool_) {
        xash::memory::destroy_pool(impl_->pool_);
        impl_->pool_ = {};
    }
}

bool Filesystem::activate_game(std::string_view gamefolder,
                               SearchPathFlags  mount_flags,
                               std::string_view language)
{
    const auto games = scan_game_directories(impl_->rootdir);
    for (const auto& g : games) {
        if (g.gamefolder == gamefolder) {
            {
                std::unique_lock lock{impl_->game_mutex};
                impl_->active_game = g;
                impl_->gamedir     = g.gamefolder;
                impl_->game_loaded = true;
                impl_->stats_.game_loaded = true;
            }
            rescan(mount_flags, language);
            return true;
        }
    }
    return false;
}

void Filesystem::rescan(SearchPathFlags mount_flags, std::string_view language) {
    // 1. Snapshot mutable game state under shared lock.
    GameInfo g;
    {
        std::shared_lock lock{impl_->game_mutex};
        if (!impl_->game_loaded) return;
        g = impl_->active_game;
    }
    (void)language;   // TODO: mount language pack

    // rootdir and rodir are init-only — read directly without a lock.
    const std::string_view rootdir = impl_->rootdir;
    const std::string_view rodir   = impl_->rodir;

    // 2. Build the full path list without holding any lock.
    std::vector<SearchPath> new_paths;

    // Mount in ascending priority order (last added = highest priority).
    // basedir (lowest) → falldir → gamedir (highest).
    if (!g.basedir.empty() && g.basedir != g.gamefolder)
        collect_hierarchy(impl_->pool_,
                          xash::utilities::path_join(rootdir, g.basedir),
                          mount_flags, new_paths);

    if (!g.falldir.empty()
            && g.falldir != g.gamefolder
            && g.falldir != g.basedir)
        collect_hierarchy(impl_->pool_,
                          xash::utilities::path_join(rootdir, g.falldir),
                          mount_flags, new_paths);

    collect_hierarchy(impl_->pool_,
                      xash::utilities::path_join(rootdir, g.gamefolder),
                      mount_flags | SearchPathFlags::GameDir, new_paths);

    // Read-only mirror (installed separately from the writable gamedir).
    if (!rodir.empty())
        collect_hierarchy(impl_->pool_,
                          xash::utilities::path_join(rodir, g.gamefolder),
                          mount_flags | SearchPathFlags::GameDir
                                      | SearchPathFlags::NoWrite,
                          new_paths);

    // 3. Atomically replace non-static paths with the freshly built list.
    std::unique_lock lock{impl_->paths_mutex};
    auto& paths = impl_->search_paths;
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const SearchPath& sp) {
        return !any(sp.flags & SearchPathFlags::Static);
    }), paths.end());
    for (auto& sp : new_paths)
        paths.push_back(std::move(sp));
    impl_->stats_.search_path_count = paths.size();
}

std::vector<GameInfo> Filesystem::scan_game_directories(std::string_view root) const {
    std::vector<GameInfo> result;
    const auto entries = platform::list_directory(root);
    for (const auto& entry : entries) {
        const std::string dir = xash::utilities::path_join(root, entry);

        // Try gameinfo.txt first.
        const auto gi = load_direct_file(dir + "/gameinfo.txt");
        if (!gi.empty()) {
            const std::string text = bytes_as_string( gi );
            auto info = xash::parse_gameinfo_txt(text, entry);
            if (info) {
                xash::apply_gameinfo_fixups(*info);
                result.push_back(std::move(*info));
                continue;
            }
        }

        // Fall back to liblist.gam.
        const auto ll = load_direct_file(dir + "/liblist.gam");
        if (!ll.empty()) {
            const std::string text = bytes_as_string( ll );
            auto info = xash::parse_liblist_gam(text, entry);
            if (info) {
                xash::apply_gameinfo_fixups(*info);
                result.push_back(std::move(*info));
            }
        }
    }
    return result;
}

void Filesystem::add_game_directory(std::string_view dir, SearchPathFlags flags) {
    std::vector<SearchPath> new_paths;
    collect_paths_for_dir(impl_->pool_, dir, flags, new_paths);
    std::unique_lock lock{ impl_->paths_mutex };
    for (auto& sp : new_paths)
        impl_->search_paths.push_back(std::move(sp));
    impl_->stats_.search_path_count = impl_->search_paths.size();
}

void Filesystem::add_game_hierarchy(std::string_view dir, SearchPathFlags flags) {
    std::vector<SearchPath> new_paths;
    collect_hierarchy(impl_->pool_, dir, flags, new_paths);
    std::unique_lock lock{ impl_->paths_mutex };
    for (auto& sp : new_paths)
        impl_->search_paths.push_back(std::move(sp));
    impl_->stats_.search_path_count = impl_->search_paths.size();
}

void Filesystem::clear_paths() {
    std::unique_lock lock{impl_->paths_mutex};
    auto& paths = impl_->search_paths;
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const SearchPath& sp) {
        return !any(sp.flags & SearchPathFlags::Static);
    }), paths.end());
    impl_->stats_.search_path_count = paths.size();
}

void Filesystem::allow_direct_paths(bool enable) {
    impl_->allow_direct_paths.store(enable, std::memory_order_relaxed);
}

bool Filesystem::mount_archive(std::string_view path, SearchPathFlags flags) {
    const auto ext_sv = xash::utilities::file_extension(path);
    if (ext_sv.size() < 2) return false;
    const std::string_view ext = ext_sv.substr(1);

    for (const auto& at : k_archive_types) {
        if (ext != at.extension) continue;
        auto backend = at.factory(impl_->pool_, path, flags);
        if (!backend) return false;
        std::unique_lock lock{ impl_->paths_mutex };
        impl_->search_paths.push_back({ std::move(backend), std::string{path}, flags });
        impl_->stats_.search_path_count = impl_->search_paths.size();
        return true;
    }
    return false;
}

std::unique_ptr<File> Filesystem::open(std::string_view path,
                                       std::string_view mode,
                                       bool gamedironly)
{
    std::shared_lock lock{ impl_->paths_mutex };
    // Back-to-front: later-added paths have higher priority.
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        auto f = it->backend->open_file(path, mode);
        if (f) return f;
    }
    return nullptr;
}

std::vector<std::byte> Filesystem::load_file(std::string_view path, bool gamedironly) {
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        auto data = it->backend->load_file(path);
        if (!data.empty()) return data;
    }
    return {};
}

std::vector<std::byte> Filesystem::load_direct_file(std::string_view disk_path) const {
    const auto sz = platform::file_size(disk_path);
    if (!sz || *sz == 0) return {};

    OsFd fd = platform::open_file(disk_path, platform::OpenMode::ReadOnly);
    if (!fd.valid()) return {};

    std::vector<std::byte> buf(static_cast<std::size_t>(*sz));
    if (platform::read(fd, buf.data(), buf.size()) != static_cast<FsOffset>(*sz))
        buf.clear();
    return buf;
}

bool Filesystem::write_file(std::string_view path, std::span<const std::byte> data) {
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (any(it->flags & SearchPathFlags::NoWrite)) continue;
        const std::string full = xash::utilities::path_join(it->source_path, path);
        OsFd fd = platform::open_file(full,
            platform::OpenMode::WriteOnly |
            platform::OpenMode::create   |
            platform::OpenMode::Truncate);
        if (!fd.valid()) continue;
        const FsOffset n   = platform::write(fd, data.data(), data.size());
        const bool     ok  = (n == static_cast<FsOffset>(data.size()));
        // invalidate the backend's directory cache so a subsequent file_exists
        // or find_file call sees the new file (critical on Linux emulated-CI).
        if (ok) it->backend->invalidate_directory(parent_dir_of(path));
        return ok;
    }
    return false;
}

bool Filesystem::file_exists(std::string_view path, bool gamedironly) const {
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        if (it->backend->find_file(path)) return true;
    }
    return false;
}

std::optional<FsOffset> Filesystem::file_size(std::string_view path,
                                              bool gamedironly) const
{
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        auto f = it->backend->open_file(path, "rb");
        if (f) return f->Length();
    }
    return std::nullopt;
}

std::optional<std::filesystem::file_time_type>
Filesystem::file_time(std::string_view path, bool gamedironly) const {
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        auto t = it->backend->file_time(path);
        if (t) return t;
    }
    return std::nullopt;
}

std::optional<std::string> Filesystem::disk_path(std::string_view name,
                                                  bool gamedironly) const
{
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        auto found = it->backend->find_file(name);
        if (!found) continue;
        // Only return a path if the file actually lives on disk (not in an archive).
        const std::string disk = xash::utilities::path_join(it->source_path, *found);
        if (platform::file_size(disk)) return disk;
    }
    return std::nullopt;
}

SearchResult Filesystem::search(std::string_view pattern,
                                 bool case_insensitive,
                                 bool gamedironly) const
{
    SearchResult result;
    std::unordered_set<std::string> seen;

    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (gamedironly && !any(it->flags & SearchPathFlags::GameDir)) continue;
        for (auto& m : it->backend->search(pattern, case_insensitive)) {
            if (seen.insert(m).second)
                result.files.push_back(std::move(m));
        }
    }
    return result;
}

bool Filesystem::rename(std::string_view from, std::string_view to) {
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (any(it->flags & SearchPathFlags::NoWrite)) continue;
        auto found = it->backend->find_file(from);
        if (!found) continue;
        const std::string src = xash::utilities::path_join(it->source_path, *found);
        if (!platform::file_size(src)) continue;
        const std::string dst = xash::utilities::path_join(it->source_path, to);
        const bool ok = platform::rename_file(src, dst);
        if (ok) {
            // invalidate CI cache for both the source and destination directories
            // so find_file picks up the new name and drops the old one.
            it->backend->invalidate_directory(parent_dir_of(from));
            const auto pd_to = parent_dir_of(to);
            if (pd_to != parent_dir_of(from))
                it->backend->invalidate_directory(pd_to);
        }
        return ok;
    }
    return false;
}

bool Filesystem::remove(std::string_view path) {
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (any(it->flags & SearchPathFlags::NoWrite)) continue;
        auto found = it->backend->find_file(path);
        if (!found) continue;
        const std::string disk = xash::utilities::path_join(it->source_path, *found);
        if (!platform::file_size(disk)) continue;
        return platform::delete_file(disk);
    }
    return false;
}

std::optional<std::uint32_t> Filesystem::crc32_file(std::string_view path) {
    const auto data = load_file(path);
    if (data.empty()) return std::nullopt;
    return xash::utilities::crc32(data.data(), data.size());
}

std::optional<std::array<std::byte, 16>> Filesystem::md5_file(std::string_view path) {
    const auto data = load_file(path);
    if (data.empty()) return std::nullopt;
    const auto digest = xash::utilities::Md5Hasher::hash(data.data(), data.size());
    std::array<std::byte, 16> out{};
    for (std::size_t i = 0; i < 16; ++i)
        out[i] = static_cast<std::byte>(digest[i]);
    return out;
}

std::optional<std::string> Filesystem::find_library(std::string_view name) {
    // Snapshot game state under shared lock.
    GameInfo g;
    {
        std::shared_lock lock{impl_->game_mutex};
        if (!impl_->game_loaded) return std::nullopt;
        g = impl_->active_game;
    }
    // rootdir is init-only — read directly without a lock.
    const std::string_view rootdir = impl_->rootdir;

    // Check dll_path sub-directory within the gamedir.
    if (!g.dll_path.empty()) {
        const std::string candidate =
            xash::utilities::path_join(
                xash::utilities::path_join(rootdir, g.gamefolder, g.dll_path), name);
        if (platform::file_size(candidate)) return candidate;
    }

    // Check gamedir root.
    {
        const std::string candidate =
            xash::utilities::path_join(rootdir, g.gamefolder, name);
        if (platform::file_size(candidate)) return candidate;
    }

    // Walk Exec-capable search paths.
    std::shared_lock lock{ impl_->paths_mutex };
    for (auto it = impl_->search_paths.rbegin();
             it != impl_->search_paths.rend(); ++it) {
        if (!any(it->flags & SearchPathFlags::Exec)) continue;
        auto found = it->backend->find_file(name);
        if (!found) continue;
        const std::string disk = xash::utilities::path_join(it->source_path, *found);
        if (platform::file_size(disk)) return disk;
    }
    return std::nullopt;
}

std::string Filesystem::gamedir() const {
    std::shared_lock lock{impl_->game_mutex};
    return impl_->gamedir;
}

GameInfo Filesystem::get_game_info() const {
    std::shared_lock lock{impl_->game_mutex};
    return impl_->active_game;
}

std::string_view Filesystem::get_root_directory() const {
    return impl_->rootdir;
}

const FilesystemStats& Filesystem::stats() const noexcept {
    return impl_->stats_;
}

} // namespace xash::filesystem
