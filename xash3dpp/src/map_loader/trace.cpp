// xash3dpp — clip-hull trace kernel
// Legacy reference: engine/common/pm_trace.c (function-by-function port;
// see trace.hpp).  Q-18 PARITY-CRITICAL: every float expression mirrors the
// legacy operation order; plane_diff keeps the axial fast path; the
// VectorLerp expansion is the exact legacy component form
// c = v1 + lerp * (v2 - v1).
//
// Parity notes (Known Deviations in the boundary doc):
//  - a bad node number logs an error and aborts the trace (returns false,
//    trace untouched) where legacy Host_Error kills the process;
//  - hull_point_contents bounds-trusts its indices per the TraceHull
//    precondition (loader-built hulls are validated at load; legacy trusts
//    raw pointers everywhere).

#include <xash3dpp/map_loader/trace.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/private/map_loader/trace_math.hpp>

namespace xash::map_loader {

using ::xash::utilities::Vec3;

namespace {

// Legacy VectorLerp( v1, lerp, v2, c ): c = v1 + lerp * (v2 - v1), per
// component, single precision.
[[nodiscard]] inline Vec3 vector_lerp( const Vec3 &v1, float lerp, const Vec3 &v2 ) noexcept
{
    return { v1.x + lerp * ( v2.x - v1.x ),
             v1.y + lerp * ( v2.y - v1.y ),
             v1.z + lerp * ( v2.z - v1.z ) };
}

} // namespace

// ---------------------------------------------------------------------------
// PM_HullPointContents
// ---------------------------------------------------------------------------

int hull_point_contents( const TraceHull &hull, int num, const Vec3 &p ) noexcept
{
    if ( hull.planes.empty() ) // legacy: !hull || !hull->planes ("fantom bmodels")
        return k_contents_none;

    while ( num >= 0 )
    {
        const ClipNode32 &node  = hull.clipnodes[static_cast<std::size_t>( num )];
        const Plane      &plane = hull.planes[static_cast<std::size_t>( node.planenum )];
        num = node.children[plane_diff( p, plane ) < 0.0f];
    }
    return num;
}

// ---------------------------------------------------------------------------
// PM_RecursiveHullCheck
// ---------------------------------------------------------------------------

bool recursive_hull_check( const TraceHull &hull, int num, float p1f, float p2f,
                           const Vec3 &p1, const Vec3 &p2, TraceResult &trace ) noexcept
{
    int   children[2];
    const Plane *plane;
    float t1, t2;

    // The legacy `goto loc0` tail loop: both-sides-same cases re-enter here.
    for ( ;; )
    {
        // check for empty
        if ( num < 0 )
        {
            if ( num != k_contents_solid )
            {
                trace.allsolid = false;
                if ( num == k_contents_empty )
                    trace.inopen = true;
                else
                    trace.inwater = true;
            }
            else
                trace.startsolid = true;
            return true; // empty
        }

        if ( hull.firstclipnode >= hull.lastclipnode )
        {
            // empty hull?
            trace.allsolid = false;
            trace.inopen   = true;
            return true;
        }

        if ( num < hull.firstclipnode || num > hull.lastclipnode )
        {
            // legacy: Host_Error (process kill); we abort the trace.
            ::xash::core::logf( ::xash::core::LogLevel::Error, "map_loader",
                                "recursive_hull_check: bad node number %i", num );
            return false;
        }

        // find the point distances
        const ClipNode32 &node = hull.clipnodes[static_cast<std::size_t>( num )];
        children[0] = node.children[0];
        children[1] = node.children[1];
        plane       = &hull.planes[static_cast<std::size_t>( node.planenum )];

        t1 = plane_diff( p1, *plane );
        t2 = plane_diff( p2, *plane );

        if ( t1 >= 0.0f && t2 >= 0.0f )
        {
            num = children[0];
            continue;
        }
        if ( t1 < 0.0f && t2 < 0.0f )
        {
            num = children[1];
            continue;
        }
        break; // the segment crosses the plane
    }

    // put the crosspoint DIST_EPSILON pixels on the near side
    const int side = ( t1 < 0.0f );

    float frac;
    if ( side )
        frac = ( t1 + k_dist_epsilon ) / ( t1 - t2 );
    else
        frac = ( t1 - k_dist_epsilon ) / ( t1 - t2 );

    if ( frac < 0.0f )
        frac = 0.0f;
    if ( frac > 1.0f )
        frac = 1.0f;

    float midf = p1f + ( p2f - p1f ) * frac;
    Vec3  mid  = vector_lerp( p1, frac, p2 );

    // move up to the node
    if ( !recursive_hull_check( hull, children[side], p1f, midf, p1, mid, trace ))
        return false;

    if ( hull_point_contents( hull, children[side ^ 1], mid ) != k_contents_solid )
    {
        // go past the node
        return recursive_hull_check( hull, children[side ^ 1], midf, p2f, mid, p2, trace );
    }

    // never got out of the solid area
    if ( trace.allsolid )
        return false;

    // the other side of the node is solid, this is the impact point
    if ( !side )
    {
        trace.plane.normal = plane->normal;
        trace.plane.dist   = plane->dist;
    }
    else
    {
        trace.plane.normal = { -plane->normal.x, -plane->normal.y, -plane->normal.z };
        trace.plane.dist   = -plane->dist;
    }

    while ( hull_point_contents( hull, hull.firstclipnode, mid ) == k_contents_solid )
    {
        // shouldn't really happen, but does occasionally
        frac -= 0.1f;

        if ( frac < 0.0f )
        {
            trace.fraction = midf;
            trace.endpos   = mid;
            ::xash::core::log( ::xash::core::LogLevel::Warning, "map_loader",
                               "trace backed up past 0.0" );
            return false;
        }

        midf = p1f + ( p2f - p1f ) * frac;
        mid  = vector_lerp( p1, frac, p2 );
    }

    trace.fraction = midf;
    trace.endpos   = mid;

    return false;
}

// ---------------------------------------------------------------------------
// trace_hull / finalize_trace
// ---------------------------------------------------------------------------

TraceResult trace_hull( const TraceHull &hull, const Vec3 &start_local,
                        const Vec3 &end_local ) noexcept
{
    // PM_InitPMTrace: zeroed, endpos = end, allsolid = true, fraction = 1.
    TraceResult trace{};
    trace.endpos = end_local;

    ( void ) recursive_hull_check( hull, hull.firstclipnode, 0.0f, 1.0f,
                                   start_local, end_local, trace );
    return trace;
}

void finalize_trace( TraceResult &tr, const Vec3 &start_world,
                     const Vec3 &end_world ) noexcept
{
    if ( tr.allsolid )
        tr.startsolid = true;

    if ( tr.startsolid )
        tr.fraction = 0.0f;

    if ( !tr.startsolid )
    {
        tr.endpos = vector_lerp( start_world, tr.fraction, end_world );
        tr.plane.dist =
            tr.endpos.x * tr.plane.normal.x +
            tr.endpos.y * tr.plane.normal.y +
            tr.endpos.z * tr.plane.normal.z;
    }
}

// ---------------------------------------------------------------------------
// hull selection
// ---------------------------------------------------------------------------

TraceHull world_hull( const WorldData &w, std::size_t submodel, int bsp_hull ) noexcept
{
    TraceHull out{};
    if ( submodel >= w.submodels().size() || bsp_hull < 0 || bsp_hull > 3 )
        return out;

    const HullDescriptor &d =
        w.submodels()[submodel].hulls[static_cast<std::size_t>( bsp_hull )];

    out.firstclipnode = d.firstclipnode;
    out.lastclipnode  = d.lastclipnode;
    out.clip_mins     = d.clip_mins;
    out.clip_maxs     = d.clip_maxs;

    if ( !d.present )
        return out; // empty spans: legacy planes == NULL marker

    out.planes    = w.planes();
    out.clipnodes = bsp_hull == 0 ? w.hull0_nodes() : w.clipnodes();
    return out;
}

HullSelection hull_for_bsp( const WorldData &w, std::size_t submodel, int usehull,
                            const HullBounds &player_bounds,
                            const Vec3 &model_origin ) noexcept
{
    // PM_HullForBsp usehull → BSP hull switch (:153-167).
    int bsp_hull;
    switch ( usehull )
    {
    case 1:  bsp_hull = 3; break; // ducked → head hull
    case 2:  bsp_hull = 0; break; // point hull
    case 3:  bsp_hull = 2; break; // large hull
    default: bsp_hull = 1; break; // standing → human hull
    }

    HullSelection sel{ world_hull( w, submodel, bsp_hull ), {} };

    // offset = hull.clip_mins - player_mins[usehull] + origin (VectorSubtract
    // then VectorAdd — component order preserved).
    sel.offset = { ( sel.hull.clip_mins.x - player_bounds.mins.x ) + model_origin.x,
                   ( sel.hull.clip_mins.y - player_bounds.mins.y ) + model_origin.y,
                   ( sel.hull.clip_mins.z - player_bounds.mins.z ) + model_origin.z };
    return sel;
}

// ---------------------------------------------------------------------------
// BoxHull
// ---------------------------------------------------------------------------

BoxHull::BoxHull() noexcept
{
    // box_clipnodes chain (mod_bmodel.c BOX_CLIPNODES_INITIALIZER): plane i,
    // one child continues the chain, the other is contents; the final node
    // closes on CONTENTS_SOLID.
    clipnodes_ = { {
        { 0, { k_contents_empty, 1 } },
        { 1, { 2, k_contents_empty } },
        { 2, { k_contents_empty, 3 } },
        { 3, { 4, k_contents_empty } },
        { 4, { k_contents_empty, 5 } },
        { 5, { k_contents_solid, k_contents_empty } },
    } };

    // PM_InitBoxHull plane setup: type = i>>1, unit axial normals, signbits 0.
    for ( int i = 0; i < 6; ++i )
    {
        Plane &p = planes_[static_cast<std::size_t>( i )];
        p        = {};
        p.type   = static_cast<std::uint8_t>( i >> 1 );
        ( i >> 1 ) == 0 ? p.normal.x = 1.0f
        : ( i >> 1 ) == 1 ? p.normal.y = 1.0f
                          : p.normal.z = 1.0f;
        p.signbits = 0;
    }

    hull_.clipnodes     = clipnodes_;
    hull_.planes        = planes_;
    hull_.firstclipnode = 0;
    hull_.lastclipnode  = 5;
}

const TraceHull &BoxHull::set_bounds( const Vec3 &mins, const Vec3 &maxs ) noexcept
{
    // PM_HullForBox distance layout: maxs/mins interleaved per axis.
    planes_[0].dist = maxs.x;
    planes_[1].dist = mins.x;
    planes_[2].dist = maxs.y;
    planes_[3].dist = mins.y;
    planes_[4].dist = maxs.z;
    planes_[5].dist = mins.z;
    return hull_;
}

} // namespace xash::map_loader
