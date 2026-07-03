#pragma once
// xash3dpp — delta encoder compile-time field-identity tables
// Legacy reference: engine/common/net_encode.c (cmd/pm/ev/wd/cd/ent/meta_fields)
//
// Field NAMES and ORDER are wire- and script-frozen: delta.lst sections refer
// to fields by these exact strings, and svc_deltatable's 8-bit nameIndex is an
// index into these arrays.  The three entity tables share k_ent_fields exactly
// like the legacy `ent_fields` array.  Do not reorder, rename, or insert.
//
// Note the deliberate legacy quirks preserved here:
//   * usercmd impact_* names alias the reserved[] members.
//   * movevars omits `entgravity` and `features` from the networked set.
//   * vector fields are per-component entries ("origin[0]", ...).

#include <xash3dpp/abi/entity_state.hpp>
#include <xash3dpp/abi/event_args.hpp>
#include <xash3dpp/abi/pm_movevars.hpp>
#include <xash3dpp/abi/usercmd.hpp>
#include <xash3dpp/abi/weaponinfo.hpp>
#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>

#include <cstddef>
#include <span>

namespace xash::networking::delta {

// Local field-def helpers (mirrors legacy UCMD_DEF/ENTS_DEF/... macros);
// #undef'd at end of header.
#define XASH_DELTA_DEF( type_, x )              \
    { #x, static_cast<int>( offsetof( type_, x ) ), \
      static_cast<int>( sizeof((( type_ * )0)->x )) }
#define XASH_DELTA_DEF_( type_, name_, x )      \
    { #name_, static_cast<int>( offsetof( type_, x ) ), \
      static_cast<int>( sizeof((( type_ * )0)->x )) }

// --------------------------------------------------------------------------
// usercmd_t (legacy cmd_fields)
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_cmd_fields[] =
{
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, lerp_msec ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, msec ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, viewangles[0] ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, viewangles[1] ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, viewangles[2] ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, forwardmove ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, sidemove ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, upmove ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, lightlevel ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, buttons ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, impulse ),
    XASH_DELTA_DEF ( ::xash::abi::usercmd_t, weaponselect ),
    XASH_DELTA_DEF_( ::xash::abi::usercmd_t, impact_index,       reserved[0] ),
    XASH_DELTA_DEF_( ::xash::abi::usercmd_t, impact_position[0], reserved[1] ),
    XASH_DELTA_DEF_( ::xash::abi::usercmd_t, impact_position[1], reserved[2] ),
    XASH_DELTA_DEF_( ::xash::abi::usercmd_t, impact_position[2], reserved[3] ),
};
static_assert( std::size( k_cmd_fields ) == 16 );

// --------------------------------------------------------------------------
// movevars_t (legacy pm_fields) — entgravity/features intentionally absent
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_pm_fields[] =
{
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, gravity ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, stopspeed ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, maxspeed ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, spectatormaxspeed ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, accelerate ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, airaccelerate ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, wateraccelerate ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, friction ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, edgefriction ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, waterfriction ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, bounce ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, stepsize ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, maxvelocity ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, zmax ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, waveHeight ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, footsteps ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, skyName ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, rollangle ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, rollspeed ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skycolor_r, skycolor[0] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skycolor_g, skycolor[1] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skycolor_b, skycolor[2] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skyvec_x,   skyvec[0] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skyvec_y,   skyvec[1] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skyvec_z,   skyvec[2] ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, fog_settings ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, wateralpha ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skydir_x, skydir[0] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skydir_y, skydir[1] ),
    XASH_DELTA_DEF_( ::xash::abi::movevars_t, skydir_z, skydir[2] ),
    XASH_DELTA_DEF ( ::xash::abi::movevars_t, skyangle ),
};
static_assert( std::size( k_pm_fields ) == 31 );

// --------------------------------------------------------------------------
// event_args_t (legacy ev_fields)
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_ev_fields[] =
{
    XASH_DELTA_DEF( ::xash::abi::event_args_t, flags ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, entindex ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, origin[0] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, origin[1] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, origin[2] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, angles[0] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, angles[1] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, angles[2] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, velocity[0] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, velocity[1] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, velocity[2] ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, ducking ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, fparam1 ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, fparam2 ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, iparam1 ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, iparam2 ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, bparam1 ),
    XASH_DELTA_DEF( ::xash::abi::event_args_t, bparam2 ),
};
static_assert( std::size( k_ev_fields ) == 18 );

