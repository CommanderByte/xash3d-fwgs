// xash3dpp — BSP header parsing, version/quirk detection, lump resolution
// Legacy reference: engine/common/mod_bmodel.c
//   Mod_LoadBmodelLumps (:4242-4295) — version dispatch, BSP30ext id probe,
//   Blue-Shift swap detection; Mod_LoadLump (:760-952) — per-lump entry-size
//   resolution + validation ladder; Mod_LumpLooksLikeEntities (:4064-4068).
//
// Existing subsystems used:
//   xash3dpp_core — core::logf diagnostics (tag "map_loader")

#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>

#include <xash3dpp/core/log.hpp>

#include <algorithm>
#include <cstring>

namespace xash::map_loader::bsp {

namespace {

// Legacy Q_memmem(lump, len, "\"classname\"", 11): the probe searches for the
// QUOTED key — eleven bytes including both quotes.
constexpr char k_classname_needle[] = "\"classname\"";
constexpr std::size_t k_classname_len = sizeof( k_classname_needle ) - 1;

[[nodiscard]] bool looks_like_entities( std::span<const std::byte> lump ) noexcept
{
    if ( lump.size() < k_classname_len )
        return false;
    const auto *needle = reinterpret_cast<const std::byte *>( k_classname_needle );
    return std::search( lump.begin(), lump.end(), needle, needle + k_classname_len )
           != lump.end();
}

// Raw directory-entry bytes, clamped to the file image.  Legacy reads with
// no bounds validation; an out-of-range entry here yields an empty span so
// downstream probes (Blue-Shift detection) simply see "no match".
[[nodiscard]] std::span<const std::byte>
lump_bytes_clamped( std::span<const std::byte> file, const dlump_t &l ) noexcept
{
    if ( l.fileofs <= 0 || l.filelen <= 0 )
        return {};
    const auto ofs = static_cast<std::size_t>( l.fileofs );
    if ( ofs >= file.size() )
        return {};
    const std::size_t avail = file.size() - ofs;
    const std::size_t len   = std::min( avail, static_cast<std::size_t>( l.filelen ));
    return file.subspan( ofs, len );
}

} // namespace

std::expected<HeaderInfo, ::xash::core::ErrorCode>
parse_header( std::span<const std::byte> file ) noexcept
{
    using ::xash::core::ErrorCode;

    if ( file.size() < sizeof( dheader_t ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "parse_header: file too small for BSP header (%zu bytes)",
                            file.size() );
        return std::unexpected( ErrorCode::BspCorruptLump );
    }

    HeaderInfo hi{};
    std::memcpy( &hi.header, file.data(), sizeof( dheader_t ));
    hi.version_raw = hi.header.version;

    switch ( hi.version_raw )
    {
    case k_q1bsp_version:
        hi.version = BspVersion::Quake1;
        break;

    case k_qbsp2_version:
        hi.version     = BspVersion::Bsp2;
        hi.clipnodes32 = true;
        break;

    case k_hlbsp_version:
    {
        hi.version = BspVersion::HalfLife;

        // BSP30ext probe: legacy reads ONLY the 4-byte id at offset
        // sizeof(dheader_t) for this flag (mod_bmodel.c:4263); the extra
        // header's own version field gates extra-LUMP loading, not this.
        std::int32_t extident = 0;
        if ( file.size() >= sizeof( dheader_t ) + sizeof( extident ))
            std::memcpy( &extident, file.data() + sizeof( dheader_t ), sizeof( extident ));

        if ( extident == k_extra_header_id )
        {
            hi.version  = BspVersion::HalfLifeExt;
            hi.bsp30ext = true;

            // Extended clipnode guess (mod_bmodel.c:826-833): 16-bit
            // interpretation does not divide evenly, or the 12-byte count
            // reaches the 16-bit cap.
            const std::int32_t cliplen = hi.header.lumps[k_lump_clipnodes].filelen;
            if ( cliplen > 0 &&
                 (( cliplen % static_cast<std::int32_t>( sizeof( dclipnode_t ))) != 0 ||
                  ( cliplen / static_cast<std::int32_t>( sizeof( dclipnode32_t ))) >= k_max_map_clipnodes_hlbsp ))
            {
                hi.clipnodes32 = true;
            }
        }
        else
        {
            // Blue-Shift probe (v30, non-ext only): entities lump does NOT
            // contain "classname" but the planes lump DOES → the two
            // directory entries are swapped (mod_bmodel.c:4268-4273).
            const auto ents   = lump_bytes_clamped( file, hi.header.lumps[k_lump_entities] );
            const auto planes = lump_bytes_clamped( file, hi.header.lumps[k_lump_planes] );
            if ( !looks_like_entities( ents ) && looks_like_entities( planes ))
                hi.blueshift_swap = true;
        }
        break;
    }

    default:
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "parse_header: unsupported BSP version %d",
                            hi.version_raw );
        return std::unexpected( ErrorCode::BspUnsupportedVersion );
    }

