// xash3dpp — server physics driver: SV_Physics + the MOVETYPE_* dispatch,
// pushers, toss/step/fly movement, gravity & velocity clamps, plus the
// SV_RunGameFrame / Host_ServerFrame fixed-step frame loop (Chunk 6 S8).
// Legacy reference: engine/server/sv_phys.c (:117-1854),
// engine/server/sv_move.c — SV_CheckBottom (:34) / SV_WaterMove (:106),
// engine/server/sv_main.c — SV_RunGameFrame (:602) / SV_PrepWorldFrame
// (:550) / SV_IsSimulating (:572) / Host_ServerFrame (:678).
// Deep dive: docs/legacy-survey/deep-dive-server-physics.md §2.
//
// Trace/link kernels are reused from world/ (SV_Move → move(), SV_LinkEdict
// → WorldLinks, SV_PointContents → point_contents); Matrix4x4 rotated
// pushes reuse utilities.  Entvars access goes through EntityView (Q-20);
// the pusher rollback stack is rt.pushed[256] (svgame.pushed).
//
// Water splash sounds (SV_CheckWaterTransition / SV_WaterMove enter+exit)
// need the S9 multicast pipeline — marked XASH3DPP-STUB(chunk6-S9) at the
// two call sites; the water LEVEL/flag/damage bookkeeping is fully ported.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/physics.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/world/trace.hpp>
#include <xash3dpp/utilities/math.hpp>
#include <xash3dpp/utilities/matrix.hpp>

#include <array>
#include <cmath>

