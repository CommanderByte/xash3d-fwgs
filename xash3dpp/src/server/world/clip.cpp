// xash3dpp — server trace clipping (Chunk 6 S5b)
// Legacy reference: engine/server/sv_world.c :836-1405;
// engine/common/world.h/.c; public/matrixlib.c :397
//
// Existing subsystems used:
//   xash3dpp_map_loader — trace kernel (trace_hull), BoxHull
//   xash3dpp_utilities  — Matrix3x4 (from_angles/invert_ortho), Vec3
//   xash3dpp_core       — logging, thread-role assertion (OQ-9)

#include <xash3dpp/private/server/world_trace.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/utilities/matrix.hpp>

namespace xash::server {

namespace ml  = ::xash::map_loader;
namespace abi = ::xash::abi;
namespace ut  = ::xash::utilities;

namespace {

// MOVE_MISSILE expands monster boxes by ±15 units (sv_world.c:1341).
inline constexpr float k_missile_expand = 15.0f;

[[nodiscard]] bool vector_is_null( const Vec3 &v ) noexcept
{
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

// world.h check_angles: exact multiples of 90 (as truncated ints).
[[nodiscard]] bool check_angles( float x ) noexcept
{
    const int i = static_cast<int>( x );
    return i == 90 || i == 180 || i == 270 ||
           i == -90 || i == -180 || i == -270;
}

[[nodiscard]] bool bounds_intersect( const Vec3 &min1, const Vec3 &max1,
                                     const Vec3 &min2, const Vec3 &max2 ) noexcept
{
    if ( min1.x > max2.x || min1.y > max2.y || min1.z > max2.z )
        return false;
    if ( max1.x < min2.x || max1.y < min2.y || max1.z < min2.z )
        return false;
    return true;
}

// World_MoveBounds: ±1 expanded sweep box.
void move_bounds( const Vec3 &start, const Vec3 &mins, const Vec3 &maxs,
                  const Vec3 &end, Vec3 &boxmins, Vec3 &boxmaxs ) noexcept
{
    const float *s = &start.x, *e = &end.x, *mn = &mins.x, *mx = &maxs.x;
    float *bmn = &boxmins.x, *bmx = &boxmaxs.x;
    for ( int i = 0; i < 3; ++i )
    {
        if ( e[i] > s[i] )
        {
            bmn[i] = s[i] + mn[i] - 1.0f;
            bmx[i] = e[i] + mx[i] + 1.0f;
        }
        else
        {
            bmn[i] = e[i] + mn[i] - 1.0f;
            bmx[i] = s[i] + mx[i] + 1.0f;
        }
    }
}

// World_CombineTraces: accept on allsolid/startsolid/nearer fraction;
// startsolid is sticky across accepted traces.
void combine_traces( SvTrace &clip, SvTrace &trace,
                     abi::edict_t *touch ) noexcept
{
    if ( trace.t.allsolid || trace.t.startsolid ||
         trace.t.fraction < clip.t.fraction )
    {
        trace.ent = touch;

        const bool was_startsolid = clip.t.startsolid;
        clip = trace;
        if ( was_startsolid )
            clip.t.startsolid = true;
    }
}

// The moveclip_t bundle threaded through the areanode walks.
struct MoveClip
{
    Vec3 boxmins{}, boxmaxs{}; // enclose the entire move
    Vec3 mins{}, maxs{};       // size of the moving object
    Vec3 mins2{}, maxs2{};     // size when clipping against monsters
    Vec3 start{}, end{};
    SvTrace trace{};
    int  type        = 0;
    bool ignoretrans = false;
    bool monsterclip = false;
    abi::edict_t *passedict = nullptr;
};

} // namespace

// ---------------------------------------------------------------------------
// transforms
// ---------------------------------------------------------------------------

void world_transform_aabb( const ut::Matrix3x4 &m, const Vec3 &mins,
                           const Vec3 &maxs, Vec3 &out_mins,
                           Vec3 &out_maxs ) noexcept
{
    const ut::Matrix3x4 inv = ut::invert_ortho( m );

    out_mins = { 99999.0f, 99999.0f, 99999.0f };
    out_maxs = { -99999.0f, -99999.0f, -99999.0f };

    for ( int i = 0; i < 8; ++i )
    {
        const Vec3 p1 = { ( i & 1 ) ? mins.x : maxs.x,
                          ( i & 2 ) ? mins.y : maxs.y,
                          ( i & 4 ) ? mins.z : maxs.z };
        const Vec3 p2 = ut::rotate_vector( inv, p1 );

        if ( p2.x < out_mins.x ) out_mins.x = p2.x;
        if ( p2.x > out_maxs.x ) out_maxs.x = p2.x;
        if ( p2.y < out_mins.y ) out_mins.y = p2.y;
        if ( p2.y > out_maxs.y ) out_maxs.y = p2.y;
        if ( p2.z < out_mins.z ) out_mins.z = p2.z;
        if ( p2.z > out_maxs.z ) out_maxs.z = p2.z;
    }

    // sanity check (legacy zeroes on inversion failure)
    if ( out_mins.x > out_maxs.x || out_mins.y > out_maxs.y ||
         out_mins.z > out_maxs.z )
    {
        out_mins = {};
        out_maxs = {};
    }
}

void transform_positive_plane( const ut::Matrix3x4 &m,
                               const ml::TracePlane &in,
                               ml::TracePlane &out ) noexcept
{
    // Scale is 1 (from_angles) — rotation rows + translation column.
    out.normal = ut::rotate_vector( m, in.normal );
    out.dist   = in.dist + ut::dot( out.normal,
                                    ut::transform_point( m, Vec3{} ));
}

// ---------------------------------------------------------------------------
// IClipHooks default: SOLID_CUSTOM with no physics interface — no-hit.
// ---------------------------------------------------------------------------

void IClipHooks::custom_clip( abi::edict_t * /*touch*/, const Vec3 & /*start*/,
                              const Vec3 & /*mins*/, const Vec3 & /*maxs*/,
                              const Vec3 &end, SvTrace &out ) noexcept
{
    out             = SvTrace{};
    out.t.allsolid  = false;
    out.t.fraction  = 1.0f;
    out.t.endpos    = end;
}

// ---------------------------------------------------------------------------
// SV_ClipMoveToEntity
// ---------------------------------------------------------------------------

SvTrace clip_move_to_entity( const MoveEnv &env, abi::edict_t *ent,
                             const Vec3 &start, const Vec3 &mins,
                             const Vec3 &maxs, const Vec3 &end ) noexcept
{
    const EntityView view( ent );

    SvTrace out;
    out.t.endpos = end; // PM_InitTrace

    // TODO(chunk7): studio hitbox hulls via IStudioHullProvider (OQ-2);
    // the null provider falls back to the bbox path below, exactly like
    // the legacy no-hitbox-data fallback (hullcount 1, hitgroup 0).
    ml::BoxHull box_storage;
    const auto sel = hull_for_entity( env, ent, mins, maxs, box_storage );
    if ( !sel.has_value() )
        return out; // logged; bridge maps to the host error policy (Q-5)

    // rotate start and end into the model's frame of reference
    const bool rotated =
        ( view.solid() == abi::k_solid_bsp ||
          view.solid() == abi::k_solid_portal ) &&
        !vector_is_null( view.angles() );

    bool transform_bbox = false;
    if ( env.pusher_ext )
    {
        // keep untransformed bbox less than 45 degrees or trains on
        // subtransit.bsp stop working (legacy comment)
        const Vec3 ang = view.angles();
        if (( check_angles( ang.x ) || check_angles( ang.z )) &&
            !vector_is_null( mins ))
            transform_bbox = true;
    }

    Vec3 start_l, end_l;
    Vec3 offset = sel->offset;
    ut::Matrix3x4 matrix = ut::Matrix3x4::identity();

    if ( rotated )
    {
        // TODO(Q-18): the rotated-brush transform is ULP-inexact vs legacy
        // (S13 parity finding, deferred to a dedicated slice with the Q-18
        // golden-vector generator — no map_loader ripple, server-only callers):
        //   (a) from_angles builds the rotation in FLOAT trig (sinf + float
        //       deg2rad) vs legacy Matrix4x4_CreateFromEntity's DOUBLE M_PI2/360
        //       + double SinCos;
        //   (b) invert_ortho + transform_point compute (v·R − t·R) whereas
        //       legacy VectorITransform is (v − t)·R (subtract-first grouping);
        //   (c) world_transform_aabb uses a plain transpose vs Invert_Simple's
        //       transpose × 1/(row0·row0);
        //   (d) transform_positive_plane assumes scale==1 (omits the
        //       sqrt(row0·row0) normalization).
        // Rotated-geometry parity ticks at S15/Chunk 11 (like pmove parity).
        matrix = ut::from_angles( transform_bbox ? view.origin() : offset,
                                  view.angles() );
        const ut::Matrix3x4 inv = ut::invert_ortho( matrix );
        start_l = ut::transform_point( inv, start );
        end_l   = ut::transform_point( inv, end );

        if ( transform_bbox )
        {
            Vec3 out_mins, out_maxs;
            world_transform_aabb( matrix, mins, maxs, out_mins, out_maxs );
            offset = sel->hull.clip_mins - out_mins; // new local offset

            // Sign-dependent offset re-application (sv_world.c:892-900).
            float *sl = &start_l.x, *el = &end_l.x;
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
        start_l = start - offset;
        end_l   = end - offset;
    }

    // single hull (hitbox arrays arrive with the Chunk 7 studio provider)
    out.t = ml::trace_hull( sel->hull, start_l, end_l );

    if ( out.t.fraction != 1.0f )
    {
        // compute endpos (generic case)
        out.t.endpos = start + ( end - start ) * out.t.fraction;

        if ( rotated )
        {
            const ml::TracePlane local = out.t.plane;
            transform_positive_plane( matrix, local, out.t.plane );
        }
        else
        {
            out.t.plane.dist = ut::dot( out.t.endpos, out.t.plane.normal );
        }
    }
    else
    {
        out.t.endpos = end; // stay in the world frame when nothing hit
    }

    if ( out.t.fraction < 1.0f || out.t.startsolid )
        out.ent = ent;

    return out;
}

// ---------------------------------------------------------------------------
// SV_ClipToEntity filter chain + areanode walks
// ---------------------------------------------------------------------------

namespace {

// Returns false to abort the whole walk (trace went allsolid).
bool clip_to_entity( const MoveEnv &env, abi::edict_t *touch,
                     MoveClip &clip ) noexcept
{
    const EntityView tv( touch );
    const EntityView pass( clip.passedict );

    if ( tv.groupinfo() != 0 && pass.valid() && pass.groupinfo() != 0 )
    {
        const bool overlap = ( tv.groupinfo() & pass.groupinfo() ) != 0;
        if ( env.group_op == GroupOp::And && !overlap )
            return true;
        if ( env.group_op == GroupOp::Nand && overlap )
            return true;
    }

    if ( touch == clip.passedict || tv.solid() == abi::k_solid_not )
        return true;

    if ( tv.solid() == abi::k_solid_trigger )
    {
        // Legacy: Host_Error "trigger in clipping list" — logged + skipped
        // per Q-5 (Known Deviation, server-boundary.md).
        ::xash::core::log( ::xash::core::LogLevel::Error, "server",
                           "trigger in clipping list" );
        return true;
    }

    if ( env.hooks != nullptr &&
         !env.hooks->should_collide( touch, clip.passedict ))
        return true;

    // monsterclip filter (solid custom is a static or dynamic body)
    if ( tv.solid() == abi::k_solid_bsp ||
         tv.solid() == abi::k_solid_custom )
    {
        // func_monsterclip works only with monsters that share the flag
        if (( tv.flags() & abi::k_fl_monsterclip ) && !clip.monsterclip )
            return true;
    }
    else
    {
        // ignore all monsters but pushables
        if ( clip.type == abi::k_move_nomonsters &&
             tv.movetype() != abi::k_movetype_pushstep )
            return true;
    }

    if ( clip.ignoretrans && env.models != nullptr &&
         env.models->brush_model( tv.modelindex() ).has_value() )
    {
        // ignore brushes with rendermode != kRenderNormal and without
        // FL_WORLDBRUSH set
        if ( tv.rendermode() != abi::k_render_normal &&
             !( tv.flags() & abi::k_fl_worldbrush ))
            return true;
    }

    if ( !bounds_intersect( clip.boxmins, clip.boxmaxs, tv.absmin(),
                            tv.absmax() ))
        return true;

    // additional sphere cull for clients (sequence bbox; hook until the
    // model cache lands)
    if ( tv.solid() != abi::k_solid_slidebox && env.hooks != nullptr &&
         !env.hooks->sphere_cull( touch, clip.start, clip.end ))
        return true;

    // Xash extension: a SOLID_TRIGGER passedict never clips clients (old
    // HL "give sticks the item in the player" bug workaround)
    if ( pass.valid() && pass.solid() == abi::k_solid_trigger &&
         ( tv.flags() & ( abi::k_fl_client | abi::k_fl_fakeclient )))
        return true;

    // make sure the size really is zero — points never interact
    if ( pass.valid() && !vector_is_null( pass.size() ) &&
         vector_is_null( tv.size() ))
        return true;

    if ( clip.trace.t.allsolid )
        return false; // abort the whole walk

    if ( pass.valid() )
    {
        if ( tv.owner() == clip.passedict )
            return true; // don't clip against own missiles
        if ( pass.owner() == touch )
            return true; // don't clip against owner
    }

    SvTrace trace;
    if ( tv.solid() == abi::k_solid_portal )
    {
        // XASH3DPP-STUB(chunk6): SV_PortalCSG trace elongation is not
        // ported (stock HL ships no SOLID_PORTAL entities); the portal
        // still clips as a plain BSP solid below.
        trace = clip_move_to_entity( env, touch, clip.start, clip.mins,
                                     clip.maxs, clip.end );
    }
    else if ( tv.solid() == abi::k_solid_custom )
    {
        if ( env.hooks != nullptr )
            env.hooks->custom_clip( touch, clip.start, clip.mins,
                                    clip.maxs, clip.end, trace );
        else
        {
            trace.t.allsolid = false;
            trace.t.fraction = 1.0f;
            trace.t.endpos   = clip.end;
        }
    }
    else if ( tv.flags() & abi::k_fl_monster )
    {
        trace = clip_move_to_entity( env, touch, clip.start, clip.mins2,
                                     clip.maxs2, clip.end );
    }
    else
    {
        trace = clip_move_to_entity( env, touch, clip.start, clip.mins,
                                     clip.maxs, clip.end );
    }

    combine_traces( clip.trace, trace, touch );
    return true;
}

enum class WalkList
{
    Solids,
    Portals,
    WorldBrush, // SOLID_BSP + FL_WORLDBRUSH subset of the solid list
};

void clip_walk( const MoveEnv &env, AreaNode *node, MoveClip &clip,
                WalkList which ) noexcept
{
    abi::link_t &sentinel = which == WalkList::Portals
        ? node->portal_edicts : node->solid_edicts;

    abi::link_t *next = nullptr;
    for ( abi::link_t *l = sentinel.next; l != &sentinel; l = next )
    {
        next = l->next;
        abi::edict_t *touch = edict_from_area( l );

        if ( which == WalkList::WorldBrush )
        {
            const EntityView tv( touch );
            if ( tv.solid() != abi::k_solid_bsp ||
                 touch == clip.passedict ||
                 !( tv.flags() & abi::k_fl_worldbrush ))
                continue;
            if ( !bounds_intersect( clip.boxmins, clip.boxmaxs,
                                    tv.absmin(), tv.absmax() ))
                continue;
            if ( clip.trace.t.allsolid )
                return;

            SvTrace trace = clip_move_to_entity(
                env, touch, clip.start, clip.mins, clip.maxs, clip.end );
            combine_traces( clip.trace, trace, touch );
            continue;
        }

        if ( !clip_to_entity( env, touch, clip ))
            return; // trace went allsolid
    }

    // recurse down both sides
    if ( node->axis == -1 )
        return;

    if ( vec_axis( clip.boxmaxs, node->axis ) > node->dist )
        clip_walk( env, node->children[0], clip, which );
    if ( vec_axis( clip.boxmins, node->axis ) < node->dist )
        clip_walk( env, node->children[1], clip, which );
}

// Shared SV_Move/SV_MoveNoEnts skeleton: world clip, entity pass over the
// world-clipped segment, fraction re-compose.
SvTrace move_internal( const MoveEnv &env, const Vec3 &start,
                       const Vec3 &mins, const Vec3 &maxs, const Vec3 &end,
                       int type, abi::edict_t *passedict, bool monsterclip,
                       bool no_ents ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    MoveClip clip;
    clip.trace = clip_move_to_entity( env, env.worldspawn, start, mins,
                                      maxs, end );

    if ( clip.trace.t.fraction != 0.0f )
    {
        const float world_fraction = clip.trace.t.fraction;
        const Vec3  world_endpos   = clip.trace.t.endpos;

        clip.trace.t.fraction = 1.0f;
        clip.start            = start;
        clip.end              = world_endpos;
        clip.type             = type & 0xFF;
        clip.ignoretrans      = ( type >> 8 ) != 0;
        clip.monsterclip      = false;
        clip.passedict        = passedict != nullptr ? passedict
                                                     : env.worldspawn;
        clip.mins             = mins;
        clip.maxs             = maxs;

        if ( monsterclip && !env.quake_compat )
            clip.monsterclip = true;

        if ( !no_ents && clip.type == abi::k_move_missile )
        {
            clip.mins2 = { -k_missile_expand, -k_missile_expand,
                           -k_missile_expand };
            clip.maxs2 = { k_missile_expand, k_missile_expand,
                           k_missile_expand };
        }
        else
        {
            clip.mins2 = mins;
            clip.maxs2 = maxs;
        }

        move_bounds( start, clip.mins2, clip.maxs2, world_endpos,
                     clip.boxmins, clip.boxmaxs );

        if ( env.area_root != nullptr )
        {
            clip_walk( env, env.area_root, clip,
                       no_ents ? WalkList::WorldBrush : WalkList::Solids );
            clip_walk( env, env.area_root, clip, WalkList::Portals );
        }

        clip.trace.t.fraction *= world_fraction;
        // (globals->trace_ent is stamped by the bridge's trace copy.)
    }

    return clip.trace;
}

} // namespace

SvTrace move( const MoveEnv &env, const Vec3 &start, const Vec3 &mins,
              const Vec3 &maxs, const Vec3 &end, int type,
              abi::edict_t *passedict, bool monsterclip ) noexcept
{
    return move_internal( env, start, mins, maxs, end, type, passedict,
                          monsterclip, /*no_ents=*/false );
}

SvTrace move_no_ents( const MoveEnv &env, const Vec3 &start,
                      const Vec3 &mins, const Vec3 &maxs, const Vec3 &end,
                      int type, abi::edict_t *passedict ) noexcept
{
    return move_internal( env, start, mins, maxs, end, type, passedict,
                          /*monsterclip=*/false, /*no_ents=*/true );
}

} // namespace xash::server
