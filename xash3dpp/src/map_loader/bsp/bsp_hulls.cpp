// xash3dpp — clipnode widening, hull-0 construction and per-submodel hull
// wiring.
// Legacy reference: engine/common/mod_bmodel.c
//   Mod_LoadClipnodes (:3875-3914) — widen every variant to 32-bit + aguirRe
//   broken-clipnode fix; Mod_MakeHull0 (:1907-1965); Mod_SetupHull
//   (:1972-2076) incl. the ZHLT empty-hull and BSP30ext per-hull remap
//   paths; Mod_SetupSubmodels (:2152-2245); CountClipNodes*_r /
//   CountDClipNodes_r / RemapClipNodes_r (:1812-1898);
//   Mod_FindModelOrigin (:1350-1402).
//
// Parity notes (Known Deviations in the boundary doc):
//  - clipnodes are kept 32-bit in memory permanently (legacy narrows back
//    to 16-bit inside model_t for non-BSP2 maps; no xash3dpp consumer needs
//    the narrow layout — a Chunk 6 ABI shim may produce one on demand);
//  - the source-record width follows the entry size resolved at the lump
//    level; legacy Mod_LoadClipnodes re-derives it as
//    (bsp30ext && count >= 32767) and would misread the pathological
//    filelen%8!=0 && count<32767 corner that its own guess accepts;
//  - BSP30ext per-hull remapped arrays are appended into the single shared
//    clipnodes() vector at a base offset (children and first/last shifted
//    uniformly — traversal is identical);
//  - count/remap recursions are iterative here (legacy recurses; a crafted
//    cyclic hull overflows its stack, we fail with BspCorruptLump).

#include <xash3dpp/private/map_loader/bsp/bsp_loader.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace xash::map_loader::bsp {

using ::xash::core::ErrorCode;
using ::xash::core::LogLevel;

namespace {

[[nodiscard]] bool vec_is_null( const ::xash::utilities::Vec3 &v ) noexcept
{
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

// Legacy CountClipNodes*_r / CountDClipNodes_r: counts every non-leaf node
// reachable from `headnode`, erroring at `cap`.  Iterative (see file note).
// `seed` mirrors the legacy call pattern where hull->lastclipnode is
// pre-seeded (Mod_SetupSubmodels seeds it with the headnode) and the cap
// check tests the RUNNING value — so the overflow threshold is
// seed + count == cap, and the return value is the final counter.
[[nodiscard]] std::expected<int, ErrorCode>
count_clipnodes( std::span<const ::xash::map_loader::ClipNode32> nodes,
                 int headnode, int seed, int cap, const char *caller ) noexcept
{
    int counter = seed;
    std::vector<int> stack;
    stack.push_back( headnode );

    while ( !stack.empty() )
    {
        const int num = stack.back();
        stack.pop_back();
        if ( num < 0 )
            continue; // leaf / contents

        if ( num >= static_cast<int>( nodes.size() ))
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "%s: clipnode index %d out of range", caller, num );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
        if ( counter == cap )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "%s: MAX_MAP_CLIPNODES (%d) limit exceeded", caller, cap );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
        ++counter;

        stack.push_back( nodes[static_cast<std::size_t>( num )].children[0] );
        stack.push_back( nodes[static_cast<std::size_t>( num )].children[1] );
    }
    return counter;
}

// Legacy RemapClipNodes_r: preorder re-emission of the subtree rooted at
// `headnode` into `out` (appended), children remapped to the new compact
// indices.  Emission order matches the legacy recursion (node, then the
// whole children[0] subtree, then children[1]).  Returns the remapped root.
[[nodiscard]] std::expected<int, ErrorCode>
remap_clipnodes( std::span<const ::xash::map_loader::ClipNode32> src,
                 std::vector<::xash::map_loader::ClipNode32> &out,
                 int headnode, int cap ) noexcept
{
    const int base = static_cast<int>( out.size() );

    struct Frame
    {
        int src_index;   // node still to emit (or leaf/contents)
        int parent;      // out-index whose child slot to patch (-1 = root)
        int slot;        // 0 or 1
    };
    std::vector<Frame> stack;
    stack.push_back( { headnode, -1, 0 } );

    int emitted = 0, root = headnode;

    while ( !stack.empty() )
    {
        const Frame f = stack.back();
        stack.pop_back();

        if ( f.src_index < 0 )
        {
            // leaf / contents value passes through unchanged
            if ( f.parent >= 0 )
                out[static_cast<std::size_t>( base + f.parent )].children[f.slot] = f.src_index;
            else
                root = f.src_index;
            continue;
        }

        if ( f.src_index >= static_cast<int>( src.size() ))
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "remap_clipnodes: clipnode index %d out of range", f.src_index );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
        if ( emitted == cap )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "remap_clipnodes: MAX_MAP_CLIPNODES (%d) limit exceeded", cap );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }

        const int c = emitted++;
        const auto &s = src[static_cast<std::size_t>( f.src_index )];
        out.push_back( { s.planenum, { 0, 0 } } );

        if ( f.parent >= 0 )
            out[static_cast<std::size_t>( base + f.parent )].children[f.slot] = base + c;
        else
            root = base + c;

        // children[1] pushed first so children[0]'s subtree emits first
        // (preorder, matching the legacy recursion order).
        stack.push_back( { s.children[1], c, 1 } );
        stack.push_back( { s.children[0], c, 0 } );
    }

    ( void ) root; // root == base for any non-leaf headnode (preorder emit)
    return emitted;
}