namespace xash::server {

using namespace ::xash::world; // consume the world trace/link kernel (Wave 1)

namespace abi = ::xash::abi;
namespace ml  = ::xash::map_loader;
namespace ut  = ::xash::utilities;
using ut::Vec3;

namespace {

// engine/server/sv_phys.c:44 (MAX_CLIP_PLANES; canonical home limits.hpp)
inline constexpr int   k_max_clip_planes = static_cast<int>( ::xash::limits::server_clip_planes );
inline constexpr float k_on_epsilon      = 0.1f; // public/xash3d_mathlib.h:72

// Quake2 current directions (sv_phys.c:46-54).
inline constexpr Vec3 k_current_table[6] = {
    { 1.0f, 0.0f, 0.0f },  { 0.0f, 1.0f, 0.0f },  { -1.0f, 0.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f },  { 0.0f, 0.0f, -1.0f },
};

// ---------------------------------------------------------------------------
// Small vector predicates (legacy VectorIsNull / VectorCompareEpsilon)
// ---------------------------------------------------------------------------

[[nodiscard]] bool fbit( int flags, int bit ) noexcept { return ( flags & bit ) != 0; }

[[nodiscard]] bool is_null( const Vec3 &v ) noexcept
{
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

[[nodiscard]] bool vec_compare_epsilon( const Vec3 &a, const Vec3 &b,
                                        float eps ) noexcept
{
    return std::fabs( a.x - b.x ) <= eps && std::fabs( a.y - b.y ) <= eps &&
           std::fabs( a.z - b.z ) <= eps;
}

[[nodiscard]] bool valid_edict( const abi::edict_t *e ) noexcept
{
    return e != nullptr && !e->free;
}

// ---------------------------------------------------------------------------
// Runtime shims for the legacy engine primitives
// ---------------------------------------------------------------------------

void link_edict( ServerRuntime &rt, abi::edict_t *ent, bool touch ) noexcept
{
    rt.links.link_edict( ent, touch, rt.link_env );
}

// SV_FreeEdict (unlink first, then arena scrub — sv_game.c:1004).
void free_edict( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    WorldLinks::unlink_edict( ent );
    rt.arena.free_edict( ent, rt.level.time );
}

[[nodiscard]] SvTrace sv_move( ServerRuntime &rt, const Vec3 &start,
                               const Vec3 &mins, const Vec3 &maxs,
                               const Vec3 &end, int type, abi::edict_t *passedict,
                               bool monsterclip ) noexcept
{
    return move( rt.move_env, start, mins, maxs, end, type, passedict,
                 monsterclip );
}

// SV_PointContents with the per-call groupmask legacy sets before every probe.
[[nodiscard]] int pt_contents( ServerRuntime &rt, int groupinfo,
                               const Vec3 &p ) noexcept
{
    rt.move_env.group_mask = groupinfo;
    return point_contents( rt.move_env, p );
}

[[nodiscard]] int true_pt_contents( ServerRuntime &rt, int groupinfo,
                                    const Vec3 &p ) noexcept
{
    rt.move_env.group_mask = groupinfo;
    return true_point_contents( rt.move_env, p );
}

// SV_CopyTraceToGlobal (sv_game.c): fill globals->trace_* so the game DLL's
// Touch handler can read the impact through its TraceResult mirror.
void copy_trace_to_global( ServerRuntime &rt, const SvTrace &tr ) noexcept
{
    abi::globalvars_t &g = rt.globals;
    g.trace_allsolid   = tr.t.allsolid ? 1.0f : 0.0f;
    g.trace_startsolid = tr.t.startsolid ? 1.0f : 0.0f;
    g.trace_fraction   = tr.t.fraction;
    store_vec3( g.trace_endpos, tr.t.endpos );
    store_vec3( g.trace_plane_normal, tr.t.plane.normal );
    g.trace_plane_dist = tr.t.plane.dist;
    g.trace_ent        = tr.ent;
    g.trace_inopen     = tr.t.inopen ? 1.0f : 0.0f;
    g.trace_inwater    = tr.t.inwater ? 1.0f : 0.0f;
    g.trace_hitgroup   = tr.hitgroup;
}

void call_think( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    if ( rt.game.funcs().pfnThink != nullptr )
        rt.game.funcs().pfnThink( ent );
}

// ---------------------------------------------------------------------------
// SV_CheckVelocity (sv_phys.c:120): NaN scrub + whole-vector maxvelocity clamp
// ---------------------------------------------------------------------------

void check_velocity( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    Vec3       vel = v.velocity();
    Vec3       org = v.origin();

    float *pvel = &vel.x;
    float *porg = &org.x;
    for ( int i = 0; i < 3; ++i )
    {
        // sv_check_errors console prints are omitted (default-off cvar).
        if ( ut::is_nan( pvel[i] ) )
            pvel[i] = 0.0f;
        if ( ut::is_nan( porg[i] ) )
            porg[i] = 0.0f;
    }
    v.set_velocity( vel );
    v.set_origin( org );

    const float maxvel  = rt.movevars.maxvelocity;
    float       wishspd = ut::dot( vel, vel );
    const float maxspd  = maxvel * maxvel * 1.73f; // half-diagonal

    if ( wishspd > maxspd )
    {
        wishspd = std::sqrt( wishspd );
        wishspd = maxvel / wishspd;
        v.set_velocity( vel * wishspd );
    }
}

// SV_UpdateBaseVelocity / SV_Impact are defined at namespace scope after this
// anonymous block (physics.hpp exposes them for the pmove run chain,
// run_cmd.cpp) — they need external linkage, so they can't live here.

// ---------------------------------------------------------------------------
// SV_TestEntityPosition (sv_phys.c:193)
// ---------------------------------------------------------------------------

[[nodiscard]] bool test_entity_position( ServerRuntime &rt, abi::edict_t *ent,
                                         abi::edict_t *blocker ) noexcept
{
    EntityView v( ent );
    const bool monster_clip = fbit( v.flags(), abi::k_fl_monsterclip );

    // XASH3DPP-STUB(chunk6-S9): FL_CLIENT/FAKECLIENT duck/stand hull resize
    // (SV_SetMinMaxSize from host.player_mins/maxs) — no clients until S9;
    // non-client pushables use their live bbox exactly like legacy.

    const SvTrace trace =
        sv_move( rt, v.origin(), v.mins(), v.maxs(), v.origin(),
                 abi::k_move_normal, ent, monster_clip );

    if ( valid_edict( blocker ) && valid_edict( trace.ent ) )
    {
        EntityView hit( trace.ent );
        if ( hit.movetype() == abi::k_movetype_push || trace.ent == blocker )
            return trace.t.startsolid;
        return false;
    }
    return trace.t.startsolid;
}

// ---------------------------------------------------------------------------
// SV_RunThink (sv_phys.c:228): think if due, returns false if it self-removed
// ---------------------------------------------------------------------------

[[nodiscard]] bool run_think( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    EntityView v( ent );

    if ( !fbit( v.flags(), abi::k_fl_killme ) )
    {
        float thinktime = v.nextthink();
        if ( thinktime <= 0.0f ||
             thinktime > ( rt.level.time + rt.level.frametime ) )
            return true;

        if ( thinktime < rt.level.time )
            thinktime = static_cast<float>( rt.level.time );

        v.set_nextthink( 0.0f );
        rt.globals.time = thinktime;
        call_think( rt, ent );
    }

    if ( fbit( v.flags(), abi::k_fl_killme ) )
        free_edict( rt, ent );

    return !ent->free;
}

// (SV_Impact is defined at namespace scope after the anonymous block.)

// ---------------------------------------------------------------------------
// SV_AngularMove / SV_LinearMove (sv_phys.c:335 / :369)
// ---------------------------------------------------------------------------

void angular_move( ServerRuntime &rt, abi::edict_t *ent, float frametime,
                   float friction ) noexcept
{
    EntityView v( ent );
    Vec3       ang  = v.angles();
    Vec3       avel = v.avelocity();
    v.set_angles( ang + avel * frametime );
    if ( friction == 0.0f )
        return;

    const float adjustment =
        frametime * ( rt.movevars.stopspeed / 10.0f ) * rt.movevars.friction *
        std::fabs( friction );

    float *a = &avel.x;
    for ( int i = 0; i < 3; ++i )
    {
        if ( a[i] > 0.0f )
            a[i] = ( a[i] - adjustment < 0.0f ) ? 0.0f : a[i] - adjustment;
        else
            a[i] = ( a[i] + adjustment > 0.0f ) ? 0.0f : a[i] + adjustment;
    }
    v.set_avelocity( avel );
}

void linear_move( ServerRuntime &rt, abi::edict_t *ent, float frametime,
                  float friction ) noexcept
{
    EntityView v( ent );
    Vec3       org = v.origin();
    Vec3       vel = v.velocity();
    v.set_origin( org + vel * frametime );
    if ( friction == 0.0f )
        return;

    const float adjustment =
        frametime * ( rt.movevars.stopspeed / 10.0f ) * rt.movevars.friction *
        std::fabs( friction );

    float *p = &vel.x;
    for ( int i = 0; i < 3; ++i )
    {
        if ( p[i] > 0.0f )
            p[i] = ( p[i] - adjustment < 0.0f ) ? 0.0f : p[i] - adjustment;
        else
            p[i] = ( p[i] + adjustment > 0.0f ) ? 0.0f : p[i] + adjustment;
    }
    v.set_velocity( vel );
}

// ---------------------------------------------------------------------------
// Water helpers (sv_phys.c:403-517)
// ---------------------------------------------------------------------------

[[nodiscard]] float recursive_water_level( ServerRuntime &rt, int groupinfo,
                                           const Vec3 &origin, float out,
                                           float in, int count ) noexcept
{
    const float offset = ( ( out - in ) * 0.5f ) + in;
    if ( ++count > 5 )
        return offset;

    const Vec3 point{ origin.x, origin.y, origin.z + offset };
    if ( pt_contents( rt, groupinfo, point ) == ml::k_contents_water )
        return recursive_water_level( rt, groupinfo, origin, out, offset, count );
    return recursive_water_level( rt, groupinfo, origin, offset, in, count );
}

[[nodiscard]] float submerged( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    const Vec3 amin   = v.absmin();
    const Vec3 amax   = v.absmax();
    const Vec3 center = ( amin + amax ) * 0.5f;
    const float start = amin.z - center.z;

    switch ( v.waterlevel() )
    {
    case 1:
        return recursive_water_level( rt, v.groupinfo(), center, 0.0f, start, 0 ) -
               start;
    case 3:
    {
        const Vec3 point{ center.x, center.y, amax.z };
        if ( pt_contents( rt, v.groupinfo(), point ) == ml::k_contents_water )
            return v.maxs().z - v.mins().z;
        [[fallthrough]];
    }
    case 2:
        return recursive_water_level( rt, v.groupinfo(), center,
                                      amax.z - center.z, 0.0f, 0 ) -
               start;
    }
    return 0.0f;
}

[[nodiscard]] bool is_water_contents( int cont ) noexcept
{
    // cont in (CONTENTS_TRANSLUCENT, CONTENTS_WATER] — water + currents.
    return cont <= ml::k_contents_water && cont > ml::k_contents_translucent;
}

// SV_CheckWater (sv_phys.c:458): 3-level probe + Quake2 current basevelocity.
[[nodiscard]] bool check_water( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    const Vec3 amin = v.absmin();
    const Vec3 amax = v.absmax();

    Vec3 point{ ( amax.x + amin.x ) * 0.5f, ( amax.y + amin.y ) * 0.5f,
                amin.z + 1.0f };

    v.set_watertype( ml::k_contents_empty );
    v.set_waterlevel( 0 );

    int cont = pt_contents( rt, v.groupinfo(), point );

    if ( is_water_contents( cont ) )
    {
        const int truecont = true_pt_contents( rt, v.groupinfo(), point );

        v.set_watertype( cont );
        v.set_waterlevel( 1 );

        if ( amin.z != amax.z )
        {
            point.z = ( amin.z + amax.z ) * 0.5f;
            cont    = pt_contents( rt, v.groupinfo(), point );

            if ( is_water_contents( cont ) )
            {
                v.set_waterlevel( 2 );
                point = point + v.view_ofs();
                cont  = pt_contents( rt, v.groupinfo(), point );
                if ( is_water_contents( cont ) )
                    v.set_waterlevel( 3 );
            }
        }
        else
        {
            v.set_waterlevel( 3 ); // a point entity
        }

        // Quake2 currents — probably never used in Half-Life.
        if ( truecont <= ml::k_contents_current_0 &&
             truecont >= ml::k_contents_current_down )
        {
            const float speed =
                150.0f * static_cast<float>( v.waterlevel() ) / 3.0f;
            const Vec3 &dir =
                k_current_table[ml::k_contents_current_0 - truecont];
            v.set_basevelocity( v.basevelocity() + dir * speed );
        }
    }

    return v.waterlevel() > 1;
}

// SV_CheckMover (sv_phys.c:526): pushable riding a moving MOVETYPE_PUSH mover.
[[nodiscard]] bool check_mover( ServerRuntime &, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    abi::edict_t *gnd = v.groundentity();
    if ( !valid_edict( gnd ) )
        return false;
    EntityView gv( gnd );
    if ( gv.movetype() != abi::k_movetype_push )
        return false;
    if ( is_null( gv.velocity() ) && is_null( gv.avelocity() ) )
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// SV_ClipVelocity (sv_phys.c:549): slide off, snap-to-zero at ±1.0
// ---------------------------------------------------------------------------

int clip_velocity( const Vec3 &in, const Vec3 &normal, Vec3 &out,
                   float overbounce ) noexcept
{
    int blocked = 0;
    if ( normal.z > 0.0f )
        blocked |= 1; // floor
    if ( normal.z == 0.0f )
        blocked |= 2; // step

    const float backoff = ut::dot( in, normal ) * overbounce;

    const float *pin = &in.x;
    const float *pn  = &normal.x;
    float       *po  = &out.x;
    for ( int i = 0; i < 3; ++i )
    {
        po[i] = pin[i] - pn[i] * backoff;
        if ( po[i] > -1.0f && po[i] < 1.0f )
            po[i] = 0.0f;
    }
    return blocked;
}

// ---------------------------------------------------------------------------
// SV_FlyMove (sv_phys.c:592): multi-plane slide, 4 bumps
// ---------------------------------------------------------------------------

int fly_move( ServerRuntime &rt, abi::edict_t *ent, float time,
              SvTrace *steptrace ) noexcept
{
    EntityView v( ent );
    int        blocked      = 0;
    const bool monster_clip = fbit( v.flags(), abi::k_fl_monsterclip );

    Vec3 original_velocity = v.velocity();
    Vec3 primal_velocity   = v.velocity();
    Vec3 new_velocity{};
    int  numplanes = 0;

    std::array<Vec3, k_max_clip_planes> planes{};
    float allFraction = 0.0f;
    float time_left   = time;

    for ( int bumpcount = 0; bumpcount < k_max_clip_planes - 1; ++bumpcount )
    {
        if ( is_null( v.velocity() ) )
            break;

        const Vec3 end = v.origin() + v.velocity() * time_left;
        SvTrace tr = sv_move( rt, v.origin(), v.mins(), v.maxs(), end,
                              abi::k_move_normal, ent, monster_clip );

        allFraction += tr.t.fraction;

        if ( tr.t.allsolid )
        {
            v.set_velocity( Vec3{} );
            return 4;
        }

        if ( tr.t.fraction > 0.0f )
        {
            v.set_origin( tr.t.endpos );
            original_velocity = v.velocity();
            numplanes         = 0;
        }

        if ( tr.t.fraction == 1.0f )
            break;

        if ( !valid_edict( tr.ent ) )
            break; // g-cont: should never happen

        if ( tr.t.plane.normal.z > 0.7f )
        {
            blocked |= 1; // floor
            EntityView hit( tr.ent );
            if ( hit.solid() == abi::k_solid_bsp ||
                 hit.solid() == abi::k_solid_slidebox ||
                 hit.movetype() == abi::k_movetype_pushstep ||
                 fbit( hit.flags(), abi::k_fl_client ) )
            {
                v.add_flags( abi::k_fl_onground );
                v.set_groundentity( tr.ent );
            }
        }

        if ( tr.t.plane.normal.z == 0.0f )
        {
            blocked |= 2; // step
            if ( steptrace != nullptr )
                *steptrace = tr;
        }

        sv_impact( rt, ent, tr.ent, tr );
        if ( ent->free )
            break;

        time_left -= time_left * tr.t.fraction;

        if ( numplanes >= k_max_clip_planes )
        {
            v.set_velocity( Vec3{} );
            break;
        }

        planes[static_cast<std::size_t>( numplanes )] = tr.t.plane.normal;
        numplanes++;

        int i = 0;
        for ( ; i < numplanes; ++i )
        {
            clip_velocity( original_velocity,
                           planes[static_cast<std::size_t>( i )], new_velocity,
                           1.0f );
            int j = 0;
            for ( ; j < numplanes; ++j )
            {
                if ( j != i &&
                     ut::dot( new_velocity,
                              planes[static_cast<std::size_t>( j )] ) < 0.0f )
                    break;
            }
            if ( j == numplanes )
                break;
        }

        if ( i != numplanes )
        {
            v.set_velocity( new_velocity );
        }
        else
        {
            if ( numplanes != 2 )
            {
                v.set_velocity( Vec3{} );
                break;
            }
            const Vec3 dir = ut::cross( planes[0], planes[1] );
            const float d  = ut::dot( dir, v.velocity() );
            v.set_velocity( dir * d );
        }

        if ( ut::dot( v.velocity(), primal_velocity ) <= 0.0f )
        {
            v.set_velocity( Vec3{} );
            break;
        }
    }

    if ( allFraction == 0.0f )
        v.set_velocity( Vec3{} );

    return blocked;
}

// SV_AddGravity (sv_phys.c:737): the "add gravity incorrectly" basevel fold.
// compliance-allow(thread-assert): TU-private per-entity step of the asserted
// sv_physics chain (physics.cpp:1652), per-frame hot path
void add_gravity( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView  v = EntityView( ent );
    const float ent_gravity = v.gravity() != 0.0f ? v.gravity() : 1.0f;
    const float dt          = rt.level.frametime;

    Vec3 vel      = v.velocity();
    Vec3 basevel  = v.basevelocity();
    vel.z -= ent_gravity * rt.movevars.gravity * dt;
    vel.z += basevel.z * dt;
    basevel.z = 0.0f;
    v.set_velocity( vel );
    v.set_basevelocity( basevel );

    check_velocity( rt, ent );
}

// ---------------------------------------------------------------------------
// Pusher machinery (sv_phys.c:768-1216)
// ---------------------------------------------------------------------------

// SV_AllowPushRotate (sv_phys.c:768): brush pushers rotate yaw only with the
// ENGINE_PHYSICS_PUSHER_EXT feature + MODEL_HAS_ORIGIN; non-brush always.
[[nodiscard]] bool allow_push_rotate( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    if ( rt.move_env.models == nullptr )
        return true;
    const std::optional<BrushModel> bm =
        rt.move_env.models->brush_model( v.modelindex() );
    if ( !bm.has_value() )
        return true; // not a brush model
    if ( !rt.move_env.pusher_ext )
        return false;
    return bm->has_origin;
}

// SV_PushEntity (sv_phys.c:793).
[[nodiscard]] SvTrace push_entity( ServerRuntime &rt, abi::edict_t *ent,
                                   const Vec3 &lpush, const Vec3 &apush,
                                   bool *blocked ) noexcept
{
    EntityView v( ent );
    const bool monster_clip = fbit( v.flags(), abi::k_fl_monsterclip );
    const Vec3 end          = v.origin() + lpush;

    int type;
    if ( v.movetype() == abi::k_movetype_flymissile )
        type = abi::k_move_missile;
    else if ( v.solid() == abi::k_solid_trigger || v.solid() == abi::k_solid_not )
        type = abi::k_move_nomonsters;
    else
        type = abi::k_move_normal;

    const SvTrace trace =
        sv_move( rt, v.origin(), v.mins(), v.maxs(), end, type, ent, monster_clip );

    if ( trace.t.fraction != 0.0f )
    {
        v.set_origin( trace.t.endpos );

        if ( rt.level.state == ServerState::Active && apush.y != 0.0f &&
             fbit( v.flags(), abi::k_fl_client ) )
        {
            Vec3 avel = v.avelocity();
            avel.y += apush.y;
            v.set_avelocity( avel );
            v.set_fixangle( 2 );
        }

        if ( allow_push_rotate( rt, ent ) )
        {
            Vec3 ang = v.angles();
            ang.y += trace.t.fraction * apush.y;
            v.set_angles( ang );
        }
    }

    link_edict( rt, ent, true );

    const bool monster_block = v.movetype() == abi::k_movetype_walk ||
                               v.movetype() == abi::k_movetype_step ||
                               v.movetype() == abi::k_movetype_pushstep;

    if ( blocked != nullptr )
    {
        if ( monster_block )
            *blocked = !vec_compare_epsilon( v.origin(), end, k_on_epsilon );
        else
            *blocked = true;
    }

    if ( valid_edict( trace.ent ) )
        sv_impact( rt, ent, trace.ent, trace );

    return trace;
}

// SV_CanPushed (sv_phys.c:855).
[[nodiscard]] bool can_pushed( abi::edict_t *ent ) noexcept
{
    switch ( EntityView( ent ).movetype() )
    {
    case abi::k_movetype_none:
    case abi::k_movetype_push:
    case abi::k_movetype_follow:
    case abi::k_movetype_noclip:
    case abi::k_movetype_compound:
        return false;
    }
    return true;
}

// SV_CanBlock (sv_phys.c:877): zeroes deadbody bounds as a side effect.
[[nodiscard]] bool can_block( abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    Vec3       mins = v.mins();
    Vec3       maxs = v.maxs();

    if ( mins.x == maxs.x )
        return false;

    if ( v.solid() == abi::k_solid_not || v.solid() == abi::k_solid_trigger )
    {
        mins.x = mins.y = 0.0f; // clear bounds for deadbody
        v.set_mins( mins );
        v.set_maxs( mins );
        return false;
    }
    return true;
}

// SV_PushMove (sv_phys.c:899).
[[nodiscard]] abi::edict_t *push_move( ServerRuntime &rt, abi::edict_t *pusher,
                                       float movetime ) noexcept
{
    EntityView pv( pusher );

    if ( rt.globals.changelevel != 0 || is_null( pv.velocity() ) )
    {
        pv.set_ltime( pv.ltime() + movetime );
        return nullptr;
    }

    Vec3 lmove = pv.velocity() * movetime;
    Vec3 mins  = pv.absmin() + lmove;
    Vec3 maxs  = pv.absmax() + lmove;

    PushedEnt *pushed_p = rt.pushed;
    PushedEnt *pushed_end = rt.pushed + ::xash::limits::server_pushed_ents;

    // save the pusher's original position
    pushed_p->ent    = pusher;
    pushed_p->origin = pv.origin();
    pushed_p->angles = pv.angles();
    pushed_p++;

    linear_move( rt, pusher, movetime, 0.0f );
    link_edict( rt, pusher, false );
    pv.set_ltime( pv.ltime() + movetime );
    const int oldsolid = pv.solid();

    if ( pv.solid() == abi::k_solid_not )
        return nullptr;

    for ( int e = 1; e < static_cast<int>( rt.arena.num_entities() ); ++e )
    {
        abi::edict_t *check = rt.arena.edict_num( static_cast<std::size_t>( e ) );
        if ( !valid_edict( check ) || !can_pushed( check ) )
            continue;

        EntityView cv( check );

        pv.set_solid( abi::k_solid_not );
        const bool block = test_entity_position( rt, check, pusher );
        pv.set_solid( oldsolid );
        if ( block )
            continue;

        const bool on_pusher = fbit( cv.flags(), abi::k_fl_onground ) &&
                               cv.groundentity() == pusher;
        if ( !on_pusher )
        {
            const Vec3 camin = cv.absmin();
            const Vec3 camax = cv.absmax();
            if ( camin.x >= maxs.x || camin.y >= maxs.y || camin.z >= maxs.z ||
                 camax.x <= mins.x || camax.y <= mins.y || camax.z <= mins.z )
                continue;
            if ( !test_entity_position( rt, check, nullptr ) )
                continue;
        }

        if ( cv.movetype() != abi::k_movetype_walk )
            cv.clear_flags( abi::k_fl_onground );

        if ( pushed_p >= pushed_end )
        {
            ::xash::core::log_error(
                "server", "SV_PushMove: pushed-ents stack overflow" );
            break; // hardening: legacy has no bounds check (server.h:59)
        }
        pushed_p->ent    = check;
        pushed_p->origin = cv.origin();
        pushed_p->angles = cv.angles();
        pushed_p++;

        bool blocked = false;
        pv.set_solid( abi::k_solid_not );
        push_entity( rt, check, lmove, Vec3{}, &blocked );
        pv.set_solid( oldsolid );

        if ( test_entity_position( rt, check, nullptr ) && blocked )
        {
            if ( !can_block( check ) )
                continue;

            pv.set_ltime( pv.ltime() - movetime );

            for ( PushedEnt *p = pushed_p - 1; p >= rt.pushed; --p )
            {
                EntityView ev( p->ent );
                ev.set_origin( p->origin );
                ev.set_angles( p->angles );
                link_edict( rt, p->ent, p->ent == check );
            }
            return check;
        }
    }
    return nullptr;
}

// SV_PushRotate (sv_phys.c:1014).
[[nodiscard]] abi::edict_t *push_rotate( ServerRuntime &rt, abi::edict_t *pusher,
                                         float movetime ) noexcept
{
    EntityView pv( pusher );

    if ( rt.globals.changelevel != 0 || is_null( pv.avelocity() ) )
    {
        pv.set_ltime( pv.ltime() + movetime );
        return nullptr;
    }

    const Vec3 amove = pv.avelocity() * movetime;

    // pusher initial position + its inverse (world→local).
    const ut::Matrix3x4 start_l     = ut::from_angles( pv.origin(), pv.angles() );
    const ut::Matrix3x4 start_l_inv = ut::invert_ortho( start_l );

    PushedEnt *pushed_p   = rt.pushed;
    PushedEnt *pushed_end = rt.pushed + ::xash::limits::server_pushed_ents;

    pushed_p->ent    = pusher;
    pushed_p->origin = pv.origin();
    pushed_p->angles = pv.angles();
    pushed_p++;

    angular_move( rt, pusher, movetime, pv.friction() );
    link_edict( rt, pusher, false );
    pv.set_ltime( pv.ltime() + movetime );
    const int oldsolid = pv.solid();

    if ( pv.solid() == abi::k_solid_not )
        return nullptr;

    // pusher final position
    const ut::Matrix3x4 end_l = ut::from_angles( pv.origin(), pv.angles() );

    for ( int e = 1; e < static_cast<int>( rt.arena.num_entities() ); ++e )
    {
        abi::edict_t *check = rt.arena.edict_num( static_cast<std::size_t>( e ) );
        if ( !valid_edict( check ) || !can_pushed( check ) )
            continue;

        EntityView cv( check );

        pv.set_solid( abi::k_solid_not );
        const bool block = test_entity_position( rt, check, pusher );
        pv.set_solid( oldsolid );
        if ( block )
            continue;

        const bool on_pusher = fbit( cv.flags(), abi::k_fl_onground ) &&
                               cv.groundentity() == pusher;
        if ( !on_pusher )
        {
            const Vec3 camin = cv.absmin();
            const Vec3 camax = cv.absmax();
            const Vec3 pamin = pv.absmin();
            const Vec3 pamax = pv.absmax();
            if ( camin.x >= pamax.x || camin.y >= pamax.y || camin.z >= pamax.z ||
                 camax.x <= pamin.x || camax.y <= pamin.y || camax.z <= pamin.z )
                continue;
            if ( !test_entity_position( rt, check, nullptr ) )
                continue;
        }

        if ( pushed_p >= pushed_end )
        {
            ::xash::core::log_error(
                "server", "SV_PushRotate: pushed-ents stack overflow" );
            break;
        }
        pushed_p->ent      = check;
        pushed_p->origin   = cv.origin();
        pushed_p->angles   = cv.angles();
        pushed_p->fixangle = cv.fixangle();
        pushed_p++;

        // destination position via the two-matrix transform
        Vec3 org;
        if ( cv.movetype() == abi::k_movetype_pushstep ||
             cv.movetype() == abi::k_movetype_step )
            org = ( cv.absmin() + cv.absmax() ) * 0.5f;
        else
            org = cv.origin();

        const Vec3 temp = ut::transform_point( start_l_inv, org );
        const Vec3 org2 = ut::transform_point( end_l, temp );
        Vec3       lmove = org2 - org;

        if ( cv.movetype() != abi::k_movetype_walk )
        {
            if ( lmove.z != 0.0f )
                cv.clear_flags( abi::k_fl_onground );
            if ( lmove.z < 0.0f && pv.dmg() == 0.0f )
                lmove.z = 0.0f; // let's the free falling
        }

        bool blocked = false;
        pv.set_solid( abi::k_solid_not );
        push_entity( rt, check, lmove, amove, &blocked );
        pv.set_solid( oldsolid );

        if ( blocked && cv.movetype() != abi::k_movetype_walk )
            cv.clear_flags( abi::k_fl_onground );

        if ( test_entity_position( rt, check, nullptr ) && blocked )
        {
            if ( !can_block( check ) )
                continue;

            pv.set_ltime( pv.ltime() - movetime );

            for ( PushedEnt *p = pushed_p - 1; p >= rt.pushed; --p )
            {
                EntityView ev( p->ent );
                ev.set_origin( p->origin );
                ev.set_angles( p->angles );
                link_edict( rt, p->ent, p->ent == check );
                ev.set_fixangle( p->fixangle );
            }
            return check;
        }
    }
    return nullptr;
}

// SV_Physics_Pusher (sv_phys.c:1152).
void physics_pusher( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );

    abi::edict_t *blocker   = nullptr;
    const float   oldtime   = v.ltime();
    const float   thinktime = v.nextthink();
    float         movetime;

    if ( thinktime < oldtime + rt.level.frametime )
    {
        movetime = thinktime - oldtime;
        if ( movetime < 0.0f )
            movetime = 0.0f;
    }
    else
    {
        movetime = rt.level.frametime;
    }

    if ( movetime != 0.0f )
    {
        if ( !is_null( v.avelocity() ) )
        {
            if ( !is_null( v.velocity() ) )
            {
                blocker = push_rotate( rt, ent, movetime );
                if ( blocker == nullptr )
                {
                    const float oldtime2 = v.ltime();
                    v.set_ltime( oldtime ); // reset local time before rotate
                    blocker = push_move( rt, ent, movetime );
                    if ( v.ltime() < oldtime2 )
                        v.set_ltime( oldtime2 );
                }
            }
            else
            {
                blocker = push_rotate( rt, ent, movetime );
            }
        }
        else
        {
            blocker = push_move( rt, ent, movetime );
        }
    }

    if ( blocker != nullptr && rt.game.funcs().pfnBlocked != nullptr )
        rt.game.funcs().pfnBlocked( ent, blocker );

    Vec3 ang = v.angles();
    float *a = &ang.x;
    for ( int i = 0; i < 3; ++i )
    {
        if ( a[i] < -3600.0f || a[i] > 3600.0f )
            a[i] = std::fmod( a[i], 3600.0f ); // note 3600, not 360
    }
    v.set_angles( ang );

    if ( thinktime > oldtime &&
         ( fbit( v.flags(), abi::k_fl_alwaysthink ) || thinktime <= v.ltime() ) )
    {
        v.set_nextthink( 0.0f );
        rt.globals.time = static_cast<float>( rt.level.time );
        call_think( rt, ent );
    }
}

// SV_Physics_Follow (sv_phys.c:1226).
void physics_follow( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    if ( !run_think( rt, ent ) )
        return;

    EntityView    v( ent );
    abi::edict_t *parent = v.aiment();
    if ( !valid_edict( parent ) )
    {
        v.set_movetype( abi::k_movetype_none );
        return;
    }

    EntityView p( parent );
    v.set_origin( p.origin() + v.v_angle() );
    v.set_angles( p.angles() );
    link_edict( rt, ent, true );
}

// SV_Physics_Noclip (sv_phys.c:1334).
void physics_noclip( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    if ( !run_think( rt, ent ) )
        return;

    check_water( rt, ent );

    EntityView  v( ent );
    const float dt = rt.level.frametime;
    v.set_origin( v.origin() + v.velocity() * dt );
    v.set_angles( v.angles() + v.avelocity() * dt );
    link_edict( rt, ent, false ); // noclip never touches triggers
}

// SV_CheckWaterTransition (sv_phys.c:1361): water level tracking + splashes.
void check_water_transition( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );
    const Vec3 amin = v.absmin();
    const Vec3 amax = v.absmax();

    Vec3 point{ ( amax.x + amin.x ) * 0.5f, ( amax.y + amin.y ) * 0.5f,
                amin.z + 1.0f };
    int  cont = pt_contents( rt, v.groupinfo(), point );

    if ( v.watertype() == 0 )
    {
        v.set_watertype( cont ); // just spawned here
        v.set_waterlevel( 1 );
        return;
    }

    if ( is_water_contents( cont ) )
    {
        if ( v.watertype() == ml::k_contents_empty )
        {
            // XASH3DPP-STUB(chunk6-S9): PlayerWaterEnter splash (SoundList +
            // SV_StartSound) needs the multicast pipeline.
            Vec3 vel = v.velocity();
            vel.z *= 0.5f;
            v.set_velocity( vel );
        }

        v.set_watertype( cont );
        v.set_waterlevel( 1 );

        if ( amin.z != amax.z )
        {
            point.z = ( amin.z + amax.z ) * 0.5f;
            cont    = pt_contents( rt, v.groupinfo(), point );
            if ( is_water_contents( cont ) )
            {
                v.set_waterlevel( 2 );
                point = point + v.view_ofs();
                cont  = pt_contents( rt, v.groupinfo(), point );
                if ( is_water_contents( cont ) )
                    v.set_waterlevel( 3 );
            }
        }
        else
        {
            v.set_waterlevel( 3 );
        }
    }
    else
    {
        // XASH3DPP-STUB(chunk6-S9): PlayerWaterExit splash when leaving water.
        v.set_watertype( ml::k_contents_empty );
        v.set_waterlevel( 0 );
    }
}

// SV_Physics_Toss (sv_phys.c:1438).
void physics_toss( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );

    check_water( rt, ent );

    if ( !run_think( rt, ent ) )
        return;

    abi::edict_t *ground = v.groundentity();

    if ( v.velocity().z > 0.0f )
        v.clear_flags( abi::k_fl_onground );

    if ( !valid_edict( ground ) ||
         ( EntityView( ground ).flags() &
           ( abi::k_fl_monster | abi::k_fl_client ) ) != 0 )
        v.clear_flags( abi::k_fl_onground );

    if ( fbit( v.flags(), abi::k_fl_onground ) && is_null( v.velocity() ) )
    {
        v.set_avelocity( Vec3{} );
        if ( is_null( v.basevelocity() ) )
            return; // at rest
    }

    check_velocity( rt, ent );

    switch ( v.movetype() )
    {
    case abi::k_movetype_fly:
    case abi::k_movetype_flymissile:
    case abi::k_movetype_bouncemissile:
        break;
    default:
        add_gravity( rt, ent );
        break;
    }

    switch ( v.movetype() )
    {
    case abi::k_movetype_toss:
    case abi::k_movetype_bounce:
        angular_move( rt, ent, rt.level.frametime, v.friction() );
        break;
    default:
        angular_move( rt, ent, rt.level.frametime, 0.0f );
        break;
    }

    // move origin — basevelocity folded in then back out across the bounce
    v.set_velocity( v.velocity() + v.basevelocity() );
    check_velocity( rt, ent );
    Vec3 mv = v.velocity() * rt.level.frametime;
    v.set_velocity( v.velocity() - v.basevelocity() );

