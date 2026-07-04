// xash3dpp — SV_UpdateMovevars: mirror the sv_* physics cvars into the
// movevars struct the pmove bridge + client delta consume (Chunk 6 S8).
// Legacy reference: engine/server/sv_main.c — SV_UpdateMovevars (:189-242).
//
// Existing subsystems used:
//   xash3dpp_cmd_cvar — the sv_* cvar registry
//   xash3dpp_core     — thread-role assertion (OQ-9)

#include <xash3dpp/private/server/physics.hpp>

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>

#include <cstring>

namespace xash::server {

namespace {

// Read a registered sv_* cvar's float value; 0 for an unregistered name
// (SV_Init registers these with defaults — a bare server yields 0, exactly
// like reading an unregistered cvar).
[[nodiscard]] float cv( ServerRuntime &rt, const char *name ) noexcept
{
    return rt.cvars != nullptr ? rt.cvars->cvar_variable_value( name ) : 0.0f;
}

} // namespace

void sv_update_movevars( ServerRuntime &rt, bool initialize ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.level.state == ServerState::Dead )
        return;

    // Legacy also early-outs on the non-initialize path when
    // host.movevars_changed is false; xash3dpp has no host changed-flag seam
    // yet, so the refresh runs every frame (cheap; identical result).

    // Clamp sv_zmax the way legacy does — some mods set insane sky-model
    // values that overflow the delta "zmax" field (sv_main.c:202-205).
    if ( rt.cvars != nullptr )
    {
        const float zmax = cv( rt, "sv_zmax" );
        if ( zmax < 256.0f )
            rt.cvars->cvar_set( "sv_zmax", "256" );
        else if ( zmax > 16777216.0f ) // 2^24
            rt.cvars->cvar_set( "sv_zmax", "16777216" );
    }

    ::xash::abi::movevars_t &mv = rt.movevars;

    mv.gravity           = cv( rt, "sv_gravity" );
    mv.stopspeed         = cv( rt, "sv_stopspeed" );
    mv.maxspeed          = cv( rt, "sv_maxspeed" );
    mv.spectatormaxspeed = cv( rt, "sv_spectatormaxspeed" );
    mv.accelerate        = cv( rt, "sv_accelerate" );
    mv.airaccelerate     = cv( rt, "sv_airaccelerate" );
    mv.wateraccelerate   = cv( rt, "sv_wateraccelerate" );
    mv.friction          = cv( rt, "sv_friction" );
    mv.edgefriction      = cv( rt, "sv_edgefriction" );
    mv.waterfriction     = cv( rt, "sv_waterfriction" );
    mv.bounce            = cv( rt, "sv_bounce" );
    mv.stepsize          = cv( rt, "sv_stepsize" );
    mv.maxvelocity       = cv( rt, "sv_maxvelocity" );
    mv.zmax              = cv( rt, "sv_zmax" );
    mv.waveHeight        = cv( rt, "sv_wateramp" );

    const char *sky = rt.cvars != nullptr
                          ? rt.cvars->cvar_variable_string( "sv_skyname" )
                          : nullptr;
    std::snprintf( mv.skyName, sizeof( mv.skyName ), "%s",
                   sky != nullptr ? sky : "" );

    mv.footsteps  = static_cast<::xash::abi::qboolean>( cv( rt, "sv_footsteps" ));
    mv.rollangle  = cv( rt, "sv_rollangle" );
    mv.rollspeed  = cv( rt, "sv_rollspeed" );
    mv.skycolor[0] = cv( rt, "sv_skycolor_r" );
    mv.skycolor[1] = cv( rt, "sv_skycolor_g" );
    mv.skycolor[2] = cv( rt, "sv_skycolor_b" );
    mv.skyvec[0]  = cv( rt, "sv_skyvec_x" );
    mv.skyvec[1]  = cv( rt, "sv_skyvec_y" );
    mv.skyvec[2]  = cv( rt, "sv_skyvec_z" );
    mv.wateralpha = cv( rt, "sv_wateralpha" );

    // svgame.movevars.features = host.features — host feature flags are not
    // wired into the server yet (Q-12 ICompatPolicy seam); 0 until then.
    mv.features   = 0;
    mv.entgravity = 1.0f;

    if ( initialize )
        return; // too early to broadcast

    // XASH3DPP-STUB(chunk6-S9): MSG_WriteDeltaMovevars broadcast to
    // sv.reliable_datagram + host.movevars_changed clear land with the
    // messaging pipeline.  Keep the oldmovevars snapshot coherent so the S9
    // delta only fires on real changes.
    if ( std::memcmp( &rt.oldmovevars, &rt.movevars, sizeof( rt.movevars )) != 0 )
        rt.oldmovevars = rt.movevars;
}

} // namespace xash::server