    return hi;
}

std::expected<LumpView, ::xash::core::ErrorCode>
resolve_lump( std::span<const std::byte> file, const HeaderInfo &hi, int lump ) noexcept
{
    using ::xash::core::ErrorCode;

    if ( lump < 0 || lump >= k_header_lumps )
        return std::unexpected( ErrorCode::InvalidArgument );

    const LumpInfo &info = k_src_lumps[lump];

    // Blue-Shift maps: entities and planes directory entries are swapped
    // (legacy LUMP_BSHIFT_SWAP handling, mod_bmodel.c:773-779).
    int dir_index = lump;
    if ( hi.blueshift_swap )
    {
        if ( lump == k_lump_entities )
            dir_index = k_lump_planes;
        else if ( lump == k_lump_planes )
            dir_index = k_lump_entities;
    }
    const dlump_t l = hi.header.lumps[dir_index];

    // fileofs == 0 marks the lump unused — silently absent, even for
    // required lumps (legacy checks this before any mincount reporting).
    if ( l.fileofs == 0 )
        return LumpView{};

    // Resolve real entry size (legacy "analyze real entrysize").
    std::size_t entrysize = info.entrysize;
    if ( hi.version == BspVersion::Bsp2 && info.entrysize32 > 0 )
        entrysize = info.entrysize32;
    else if ( hi.bsp30ext && lump == k_lump_clipnodes && hi.clipnodes32 )
        entrysize = info.entrysize32;

    // Lump not present (zero/negative length): error only when required.
    if ( l.filelen <= 0 )
    {
        if ( info.mincount > 0 && entrysize != 1 )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                                "resolve_lump: map has no %s", info.name );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
        return LumpView{};
    }

    // Bounds check against the file image.  Legacy has no such check (it
    // trusts fileofs/filelen) — hardening, recorded as a Known Deviation.
    if ( l.fileofs < 0 ||
         static_cast<std::size_t>( l.fileofs ) > file.size() ||
         static_cast<std::size_t>( l.filelen ) > file.size() - static_cast<std::size_t>( l.fileofs ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "resolve_lump: %s lump extends past end of file", info.name );
        return std::unexpected( ErrorCode::BspCorruptLump );
    }

    if ( static_cast<std::size_t>( l.filelen ) % entrysize != 0 )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "resolve_lump: %s lump size %d is not a multiple of %zu bytes",
                            info.name, l.filelen, entrysize );
        return std::unexpected( ErrorCode::BspCorruptLump );
    }

    const std::size_t numelems = static_cast<std::size_t>( l.filelen ) / entrysize;

    if ( numelems < static_cast<std::size_t>( info.mincount ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                            "resolve_lump: map has no %s", info.name );
        return std::unexpected( ErrorCode::BspCorruptLump );
    }

    if ( numelems > static_cast<std::size_t>( info.maxcount ))
    {
        if ( info.check_overflow )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                                "resolve_lump: map has too many %s", info.name );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
        ::xash::core::logf( ::xash::core::LogLevel::Warning, "map_loader",
                            "resolve_lump: map has too many %s", info.name );
    }

    LumpView out;
    out.bytes     = file.subspan( static_cast<std::size_t>( l.fileofs ),
                                  static_cast<std::size_t>( l.filelen ));
    out.count     = numelems;
    out.entrysize = entrysize;
    out.present   = true;
    return out;
}

} // namespace xash::map_loader::bsp