    SvTrace trace = push_entity( rt, ent, mv, Vec3{}, nullptr );
    if ( ent->free )
        return;

    check_velocity( rt, ent );

    if ( trace.t.allsolid )
    {
        v.set_avelocity( Vec3{} );
        v.set_velocity( Vec3{} );
        return;
    }

    if ( trace.t.fraction == 1.0f )
    {
        check_water_transition( rt, ent );
        return;
    }

    float backoff;
    if ( v.movetype() == abi::k_movetype_bounce )
        backoff = 2.0f - v.friction();
    else if ( v.movetype() == abi::k_movetype_bouncemissile )
        backoff = 2.0f;
    else
        backoff = 1.0f;

    Vec3 vel = v.velocity();
    clip_velocity( vel, trace.t.plane.normal, vel, backoff );
    v.set_velocity( vel );

    if ( trace.t.plane.normal.z > 0.7f )
    {
        Vec3        move2 = v.velocity() + v.basevelocity();
        const float vel2  = ut::dot( move2, move2 );

        if ( v.velocity().z < rt.movevars.gravity * rt.level.frametime )
        {
            v.set_groundentity( trace.ent );
            v.add_flags( abi::k_fl_onground );
            Vec3 vv = v.velocity();
            vv.z    = 0.0f;
            v.set_velocity( vv );
        }

        if ( vel2 < 900.0f || ( v.movetype() != abi::k_movetype_bounce &&
                                v.movetype() != abi::k_movetype_bouncemissile ) )
        {
            v.add_flags( abi::k_fl_onground );
            v.set_groundentity( trace.ent );
            v.set_avelocity( Vec3{} );
            v.set_velocity( Vec3{} );
        }
        else
        {
            const float scale = ( 1.0f - trace.t.fraction ) *
                                rt.level.frametime * 0.9f;
            Vec3 mv2 = v.velocity() * scale + v.basevelocity() * scale;
            trace    = push_entity( rt, ent, mv2, Vec3{}, nullptr );
            if ( ent->free )
                return;
        }
    }

