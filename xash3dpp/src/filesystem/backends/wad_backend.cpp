// xash3dpp — WAD2 / WAD3 lump archive backend
// Legacy reference: filesystem/wad.c  (W_Open, W_FindLump, W_ReadLump, …)
//
// WAD entries are small discrete lumps (textures, fonts, palettes); there is
// no streaming API in the legacy engine (FS_OpenFile_WAD returned NULL).
// OpenFile() therefore loads the full lump into a MemFile.  All searches use
// a binary-searched, sorted-by-(name,type) entry table, mirroring the legacy
// W_FindLump / W_AddFileToWad logic.

#include <xash3dpp/private/filesystem/backends/wad_backend.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/filesystem/mem_file.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cctype>
#include <cstdio>    // SEEK_SET, SEEK_CUR, SEEK_END
#include <cstring>   // memcpy
#include <filesystem>
#include <span>

namespace xash::filesystem::backends {

namespace platform = ::xash::platform;
using ::xash::platform::OsFd;

// ---------------------------------------------------------------------------
// On-disk format — all fields little-endian; structs are naturally aligned
// ---------------------------------------------------------------------------

static_assert(std::endian::native == std::endian::little,
    "WAD magic constants assume little-endian byte order");
static constexpr std::uint32_t k_WAD2 =
    std::bit_cast<std::uint32_t>(std::array<char,4>{'W','A','D','2'});

static constexpr std::uint32_t k_WAD3 =
    std::bit_cast<std::uint32_t>(std::array<char,4>{'W','A','D','3'});

static constexpr int k_MAX_LUMPS = static_cast<int>(xash::limits::wad_max_lumps);

struct DiskHeader {
    std::uint32_t ident;
    std::int32_t  numlumps;
    std::int32_t  infotableofs;
};
static_assert(sizeof(DiskHeader) == 12);

struct DiskLump {
    std::int32_t filepos;
    std::int32_t disksize;
    std::int32_t size;       // uncompressed (== disksize in practice)
    std::int8_t  type;
    std::int8_t  attribs;
    char         : 8;        // pad0 — unused
    char         : 8;        // pad1 — unused
    char         name[16];   // NUL-padded, max 15 significant chars
};
static_assert(sizeof(DiskLump) == 32);

// ---------------------------------------------------------------------------
// Lump type mapping  (mirrors wad_types[] in wad.c / TYP_* in wadfile.h)
// ---------------------------------------------------------------------------

static constexpr std::uint8_t k_TYP_ANY     = 0xFFu; // -1 cast to uint8: "accept any"
static constexpr std::uint8_t k_TYP_NONE    =  0u;
static constexpr std::uint8_t k_TYP_PALETTE = 64u;
static constexpr std::uint8_t k_TYP_DDSTEX  = 65u;
static constexpr std::uint8_t k_TYP_GFXPIC  = 66u;
static constexpr std::uint8_t k_TYP_MIPTEX  = 67u;
static constexpr std::uint8_t k_TYP_SCRIPT  = 68u;
static constexpr std::uint8_t k_TYP_QFONT   = 70u;

struct WadTypeEntry { std::string_view ext; std::uint8_t type; };

static constexpr WadTypeEntry k_wad_types[] = {
    { "pal", k_TYP_PALETTE },
    { "dds", k_TYP_DDSTEX  },
    { "lmp", k_TYP_GFXPIC  },
    { "fnt", k_TYP_QFONT   },
    { "mip", k_TYP_MIPTEX  },
    { "txt", k_TYP_SCRIPT  },
};

static std::uint8_t type_from_ext(std::string_view ext) noexcept {
    if (ext.empty() || ext == "*") return k_TYP_ANY;
    for (const auto& e : k_wad_types)
        if (xash::utilities::ci_equal(e.ext, ext)) return e.type;
    return k_TYP_NONE;
}

static std::string_view ext_for_type(std::uint8_t type) noexcept {
    for (const auto& e : k_wad_types)
        if (e.type == type) return e.ext;
    return {};
}

// ---------------------------------------------------------------------------
// Lump name normalisation (mirrors W_Open loop in wad.c)
// ---------------------------------------------------------------------------

static std::string normalise_name(const char (&raw)[16]) {
    // Strip NUL padding
    std::size_t len = 0;
    while (len < 16 && raw[len] != '\0') ++len;
    std::string name(raw, len);

    // Lowercase (legacy: Q_strnlwr)
    xash::utilities::to_lower(name);

    // Quake1 sky/liquid names use '*'; replace with '!' to keep them sortable
    auto star = name.rfind('*');
    if (star != std::string::npos)
        name[star] = '!';

    return name;
}

// ---------------------------------------------------------------------------
// Constructor — open, validate, read LAT, sort entries
// ---------------------------------------------------------------------------

WadBackend::WadBackend(xash::memory::PoolHandle pool,
                       std::string_view wad_path, SearchPathFlags flags)
    : ISearchBackend{pool}, path_{wad_path}, flags_{flags}
{
    // Cache lowercase WAD stem (e.g. "halflife.wad" → "halflife")
    stem_ = xash::utilities::file_base(path_);
    xash::utilities::to_lower(stem_);

    OsFd fd = platform::open_file(path_, platform::OpenMode::ReadOnly);
    if (!fd.valid()) return;

    DiskHeader hdr{};
    if (platform::read(fd, &hdr, sizeof(hdr)) != static_cast<std::int64_t>(sizeof(hdr)))
        return;

    if (hdr.ident != k_WAD2 && hdr.ident != k_WAD3) return;

    const int numlumps = hdr.numlumps;
    if (numlumps <= 0 || numlumps > k_MAX_LUMPS) return;

    if (platform::seek(fd, static_cast<std::int64_t>(hdr.infotableofs), SEEK_SET) < 0)
        return;

    std::vector<DiskLump> raw(static_cast<std::size_t>(numlumps));
    const std::int64_t lat_bytes = static_cast<std::int64_t>(numlumps) * sizeof(DiskLump);
    if (platform::read(fd, raw.data(), static_cast<std::size_t>(lat_bytes)) != lat_bytes)
        return;

    entries_.reserve(static_cast<std::size_t>(numlumps));
    for (const auto& dl : raw) {
        std::string name = normalise_name(dl.name);
        auto type = static_cast<std::uint8_t>(dl.type);

        // Quake1: "conchars" was mis-typed as TYP_SCRIPT; fix it to TYP_GFXPIC
        if (type == k_TYP_SCRIPT && name == "conchars")
            type = k_TYP_GFXPIC;

        entries_.push_back(Entry{
            std::move(name),
            static_cast<std::uint32_t>(dl.filepos),
            static_cast<std::uint32_t>(dl.disksize),
            static_cast<std::uint32_t>(dl.size),
            static_cast<std::uint8_t>(dl.attribs),
            type
        });
    }

    // Sort by (name, type) — mirrors W_AddFileToWad insertion sort in wad.c
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        const int nc = a.name.compare(b.name);
        if (nc != 0) return nc < 0;
        return a.type < b.type;
    });

    if (auto ft = platform::file_time(path_))
        file_time_ = *ft;

    valid_ = true;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<ISearchBackend>
