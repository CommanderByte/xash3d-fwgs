// xash3dpp — SV_RunCmd: the per-usercmd player-move chain that drives the
// pmove bridge (Chunk 6 pmove-bridge P4).
// Legacy reference: engine/server/sv_pmove.c — SV_RunCmd (:887); the sv_phys
// helpers it composes — SV_PlayerRunThink (:263), SV_Impact (:299),
// SV_UpdateBaseVelocity (:162) [the last two exposed from physics.cpp]; plus
// PM_CheckMovingGround (sv_pmove.c:500) and PM_ConvertTrace (common/pm_local.h:44).
// Deep dive: docs/legacy-survey/deep-dive-server-physics.md §4.
//
// Q-20: SV_RunCmd is part of the pmove bridge, so raw `edict->v.` access is
// sanctioned here (as in pmove.cpp) — the state it shuttles (v_angle,
// clbasevelocity, light_level, buttons) is pmove-adjacent and deliberately
// not surfaced on the EntityView facade.
//
// Lag compensation (SV_SetupMoveInterpolant / SV_RestoreMoveInterpolant,
// sv_pmove.c:693/846) is P5 — deferred for the milestone; the non-fakeclient
// interpolant hooks are no-ops here (marked below).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/pmove.hpp>

#include <xash3dpp/abi/pm_defs.hpp>
#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/physics.hpp>    // update_base_velocity, sv_impact
#include <xash3dpp/world/links.hpp> // link_edict, LinkEnv
#include <xash3dpp/world/trace.hpp> // SvTrace

#include <cstddef>

