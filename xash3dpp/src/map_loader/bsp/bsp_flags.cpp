// xash3dpp — texture names, texinfo and the surface-flag subset.
// Legacy reference: engine/common/mod_bmodel.c
//   Mod_LoadTextures/Mod_LoadTexture (:3167-3212, :2985-3016) — name
//   extraction only (texel upload is render-side; a dedicated server skips
//   it too, :2913-2916); Mod_GetMipTexForTexture (:604-613);
//   Mod_LoadTexInfo (:3318-3358); Mod_LoadSurfaces flag block (:3384-3451);
//   Mod_LooksLikeWaterTexture (:2637-2649); CRC32_MapFile (:4093).
//
// Parity notes (Known Deviations in the boundary doc):
//  - only miptex NAMES are read (lowercased, "*default" for missing data,
//    "miptex_N" for empty names — exact legacy fallbacks); no texel data;
//  - texinfo/face indices are validated (legacy builds raw pointers);
//  - the Quake-compatibility runtime toggle is not modeled: water-name
//    matching always uses GoldSrc rules ("water"/"laser" prefixes count);
//  - surface extents/bevels/lightmaps are not computed (render-side); the
//    legacy corrupt-face guard (firstedge+numedges > numsurfedges → skip)
//    IS replicated using the surfedge record count.

#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>
#include <xash3dpp/private/map_loader/bsp/map_crc.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace xash::map_loader::bsp {

using ::xash::core::ErrorCode;
using ::xash::core::LogLevel;

namespace {

[[nodiscard]] std::string lowercase( std::string_view s )
{
    std::string out( s );
    for ( char &c : out )
        c = static_cast<char>( std::tolower( static_cast<unsigned char>( c )));
    return out;
}

// Mod_LooksLikeWaterTexture, GoldSrc mode (Quake-compat toggle not modeled).
// Note the legacy asymmetry: "water" matches case-SENSITIVELY, "laser"
// case-insensitively (names are lowercased at load, so it rarely matters).
[[nodiscard]] bool looks_like_water( const char *name ) noexcept
{
    if (( name[0] == '*' && ::xash::utilities::stricmp( name, "*default" ) != 0 ) ||
        name[0] == '!' )
        return true;
    if ( std::strncmp( name, "water", 5 ) == 0 )
        return true;
    return ::xash::utilities::strnicmp( name, "laser", 5 ) == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// textures — names only
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::textures( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_textures );
    if ( !lv )
        return std::unexpected( lv.error() );

    // Legacy: no lump / nummiptex < 1 → mod->textures = NULL.
    if ( !lv->present || lv->bytes.size() < sizeof( std::int32_t ))
        return {};

    const auto nummiptex = read_record_at<std::int32_t>( lv->bytes, 0 );
    if ( nummiptex < 1 )
        return {};

    // dataofs[] directly follows nummiptex; bounds-check the directory
    // itself (hardening; legacy trusts it).
    const std::size_t dir_bytes = sizeof( std::int32_t ) * ( 1 + static_cast<std::size_t>( nummiptex ));
    if ( dir_bytes > lv->bytes.size() )
    {
        ::xash::core::log( LogLevel::Error, "map_loader",
                           "textures: miptex directory extends past the lump" );
        return std::unexpected( ErrorCode::BspCorruptLump );
    }

    w.texture_names_.resize( static_cast<std::size_t>( nummiptex ));
    for ( std::int32_t i = 0; i < nummiptex; ++i )
    {
        const auto dataofs = read_record_at<std::int32_t>(
            lv->bytes, sizeof( std::int32_t ) * ( 1 + static_cast<std::size_t>( i )));

        // Missing data → default texture (legacy Mod_CreateDefaultTexture).
        // Out-of-lump offsets get the same treatment (hardening).
        if ( dataofs < 0 ||
             static_cast<std::size_t>( dataofs ) + sizeof( mip_t ) > lv->bytes.size() )
        {
            if ( dataofs >= 0 )
                ::xash::core::logf( LogLevel::Warning, "map_loader",
                                    "textures: miptex %d offset out of range", i );
            w.texture_names_[static_cast<std::size_t>( i )] = "*default";
            continue;
        }

        const auto mip = read_record_at<mip_t>( lv->bytes, static_cast<std::size_t>( dataofs ));
        char name[17];
        std::memcpy( name, mip.name, 16 );
        name[16] = '\0';

        if ( name[0] == '\0' )
        {
            // legacy: unnamed miptex → "miptex_%i"
            ::xash::utilities::snprintf( name, sizeof name, "miptex_%d", i );
        }

        w.texture_names_[static_cast<std::size_t>( i )] = lowercase( name );
    }

    return {};
}

// ---------------------------------------------------------------------------
// texinfo — miptex clamp + TEX_* flags
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::texinfo( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_texinfo );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    const int numtextures = static_cast<int>( w.texture_names_.size() );