WadBackend::Create(xash::memory::PoolHandle pool,
                   std::string_view path, SearchPathFlags flags) {
    auto* raw = xash::memory::pool_new<WadBackend>( pool, pool, path, flags );
    if (!raw) return nullptr;
    if (!raw->valid_) { delete raw; return nullptr; }
    return std::unique_ptr<ISearchBackend>{ raw };
}

std::unique_ptr<ISearchBackend>
create_wad(xash::memory::PoolHandle pool,
           std::string_view path, SearchPathFlags flags) {
    return WadBackend::Create(pool, path, flags);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

const WadBackend::Entry*
WadBackend::find_entry(std::string_view name, std::uint8_t type) const noexcept {
    // Binary-search to first entry whose name >= query
    auto it = std::lower_bound(entries_.begin(), entries_.end(), name,
        [](const Entry& e, std::string_view n) { return e.name < n; });

    if (it == entries_.end() || it->name != name) return nullptr;

    if (type == k_TYP_ANY) return &*it;

    while (it != entries_.end() && it->name == name) {
        if (it->type == type) return &*it;
        ++it;
    }
    return nullptr;
}

const WadBackend::Entry*
WadBackend::lookup(std::string_view path) const noexcept {
    using namespace xash::utilities;

    // Extract extension for type determination
    std::string_view ext = file_extension(path);
    if (!ext.empty() && ext[0] == '.') ext.remove_prefix(1);
    const std::uint8_t type = type_from_ext(ext);
    if (type == k_TYP_NONE) return nullptr;

    // Split path into directory qualifier + filename
    std::string_view file_part = path;
    auto slash = path.rfind('/');
    if (slash == std::string_view::npos) slash = path.rfind('\\');

    if (slash != std::string_view::npos) {
        const std::string_view dir_part = path.substr(0, slash);
        file_part = path.substr(slash + 1);

        // Strip to bare stem of the last directory component
        std::string dir_no_ext = strip_extension(dir_part);
        std::string_view dir_stem = filename(dir_no_ext);

        // If a WAD qualifier is given it must match our stem
        if (!dir_stem.empty() && !ci_equal(dir_stem, stem_))
            return nullptr;
    }

    // Build lowercase lump name (no extension)
    std::string name = strip_extension(filename(file_part));
    to_lower(name);

    return find_entry(name, type);
}

std::vector<std::byte>
WadBackend::read_lump_bytes(const Entry& e) const {
    OsFd fd = platform::open_file(path_, platform::OpenMode::ReadOnly);
    if (!fd.valid()) return {};

    if (platform::seek(fd, static_cast<std::int64_t>(e.offset), SEEK_SET) < 0)
        return {};

    std::vector<std::byte> buf(e.disk_size);
    if (platform::read(fd, buf.data(), e.disk_size) !=
            static_cast<std::int64_t>(e.disk_size))
        return {};

    return buf;
}

// ---------------------------------------------------------------------------
// ISearchBackend interface
// ---------------------------------------------------------------------------

std::string WadBackend::Info() const {
    return path_ + " (" + std::to_string(entries_.size()) + " files)";
}

std::unique_ptr<File>
WadBackend::OpenFile(std::string_view path, std::string_view mode) {
    // WAD is read-only
    if (is_write_mode(mode))
        return nullptr;

    const Entry* e = lookup(path);
    if (!e) return nullptr;

    auto data = read_lump_bytes(*e);
    if (data.empty() && e->disk_size > 0) return nullptr;

    return std::unique_ptr<File>{
        xash::memory::pool_new<MemFile>( pool_, std::move(data) ) };
}

std::optional<std::filesystem::file_time_type>
WadBackend::FileTime(std::string_view path) {
    if (!lookup(path)) return std::nullopt;
    return file_time_;
}

std::optional<std::string>
WadBackend::FindFile(std::string_view path) {
    const Entry* e = lookup(path);
    if (!e) return std::nullopt;
    return e->name;
}

std::vector<std::string>
WadBackend::Search(std::string_view pattern, bool /*case_insensitive*/) {
    using namespace xash::utilities;

    // Split the pattern into optional WAD-qualifier and the bare filename glob.
    // e.g. "textures/*.mip"  →  qualifier="textures", bare="*.mip"
    std::string      wad_qualifier;
    std::string_view bare_pattern = pattern;

    auto slash = pattern.rfind('/');
    if (slash == std::string_view::npos) slash = pattern.rfind('\\');
    if (slash != std::string_view::npos) {
        const std::string_view dir_part = pattern.substr(0, slash);
        bare_pattern = pattern.substr(slash + 1);

        std::string dir_no_ext = strip_extension(dir_part);
        std::string_view dir_stem = filename(dir_no_ext);

        if (!dir_stem.empty()) {
            if (!ci_equal(dir_stem, stem_)) return {};  // quick reject
            wad_qualifier = std::string{dir_stem};
        }
    }

    // Infer type from glob extension for early filtering
    std::string_view ext = file_extension(bare_pattern);
    if (!ext.empty() && ext[0] == '.') ext.remove_prefix(1);
    const std::uint8_t type = type_from_ext(ext);
    if (type == k_TYP_NONE) return {};

    std::vector<std::string> results;
    for (const auto& e : entries_) {
        if (type != k_TYP_ANY && e.type != type) continue;

        // Build "name.ext" for this entry
        const std::string_view entry_ext = ext_for_type(e.type);
        const std::string entry_file = entry_ext.empty()
            ? e.name
            : e.name + "." + std::string{entry_ext};

        if (!match_pattern(entry_file, bare_pattern, /*case_insensitive=*/true))
            continue;

        results.push_back(wad_qualifier.empty()
            ? entry_file
            : wad_qualifier + "/" + entry_file);
    }
    return results;
}

std::vector<std::byte>
WadBackend::LoadFile(std::string_view path) {
    const Entry* e = lookup(path);
    if (!e) return {};
    return read_lump_bytes(*e);
}

} // namespace xash::filesystem::backends
