// xash3dpp — pmove trace family (Chunk 6 pmove-bridge P3a): the PM_* trace /
// point-contents callbacks the game DLL's PM_Move invokes, composed over the
// edict-free map_loader kernel and sourced from the gathered physent list.
// Legacy reference: engine/common/pm_trace.c — see pm_trace.hpp for the
// per-function line map.  Deep dive: deep-dive-server-physics.md §4/§7.
//
// Structure mirrors the S6 world composition (world/clip.cpp): resolve each
// physent to a map_loader hull (usehull-indexed player bounds, unlike S6's
// size-based select), rotate the ray into the model frame, run
// recursive_hull_check, then post-process (world-frame endpos + plane
// transform).  The kernel, BoxHull, hull_for_bsp and the rotated-brush
// transforms are all reused, not re-derived.
//
// Deferrals (all Chunk 7 / OQ-2, matching the clip.cpp studio fallback):
//   * studio hitbox hulls (PM_HullForStudio) — studio-flagged physents take
//     the bbox path exactly like the legacy no-hitbox-data fallback.
//   * SOLID_CUSTOM sweep (SV_ClipPMoveToEntity) is an S8 physics-interface
//     seam (physFuncs.ClipPMoveToEntity); the milestone default is no-hit.
//
// Q-20: pmove bridge — raw `edict->v.modelindex` read (physent resolution).
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/pm_trace.hpp>

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/trace.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/world_trace.hpp> // IModelResolver, transforms
#include <xash3dpp/utilities/matrix.hpp>

#include <cmath>
#include <cstddef>
#include <optional>