// --------------------------------------------------------------------------
// weapon_data_t (legacy wd_fields)
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_wd_fields[] =
{
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_iId ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_iClip ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_flNextPrimaryAttack ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_flNextSecondaryAttack ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_flTimeWeaponIdle ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_fInReload ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_fInSpecialReload ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_flNextReload ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_flPumpTime ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_fReloadTime ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_fAimedDamage ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_fNextAimBonus ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_fInZoom ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, m_iWeaponState ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, iuser1 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, iuser2 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, iuser3 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, iuser4 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, fuser1 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, fuser2 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, fuser3 ),
    XASH_DELTA_DEF( ::xash::abi::weapon_data_t, fuser4 ),
};
static_assert( std::size( k_wd_fields ) == 22 );

// --------------------------------------------------------------------------
// clientdata_t (legacy cd_fields)
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_cd_fields[] =
{
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, origin[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, origin[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, origin[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, velocity[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, velocity[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, velocity[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, viewmodel ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, punchangle[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, punchangle[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, punchangle[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, flags ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, waterlevel ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, watertype ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, view_ofs[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, view_ofs[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, view_ofs[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, health ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, bInDuck ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, weapons ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, flTimeStepSound ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, flDuckTime ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, flSwimTime ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, waterjumptime ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, maxspeed ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, fov ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, weaponanim ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, m_iId ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, ammo_shells ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, ammo_nails ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, ammo_cells ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, ammo_rockets ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, m_flNextAttack ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, tfstate ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, pushmsec ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, deadflag ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, physinfo ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, iuser1 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, iuser2 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, iuser3 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, iuser4 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, fuser1 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, fuser2 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, fuser3 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, fuser4 ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser1[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser1[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser1[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser2[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser2[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser2[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser3[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser3[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser3[2] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser4[0] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser4[1] ),
    XASH_DELTA_DEF( ::xash::abi::clientdata_t, vuser4[2] ),
};
static_assert( std::size( k_cd_fields ) == 56 );

// --------------------------------------------------------------------------
// entity_state_t (legacy ent_fields) — shared by entity_state_t,
// entity_state_player_t and custom_entity_state_t tables
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_ent_fields[] =
{
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, entityType ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, origin[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, origin[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, origin[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, angles[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, angles[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, angles[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, modelindex ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, sequence ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, frame ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, colormap ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, skin ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, solid ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, effects ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, scale ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, eflags ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, rendermode ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, renderamt ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, rendercolor.r ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, rendercolor.g ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, rendercolor.b ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, renderfx ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, movetype ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, animtime ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, framerate ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, body ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, controller[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, controller[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, controller[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, controller[3] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, blending[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, blending[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, blending[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, blending[3] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, velocity[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, velocity[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, velocity[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, mins[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, mins[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, mins[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, maxs[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, maxs[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, maxs[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, aiment ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, owner ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, friction ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, gravity ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, team ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, playerclass ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, health ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, spectator ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, weaponmodel ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, gaitsequence ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, basevelocity[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, basevelocity[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, basevelocity[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, usehull ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, oldbuttons ), // probably never transmitted
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, onground ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, iStepLeft ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, flFallVelocity ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, fov ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, weaponanim ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, startpos[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, startpos[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, startpos[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, endpos[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, endpos[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, endpos[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, impacttime ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, starttime ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, iuser1 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, iuser2 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, iuser3 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, iuser4 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, fuser1 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, fuser2 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, fuser3 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, fuser4 ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser1[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser1[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser1[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser2[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser2[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser2[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser3[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser3[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser3[2] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser4[0] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser4[1] ),
    XASH_DELTA_DEF( ::xash::abi::entity_state_t, vuser4[2] ),
};
static_assert( std::size( k_ent_fields ) == 91 );

// --------------------------------------------------------------------------
// goldsrc_delta_t meta descriptor (legacy meta_fields + dt_goldsrc_meta).
// The runtime field array is immutable and cannot be overridden by scripts.
// premultiply/postmultiply travel as floats pre-scaled by 4000.
// --------------------------------------------------------------------------

inline constexpr DeltaFieldInfo k_meta_fields[] =
{
    XASH_DELTA_DEF( goldsrc_delta_t, fieldType ),
    XASH_DELTA_DEF( goldsrc_delta_t, fieldName ),
    XASH_DELTA_DEF( goldsrc_delta_t, fieldOffset ),
    XASH_DELTA_DEF( goldsrc_delta_t, fieldSize ),
    XASH_DELTA_DEF( goldsrc_delta_t, significant_bits ),
    XASH_DELTA_DEF( goldsrc_delta_t, premultiply ),
    XASH_DELTA_DEF( goldsrc_delta_t, postmultiply ),
};
static_assert( std::size( k_meta_fields ) == 7 );

// {name, offset, size, flags, multiplier, post_multiplier, bits, inactive}
inline constexpr DeltaField k_goldsrc_meta_runtime[] =
{
    { k_meta_fields[0].name, k_meta_fields[0].offset, k_meta_fields[0].size,
      k_dt_integer,    1.0f, 1.0f, 32, false },
    { k_meta_fields[1].name, k_meta_fields[1].offset, k_meta_fields[1].size,
      k_dt_string,     1.0f, 1.0f,  1, false },
    { k_meta_fields[2].name, k_meta_fields[2].offset, k_meta_fields[2].size,
      k_dt_integer,    1.0f, 1.0f, 16, false },
    { k_meta_fields[3].name, k_meta_fields[3].offset, k_meta_fields[3].size,
      k_dt_integer,    1.0f, 1.0f,  8, false },
    { k_meta_fields[4].name, k_meta_fields[4].offset, k_meta_fields[4].size,
      k_dt_integer,    1.0f, 1.0f,  8, false },
    { k_meta_fields[5].name, k_meta_fields[5].offset, k_meta_fields[5].size,
      k_dt_float,   4000.0f, 1.0f, 32, false },
    { k_meta_fields[6].name, k_meta_fields[6].offset, k_meta_fields[6].size,
      k_dt_float,   4000.0f, 1.0f, 32, false },
};
static_assert( std::size( k_goldsrc_meta_runtime ) == std::size( k_meta_fields ));

#undef XASH_DELTA_DEF
#undef XASH_DELTA_DEF_

// --------------------------------------------------------------------------
// Table identity lookup — wire-frozen names and info spans per DeltaStructId
// --------------------------------------------------------------------------

[[nodiscard]] constexpr const char *table_name_for( DeltaStructId id ) noexcept
{
    switch( id )
    {
    case DeltaStructId::Event:             return "event_t";
    case DeltaStructId::Movevars:          return "movevars_t";
    case DeltaStructId::Usercmd:           return "usercmd_t";
    case DeltaStructId::ClientData:        return "clientdata_t";
    case DeltaStructId::WeaponData:        return "weapon_data_t";
    case DeltaStructId::EntityState:       return "entity_state_t";
    case DeltaStructId::EntityStatePlayer: return "entity_state_player_t";
    case DeltaStructId::CustomEntityState: return "custom_entity_state_t";
    default:                               return "";
    }
}

[[nodiscard]] constexpr std::span<const DeltaFieldInfo>
field_info_for( DeltaStructId id ) noexcept
{
    switch( id )
    {
    case DeltaStructId::Event:             return k_ev_fields;
    case DeltaStructId::Movevars:          return k_pm_fields;
    case DeltaStructId::Usercmd:           return k_cmd_fields;
    case DeltaStructId::ClientData:        return k_cd_fields;
    case DeltaStructId::WeaponData:        return k_wd_fields;
    case DeltaStructId::EntityState:       return k_ent_fields;
    case DeltaStructId::EntityStatePlayer: return k_ent_fields;
    case DeltaStructId::CustomEntityState: return k_ent_fields;
    default:                               return {};
    }
}

} // namespace xash::networking::delta
