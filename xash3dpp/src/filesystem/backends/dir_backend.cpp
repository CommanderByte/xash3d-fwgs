// xash3dpp — plain-directory search backend
// Legacy reference: filesystem/dir.c  (FS_FindFile_DIR, FS_Search_DIR,
//                                      FS_OpenFile_DIR, FS_FileTime_DIR)
//
// All path lookups are routed through CIDirectory so case-insensitive access
// works on Linux (emulated) as well as on Windows / macOS (native).
// Write-mode opens skip CI resolution and use the caller-supplied path directly,
// matching the legacy FS_OpenFile_DIR behaviour.

#include <xash3dpp/private/filesystem/backends/dir_backend.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/os_file_factory.hpp>
#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <string>
#include <string_view>

namespace xash::filesystem::backends {

namespace platform = ::xash::platform;
using ::xash::platform::OsFd;

// ---------------------------------------------------------------------------
// Construction / factory
// ---------------------------------------------------------------------------

DirBackend::DirBackend(xash::memory::PoolHandle pool,
                       std::string_view root_path, SearchPathFlags flags)
    : ISearchBackend{pool}, root_{root_path}, flags_{flags}, ci_{root_path}
{
    // Trim any trailing slash so path concatenation is uniform.
    while (!root_.empty() && (root_.back() == '/' || root_.back() == '\\'))
        root_.pop_back();
}

std::unique_ptr<ISearchBackend>
DirBackend::Create(xash::memory::PoolHandle pool,
                   std::string_view path, SearchPathFlags flags) {
    return std::unique_ptr<ISearchBackend>{
        xash::memory::pool_new<DirBackend>( pool, pool, path, flags ) };
}

// ---------------------------------------------------------------------------
// resolve_path — walk each component through CIDirectory
// ---------------------------------------------------------------------------

std::string DirBackend::resolve_path(std::string_view rel_path) {
    if (rel_path.empty()) return {};

    std::string      resolved;
    std::string_view remaining = rel_path;

    while (!remaining.empty()) {
        auto slash = remaining.find('/');
        if (slash == std::string_view::npos) slash = remaining.find('\\');

        std::string_view component;
        if (slash != std::string_view::npos) {
            component = remaining.substr(0, slash);
            remaining = remaining.substr(slash + 1);
        } else {
            component = remaining;
            remaining = {};
        }

        if (component.empty() || component == ".") continue;

        auto canon = ci_.Resolve(resolved, component);
        if (!canon) return {};  // component not found on disk

        if (!resolved.empty()) resolved += '/';
        resolved += *canon;
    }
    return resolved;
}

// ---------------------------------------------------------------------------
// ISearchBackend interface
// ---------------------------------------------------------------------------

std::string DirBackend::Info() const {
    return root_;
}

std::unique_ptr<File>
DirBackend::OpenFile(std::string_view path, std::string_view mode) {
    const bool is_write = is_write_mode(mode);

    std::string disk;
    FsOffset    length = 0;

    if (is_write) {
        // Writes use the path as-is (same as legacy FS_OpenFile_DIR).
        disk = xash::utilities::path_join(root_, path);
    } else {
        const std::string resolved = resolve_path(path);
        if (resolved.empty() && !path.empty()) return nullptr;
        disk = xash::utilities::path_join(root_, resolved);

        if (auto sz = platform::file_size(disk))
            length = static_cast<FsOffset>(*sz);
    }

    OsFd fd = platform::open_file(disk, mode_flags(mode));
    if (!fd.valid()) return nullptr;

    return make_os_file(pool_, std::move(fd), length);
}

std::optional<std::filesystem::file_time_type>
DirBackend::FileTime(std::string_view path) {
    const std::string resolved = resolve_path(path);
    if (resolved.empty() && !path.empty()) return std::nullopt;
    return platform::file_time(xash::utilities::path_join(root_, resolved));
}

std::optional<std::string>
DirBackend::FindFile(std::string_view path) {
    const std::string resolved = resolve_path(path);
    if (resolved.empty() && !path.empty()) return std::nullopt;

    // Confirm the file actually exists (catches stale cache and native-mode
    // paths where Resolve returns the name unchanged).
    if (!platform::file_size(xash::utilities::path_join(root_, resolved)).has_value())
        return std::nullopt;

    return resolved;
}

std::vector<std::string>
DirBackend::Search(std::string_view pattern, bool case_insensitive) {
    // Split "dir/prefix/*.ext" into the directory part and the filename glob.
    auto slash = pattern.rfind('/');
    if (slash == std::string_view::npos) slash = pattern.rfind('\\');

    std::string_view dir_prefix;
    std::string_view filename_glob;

    if (slash != std::string_view::npos) {
        dir_prefix    = pattern.substr(0, slash);
        filename_glob = pattern.substr(slash + 1);
    } else {
        filename_glob = pattern;
    }

    // Resolve the directory component (CI-safe, may be empty for root).
    std::string resolved_dir;
    if (!dir_prefix.empty()) {
        resolved_dir = resolve_path(dir_prefix);
        if (resolved_dir.empty()) return {};  // directory doesn't exist
    }

    // List entries in resolved_dir that match filename_glob.
    auto names = ci_.Glob(resolved_dir, filename_glob, case_insensitive);

    // Prepend the (resolved) directory prefix.
    std::vector<std::string> results;
    results.reserve(names.size());
    for (auto& n : names) {
        results.push_back(resolved_dir.empty()
            ? std::move(n)
            : xash::utilities::path_join(resolved_dir, n));
    }
    return results;
}

std::vector<std::byte>
DirBackend::LoadFile(std::string_view path) {
    const std::string resolved = resolve_path(path);
    if (resolved.empty() && !path.empty()) return {};

    const std::string disk = xash::utilities::path_join(root_, resolved);
    const auto sz = platform::file_size(disk);
    if (!sz || *sz == 0) return {};

    OsFd fd = platform::open_file(disk, platform::OpenMode::ReadOnly);
    if (!fd.valid()) return {};

    std::vector<std::byte> buf(static_cast<std::size_t>(*sz));
    if (platform::read(fd, buf.data(), buf.size()) !=
            static_cast<std::int64_t>(*sz))
        return {};

    return buf;
}

void DirBackend::InvalidateDirectory(std::string_view subdir) noexcept {
    ci_.Invalidate(subdir);
}

} // namespace xash::filesystem::backends
