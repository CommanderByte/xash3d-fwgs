// xash3dpp — BSP heap-builder stages: entities, planes, submodels,
// visibility, marksurfaces, leafs, nodes.
// Legacy reference: engine/common/mod_bmodel.c
//   Mod_LoadEntities (:2347-2468), Mod_LoadPlanes (:2475-2502),
//   Mod_LoadSubmodels (:2259-2309), Mod_LoadVisibility (:3921-3929),
//   Mod_LoadMarkSurfaces (:2594-2635), Mod_LoadLeafs (:3626-3721),
//   Mod_LoadNodes (:3515-3619).
//
// Parity notes (Known Deviations in the boundary doc):
//  - node plane/child indices are validated at load (legacy builds raw
//    pointers and crashes later on corrupt input);
//  - node parent links (Mod_SetParent) are not built — consumed only by the
//    renderer/efrag path, not by trace or PVS queries;
//  - worldspawn scan captures "wad" (raw, unsplit) and "message" only;
//    compiler/generator/litwater diagnostics keys are not retained;
//  - malformed world entity text returns BspBadWorld instead of Host_Error.

#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstring>
#include <string>

namespace xash::map_loader::bsp {

using ::xash::core::ErrorCode;
using ::xash::core::LogLevel;

namespace {

// Legacy `token[0]` tests are applied to possibly-empty token text.
[[nodiscard]] char first_char( std::string_view s ) noexcept
{
    return s.empty() ? '\0' : s[0];
}

template <typename T>
[[nodiscard]] T read_record( std::span<const std::byte> bytes, std::size_t index ) noexcept
{
    T out;
    std::memcpy( &out, bytes.data() + index * sizeof( T ), sizeof( T ));
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------

void WorldDataFill::begin( const LoadContext &ctx, World &w )
{
    w.version_ = ctx.hi.version;
    w.name_.assign( ctx.name );
}

// ---------------------------------------------------------------------------
// entities — Mod_LoadEntities
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::entities( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_entities );
    if ( !lv )
        return std::unexpected( lv.error() );

    // Raw text copy; std::string guarantees the trailing NUL the legacy
    // loader appends manually.  Absent lump → empty string (legacy parity:
    // fileofs 0 silently yields empty entdata).
    w.entities_.assign( reinterpret_cast<const char *>( lv->bytes.data() ),
                        lv->bytes.size() );

    if ( !ctx.opts.is_world )
        return {};

    // Worldspawn key scan — first entity block only (legacy returns after
    // the first closing brace).  Tokenization via utilities::Tokenizer, the
    // project's COM_ParseFileSafe port (escaped quotes, NUL handling and
    // single-char tokens follow legacy; token length caps at
    // limits::tokenizer_token_max vs the legacy MAX_TOKEN 2048).
    ::xash::utilities::Tokenizer tk( w.entities_.c_str() );
    std::string keyname;

    const auto first = tk.next();
    if ( !first )
        return {}; // empty entities: nothing to scan (legacy loop never runs)

    if ( first_char( first->text ) != '{' )
    {
        ::xash::core::logf( LogLevel::Error, "map_loader",
                            "entities: found '%.*s' when expecting '{'",
                            static_cast<int>( first->text.size() ), first->text.data() );
        return std::unexpected( ErrorCode::BspBadWorld );
    }

    for ( ;; )
    {
        const auto key = tk.next();
        if ( !key )
        {
            ::xash::core::log( LogLevel::Error, "map_loader",
                               "entities: EOF without closing brace" );
            return std::unexpected( ErrorCode::BspBadWorld );
        }
        if ( first_char( key->text ) == '}' )
            break;
        keyname.assign( key->text );

        const auto value = tk.next();
        if ( !value )
        {
            ::xash::core::log( LogLevel::Error, "map_loader",
                               "entities: EOF without closing brace" );
            return std::unexpected( ErrorCode::BspBadWorld );
        }
        if ( first_char( value->text ) == '}' )
        {
            ::xash::core::log( LogLevel::Error, "map_loader",
                               "entities: closing brace without data" );
            return std::unexpected( ErrorCode::BspBadWorld );
        }

        if ( ::xash::utilities::stricmp( keyname.c_str(), "wad" ) == 0 )
            w.wadlist_.assign( value->text );
        else if ( ::xash::utilities::stricmp( keyname.c_str(), "message" ) == 0 )
            w.message_.assign( value->text );
    }

    return {};
}

// ---------------------------------------------------------------------------
// planes — Mod_LoadPlanes
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::planes( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_planes );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    w.planes_.resize( lv->count );
    for ( std::size_t i = 0; i < lv->count; ++i )
    {
        const auto in  = read_record<dplane_t>( lv->bytes, i );
        Plane      &out = w.planes_[i];

        out.signbits = 0;
        out.normal   = { in.normal[0], in.normal[1], in.normal[2] };
        if ( in.normal[0] < 0.0f ) out.signbits |= 1u << 0;
        if ( in.normal[1] < 0.0f ) out.signbits |= 1u << 1;
        if ( in.normal[2] < 0.0f ) out.signbits |= 1u << 2;

        // Legacy logs and proceeds (Mod_LoadPlanes:2496-2497).
        if ( ::xash::utilities::length( out.normal ) < 0.5f )
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "planes: bad normal for plane #%zu", i );