// Legacy `token[0]` tests are applied to possibly-empty token text.
[[nodiscard]] char first_char( std::string_view s ) noexcept
{
    return s.empty() ? '\0' : s[0];
}

// Legacy Mod_FindModelOrigin: scans the FULL entity text for an entity whose
// "model" key equals `modelname`, extracting its "origin".  Only applied
// when the current origin is null (legacy early-out).  Tokenization via
// utilities::Tokenizer (COM_ParseFileSafe port), matching Mod_LoadEntities.
[[nodiscard]] std::expected<void, ErrorCode>
find_model_origin( const std::string &entities, const char *modelname,
                   ::xash::utilities::Vec3 &origin ) noexcept
{
    if ( entities.empty() || !vec_is_null( origin ))
        return {};

    ::xash::utilities::Tokenizer tk( entities.c_str() );
    std::string keyname, value;

    for ( ;; )
    {
        const auto open = tk.next();
        if ( !open )
            return {}; // scanned every entity without a match

        if ( first_char( open->text ) != '{' )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "find_model_origin: found '%.*s' when expecting '{'",
                                static_cast<int>( open->text.size() ), open->text.data() );
            return std::unexpected( ErrorCode::BspBadWorld );
        }

        bool model_found = false;
        ::xash::utilities::Vec3 candidate{};

        for ( ;; )
        {
            const auto key = tk.next();
            if ( !key )
            {
                ::xash::core::log( LogLevel::Error, "map_loader",
                                   "find_model_origin: EOF without closing brace" );
                return std::unexpected( ErrorCode::BspBadWorld );
            }
            if ( first_char( key->text ) == '}' )
                break;
            keyname.assign( key->text );

            const auto val = tk.next();
            if ( !val )
            {
                ::xash::core::log( LogLevel::Error, "map_loader",
                                   "find_model_origin: EOF without closing brace" );
                return std::unexpected( ErrorCode::BspBadWorld );
            }
            if ( first_char( val->text ) == '}' )
            {
                ::xash::core::log( LogLevel::Error, "map_loader",
                                   "find_model_origin: closing brace without data" );
                return std::unexpected( ErrorCode::BspBadWorld );
            }
            value.assign( val->text );

            if ( ::xash::utilities::stricmp( keyname.c_str(), "model" ) == 0 &&
                 ::xash::utilities::stricmp( value.c_str(), modelname ) == 0 )
                model_found = true;

            if ( ::xash::utilities::stricmp( keyname.c_str(), "origin" ) == 0 )
            {
                // Legacy Q_atov( origin, token, 3 ).
                float v[3] = { 0.0f, 0.0f, 0.0f };
                ::xash::utilities::atov( std::span<float>( v, 3 ), value );
                candidate = { v[0], v[1], v[2] };
            }
        }

        if ( model_found )
        {
            origin = candidate;
            return {};
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// clipnodes — Mod_LoadClipnodes (widen to 32-bit; aguirRe fix on 16-bit)
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::clipnodes( const LoadContext &ctx, World &w,
                                                LoadScratch &s )
{
    const auto lv = resolve_lump( ctx.file, ctx.hi, k_lump_clipnodes );
    if ( !lv )
        return std::unexpected( lv.error() );
    if ( !lv->present )
        return {};

    const int numclipnodes = static_cast<int>( lv->count );
    s.clipnodes_widened.resize( lv->count );

    if ( lv->entrysize == sizeof( dclipnode32_t ))
    {
        for ( std::size_t i = 0; i < lv->count; ++i )
        {
            const auto in = read_record<dclipnode32_t>( lv->bytes, i );

            // 32-bit children have no aguirRe wrap; validate the node range
            // so the trace kernel can trust its indices (hardening — legacy
            // walks raw pointers).  Negative values are contents.
            if ( in.children[0] >= numclipnodes || in.children[1] >= numclipnodes )
            {
                ::xash::core::logf( LogLevel::Error, "map_loader",
                                    "clipnodes: bad child on clipnode %zu", i );
                return std::unexpected( ErrorCode::BspCorruptLump );
            }
            s.clipnodes_widened[i] = { in.planenum, { in.children[0], in.children[1] } };
        }
    }
    else
    {
        for ( std::size_t i = 0; i < lv->count; ++i )
        {
            const auto in = read_record<dclipnode_t>( lv->bytes, i );
            ::xash::map_loader::ClipNode32 &out = s.clipnodes_widened[i];
            out.planenum = in.planenum;

            for ( int j = 0; j < 2; ++j )
            {
                // aguirRe QBSP 'broken' clipnodes: children pass through an
                // unsigned-16 reinterpretation; anything at or above the
                // clipnode count wraps back to a negative value.  (This
                // also guarantees every non-negative child is in range.)
                int c = static_cast<std::uint16_t>( in.children[j] );
                if ( c >= numclipnodes )
                    c -= 65536;
                out.children[j] = c;
            }
        }
    }

    // Plane indices feed hull.planes[] lookups in the kernel — validate once
    // at load (hardening; legacy trusts them).
    const int numplanes = static_cast<int>( w.planes_.size() );
    for ( std::size_t i = 0; i < s.clipnodes_widened.size(); ++i )
    {
        const int p = s.clipnodes_widened[i].planenum;
        if ( p < 0 || p >= numplanes )
        {
            ::xash::core::logf( LogLevel::Error, "map_loader",
                                "clipnodes: bad plane index %d on clipnode %zu", p, i );
            return std::unexpected( ErrorCode::BspCorruptLump );
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// make_hull0 — Mod_MakeHull0 (drawing nodes duplicated as clipping hull 0)
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::make_hull0( const LoadContext &, World &w )
{
    w.hull0_nodes_.resize( w.nodes_.size() );

    for ( std::size_t i = 0; i < w.nodes_.size(); ++i )
    {
        const Node &in = w.nodes_[i];
        ::xash::map_loader::ClipNode32 &out = w.hull0_nodes_[i];

        out.planenum = in.planenum;
        for ( int j = 0; j < 2; ++j )
        {
            const int c = in.children[j];
            // node → node index; leaf → its CONTENTS value (indices were
            // validated during the node stage).
            out.children[j] = c >= 0
                ? c
                : w.leafs_[static_cast<std::size_t>( -1 - c )].contents;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// setup_submodels — Mod_SetupSubmodels + Mod_SetupHull
// ---------------------------------------------------------------------------

WorldDataFill::Result WorldDataFill::setup_submodels( const LoadContext &ctx, World &w,
                                                      LoadScratch &s )
{
    const bool bsp2     = ctx.hi.version == ::xash::map_loader::BspVersion::Bsp2;
    const bool bsp30ext = ctx.hi.bsp30ext;
    const int  numclipnodes = static_cast<int>( s.clipnodes_widened.size() );
    const int  hull0_cap    = bsp2 ? k_max_map_clipnodes_bsp2 : k_max_map_clipnodes_hlbsp;
    const int  remap_cap    = bsp2 ? k_max_map_clipnodes_bsp2 : k_max_map_clipnodes_hlbsp;

    // Non-BSP30ext maps share the widened array directly (legacy: world
    // hull 1 owns the allocation, everything else aliases it).
    if ( !bsp30ext )
        w.clipnodes_ = std::move( s.clipnodes_widened );

    // hull_bounds remap: BSP hull 1 ← usehull 0 (human), hull 2 ← usehull 3
    // (large), hull 3 ← usehull 1 (head/duck) — Mod_SetupHull:1976-1993.
    const ::xash::map_loader::HullBounds *bounds_for_hull[4] = {
        nullptr,
        &ctx.opts.hull_bounds[0],
        &ctx.opts.hull_bounds[3],
        &ctx.opts.hull_bounds[1],
    };

    for ( std::size_t i = 0; i < w.submodels_.size(); ++i )
    {
        SubModel &bm = w.submodels_[i];

        // ---- hull 0: shared drawing-node hull, per-submodel span --------
        {
            HullDescriptor &h0 = bm.hulls[0];
            const int headnode = bm.headnode[0];

            // Legacy: lastclipnode seeded with the headnode, then the count
            // recursion increments it → final = headnode + subtree count
            // (with the cap tested against the running value).
            const auto last = count_clipnodes( w.hull0_nodes_, headnode, headnode,
                                               hull0_cap, "setup_submodels(hull0)" );
            if ( !last )
                return std::unexpected( last.error() );

            h0.firstclipnode = headnode;
            h0.lastclipnode  = *last;
            h0.clip_mins     = {};
            h0.clip_maxs     = {};
            h0.present       = true;
        }

        // ---- hulls 1-3 ----------------------------------------------------
        for ( int hullnum = 1; hullnum < 4; ++hullnum )
        {
            HullDescriptor &h = bm.hulls[static_cast<std::size_t>( hullnum )];
            const int headnode = bm.headnode[static_cast<std::size_t>( hullnum )];

            h.clip_mins = bounds_for_hull[hullnum]->mins;
            h.clip_maxs = bounds_for_hull[hullnum]->maxs;

            // "No hull specified" (null bounds — e.g. the point-hull slot).
            if ( vec_is_null( h.clip_mins ) && vec_is_null( h.clip_maxs ))
            {
                h.present = false;
                continue;
            }

            // Assume no hull (legacy planes=NULL marker).
            h.firstclipnode = 0;
            h.lastclipnode  = 0;
            h.present       = false;

            // ZHLT weird empty hulls.
            if ( headnode >= numclipnodes )
                continue;

            if ( !bsp30ext )
            {
                // Simple route: span over the shared array.  Optimizer-made
                // -1 headnodes pass through raw (kernel treats them as an
                // immediate CONTENTS_EMPTY — legacy comment :2012-2014).
                h.firstclipnode = headnode;
                h.lastclipnode  = numclipnodes - 1;
                h.present       = true;
                continue;
            }

            // BSP30ext: per-submodel index space is still 16-bit — remap
            // the subtree into a compact array (appended at a base offset).
            if ( headnode == -1 || ( hullnum != 1 && headnode == 0 ))
                continue; // hull missed

            const int base = static_cast<int>( w.clipnodes_.size() );
            const auto emitted = remap_clipnodes( s.clipnodes_widened, w.clipnodes_,
                                                  headnode, remap_cap );
            if ( !emitted )
                return std::unexpected( emitted.error() );

            h.firstclipnode = base;          // legacy: 0 within the per-hull array
            h.lastclipnode  = base + *emitted;
            h.present       = true;
        }

        // ---- origin detection for inline "*N" models ----------------------
        if ( i != 0 )
        {
            char modelname[16];
            ::xash::utilities::snprintf( modelname, sizeof modelname, "*%zu", i );

            const auto r = find_model_origin( w.entities_, modelname, bm.origin );
            if ( !r )
                return std::unexpected( r.error() );

            if ( !vec_is_null( bm.origin ))
                bm.flags |= k_model_has_origin;

            // c2a1 doesn't have an origin brush; it's just placed at the
            // centre of the level (legacy HACKS_RELATED_HLMODS, always on).
            if ( i == 11 &&
                 ::xash::utilities::stricmp( w.name_.c_str(), "maps/c2a1.bsp" ) == 0 )
                bm.flags |= k_model_has_origin;
        }

        // Surface-derived model flags — submodels only (legacy loop is
        // gated on i != 0).  Range clamped to the loaded surface array
        // (hardening; legacy indexes raw pointers).
        if ( i != 0 )
        {
            const std::size_t begin =
                bm.firstface >= 0 ? static_cast<std::size_t>( bm.firstface ) : 0;
            const std::size_t end_face =
                bm.numfaces >= 0 ? begin + static_cast<std::size_t>( bm.numfaces ) : begin;

            for ( std::size_t f = begin; f < end_face && f < w.surfaces_.size(); ++f )
            {
                const std::uint32_t sf = w.surfaces_[f].flags;
                if (( sf & k_surf_conveyor ) != 0 )
                    bm.flags |= k_model_conveyor;
                if (( sf & k_surf_transparent ) != 0 )
                    bm.flags |= k_model_transparent;
                if (( sf & k_surf_drawturb ) != 0 )
                    bm.flags |= k_model_liquid;
            }
        }
    }

    return {};
}

} // namespace xash::map_loader::bsp