    check_water_transition( rt, ent );
}

// SV_CheckBottom (sv_move.c:34): ledge test used by Step's friction gate.
[[nodiscard]] bool check_bottom( ServerRuntime &rt, abi::edict_t *ent,
                                 int mode ) noexcept
{
    EntityView v( ent );
    const bool monster_clip = fbit( v.flags(), abi::k_fl_monsterclip );
    const Vec3 mins         = v.origin() + v.mins();
    const Vec3 maxs         = v.origin() + v.maxs();

    Vec3 start;
    start.z = mins.z - 1.0f;
    for ( int x = 0; x <= 1; ++x )
    {
        for ( int y = 0; y <= 1; ++y )
        {
            start.x = x ? maxs.x : mins.x;
            start.y = y ? maxs.y : mins.y;
            if ( pt_contents( rt, v.groupinfo(), start ) != ml::k_contents_solid )
                goto realcheck;
        }
    }
    return true; // got out easy

realcheck:
    const float stepsize = rt.movevars.stepsize;
    start.z = mins.z; // ENGINE_QUAKE_COMPATIBLE would skip the += stepsize
    start.z += stepsize;
    start.x = ( mins.x + maxs.x ) * 0.5f;
    start.y = ( mins.y + maxs.y ) * 0.5f;

    Vec3 stop = start;
    stop.z    = start.z - 2.0f * stepsize;

    SvTrace tr = ( mode == 1 /*WALKMOVE_WORLDONLY*/ )
                     ? move_no_ents( rt.move_env, start, {}, {}, stop,
                                     abi::k_move_nomonsters, ent )
                     : sv_move( rt, start, {}, {}, stop, abi::k_move_nomonsters,
                                ent, monster_clip );

    if ( tr.t.fraction == 1.0f )
        return false;

    const float mid    = tr.t.endpos.z;
    for ( int x = 0; x <= 1; ++x )
    {
        for ( int y = 0; y <= 1; ++y )
        {
            start.x = stop.x = x ? maxs.x : mins.x;
            start.y = stop.y = y ? maxs.y : mins.y;

            tr = ( mode == 1 )
                     ? move_no_ents( rt.move_env, start, {}, {}, stop,
                                     abi::k_move_nomonsters, ent )
                     : sv_move( rt, start, {}, {}, stop,
                                abi::k_move_nomonsters, ent, monster_clip );

            if ( tr.t.fraction == 1.0f || mid - tr.t.endpos.z > stepsize )
                return false;
        }
    }
    return true;
}

// SV_WaterMove (sv_move.c:106): drowning/lava/slime bookkeeping + water drag.
void water_move( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );

    if ( v.movetype() == abi::k_movetype_noclip )
    {
        v.set_air_finished( static_cast<float>( rt.level.time ) + 12.0f );
        return;
    }

    // no watermove for dead monsters (but pushables get it)
    if ( fbit( v.flags(), abi::k_fl_monster ) && v.health() <= 0.0f )
        return;

    const float drownlevel = ( v.deadflag() == abi::k_dead_no ) ? 3.0f : 1.0f;
    const int   waterlevel = v.waterlevel();
    const int   watertype  = v.watertype();
    const int   flags      = v.flags();
    // Q-18: the timers compare/store against the DOUBLE server clock (legacy
    // uses `sv.time` directly — float field promoted to double for the compare,
    // `sv.time + Nf` computed in double then rounded once on the float store).
    // A pre-rounded float clock here would do the arithmetic in single precision
    // and drift by a ULP, flipping a drown/lava boundary a frame early/late.
    const double now       = rt.level.time;

    if ( ( flags & ( abi::k_fl_immune_water | abi::k_fl_godmode ) ) == 0 )
    {
        if ( ( fbit( flags, abi::k_fl_swim ) &&
               static_cast<float>( waterlevel ) > drownlevel ) ||
             static_cast<float>( waterlevel ) <= drownlevel )
        {
            if ( v.air_finished() > now && v.pain_finished() > now )
            {
                float dmg = v.dmg() + 2.0f;
                if ( dmg < 15.0f )
                    dmg = 10.0f; // quake1 original code
                v.set_dmg( dmg );
                v.set_pain_finished( static_cast<float>( now + 1.0f ) );
            }
        }
        else
        {
            v.set_air_finished( static_cast<float>( now + 12.0f ) );
            v.set_dmg( 2.0f );
        }
    }

    if ( waterlevel == 0 )
    {
        if ( fbit( flags, abi::k_fl_inwater ) )
        {
            // XASH3DPP-STUB(chunk6-S9): EntityWaterExit splash sound.
            v.set_flags( flags & ~abi::k_fl_inwater );
        }
        v.set_air_finished( static_cast<float>( now + 12.0f ) );
        return;
    }

    if ( watertype == ml::k_contents_lava )
    {
        if ( ( flags & ( abi::k_fl_immune_lava | abi::k_fl_godmode ) ) == 0 &&
             v.dmgtime() < now )
            v.set_dmgtime( static_cast<float>(
                v.radsuit_finished() < now ? now + 0.2f : now + 1.0f ) );
    }
    else if ( watertype == ml::k_contents_slime )
    {
        if ( ( flags & ( abi::k_fl_immune_slime | abi::k_fl_godmode ) ) == 0 &&
             v.dmgtime() < now && v.radsuit_finished() < now )
            v.set_dmgtime( static_cast<float>( now + 1.0f ) );
    }

    if ( !fbit( flags, abi::k_fl_inwater ) )
    {
        // XASH3DPP-STUB(chunk6-S9): EntityWaterEnter splash sound.
        v.set_flags( flags | abi::k_fl_inwater );
        v.set_dmgtime( 0.0f );
    }