        out.dist = in.dist;
        out.type = static_cast<std::uint8_t>( in.type );
    }
    return {};
}

// ---------------------------------------------------------------------------
// submodels — Mod_LoadSubmodels
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::submodels( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_models );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    w.submodels_.resize( lv->count );
    for ( std::size_t i = 0; i < lv->count; ++i )
    {
        const auto in   = read_record<dmodel_t>( lv->bytes, i );
        SubModel   &out = w.submodels_[i];

        for ( int j = 0; j < 3; ++j )
        {
            // Reset empty bounds, then spread by one unit (legacy).
            float mn = in.mins[j];
            float mx = in.maxs[j];
            if ( mn == 999999.0f )
                mn = 0.0f;
            if ( mx == -999999.0f )
                mx = 0.0f;

            ( j == 0 ? out.mins.x : j == 1 ? out.mins.y : out.mins.z ) = mn - 1.0f;
            ( j == 0 ? out.maxs.x : j == 1 ? out.maxs.y : out.maxs.z ) = mx + 1.0f;
            ( j == 0 ? out.origin.x : j == 1 ? out.origin.y : out.origin.z ) = in.origin[j];
        }

        for ( int j = 0; j < k_max_map_hulls; ++j )
            out.headnode[static_cast<std::size_t>( j )] = in.headnode[j];

        out.visleafs  = in.visleafs;
        out.firstface = in.firstface;
        out.numfaces  = in.numfaces;
    }
    return {};
}

// ---------------------------------------------------------------------------
// visibility — Mod_LoadVisibility
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::visibility( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_visibility );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    w.visdata_.assign( lv->bytes.begin(), lv->bytes.end() );
    return {};
}