    w.texinfos_.resize( lv->count );
    for ( std::size_t i = 0; i < lv->count; ++i )
    {
        const auto in = read_record<dtexinfo_t>( lv->bytes, i );
        int miptex = in.miptex;
        if ( miptex < 0 || miptex >= numtextures )
            miptex = 0; // legacy clamp ("this is possible?")
        w.texinfos_[i] = { miptex, in.flags };
    }
    return {};
}

// ---------------------------------------------------------------------------
// surfaces — SURF_* flag subset
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::surfaces( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_faces );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    // The corrupt-face guard compares against the surfedge record count;
    // the records themselves are render-side and stay unloaded.
    const auto surfedges = resolve_lump( ctx.file, ctx.hi, k_lump_surfedges );
    if ( !surfedges )
        return std::unexpected( surfedges.error() );
    const int numsurfedges = static_cast<int>( surfedges->count );

    const bool wide        = lv->entrysize == sizeof( dface32_t );
    const int  numtexinfo  = static_cast<int>( w.texinfos_.size() );
    const int  numplanes   = static_cast<int>( w.planes_.size() );

    w.surfaces_.resize( lv->count );
    for ( std::size_t i = 0; i < lv->count; ++i )
    {
        Surface &out = w.surfaces_[i];
        out = { 0, 0, 0 };

        int planenum, side, texinfo_index, firstedge, numedges;
        if ( wide )
        {
            const auto in = read_record<dface32_t>( lv->bytes, i );
            planenum      = in.planenum;
            side          = in.side;
            texinfo_index = in.texinfo;
            firstedge     = in.firstedge;
            numedges      = in.numedges;
        }
        else
        {
            const auto in = read_record<dface_t>( lv->bytes, i );
            planenum      = in.planenum;
            side          = in.side;
            texinfo_index = in.texinfo;
            firstedge     = in.firstedge;
            numedges      = in.numedges;
        }

        // Legacy corrupt-face guard: fields stay zeroed, no flags derived.
        // Widened addition — a crafted firstedge near INT_MAX must trip the
        // guard, not overflow (legacy adds raw ints, UB on such input).
        if ( static_cast<long long>( firstedge ) + numedges > numsurfedges )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "surfaces: bad surface %zu of %zu", i, lv->count );
            continue;
        }

        // Index validation (hardening; legacy builds raw pointers).
        if ( planenum < 0 || planenum >= numplanes ||
             texinfo_index < 0 || texinfo_index >= numtexinfo )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "surfaces: bad plane/texinfo index on surface %zu", i );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }

        out.planenum = planenum;
        out.texinfo  = texinfo_index;

        if ( side != 0 )
            out.flags |= k_surf_planeback;

        const TexInfo &ti  = w.texinfos_[static_cast<std::size_t>( texinfo_index )];
        const char *name   = w.texture_names_.empty()
            ? ""
            : w.texture_names_[static_cast<std::size_t>( ti.miptex )].c_str();

        if ( std::strncmp( name, "sky", 3 ) == 0 )
            out.flags |= k_surf_drawsky;
        if ( looks_like_water( name ))
            out.flags |= k_surf_drawturb;
        if ( std::strncmp( name, "scroll", 6 ) == 0 )
            out.flags |= k_surf_conveyor;
        if (( ti.flags & k_tex_scroll ) != 0 )
            out.flags |= k_surf_conveyor;
        if ( std::strncmp( name, "{scroll", 7 ) == 0 )
            out.flags |= k_surf_conveyor | k_surf_transparent;
        if ( name[0] == '{' )
            out.flags |= k_surf_transparent;
        if (( ti.flags & k_tex_special ) != 0 )
            out.flags |= k_surf_drawtiled;
    }

    return {};
}

// ---------------------------------------------------------------------------
// checksum — CRC32_MapFile
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::checksum( const LoadContext &ctx, World &w )
{
    w.checksum_ = ctx.opts.multiplayer_crc
        ? map_checksum_multiplayer( ctx.file, ctx.hi.header )
        : k_map_crc_singleplayer;
    return {};
}

} // namespace xash::map_loader::bsp