namespace xash::server {

namespace abi = ::xash::abi;
namespace ml  = ::xash::map_loader;
namespace ut  = ::xash::utilities;
using ut::Vec3;

namespace {

// --- small conversions between the ABI's bare float[3] and Vec3 -------------

[[nodiscard]] Vec3 vec_of( const abi::vec3_t v ) noexcept
{
    return { v[0], v[1], v[2] };
}

void store_vec( abi::vec3_t dst, const Vec3 &v ) noexcept
{
    dst[0] = v.x;
    dst[1] = v.y;
    dst[2] = v.z;
}

// world.h VectorIsNull.
[[nodiscard]] bool vector_is_null( const Vec3 &v ) noexcept
{
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

// world.h check_angles: exact ±90/±180/±270 (as truncated ints) — the same
// helper clip.cpp uses for the transform_bbox decision.  (NOT "any non-multiple
// of 90" — 0/360 and off-axis angles must read false.)
[[nodiscard]] bool check_angles( float x ) noexcept
{
    const int i = static_cast<int>( x );
    return i == 90 || i == 180 || i == 270 ||
           i == -90 || i == -180 || i == -270;
}

// clamp usehull to the 4-entry player-bounds table (SetupPMove only ever sets
// 0/1; defensive, never alters parity for valid input).
[[nodiscard]] int clamp_usehull( int usehull ) noexcept
{
    return ( usehull >= 0 && usehull < 4 ) ? usehull : 0;
}

// physent -> legacy modelindex (pe->info indexes the live edict arena, set by
// SV_CopyEdictToPhysEnt at gather time).
[[nodiscard]] int physent_modelindex( const PmTraceEnv &env,
                                      const abi::physent_t *pe ) noexcept
{
    if ( env.arena == nullptr )
        return 0;
    abi::edict_t *ed = env.arena->edict_num( static_cast<std::size_t>( pe->info ));
    return ed != nullptr ? ed->v.modelindex : 0;
}

[[nodiscard]] std::optional<BrushModel>
physent_brush( const PmTraceEnv &env, const abi::physent_t *pe ) noexcept
{
    if ( env.models == nullptr || env.world == nullptr )
        return std::nullopt;
    return env.models->brush_model( physent_modelindex( env, pe ));
}

// pmtrace_t assembled from a computed (world-frame) kernel result + ent index.
[[nodiscard]] abi::pmtrace_t make_pmtrace( const ml::TraceResult &t,
                                           int ent ) noexcept
{
    abi::pmtrace_t pm{};
    pm.allsolid   = t.allsolid ? 1 : 0;
    pm.startsolid = t.startsolid ? 1 : 0;
    pm.inopen     = t.inopen ? 1 : 0;
    pm.inwater    = t.inwater ? 1 : 0;
    pm.fraction   = t.fraction;
    store_vec( pm.endpos, t.endpos );
    store_vec( pm.plane.normal, t.plane.normal );
    pm.plane.dist = t.plane.dist;
    pm.ent        = ent;
    pm.hitgroup   = 0;
    return pm;
}

// The three hull kinds the pmove trace resolves a physent to (studio always
// takes Box until the Chunk 7 hitbox provider; custom is the physics seam).
enum class HullKind
{
    Brush,
    Box,
    Custom,
};

// Hull selection shared by PM_PlayerTraceExt (:373-413) and
// PM_TestPlayerPosition (:563-585): fills `hull` (via caller-owned box storage
// for the box path) and `offset`.  usehull is already clamped.
[[nodiscard]] HullKind
select_hull( const PmTraceEnv &env, const abi::physent_t *pe, int usehull,
             const std::optional<BrushModel> &brush, ml::BoxHull &box_storage,
             ml::TraceHull &hull, Vec3 &offset ) noexcept
{
    const ml::HullBounds &pb = ( *env.player_bounds )[static_cast<std::size_t>( usehull )];

    if ( pe->solid == abi::k_solid_custom )
    {
        offset = Vec3{};
        return HullKind::Custom;
    }

    if ( brush.has_value() )
    {
        const auto sel = ml::hull_for_bsp( *env.world, brush->submodel, usehull,
                                           pb, vec_of( pe->origin ));
        hull   = sel.hull;
        offset = sel.offset;
        return HullKind::Brush;
    }

    // studio hitbox hulls arrive with the Chunk 7 provider (OQ-2); until then,
    // and for plain bbox physents, expand the box by the player hull.
    const Vec3 mins = vec_of( pe->mins ) - pb.maxs;
    const Vec3 maxs = vec_of( pe->maxs ) - pb.mins;
    hull   = box_storage.set_bounds( mins, maxs );
    offset = vec_of( pe->origin );
    return HullKind::Box;
}

// Rotate [start,end] into the model-local frame for a (possibly rotated) brush
// physent, mirroring clip.cpp:189-237 with the pmove transform_bbox condition
// (check_angles on pitch/roll, usehull != 2 — no mins gate).  `offset` is
// updated for the transform_bbox case; `matrix` is the forward entity matrix
// (identity when not rotated) for the later plane transform.
struct LocalRay
{
    Vec3          start_l, end_l;
    ut::Matrix3x4 matrix  = ut::Matrix3x4::identity();
    bool          rotated = false;
};

[[nodiscard]] LocalRay
to_local( const PmTraceEnv &env, const abi::physent_t *pe, int usehull,
          const ml::TraceHull &hull, Vec3 offset, const Vec3 &start,
          const Vec3 &end ) noexcept
{
    LocalRay r;
    const Vec3 angles = vec_of( pe->angles );
    r.rotated = ( pe->solid == abi::k_solid_bsp ) && !vector_is_null( angles );

    bool transform_bbox = false;
    if ( env.pusher_ext )
    {
        if (( check_angles( angles.x ) || check_angles( angles.z )) &&
            usehull != 2 )
            transform_bbox = true;
    }

    if ( r.rotated )
    {
        r.matrix =
            ut::from_angles( transform_bbox ? vec_of( pe->origin ) : offset,
                             angles );
        const ut::Matrix3x4 inv = ut::invert_ortho( r.matrix );
        r.start_l = ut::transform_point( inv, start );
        r.end_l   = ut::transform_point( inv, end );

        if ( transform_bbox )
        {
            const ml::HullBounds &pb =
                ( *env.player_bounds )[static_cast<std::size_t>( usehull )];
            Vec3 om, ox;
            world_transform_aabb( r.matrix, pb.mins, pb.maxs, om, ox );
            offset = hull.clip_mins - om; // new local offset

            float       *sl = &r.start_l.x;
            float       *el = &r.end_l.x;
            const float *of = &offset.x;
            for ( int j = 0; j < 3; ++j )
            {
                sl[j] += sl[j] >= 0.0f ? -of[j] : of[j];
                el[j] += el[j] >= 0.0f ? -of[j] : of[j];
            }
        }
    }
    else
    {
        r.start_l = start - offset;
        r.end_l   = end - offset;
    }
    return r;
}

// PM_PlayerTraceExt per-entity post-processing (:504-523): allsolid->startsolid,
// startsolid->fraction 0, else world-frame endpos + plane transform/recompute.
void finalize( ml::TraceResult &t, const LocalRay &r, const Vec3 &start,
               const Vec3 &end ) noexcept
{
    if ( t.allsolid )
        t.startsolid = true;
    if ( t.startsolid )
        t.fraction = 0.0f;

    if ( !t.startsolid )
    {
        t.endpos = start + ( end - start ) * t.fraction;
        if ( r.rotated )
        {
            const ml::TracePlane local = t.plane;
            transform_positive_plane( r.matrix, local, t.plane );
        }
        else
        {
            t.plane.dist = ut::dot( t.endpos, t.plane.normal );
        }
    }
    // else: endpos stays at the world `end` init (legacy PM_InitPMTrace).
}

} // namespace

// ---------------------------------------------------------------------------
// PM_PlayerTraceExt (pm_trace.c:325)
// ---------------------------------------------------------------------------

abi::pmtrace_t
pm_player_trace_ext( const PmTraceEnv &env, abi::playermove_t &pm,
                     const Vec3 &start, const Vec3 &end, int flags,
                     abi::physent_t *ents, int numents, int ignore_pe,
                     PmIgnore filter ) noexcept
{
    const int usehull = clamp_usehull( pm.usehull );

    ml::TraceResult total{};
    total.allsolid = false; // legacy trace_total is memset to 0 (allsolid false)
    total.fraction = 1.0f;
    total.endpos   = end;
    int total_ent  = -1;

    for ( int i = 0; i < numents; ++i )
    {
        abi::physent_t *pe = &ents[i];

        if ( i != 0 && ( flags & abi::k_pm_world_only ))
            break;

        if ( filter != nullptr )
        {
            if ( filter( pe ))
                continue;
        }
        else if ( ignore_pe != -1 && i == ignore_pe )
        {
            continue;
        }

        const auto brush = physent_brush( env, pe );

        // SOLID_NOT water/content brushes are gathered but not swept.
        if ( brush.has_value() && pe->solid == abi::k_solid_not &&
             pe->skin != ml::k_contents_none )
            continue;

        if (( flags & abi::k_pm_glass_ignore ) &&
            pe->rendermode != abi::k_render_normal )
            continue;

        if (( flags & abi::k_pm_custom_ignore ) &&
            pe->solid == abi::k_solid_custom )
            continue;

        // studio physents skip when PM_STUDIO_IGNORE is set (the hitbox path
        // is Chunk 7; otherwise they fall through to the bbox hull).
        if ( !brush.has_value() && ( flags & abi::k_pm_studio_ignore ) &&
             env.models != nullptr &&
             env.models->is_studio( physent_modelindex( env, pe )))
            continue;

        ml::BoxHull   box_storage;
        ml::TraceHull hull{};
        Vec3          offset{};
        const HullKind kind =
            select_hull( env, pe, usehull, brush, box_storage, hull, offset );

        if ( kind == HullKind::Custom )
            continue; // SV_ClipPMoveToEntity no-hit stub (S8 physics iface)

        const LocalRay r = to_local( env, pe, usehull, hull, offset, start, end );

        ml::TraceResult t{};      // PM_InitPMTrace: allsolid=true, fraction=1,
        t.endpos = end;           // endpos = world end.
        (void)ml::recursive_hull_check( hull, hull.firstclipnode, 0.0f, 1.0f,
                                        r.start_l, r.end_l, t );

        finalize( t, r, start, end );

        if ( t.fraction < total.fraction )
        {
            total     = t;
            total_ent = i;
        }
    }

    return make_pmtrace( total, total_ent );
}

// ---------------------------------------------------------------------------
// PM_TestPlayerPosition (pm_trace.c:535)
// ---------------------------------------------------------------------------

int pm_test_player_position( const PmTraceEnv &env, abi::playermove_t &pm,
                             const Vec3 &pos, abi::pmtrace_t *ptrace,
                             PmIgnore filter ) noexcept
{
    const int usehull = clamp_usehull( pm.usehull );

    // legacy quirk: the reported trace is an origin->origin sweep, but the
    // solid test below uses the passed `pos`.
    const abi::pmtrace_t trace =
        pm_player_trace_ext( env, pm, vec_of( pm.origin ), vec_of( pm.origin ),
                             0, pm.physents, pm.numphysent, -1, filter );
    if ( ptrace != nullptr )
        *ptrace = trace;

    for ( int i = 0; i < pm.numphysent; ++i )
    {
        abi::physent_t *pe = &pm.physents[i];

        if ( filter != nullptr && filter( pe ))
            continue;

        const auto brush = physent_brush( env, pe );

        if ( brush.has_value() && pe->solid == abi::k_solid_not &&
             pe->skin != ml::k_contents_none )
            continue;

        ml::BoxHull   box_storage;
        ml::TraceHull hull{};
        Vec3          offset{};
        const HullKind kind =
            select_hull( env, pe, usehull, brush, box_storage, hull, offset );

        if ( kind == HullKind::Custom )
            continue; // SV_ClipPMoveToEntity no-hit stub (S8 physics iface)

        // point transform (CM_TransformedPointContents): rotate/offset `pos`.
        Vec3       pos_l;
        const Vec3 angles = vec_of( pe->angles );
        const bool rotated =
            ( pe->solid == abi::k_solid_bsp ) && !vector_is_null( angles );

        if ( rotated )
        {
            bool transform_bbox = false;
            if ( env.pusher_ext &&
                 ( check_angles( angles.x ) || check_angles( angles.z )) &&
                 usehull != 2 )
                transform_bbox = true;

            const ut::Matrix3x4 matrix = ut::from_angles(
                transform_bbox ? vec_of( pe->origin ) : offset, angles );
            pos_l = ut::transform_point( ut::invert_ortho( matrix ), pos );

            if ( transform_bbox )
            {
                const ml::HullBounds &pb =
                    ( *env.player_bounds )[static_cast<std::size_t>( usehull )];
                Vec3 om, ox;
                world_transform_aabb( matrix, pb.mins, pb.maxs, om, ox );
                offset = hull.clip_mins - om;

                float       *pl = &pos_l.x;
                const float *of = &offset.x;
                for ( int j = 0; j < 3; ++j )
                    pl[j] += pl[j] >= 0.0f ? -of[j] : of[j];
            }
        }
        else
        {
            pos_l = pos - offset;
        }

        if ( ml::hull_point_contents( hull, hull.firstclipnode, pos_l ) ==
             ml::k_contents_solid )
            return i;
    }

    return -1; // didn't hit anything
}

// ---------------------------------------------------------------------------
// PM_TraceModel (pm_trace.c:743)
// ---------------------------------------------------------------------------

abi::pmtrace_t pm_trace_model( const PmTraceEnv &env, abi::playermove_t &pm,
                               abi::physent_t *pe, const Vec3 &start,
                               const Vec3 &end ) noexcept
{
    ml::TraceResult t{};
    t.endpos = end; // PM_InitTrace(end)

    const auto brush = physent_brush( env, pe );
    if ( !brush.has_value() )
        return make_pmtrace( t, -1 ); // legacy asserts a brush; no-hit instead

    // hull always usehull 2 (point hull) for model traces.
    const auto sel = ml::hull_for_bsp( *env.world, brush->submodel, 2,
                                       ( *env.player_bounds )[2],
                                       vec_of( pe->origin ));

    const Vec3 angles  = vec_of( pe->angles );
    const bool rotated = ( pe->solid == abi::k_solid_bsp ) &&
                         !vector_is_null( angles );

    Vec3          start_l, end_l;
    ut::Matrix3x4 matrix = ut::Matrix3x4::identity();
    if ( rotated )
    {
        matrix = ut::from_angles( sel.offset, angles );
        const ut::Matrix3x4 inv = ut::invert_ortho( matrix );
        start_l = ut::transform_point( inv, start );
        end_l   = ut::transform_point( inv, end );
    }
    else
    {
        start_l = start - sel.offset;
        end_l   = end - sel.offset;
    }

    (void)ml::recursive_hull_check( sel.hull, sel.hull.firstclipnode, 0.0f, 1.0f,
                                    start_l, end_l, t );

    if ( rotated )
    {
        const ml::TracePlane local = t.plane;
        transform_positive_plane( matrix, local, t.plane );
    }

    t.endpos = start + ( end - start ) * t.fraction;
    return make_pmtrace( t, -1 );
}

// ---------------------------------------------------------------------------
// PM_TraceLine (pm_trace.c:791) / PM_TraceLineEx (pm_trace.c:814)
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] abi::pmtrace_t
trace_line_impl( const PmTraceEnv &env, abi::playermove_t &pm, const Vec3 &start,
                 const Vec3 &end, int flags, int usehull, int ignore_pe,
                 PmIgnore filter ) noexcept
{
    const int old_usehull = pm.usehull;
    pm.usehull            = usehull; // legacy swaps pmove->usehull around the trace

    abi::pmtrace_t tr{};
    tr.fraction = 1.0f;
    tr.ent      = -1;
    switch ( flags )
    {
    case abi::k_pm_traceline_physentsonly:
        tr = pm_player_trace_ext( env, pm, start, end, 0, pm.physents,
                                  pm.numphysent, ignore_pe, filter );
        break;
    case abi::k_pm_traceline_anyvisible:
        tr = pm_player_trace_ext( env, pm, start, end, 0, pm.visents,
                                  pm.numvisent, ignore_pe, filter );
        break;
    default:
        break;
    }

    pm.usehull = old_usehull;
    return tr;
}

} // namespace