// ---------------------------------------------------------------------------
// marksurfaces — Mod_LoadMarkSurfaces
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::marksurfaces( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_marksurfaces );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    // Validation is against the FACE RECORD COUNT from the directory — face
    // contents themselves load in C6 (render-flag subset).
    const auto faces = resolve_lump( ctx.file, ctx.hi, k_lump_faces );
    if ( !faces )
        return std::unexpected( faces.error() );
    const int numsurfaces = static_cast<int>( faces->count );

    w.marksurfaces_.resize( lv->count );

    if ( lv->entrysize == sizeof( dmarkface32_t ))
    {
        for ( std::size_t i = 0; i < lv->count; ++i )
        {
            const auto v = read_record<dmarkface32_t>( lv->bytes, i );
            if ( v < 0 || v >= numsurfaces )
            {
                ::xash::core::logf( LogLevel::Error, "map_loader",
                                    "marksurfaces: bad surface number %d at %zu (max %d)",
                                    v, i, numsurfaces );
                return std::unexpected( ErrorCode::BspCorruptLump );
            }
            w.marksurfaces_[i] = v;
        }
    }
    else
    {
        for ( std::size_t i = 0; i < lv->count; ++i )
        {
            const auto v = read_record<dmarkface_t>( lv->bytes, i );

            // Broken-compiler fix-up (darkf6/darkf26.bsp): negative 16-bit
            // surface index remapped to surface 0 when the count fits int16.
            if ( numsurfaces <= INT16_MAX && static_cast<std::int16_t>( v ) < 0 )
            {
                ::xash::core::logf( LogLevel::Warning, "map_loader",
                                    "marksurfaces: fixing up bad surface number %u at %zu (max %d)",
                                    v, i, numsurfaces );
                w.marksurfaces_[i] = 0;
                continue;
            }

            if ( v >= numsurfaces )
            {
                ::xash::core::logf( LogLevel::Error, "map_loader",
                                    "marksurfaces: bad surface number %u at %zu (max %d)",
                                    v, i, numsurfaces );
                return std::unexpected( ErrorCode::BspCorruptLump );
            }
            w.marksurfaces_[i] = v;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// leafs — Mod_LoadLeafs
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::leafs( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_leafs );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    int visclusters = 0;
    if ( ctx.opts.is_world )
    {
        // Requires submodels — legacy load order guarantees it.
        visclusters    = w.submodels_.empty() ? 0 : w.submodels_[0].visleafs;
        w.visclusters_ = visclusters;
        w.visbytes_    = ( static_cast<std::size_t>( visclusters ) + 7 ) >> 3;
    }

    const bool wide = lv->entrysize == sizeof( dleaf32_t );
    w.leafs_.resize( lv->count );

    for ( std::size_t i = 0; i < lv->count; ++i )
    {
        Leaf &out = w.leafs_[i];
        int   visofs;

        if ( wide )
        {
            const auto in = read_record<dleaf32_t>( lv->bytes, i );
            out.contents = in.contents;
            visofs       = in.visofs;
            out.mins     = { in.mins[0], in.mins[1], in.mins[2] };
            out.maxs     = { in.maxs[0], in.maxs[1], in.maxs[2] };
            for ( int j = 0; j < 4; ++j )
                out.ambient_sound_level[static_cast<std::size_t>( j )] = in.ambient_level[j];
            out.firstmarksurface = in.firstmarksurface;
            out.nummarksurfaces  = in.nummarksurfaces;
        }
        else
        {
            const auto in = read_record<dleaf_t>( lv->bytes, i );
            out.contents = in.contents;
            visofs       = in.visofs;
            out.mins     = { static_cast<float>( in.mins[0] ),
                             static_cast<float>( in.mins[1] ),
                             static_cast<float>( in.mins[2] ) };
            out.maxs     = { static_cast<float>( in.maxs[0] ),
                             static_cast<float>( in.maxs[1] ),
                             static_cast<float>( in.maxs[2] ) };
            for ( int j = 0; j < 4; ++j )
                out.ambient_sound_level[static_cast<std::size_t>( j )] = in.ambient_level[j];
            out.firstmarksurface = in.firstmarksurface;
            out.nummarksurfaces  = in.nummarksurfaces;
        }

        if ( ctx.opts.is_world )
        {
            out.cluster = static_cast<int>( i ) - 1; // solid leaf 0 has no visdata
            if ( out.cluster >= visclusters )
                out.cluster = -1;

            // Legacy only WARNS on out-of-range visofs (leaf 0 excluded).
            if ( visofs >= 0 && out.cluster >= 0 && !w.visdata_.empty() &&
                 static_cast<std::size_t>( visofs ) >= w.visdata_.size() )
            {
                ::xash::core::logf( LogLevel::Warning, "map_loader",
                                    "leafs: invalid visofs for leaf #%zu", i );
            }
        }
        else
        {
            out.cluster = -1; // no visclusters on bmodels
        }

        // Raw, unclamped — legacy parity (final compressed_vis assignment).
        out.visofs = visofs;

        // GL underwater warp: mark every surface referenced by a non-empty
        // leaf (legacy :3704-3712, world and bmodels alike).  Marksurface
        // range clamped to the loaded array (hardening; legacy indexes raw
        // pointers).
        if ( out.contents != k_contents_empty )
        {
            for ( int j = 0; j < out.nummarksurfaces; ++j )
            {
                const long long idx =
                    static_cast<long long>( out.firstmarksurface ) + j;
                if ( idx < 0 ||
                     idx >= static_cast<long long>( w.marksurfaces_.size() ))
                    break;
                const int si = w.marksurfaces_[static_cast<std::size_t>( idx )];
                w.surfaces_[static_cast<std::size_t>( si )].flags |= k_surf_underwater;
            }
        }
    }

    // Legacy Host_Error → BspBadWorld.
    if ( ctx.opts.is_world && w.leafs_[0].contents != k_contents_solid )
    {
        ::xash::core::log( LogLevel::Error, "map_loader",
                           "leafs: leaf 0 is not CONTENTS_SOLID" );
        return std::unexpected( ErrorCode::BspBadWorld );
    }

    // Water-alpha probe — Mod_CheckWaterAlphaSupport (:1412-1437): a map
    // supports r_wateralpha when any liquid leaf can see an empty leaf.
    // No visdata at all counts as supported.
    if ( ctx.opts.is_world )
    {
        bool wateralpha = w.visdata_.empty();

        if ( !wateralpha )
        {
            std::vector<std::byte> vis( w.visbytes_ );
            for ( const Leaf &leaf : w.leafs_ )
            {
                if (( leaf.contents != k_contents_water &&
                      leaf.contents != k_contents_slime ) || leaf.cluster < 0 )
                    continue;

                // Legacy passes leaf->compressed_vis = visdata + visofs with
                // no clamp; an in-range offset sees the identical byte
                // stream, an out-of-range one is treated like a missing vis
                // pointer → full visibility (hardening — legacy reads out
                // of buffer).
                std::span<const std::byte> in{};
                if ( leaf.visofs >= 0 &&
                     static_cast<std::size_t>( leaf.visofs ) < w.visdata_.size() )
                    in = std::span<const std::byte>( w.visdata_ )
                             .subspan( static_cast<std::size_t>( leaf.visofs ));

                decompress_pvs( in, w.visbytes_, vis );

                for ( const Leaf &other : w.leafs_ )
                {
                    const int c = other.cluster;
                    const bool visible = c >= 0 &&
                        ( static_cast<unsigned char>( vis[static_cast<std::size_t>( c ) >> 3] ) &
                          ( 1u << ( static_cast<unsigned>( c ) & 7u ))) != 0;
                    if ( visible && other.contents == k_contents_empty )
                    {
                        wateralpha = true;
                        break;
                    }
                }
                if ( wateralpha )
                    break;
            }
        }

        if ( wateralpha )
            w.flags_ |= k_fworld_wateralpha;
    }

    return {};
}

// ---------------------------------------------------------------------------
// nodes — Mod_LoadNodes
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::nodes( const LoadContext &ctx, World &w )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_nodes );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    const bool wide     = lv->entrysize == sizeof( dnode32_t );
    const int  numnodes = static_cast<int>( lv->count );
    const int  numleafs = static_cast<int>( w.leafs_.size() );
    const int  numplanes = static_cast<int>( w.planes_.size() );

    w.nodes_.resize( lv->count );

    for ( std::size_t i = 0; i < lv->count; ++i )
    {
        Node &out = w.nodes_[i];

        if ( wide )
        {
            const auto in = read_record<dnode32_t>( lv->bytes, i );
            out.planenum     = in.planenum;
            out.children[0]  = in.children[0];
            out.children[1]  = in.children[1];
            out.mins         = { in.mins[0], in.mins[1], in.mins[2] };
            out.maxs         = { in.maxs[0], in.maxs[1], in.maxs[2] };
            out.firstsurface = in.firstface;
            out.numsurfaces  = in.numfaces;
        }
        else
        {
            const auto in = read_record<dnode_t>( lv->bytes, i );
            out.planenum     = in.planenum;
            out.children[0]  = in.children[0];
            out.children[1]  = in.children[1];
            out.mins         = { static_cast<float>( in.mins[0] ),
                                 static_cast<float>( in.mins[1] ),
                                 static_cast<float>( in.mins[2] ) };
            out.maxs         = { static_cast<float>( in.maxs[0] ),
                                 static_cast<float>( in.maxs[1] ),
                                 static_cast<float>( in.maxs[2] ) };
            out.firstsurface = in.firstface;
            out.numsurfaces  = in.numfaces;
        }

        // Index validation (hardening; legacy builds raw pointers unchecked).
        if ( out.planenum < 0 || out.planenum >= numplanes )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "nodes: bad plane index %d on node %zu", out.planenum, i );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
        for ( int j = 0; j < 2; ++j )
        {
            const int p = out.children[j];
            if ( p >= 0 ? p >= numnodes : ( -1 - p ) >= numleafs )
            {
                ::xash::core::logf( LogLevel::Error, "map_loader",
                                    "nodes: bad child %d on node %zu", p, i );
                return std::unexpected( ErrorCode::BspCorruptLump );
            }
        }
    }

    // Mod_SetParent is deliberately not replicated: parent links are consumed
    // only by the renderer/efrag path (Known Deviation).
    return {};
}

// ---------------------------------------------------------------------------
// finalize — required-lump presence (hardening; see header note)
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::finalize( const LoadContext &, World &w )
{
    const bool ok = !w.planes_.empty() && !w.nodes_.empty() &&
                    !w.leafs_.empty() && !w.submodels_.empty();
    if ( !ok )
    {
        ::xash::core::log( LogLevel::Error, "map_loader",
                           "load: map is missing a required lump (planes/nodes/leafs/models)" );
        return std::unexpected( ErrorCode::BspBadWorld );
    }
    return {};
}

} // namespace xash::map_loader::bsp