    if ( !fbit( flags, abi::k_fl_waterjump ) )
    {
        const float f = static_cast<float>( waterlevel ) * -0.8f *
                        rt.level.frametime;
        v.set_velocity( v.velocity() + v.velocity() * f );
    }
}

// SV_Physics_Step (sv_phys.c:1584).
void physics_step( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    EntityView v( ent );

    water_move( rt, ent );
    check_velocity( rt, ent );

    const bool wasonground = fbit( v.flags(), abi::k_fl_onground );
    const bool wasonmover  = check_mover( rt, ent );
    const bool inwater      = check_water( rt, ent );

    if ( fbit( v.flags(), abi::k_fl_float ) && v.waterlevel() > 0 )
    {
        const float buoyancy = submerged( rt, ent ) *
                               static_cast<float>( v.skin() ) *
                               rt.level.frametime;
        add_gravity( rt, ent );
        Vec3 vel = v.velocity();
        vel.z += buoyancy;
        v.set_velocity( vel );
    }

    if ( !wasonground )
    {
        if ( !fbit( v.flags(), abi::k_fl_fly ) )
        {
            if ( !fbit( v.flags(), abi::k_fl_swim ) || v.waterlevel() <= 0 )
            {
                if ( !inwater )
                    add_gravity( rt, ent );
            }
        }
    }

    if ( !is_null( v.velocity() ) || !is_null( v.basevelocity() ) )
    {
        v.clear_flags( abi::k_fl_onground );

        if ( ( wasonground || wasonmover ) &&
             ( v.health() > 0.0f || check_bottom( rt, ent, 0 ) ) )
        {
            Vec3        vel   = v.velocity();
            const float speed = std::sqrt( vel.x * vel.x + vel.y * vel.y );
            if ( speed != 0.0f )
            {
                float friction = rt.movevars.friction * v.friction();
                v.set_friction( 1.0f ); // g-cont ???
                if ( wasonmover )
                    friction *= 0.5f;

                const float control =
                    ( speed < rt.movevars.stopspeed ) ? rt.movevars.stopspeed
                                                      : speed;
                float newspeed = speed - rt.level.frametime * control * friction;
                if ( newspeed < 0.0f )
                    newspeed = 0.0f;
                newspeed /= speed;
                vel.x *= newspeed;
                vel.y *= newspeed;
                v.set_velocity( vel );
            }
        }

        v.set_velocity( v.velocity() + v.basevelocity() );
        check_velocity( rt, ent );

        fly_move( rt, ent, rt.level.frametime, nullptr );
        if ( ent->free )
            return;

        check_velocity( rt, ent );
        v.set_velocity( v.velocity() - v.basevelocity() );
        check_velocity( rt, ent );

        const Vec3 mins = v.origin() + v.mins();
        const Vec3 maxs = v.origin() + v.maxs();
        Vec3       point;
        point.z = mins.z - 1.0f;

        for ( int x = 0; x <= 1; ++x )
        {
            if ( fbit( v.flags(), abi::k_fl_onground ) )
                break;
            for ( int y = 0; y <= 1; ++y )
            {
                point.x = x ? maxs.x : mins.x;
                point.y = y ? maxs.y : mins.y;

                const SvTrace tr = sv_move( rt, point, {}, {}, point,
                                            abi::k_move_normal, ent, false );
                if ( tr.t.startsolid )
                {
                    v.add_flags( abi::k_fl_onground );
                    v.set_groundentity( tr.ent );
                    v.set_friction( 1.0f );
                    break;
                }
            }
        }
        link_edict( rt, ent, true );
    }
    else
    {
        if ( rt.globals.force_retouch != 0.0f )
        {
            const bool monster_clip = fbit( v.flags(), abi::k_fl_monsterclip );
            const SvTrace tr =
                sv_move( rt, v.origin(), v.mins(), v.maxs(), v.origin(),
                         abi::k_move_normal, ent, monster_clip );
            if ( ( tr.t.fraction < 1.0f || tr.t.startsolid ) &&
                 valid_edict( tr.ent ) )
            {
                sv_impact( rt, ent, tr.ent, tr );
                if ( ent->free )
                    return;
            }
        }
    }

    if ( !run_think( rt, ent ) )
        return;
    check_water_transition( rt, ent );
}

// SV_Physics_None (sv_phys.c:1716).
void physics_none( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    (void)run_think( rt, ent );
}

// SV_Physics_Entity (sv_phys.c:1722).
void physics_entity( ServerRuntime &rt, abi::edict_t *ent ) noexcept
{
    // XASH3DPP-STUB(chunk6-S9): physFuncs.SV_PhysicsEntity DLL override — the
    // physics interface (physint.h) negotiation lands with the messaging slice.

    update_base_velocity( rt, ent );

    EntityView v( ent );
    if ( !fbit( v.flags(), abi::k_fl_basevelocity ) && !is_null( v.basevelocity() ) )
    {
        // apply momentum (add half of the previous frame's velocity first)
        v.set_velocity( v.velocity() +
                        v.basevelocity() * ( 1.0f + rt.level.frametime * 0.5f ) );
        v.set_basevelocity( Vec3{} );
    }
    v.clear_flags( abi::k_fl_basevelocity );

    if ( rt.globals.force_retouch != 0.0f )
        link_edict( rt, ent, true ); // force retouch even for stationary

    switch ( v.movetype() )
    {
    case abi::k_movetype_none:
        physics_none( rt, ent );
        break;
    case abi::k_movetype_noclip:
        physics_noclip( rt, ent );
        break;
    case abi::k_movetype_follow:
        physics_follow( rt, ent );
        break;
    case abi::k_movetype_compound:
        // XASH3DPP-STUB(chunk6-S9): SV_Physics_Compound needs the Matrix4x4
        // Invert_Simple/Concat family (utilities) + ENGINE_COMPENSATE_QUAKE_BUG
        // host feature; the glue-child movetype is rarely used and lands with
        // the host feature-flag wiring.  Behaves as MOVETYPE_NONE until then.
        physics_none( rt, ent );
        break;
    case abi::k_movetype_step:
    case abi::k_movetype_pushstep:
        physics_step( rt, ent );
        break;
    case abi::k_movetype_fly:
    case abi::k_movetype_toss:
    case abi::k_movetype_bounce:
    case abi::k_movetype_flymissile:
    case abi::k_movetype_bouncemissile:
        physics_toss( rt, ent );
        break;
    case abi::k_movetype_push:
        physics_pusher( rt, ent );
        break;
    case abi::k_movetype_walk:
        // players never come through the entity loop (sv_phys.c:1774
        // Host_Error).  Route through the host error hook rather than crash.
        if ( rt.cfg.host_error != nullptr )
            rt.cfg.host_error( rt.cfg.host_error_ctx,
                               "SV_Physics_Entity: bad movetype MOVETYPE_WALK" );
        break;
    default:
        break;
    }

    // don't free during loading — produces corrupted baselines (sv_phys.c:1780)
    if ( rt.level.state == ServerState::Active &&
         fbit( v.flags(), abi::k_fl_killme ) )
        free_edict( rt, ent );
}

} // namespace

// ---------------------------------------------------------------------------
// SV_UpdateBaseVelocity / SV_Impact — namespace-scope (external linkage) so the
// pmove run chain (run_cmd.cpp) can compose them; the definitions still reach
// the anonymous-block helpers above (same TU).
// ---------------------------------------------------------------------------