namespace xash::server {

namespace abi = ::xash::abi;

namespace {

[[nodiscard]] inline bool fbit( int flags, int bit ) noexcept
{
    return ( flags & bit ) != 0;
}

[[nodiscard]] inline bool vec_is_null( const abi::vec3_t v ) noexcept
{
    return v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f;
}

// SV_PlayerRunThink (sv_phys.c:263): run the player's think if it is due.
// Unlike SV_RunThink it also skips FL_DORMANT and takes the passed timebase
// (not sv.time), and it never frees the edict — it only clears FL_KILLME.
// The physFuncs.SV_PlayerThink override is a physics-interface hook (S9 stub).
// Note the legacy control flow: when no think is due it returns *before* the
// trailing FL_KILLME clear, so that clear only runs on the not-due-but-not-
// dormant fall-through and the dormant/killme path.
void player_run_think( ServerRuntime &rt, abi::edict_t *ent, float frametime,
                       double time ) noexcept
{
    if ( !fbit( ent->v.flags, abi::k_fl_killme | abi::k_fl_dormant ) )
    {
        float thinktime = ent->v.nextthink;
        if ( thinktime <= 0.0f || thinktime > ( time + frametime ) )
            return; // not due — legacy returns here, skipping the KILLME clear

        if ( thinktime < time )
            thinktime = static_cast<float>( time ); // don't let it stay in the past

        ent->v.nextthink = 0.0f;
        rt.globals.time  = thinktime;
        if ( rt.game.funcs().pfnThink != nullptr )
            rt.game.funcs().pfnThink( ent );
    }

    if ( fbit( ent->v.flags, abi::k_fl_killme ) )
        ent->v.flags &= ~abi::k_fl_killme;
}

// PM_CheckMovingGround (sv_pmove.c:500): fold conveyor momentum into velocity.
// The physFuncs.SV_UpdatePlayerBaseVelocity override is an S9 physics-interface
// hook; the engine default is SV_UpdateBaseVelocity.
void check_moving_ground( ServerRuntime &rt, abi::edict_t *ent,
                          float frametime ) noexcept
{
    update_base_velocity( rt, ent );

    if ( !fbit( ent->v.flags, abi::k_fl_basevelocity ) )
    {
        // apply momentum (add in half of the previous frame of velocity first):
        // VectorMA( velocity, 1 + frametime*0.5, basevelocity, velocity )
        const float scale = 1.0f + ( frametime * 0.5f );
        for ( int i = 0; i < 3; ++i )
            ent->v.velocity[i] += scale * ent->v.basevelocity[i];
        ent->v.basevelocity[0] = ent->v.basevelocity[1] = ent->v.basevelocity[2] =
            0.0f;
    }

    ent->v.flags &= ~abi::k_fl_basevelocity;
}

// PM_ConvertTrace (common/pm_local.h:44): project a pmtrace_t back onto the
// engine-internal SvTrace (trace_t) the touch dispatch (SV_Impact) consumes.
[[nodiscard]] SvTrace convert_pmtrace( const abi::pmtrace_t &in,
                                       abi::edict_t *ent ) noexcept
{
    SvTrace out;
    out.t.allsolid   = ( in.allsolid != 0 );
    out.t.startsolid = ( in.startsolid != 0 );
    out.t.inopen     = ( in.inopen != 0 );
    out.t.inwater    = ( in.inwater != 0 );
    out.t.fraction   = in.fraction;
    out.t.endpos     = { in.endpos[0], in.endpos[1], in.endpos[2] };
    out.t.plane.normal = { in.plane.normal[0], in.plane.normal[1],
                           in.plane.normal[2] };
    out.t.plane.dist = in.plane.dist;
    out.ent          = ent;
    out.hitgroup     = in.hitgroup;
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// SV_RunCmd (sv_pmove.c:887)
// ---------------------------------------------------------------------------

void sv_run_cmd( ServerRuntime &rt, ServerClient &cl,
                 const abi::usercmd_t &ucmd, int random_seed ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // if the player got kicked, do not process commands.  Legacy guards on
    // `cl->state <= cs_zombie` (cs_free/cs_zombie); our ClientState enum orders
    // Zombie AFTER Spawned, so the guard is spelled out semantically.
    if ( cl.state == ClientState::Free || cl.state == ClientState::Zombie )
        return;
    if ( cl.edict == nullptr || rt.pmove == nullptr )
        return;

    abi::edict_t  *clent = cl.edict;
    abi::usercmd_t cmd   = ucmd; // local copy — msec gets chopped for long cmds

    // Speed-hack clock (dormant until SV_CheckCmdTimes, the S9 stub at
    // physics.cpp:1768, arms cl.ignorecmdtime; host.realtime == clients.realtime).
    if ( cl.ignorecmdtime > rt.clients.realtime )
    {
        if ( !cl.ignorecmdtime_warned && !cl.fakeclient )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Warning, "server",
                                "%s time is faster than server time (speed hack?)",
                                cl.name );
            cl.ignorecmdtime_warned = true;
            cl.ignorecmdtime_warns++;

            // automatically kick after sv_speedhack_kick warnings (0 disables).
            // SV_KickPlayer's broadcast kick message is an S9 stub; drop_client
            // is the engine-side removal.
            const float kick =
                rt.cvars != nullptr
                    ? rt.cvars->cvar_variable_value( "sv_speedhack_kick" )
                    : 0.0f;
            if ( kick != 0.0f &&
                 static_cast<float>( cl.ignorecmdtime_warns ) > kick )
                drop_client( rt, cl, false );
        }
        cl.cmdtime += static_cast<double>( ucmd.msec ) / 1000.0;
        return;
    }

    cl.ignorecmdtime        = 0.0;
    cl.ignorecmdtime_warned = false;

    // chop up very long commands (msec > 50): recurse in two halves, the
    // second with impulse zeroed so it can't double-fire.
    if ( cmd.msec > 50 )
    {
        const int oldmsec = ucmd.msec;
        cmd.msec = static_cast<std::int8_t>( oldmsec / 2 );
        sv_run_cmd( rt, cl, cmd, random_seed );
        cmd.msec    = static_cast<std::int8_t>( oldmsec / 2 );
        cmd.impulse = 0;
        sv_run_cmd( rt, cl, cmd, random_seed );
        return;
    }

    // SV_SetupMoveInterpolant (lag comp, non-fakeclient) — P5; no-op here.

    if ( rt.game.funcs().pfnCmdStart != nullptr )
        // pfnCmdStart's random_seed is the frozen ABI type `unsigned int`
        // (eiface.hpp) — match it exactly, not uint32_t.
        rt.game.funcs().pfnCmdStart(
            clent, reinterpret_cast<const abi::usercmd_s *>( &ucmd ), // SAFETY: usercmd_t->usercmd_s ABI pun — ucmd is the complete xash3dpp mirror; pfnCmdStart's slot names the forward-declared struct tag (eiface.hpp:226); same frozen SDK layout
            static_cast<unsigned int>( random_seed ) ); // compliance-allow(int-width): ABI unsigned int

