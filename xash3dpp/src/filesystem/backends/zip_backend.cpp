// xash3dpp — ZIP / PK3 archive backend
// Legacy reference: filesystem/zip.c  (FS_LoadZip, FS_FindFile_ZIP,
//                                      FS_OpenFile_ZIP, FS_Search_ZIP,
//                                      FS_LoadZIPFile)
//
// ZIP loading is two-phase (matching legacy FS_LoadZip):
//   Phase 1 — scan the central directory for entry metadata (name, sizes,
//              compression method, local-header offset).
//   Phase 2 — seek to each local file header (LFH) to derive the real data
//              offset = lfh_offset + sizeof(LFH) + fname_len + extra_len.
// Only entries with non-zero uncompressed size are retained (replicates
// legacy's directory/zero-file skip).

#include <xash3dpp/private/filesystem/backends/zip_backend.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/filesystem/archive_helpers.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/os_file_factory.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <miniz.h>

#include <algorithm>
#include <cstdio>    // SEEK_SET, SEEK_CUR
#include <cstring>   // memcpy
#include <filesystem>

namespace xash::filesystem::backends {

using ::xash::platform::OsFd;

// ---------------------------------------------------------------------------
// On-disk structures — all little-endian; #pragma pack for ZIP's odd layout
// ---------------------------------------------------------------------------

#pragma pack(push, 1)

struct DiskLfh {
    std::uint32_t signature;     // 0x04034b50  "PK\x03\x04"
    std::uint16_t version_need;
    std::uint16_t flags;
    std::uint16_t compression;
    std::uint16_t mod_time;
    std::uint16_t mod_date;
    std::uint32_t crc32;
    std::uint32_t comp_size;
    std::uint32_t uncomp_size;
    std::uint16_t fname_len;
    std::uint16_t extra_len;
    // Followed by fname_len + extra_len bytes of variable data.
};
static_assert(sizeof(DiskLfh) == 30);

struct DiskCdfh {
    std::uint32_t signature;     // 0x02014b50  "PK\x01\x02"
    std::uint16_t version_made;
    std::uint16_t version_need;
    std::uint16_t flags;
    std::uint16_t compression;
    std::uint16_t mod_time;
    std::uint16_t mod_date;
    std::uint32_t crc32;
    std::uint32_t comp_size;
    std::uint32_t uncomp_size;
    std::uint16_t fname_len;
    std::uint16_t extra_len;
    std::uint16_t comment_len;
    std::uint16_t disk_start;
    std::uint16_t int_attr;
    std::uint32_t ext_attr;
    std::uint32_t lfh_offset;
    // Followed by fname_len + extra_len + comment_len bytes.
};
static_assert(sizeof(DiskCdfh) == 46);

struct DiskEocd {
    std::uint32_t signature;         // 0x06054b50  "PK\x05\x06"
    std::uint16_t disk_num;
    std::uint16_t start_disk;
    std::uint16_t records_this_disk;
    std::uint16_t total_records;
    std::uint32_t cd_size;
    std::uint32_t cd_offset;
    std::uint16_t comment_len;
};
static_assert(sizeof(DiskEocd) == 22);

#pragma pack(pop)

static constexpr std::uint32_t k_SIG_LFH  = 0x04034b50u;
static constexpr std::uint32_t k_SIG_CDFH = 0x02014b50u;
static constexpr std::uint32_t k_SIG_EOCD = 0x06054b50u;

static constexpr std::uint16_t k_METHOD_STORED   = 0;
static constexpr std::uint16_t k_METHOD_DEFLATED = 8;

// ---------------------------------------------------------------------------
// Constructor — open, parse central directory, sort entries
// ---------------------------------------------------------------------------

ZipBackend::ZipBackend(xash::memory::PoolHandle pool,
                       std::string_view zip_path, SearchPathFlags flags)
    : ISearchBackend{pool}, path_{zip_path}, flags_{flags}
{
    auto fsz = ::xash::platform::file_size(path_);
    if (!fsz || *fsz < static_cast<std::int64_t>(sizeof(DiskEocd))) return;

    const std::int64_t file_size = *fsz;

    OsFd fd = ::xash::platform::open_file(path_, ::xash::platform::OpenMode::ReadOnly);
    if (!fd.valid()) return;

    // --- Phase 0: scan backwards for EOCD signature -------------------------
    // Read the last min(22 + 65535, file_size) bytes into a buffer and search
    // backwards for "PK\x05\x06".

    const std::int64_t scan_len =
        std::min<std::int64_t>(static_cast<std::int64_t>(sizeof(DiskEocd)) + xash::limits::zip_eocd_scan_max,
                               file_size);
    const std::int64_t scan_start = file_size - scan_len;

    std::vector<std::uint8_t> scan_buf(static_cast<std::size_t>(scan_len));
    if (::xash::platform::seek(fd, scan_start, SEEK_SET) < 0) return;
    if (::xash::platform::read(fd, scan_buf.data(), scan_buf.size()) != scan_len) return;

    std::int64_t eocd_pos = -1;  // offset from scan_start
    for (std::int64_t i = scan_len - static_cast<std::int64_t>(sizeof(DiskEocd));
         i >= 0; --i)
    {
        if (scan_buf[i]   == 0x50 && scan_buf[i+1] == 0x4B &&
            scan_buf[i+2] == 0x05 && scan_buf[i+3] == 0x06)
        {
            eocd_pos = i;
            break;
        }
    }
    if (eocd_pos < 0) return;

    DiskEocd eocd{};
    std::memcpy(&eocd, scan_buf.data() + eocd_pos, sizeof(DiskEocd));
    if (eocd.total_records == 0) return;

    // --- Phase 1: read central directory ------------------------------------

    if (::xash::platform::seek(fd, static_cast<std::int64_t>(eocd.cd_offset), SEEK_SET) < 0)
        return;

    static constexpr std::size_t k_MAX_FNAME = xash::limits::zip_filename_max;

    // Temporary entries before LFH resolution.
    struct PhaseEntry {
        std::string   name;
        std::uint32_t lfh_offset  = 0;
        std::uint32_t comp_size   = 0;
        std::uint32_t uncomp_size = 0;
        std::uint16_t compression = 0;
    };

    std::vector<PhaseEntry> phase1;
    phase1.reserve(eocd.total_records);

    for (std::uint16_t i = 0; i < eocd.total_records; ++i) {
        DiskCdfh cdfh{};
        if (::xash::platform::read(fd, &cdfh, sizeof(cdfh)) !=
                static_cast<std::int64_t>(sizeof(cdfh))) return;
        if (cdfh.signature != k_SIG_CDFH) return;

        // Skip: directories and zero-byte files (mirrors legacy FS_LoadZip).
        const bool keep = cdfh.uncomp_size > 0 &&
                          cdfh.fname_len  > 0 &&
                          cdfh.fname_len  < static_cast<std::uint16_t>(k_MAX_FNAME);
        if (keep) {
            std::string name(static_cast<std::size_t>(cdfh.fname_len), '\0');
            if (::xash::platform::read(fd, name.data(), cdfh.fname_len) !=
                    static_cast<std::int64_t>(cdfh.fname_len)) return;
            phase1.push_back(PhaseEntry{
                std::move(name),
                cdfh.lfh_offset,
                cdfh.comp_size,
                cdfh.uncomp_size,
                cdfh.compression
            });
        } else {
            if (cdfh.fname_len &&
                ::xash::platform::seek(fd, cdfh.fname_len, SEEK_CUR) < 0) return;
        }

        // Skip extra field and per-entry comment.
        const std::int32_t skip = cdfh.extra_len + cdfh.comment_len;
        if (skip > 0 && ::xash::platform::seek(fd, skip, SEEK_CUR) < 0) return;
    }

    if (phase1.empty()) return;

    // --- Phase 2: resolve actual data offsets via local file headers --------

    entries_.reserve(phase1.size());
    for (const auto& pe : phase1) {
        if (::xash::platform::seek(fd, static_cast<std::int64_t>(pe.lfh_offset),
                           SEEK_SET) < 0) return;

        DiskLfh lfh{};
        if (::xash::platform::read(fd, &lfh, sizeof(lfh)) !=
                static_cast<std::int64_t>(sizeof(lfh))) return;
        if (lfh.signature != k_SIG_LFH) return;

        const std::uint32_t data_offset =
            pe.lfh_offset +
            static_cast<std::uint32_t>(sizeof(DiskLfh)) +
            lfh.fname_len + lfh.extra_len;

        entries_.push_back(Entry{
            pe.name,
            data_offset,
            pe.comp_size,
            pe.uncomp_size,
            lfh.compression   // use LFH compression flags (matches legacy)
        });
    }

    // Sort case-insensitively — mirrors FS_SortZip(Q_stricmp) in zip.c.
    std::sort(entries_.begin(), entries_.end(), CiNameLess<Entry>{});

    if (auto ft = ::xash::platform::file_time(path_))
        file_time_ = *ft;

    valid_ = true;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<ISearchBackend>
ZipBackend::create(xash::memory::PoolHandle pool,
                   std::string_view path, SearchPathFlags flags) {
    auto* raw = xash::memory::pool_new<ZipBackend>( pool, pool, path, flags );
    if (!raw) return nullptr;
    // compliance-allow(raw-new-delete): ISearchBackend defines a pool-aware
    // operator delete (mem_free); `delete raw` on the failed-construction path
    // runs ~ZipBackend + mem_free — the same deallocation the success-path
    // unique_ptr's deleter performs. Correct pairing with pool_new.
    if (!raw->valid_) { delete raw; return nullptr; }
    return std::unique_ptr<ISearchBackend>{ raw };
}

std::unique_ptr<ISearchBackend>
create_zip(xash::memory::PoolHandle pool,
           std::string_view path, SearchPathFlags flags) {
    return ZipBackend::create(pool, path, flags);
}

// ---------------------------------------------------------------------------
// Private helper — binary search (case-insensitive), mirrors FS_FindFile_ZIP
// ---------------------------------------------------------------------------

const ZipBackend::Entry*
ZipBackend::find_entry(std::string_view name) const noexcept {
    return ci_find_by_name(entries_, name);
}

// ---------------------------------------------------------------------------
// ISearchBackend interface
// ---------------------------------------------------------------------------

std::string ZipBackend::info() const {
    return path_ + " (" + std::to_string(entries_.size()) + " files)";
}

std::unique_ptr<File>
ZipBackend::open_file(std::string_view path, std::string_view mode) {
    // ZIP archives are read-only.
    if (is_write_mode(mode))
        return nullptr;

    const Entry* e = find_entry(path);
    if (!e) return nullptr;

    OsFd fd = ::xash::platform::open_file(path_, ::xash::platform::OpenMode::ReadOnly);
    if (!fd.valid()) return nullptr;

    const bool deflated = (e->method == k_METHOD_DEFLATED);
    return create_os_file(pool_,
                        std::move(fd),
                        static_cast<FsOffset>(e->uncomp_size),
                        static_cast<FsOffset>(e->data_offset),
                        deflated);
}

std::optional<std::filesystem::file_time_type>
ZipBackend::file_time(std::string_view path) {
    // Return archive-level mtime for any found entry (mirrors FS_FileTime_ZIP).
    if (!find_entry(path)) return std::nullopt;
    return file_time_;
}

std::optional<std::string>
ZipBackend::find_file(std::string_view path) {
    const Entry* e = find_entry(path);
    if (!e) return std::nullopt;
    return e->name;   // canonical (on-disk) name
}

std::vector<std::string>
ZipBackend::search(std::string_view pattern, bool /*case_insensitive*/) {
    // Same as FS_Search_ZIP: iterate all entries, try the full path then
    // progressively strip trailing path components so directory names match too.
    return archive_search_by_name(entries_, pattern);
}

std::vector<std::byte>
ZipBackend::load_file(std::string_view path) {
    const Entry* e = find_entry(path);
    if (!e || e->uncomp_size == 0) return {};

    OsFd fd = ::xash::platform::open_file(path_, ::xash::platform::OpenMode::ReadOnly);
    if (!fd.valid()) return {};

    if (::xash::platform::seek(fd, static_cast<std::int64_t>(e->data_offset), SEEK_SET) < 0)
        return {};

    if (e->method == k_METHOD_STORED) {
        std::vector<std::byte> buf(e->comp_size);
        if (::xash::platform::read(fd, buf.data(), e->comp_size) !=
                static_cast<std::int64_t>(e->comp_size))
            return {};
        return buf;
    }

    if (e->method == k_METHOD_DEFLATED) {
        // Read the compressed blob, then decompress in one shot with miniz tinfl.
        std::vector<std::byte> comp(e->comp_size);
        if (::xash::platform::read(fd, comp.data(), e->comp_size) !=
                static_cast<std::int64_t>(e->comp_size))
            return {};

        std::vector<std::byte> out(e->uncomp_size);
        const std::size_t written = tinfl_decompress_mem_to_mem(
            out.data(),   out.size(),
            comp.data(),  comp.size(),
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);   // raw deflate, no zlib header

        if (written == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) return {};

        return out;
    }

    // Unknown compression method — skip (matches legacy FS_LoadZIPFile).
    return {};
}

} // namespace xash::filesystem::backends
