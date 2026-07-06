// xash3dpp — Quake PAK archive backend
// Legacy reference: filesystem/pak.c  (FS_LoadPackPAK, FS_FindFile_PAK,
//                                      FS_OpenFile_PAK, FS_Search_PAK)
//
// PAK entries are raw, uncompressed blobs stored at a fixed offset within the
// archive file.  OpenFile returns a streaming OsFile positioned at that offset
// so callers get seek/read semantics without loading the whole entry.

#include <xash3dpp/private/filesystem/backends/pak_backend.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/filesystem/archive_helpers.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/os_file_factory.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>   // memcpy
#include <cstdio>    // SEEK_SET
#include <filesystem>

namespace xash::filesystem::backends {

using ::xash::platform::OsFd;

// ---------------------------------------------------------------------------
// On-disk format — all fields little-endian
// ---------------------------------------------------------------------------

static_assert(std::endian::native == std::endian::little,
    "PAK magic constant assumes little-endian byte order");
static constexpr std::uint32_t k_IDPACK =
    std::bit_cast<std::uint32_t>(std::array<char,4>{'P','A','C','K'});

static constexpr int k_MAX_FILES = static_cast<int>(xash::limits::pak_max_files);

struct DiskHeader {
    std::uint32_t ident;
    std::int32_t  dirofs;
    std::int32_t  dirlen;
};
static_assert(sizeof(DiskHeader) == 12);

struct DiskEntry {
    char          name[56];
    std::int32_t  filepos;
    std::int32_t  filelen;
};
static_assert(sizeof(DiskEntry) == 64);

// ---------------------------------------------------------------------------
// Constructor — open, validate, read directory, sort entries
// ---------------------------------------------------------------------------

PakBackend::PakBackend(xash::memory::PoolHandle pool,
                       std::string_view pak_path, SearchPathFlags flags)
    : ISearchBackend{pool}, path_{pak_path}, flags_{flags}
{
    OsFd fd = ::xash::platform::open_file(path_, ::xash::platform::OpenMode::ReadOnly);
    if (!fd.valid()) return;

    DiskHeader hdr{};
    if (::xash::platform::read(fd, &hdr, sizeof(hdr)) != static_cast<std::int64_t>(sizeof(hdr)))
        return;

    if (hdr.ident != k_IDPACK) return;

    if (hdr.dirlen % sizeof(DiskEntry) != 0) return;

    const int numfiles = hdr.dirlen / static_cast<int>(sizeof(DiskEntry));
    if (numfiles <= 0 || numfiles > k_MAX_FILES) return;

    if (::xash::platform::seek(fd, static_cast<std::int64_t>(hdr.dirofs), SEEK_SET) < 0)
        return;

    std::vector<DiskEntry> raw(static_cast<std::size_t>(numfiles));
    const std::int64_t dir_bytes = static_cast<std::int64_t>(hdr.dirlen);
    if (::xash::platform::read(fd, raw.data(), static_cast<std::size_t>(dir_bytes)) != dir_bytes)
        return;

    entries_.reserve(static_cast<std::size_t>(numfiles));
    for (const auto& de : raw) {
        const std::size_t len = static_cast<std::size_t>(
            std::find(std::begin(de.name), std::end(de.name), '\0') - de.name);
        entries_.push_back(Entry{
            std::string(de.name, len),
            static_cast<std::uint32_t>(de.filepos),
            static_cast<std::uint32_t>(de.filelen)
        });
    }

    // Sort case-insensitively — mirrors FS_SortPak(Q_stricmp) in pak.c
    std::sort(entries_.begin(), entries_.end(), CiNameLess<Entry>{});

    if (auto ft = ::xash::platform::file_time(path_))
        file_time_ = *ft;

    valid_ = true;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<ISearchBackend>
PakBackend::create(xash::memory::PoolHandle pool,
                   std::string_view path, SearchPathFlags flags) {
    auto* raw = xash::memory::pool_new<PakBackend>( pool, pool, path, flags );
    if (!raw) return nullptr;
    // compliance-allow(raw-new-delete): ISearchBackend defines a pool-aware
    // operator delete (mem_free); `delete raw` on the failed-construction path
    // runs ~PakBackend + mem_free — the same deallocation the success-path
    // unique_ptr's deleter performs. Correct pairing with pool_new.
    if (!raw->valid_) { delete raw; return nullptr; }
    return std::unique_ptr<ISearchBackend>{ raw };
}

std::unique_ptr<ISearchBackend>
create_pak(xash::memory::PoolHandle pool,
           std::string_view path, SearchPathFlags flags) {
    return PakBackend::create(pool, path, flags);
}

// ---------------------------------------------------------------------------
// Private helper — binary search (case-insensitive), mirrors FS_FindFile_PAK
// ---------------------------------------------------------------------------

const PakBackend::Entry*
PakBackend::find_entry(std::string_view name) const noexcept {
    return ci_find_by_name(entries_, name);
}

// ---------------------------------------------------------------------------
// ISearchBackend interface
// ---------------------------------------------------------------------------

std::string PakBackend::info() const {
    return path_ + " (" + std::to_string(entries_.size()) + " files)";
}

std::unique_ptr<File>
PakBackend::open_file(std::string_view path, std::string_view mode) {
    // PAK archives are read-only.
    if (is_write_mode(mode))
        return nullptr;

    const Entry* e = find_entry(path);
    if (!e) return nullptr;

    OsFd fd = ::xash::platform::open_file(path_, ::xash::platform::OpenMode::ReadOnly);
    if (!fd.valid()) return nullptr;

    return create_os_file(pool_,
                        std::move(fd),
                        static_cast<FsOffset>(e->size),
                        static_cast<FsOffset>(e->offset));
}

std::optional<std::filesystem::file_time_type>
PakBackend::file_time(std::string_view path) {
    // PAK returns archive-level mtime for any existing entry.
    if (!find_entry(path)) return std::nullopt;
    return file_time_;
}

std::optional<std::string>
PakBackend::find_file(std::string_view path) {
    const Entry* e = find_entry(path);
    if (!e) return std::nullopt;
    return e->name;  // canonical (original case from on-disk directory)
}

std::vector<std::string>
PakBackend::search(std::string_view pattern, bool /*case_insensitive*/) {
    // Legacy FS_Search_PAK matched each entry (and every directory-prefix
    // thereof) against the pattern.  We replicate that: iterate entries, try
    // the full name and then progressively strip trailing path components.
    return archive_search_by_name(entries_, pattern);
}

std::vector<std::byte>
PakBackend::load_file(std::string_view path) {
    const Entry* e = find_entry(path);
    if (!e || e->size == 0) return {};

    OsFd fd = ::xash::platform::open_file(path_, ::xash::platform::OpenMode::ReadOnly);
    if (!fd.valid()) return {};

    if (::xash::platform::seek(fd, static_cast<std::int64_t>(e->offset), SEEK_SET) < 0)
        return {};

    std::vector<std::byte> buf(e->size);
    if (::xash::platform::read(fd, buf.data(), e->size) !=
            static_cast<std::int64_t>(e->size))
        return {};

    return buf;
}

} // namespace xash::filesystem::backends