abi::pmtrace_t pm_trace_line( const PmTraceEnv &env, abi::playermove_t &pm,
                              const Vec3 &start, const Vec3 &end, int flags,
                              int usehull, int ignore_pe ) noexcept
{
    // main-thread only: the usehull swap inside mutates shared pmove state.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return trace_line_impl( env, pm, start, end, flags, usehull, ignore_pe,
                            nullptr );
}

abi::pmtrace_t pm_trace_line_ex( const PmTraceEnv &env, abi::playermove_t &pm,
                                 const Vec3 &start, const Vec3 &end, int flags,
                                 int usehull, PmIgnore filter ) noexcept
{
    // main-thread only: the usehull swap inside mutates shared pmove state.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return trace_line_impl( env, pm, start, end, flags, usehull, -1, filter );
}

// ---------------------------------------------------------------------------
// Point contents (pm_trace.c:665 / :685 / :860)
// ---------------------------------------------------------------------------

int pm_true_point_contents( const PmTraceEnv &env, abi::playermove_t &pm,
                            const Vec3 &p ) noexcept
{
    (void)pm;
    if ( env.world == nullptr )
        return ml::k_contents_empty;
    const ml::TraceHull wh = ml::world_hull( *env.world, 0, 0 );
    return ml::hull_point_contents( wh, wh.firstclipnode, p );
}