    const double frametime = static_cast<double>( ucmd.msec ) / 1000.0;
    cl.timebase += frametime;
    cl.cmdtime  += frametime;

    check_moving_ground( rt, clent, static_cast<float>( frametime ) );

    // save oldangles, then latch the command's viewangles unless the game
    // pinned them (fixangle).
    for ( int i = 0; i < 3; ++i )
        rt.pmove->oldangles[i] = clent->v.v_angle[i];
    if ( !clent->v.fixangle )
        for ( int i = 0; i < 3; ++i )
            clent->v.v_angle[i] = ucmd.viewangles[i];

    clent->v.clbasevelocity[0] = clent->v.clbasevelocity[1] =
        clent->v.clbasevelocity[2] = 0.0f;

    // copy player buttons
    clent->v.button      = ucmd.buttons;
    clent->v.light_level = ucmd.lightlevel;
    if ( ucmd.impulse )
        clent->v.impulse = ucmd.impulse;

    rt.globals.time = cl.timebase;
    if ( rt.game.funcs().pfnPlayerPreThink != nullptr )
        rt.game.funcs().pfnPlayerPreThink( clent );
    player_run_think( rt, clent, static_cast<float>( frametime ), cl.timebase );

    // If conveyor, or think, set basevelocity, then send to client asap too.
    if ( !vec_is_null( clent->v.basevelocity ) )
        for ( int i = 0; i < 3; ++i )
            clent->v.clbasevelocity[i] = clent->v.basevelocity[i];

    // setup playermove state → motor! → copy results back
    sv_setup_pmove( rt, cl, ucmd, cl.physinfo );
    if ( rt.game.funcs().pfnPM_Move != nullptr )
        rt.game.funcs().pfnPM_Move(
            reinterpret_cast<abi::playermove_s *>( &*rt.pmove ), 1 ); // SAFETY: playermove_t->playermove_s ABI pun — rt.pmove is the complete xash3dpp mirror; pfnPM_Move's slot names the forward-declared struct tag (eiface.hpp:224); same frozen SDK layout
    sv_finish_pmove( rt, cl );

    // Touch dispatch (no custom physFuncs.PM_PlayerTouch hook → the engine
    // default: link into place and touch triggers, then impact each physent).
    if ( clent->v.solid != abi::k_solid_not && !rt.level.playersonly )
    {
        rt.links.link_edict( clent, true, rt.link_env );

        abi::vec3_t oldvel; // save velocity
        for ( int i = 0; i < 3; ++i )
            oldvel[i] = clent->v.velocity[i];

        for ( int i = 0; i < rt.pmove->numtouch; ++i )
        {
            const abi::pmtrace_t &pmtrace = rt.pmove->touchindex[i];
            abi::edict_t         *touch   = rt.arena.edict_num(
                static_cast<std::size_t>( rt.pmove->physents[pmtrace.ent].info ) );
            if ( touch == nullptr )
                continue; // corrupt physent index — legacy would deref; stay safe

            // legacy hands SV_Impact the per-touch deltavelocity as the client's
            // velocity, then restores the real velocity after the loop.
            for ( int j = 0; j < 3; ++j )
                clent->v.velocity[j] = pmtrace.deltavelocity[j];

            const SvTrace tr = convert_pmtrace( pmtrace, touch );
            sv_impact( rt, touch, clent, tr );
        }

        for ( int i = 0; i < 3; ++i ) // restore velocity
            clent->v.velocity[i] = oldvel[i];
    }

    rt.pmove->numtouch   = 0;
    rt.globals.time      = cl.timebase;
    rt.globals.frametime = static_cast<float>( frametime );

    // run post-think
    if ( rt.game.funcs().pfnPlayerPostThink != nullptr )
        rt.game.funcs().pfnPlayerPostThink( clent );
    if ( rt.game.funcs().pfnCmdEnd != nullptr )
        rt.game.funcs().pfnCmdEnd( clent );

    // SV_RestoreMoveInterpolant (lag comp, non-fakeclient) — P5; no-op here.
}

} // namespace xash::server