// SV_UpdateBaseVelocity (sv_phys.c:162): conveyor momentum handshake.
void update_base_velocity( ServerRuntime &, abi::edict_t *ent ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    EntityView v( ent );
    if ( !fbit( v.flags(), abi::k_fl_onground ) )
        return;

    abi::edict_t *ground = v.groundentity();
    if ( !valid_edict( ground ) )
        return;

    EntityView gv( ground );
    if ( !fbit( gv.flags(), abi::k_fl_conveyor ) )
        return;

    Vec3 new_basevel = gv.movedir() * gv.speed();
    if ( fbit( v.flags(), abi::k_fl_basevelocity ) )
        new_basevel = new_basevel + v.basevelocity();

    v.add_flags( abi::k_fl_basevelocity );
    v.set_basevelocity( new_basevel );
}

// SV_Impact (sv_phys.c:299): dispatch both touch functions.
void sv_impact( ServerRuntime &rt, abi::edict_t *e1, abi::edict_t *e2,
                const SvTrace &trace ) noexcept
{
    rt.globals.time = static_cast<float>( rt.level.time );

    EntityView v1( e1 ), v2( e2 );

    if ( ( ( v1.flags() | v2.flags() ) & abi::k_fl_killme ) != 0 )
        return;

    if ( v1.groupinfo() != 0 && v2.groupinfo() != 0 )
    {
        const bool intersect = ( v1.groupinfo() & v2.groupinfo() ) != 0;
        if ( rt.move_env.group_op == GroupOp::And && !intersect )
            return;
        if ( rt.move_env.group_op == GroupOp::Nand && intersect )
            return;
    }

    if ( v1.solid() != abi::k_solid_not )
    {
        copy_trace_to_global( rt, trace );
        if ( rt.game.funcs().pfnTouch != nullptr )
            rt.game.funcs().pfnTouch( e1, e2 );
    }

    if ( v2.solid() != abi::k_solid_not )
    {
        copy_trace_to_global( rt, trace );
        if ( rt.game.funcs().pfnTouch != nullptr )
            rt.game.funcs().pfnTouch( e2, e1 );
    }
}

// ---------------------------------------------------------------------------
// SV_Physics (sv_phys.c:1812)
// ---------------------------------------------------------------------------

void sv_physics( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // XASH3DPP-STUB(chunk6-S9): SV_CheckAllEnts — the 5-second self-heal sweep
    // is gated on sv_check_errors (default 0) and only repairs trashed
    // pContainingEntity/private-data; inert on the default path.

    rt.globals.time = static_cast<float>( rt.level.time );

    // let the game know a new frame has started
    if ( rt.game.funcs().pfnStartFrame != nullptr )
        rt.game.funcs().pfnStartFrame();

    const int maxclients = rt.persistent.maxclients;
    for ( int i = 0; i < static_cast<int>( rt.arena.num_entities() ); ++i )
    {
        abi::edict_t *ent = rt.arena.edict_num( static_cast<std::size_t>( i ) );
        if ( !valid_edict( ent ) )
            continue;

        // players (1..maxclients) simulate via usercmds, not this loop
        if ( i > 0 && i <= maxclients )
            continue;

        physics_entity( rt, ent );
    }

    if ( rt.globals.force_retouch != 0.0f )
        rt.globals.force_retouch -= 1.0f;

    // XASH3DPP-STUB(chunk6-S9): physFuncs.SV_EndFrame override hook.

    rt.lightstyles.run_frame( rt.level.frametime ); // SV_RunLightStyles

    rt.level.framecount++;
}

// ---------------------------------------------------------------------------
// SV_PrepWorldFrame (sv_main.c:550)
// ---------------------------------------------------------------------------

void sv_prep_world_frame( ServerRuntime &rt ) noexcept
{
    for ( int i = 1; i < static_cast<int>( rt.arena.num_entities() ); ++i )
    {
        abi::edict_t *ent = rt.arena.edict_num( static_cast<std::size_t>( i ) );
        if ( ent == nullptr || ent->free )
            continue;
        EntityView v( ent );
        v.set_effects( v.effects() &
                       ~( abi::k_ef_muzzleflash | abi::k_ef_nointerp ) );
    }

    // XASH3DPP-STUB(chunk6-S9): physFuncs.pfnPrepWorldFrame override hook.
}

// ---------------------------------------------------------------------------
// SV_IsSimulating (sv_main.c:572)
// ---------------------------------------------------------------------------

bool sv_is_simulating( const ServerRuntime &rt ) noexcept
{
    if ( rt.cfg.dedicated )
        return true; // always active for dedicated servers

    // XASH3DPP-STUB(chunk6-S9/OQ-4): the listen-server path needs the client
    // hooks (CL_Active/CL_IsInGame/background map).  Until they exist, honour
    // only the local pause/players-only freeze.
    if ( rt.persistent.maxclients <= 1 && rt.level.playersonly )
        return false;
    return !rt.level.paused;
}

// ---------------------------------------------------------------------------
// SV_RunGameFrame (sv_main.c:602)
// ---------------------------------------------------------------------------

bool sv_run_game_frame( ServerRuntime &rt, float sv_fps ) noexcept
{
    rt.level.simulating = sv_is_simulating( rt );
    if ( !rt.level.simulating )
        return true;

    if ( sv_fps != 0.0f )
    {
        const double fps = 1.0 / static_cast<double>( sv_fps - 0.01f ); // FP fudge
        int          numFrames = 0;

        while ( rt.level.time_residual >= fps )
        {
            rt.level.frametime = static_cast<float>( fps );
            sv_physics( rt );
            rt.level.time_residual -= fps;
            rt.level.time += fps;
            rt.bridge.sv_time = rt.level.time; // ABI-shim mirror of sv.time
            numFrames++;
        }
        return numFrames != 0;
    }

    sv_physics( rt );
    rt.level.time += rt.level.frametime;
    rt.bridge.sv_time = rt.level.time; // ABI-shim mirror of sv.time
    return true;
}

// ---------------------------------------------------------------------------
// Host_ServerFrame (sv_main.c:678)
// ---------------------------------------------------------------------------

void host_server_frame( ServerRuntime &rt, double host_frametime ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( !rt.persistent.initialized )
        return;

    // sv_fps is only registered on listen builds (sv_main.c:893-895); an
    // absent cvar reads 0 → the dedicated one-step-per-host-frame path.
    const float sv_fps =
        rt.cvars != nullptr ? rt.cvars->cvar_variable_value( "sv_fps" ) : 0.0f;

    if ( sv_fps != 0.0f &&
         ( rt.level.simulating || rt.level.state != ServerState::Active ) )
        rt.level.time_residual += host_frametime;

    if ( sv_fps == 0.0f )
        rt.level.frametime = static_cast<float>( host_frametime );
    rt.globals.frametime = rt.level.frametime;

    // Client-facing ingress: SV_ReadPackets drains the shared NetworkContext's
    // server socket and routes connectionless traffic into the connection state
    // machine (OOB replies leave through the same context).  A null rt.net
    // (offline server) makes this a no-op.
    read_packets( rt );

    // XASH3DPP-STUB(chunk6-S9): SV_CheckCmdTimes (speed-hack clock) /
    // SV_RequestMissingResources (upload sweep) / SV_CheckTimeouts — land with
    // the in-session receive path (client realtime + last_received ageing).

    sv_update_movevars( rt, false );

    // let everything in the world think and move
    if ( !sv_run_game_frame( rt, sv_fps ) )
        return; // the zero-physics-frames early-return quirk

    // SV_SendClientMessages: flush each client's snapshot / keepalive through
    // its netchan (send-rate + bandwidth-choke gated) to the NetworkContext.
    send_client_messages( rt );

    sv_prep_world_frame( rt );

    // XASH3DPP-STUB(chunk6-S9): NET_MasterHeartbeat.
}

} // namespace xash::server