int pm_point_contents( const PmTraceEnv &env, abi::playermove_t &pm,
                       const Vec3 &p ) noexcept
{
    if ( env.world == nullptr )
        return ml::k_contents_none;

    // base contents from the world (physents[0]) hull 0.
    const ml::TraceHull world = ml::world_hull( *env.world, 0, 0 );
    int contents = ml::hull_point_contents( world, 0, p );

    for ( int i = 1; i < pm.numphysent; ++i )
    {
        const abi::physent_t *pe = &pm.physents[i];

        if ( pe->solid != abi::k_solid_not ) // disabled?
            continue;

        const auto brush = physent_brush( env, pe );
        if ( !brush.has_value() ) // only brushes have special contents
            continue;

        const ml::TraceHull hull = ml::world_hull( *env.world, brush->submodel, 0 );

        Vec3       test;
        const Vec3 angles = vec_of( pe->angles );
        if ( brush->has_origin && !vector_is_null( angles ))
        {
            const ut::Matrix3x4 matrix =
                ut::from_angles( vec_of( pe->origin ), angles );
            test = ut::transform_point( ut::invert_ortho( matrix ), p );
        }
        else
        {
            test = p - vec_of( pe->origin );
        }

        if ( ml::hull_point_contents( hull, hull.firstclipnode, test ) ==
             ml::k_contents_empty )
            continue;

        if ( rank_for_contents( pe->skin ) > rank_for_contents( contents ))
            contents = pe->skin; // higher-priority content wins
    }

    return contents;
}

int pm_point_contents_pmove( const PmTraceEnv &env, abi::playermove_t &pm,
                             const Vec3 &p, int *truecontents ) noexcept
{
    const int cont = pm_point_contents( env, pm, p );
    if ( truecontents != nullptr )
        *truecontents = cont;

    if ( cont <= ml::k_contents_current_0 && cont >= ml::k_contents_current_down )
        return ml::k_contents_water;
    return cont;
}

// ---------------------------------------------------------------------------
// PM_StuckTouch (pm_trace.c:872)
// ---------------------------------------------------------------------------

void pm_stuck_touch( abi::playermove_t &pm, int hitent,
                     abi::pmtrace_t *tr ) noexcept
{
    for ( int i = 0; i < pm.numtouch; ++i )
    {
        if ( pm.touchindex[i].ent == hitent )
            return;
    }

    if ( pm.numtouch >= abi::k_max_physents )
        return;

    tr->deltavelocity[0] = pm.velocity[0];
    tr->deltavelocity[1] = pm.velocity[1];
    tr->deltavelocity[2] = pm.velocity[2];
    tr->ent              = hitent;

    pm.touchindex[pm.numtouch++] = *tr;
}

} // namespace xash::server
