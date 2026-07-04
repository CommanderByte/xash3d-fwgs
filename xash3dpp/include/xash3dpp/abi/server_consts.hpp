#pragma once
// xash3dpp — vendored frozen SDK constants used across the game-DLL ABI
// Legacy reference: common/const.h (values are wire/ABI-frozen — game
// DLLs compare entvars fields against these numerals).
//
// Only the constants consumed by implemented slices are vendored; each
// later slice adds its set verbatim (values never change).

#include <cstdint>

namespace xash::abi {

// common/const.h :88-94 — entvars_t.solid
inline constexpr int k_solid_not      = 0; // no interaction with other objects
inline constexpr int k_solid_trigger  = 1; // touch on edge, but not blocking
inline constexpr int k_solid_bbox     = 2; // touch on edge, block
inline constexpr int k_solid_slidebox = 3; // touch on edge, but not an onground
inline constexpr int k_solid_bsp      = 4; // bsp clip, touch on edge, block
inline constexpr int k_solid_custom   = 5; // call external callbacks for tracing
inline constexpr int k_solid_portal   = 6; // borrowed from FTE

// common/const.h :69-83 — entvars_t.movetype (S8 vendors the full set)
inline constexpr int k_movetype_none         = 0;  // never moves
inline constexpr int k_movetype_walk         = 3;  // player only — Host_Error in the entity loop
inline constexpr int k_movetype_step         = 4;  // gravity, special edge handling (monsters)
inline constexpr int k_movetype_fly          = 5;  // no gravity, but still collides
inline constexpr int k_movetype_toss         = 6;  // gravity/collisions
inline constexpr int k_movetype_push         = 7;  // no clip to world, push and crush
inline constexpr int k_movetype_noclip       = 8;  // no gravity, no collisions, velocity only
inline constexpr int k_movetype_flymissile   = 9;  // extra size to monsters
inline constexpr int k_movetype_bounce       = 10; // toss + reflect velocity on impact
inline constexpr int k_movetype_bouncemissile = 11; // bounce w/o gravity
inline constexpr int k_movetype_follow       = 12; // track movement of aiment
inline constexpr int k_movetype_pushstep     = 13; // BSP model with physics/world collisions
inline constexpr int k_movetype_compound     = 14; // glue two entities together

// common/const.h :23-57 — entvars_t.flags bits used by the clip/physics code
inline constexpr int k_fl_fly          = 1 << 0;
inline constexpr int k_fl_swim         = 1 << 1;
inline constexpr int k_fl_conveyor     = 1 << 2;
inline constexpr int k_fl_client       = 1 << 3;
inline constexpr int k_fl_inwater      = 1 << 4;
inline constexpr int k_fl_monster      = 1 << 5;
inline constexpr int k_fl_godmode      = 1 << 6;
inline constexpr int k_fl_onground     = 1 << 9;
inline constexpr int k_fl_partialground = 1 << 10;
inline constexpr int k_fl_waterjump    = 1 << 11;
inline constexpr int k_fl_immune_water = 1 << 17;
inline constexpr int k_fl_immune_slime = 1 << 18;
inline constexpr int k_fl_immune_lava  = 1 << 19;
inline constexpr int k_fl_fakeclient   = 1 << 13;
inline constexpr int k_fl_ducking      = 1 << 14;
inline constexpr int k_fl_float        = 1 << 15;
inline constexpr int k_fl_alwaysthink  = 1 << 21; // think every frame regardless of nextthink
inline constexpr int k_fl_basevelocity = 1 << 22; // base velocity applied this frame
inline constexpr int k_fl_monsterclip  = 1 << 23;
inline constexpr int k_fl_worldbrush   = 1 << 25;
inline constexpr int k_fl_customentity = 1 << 29; // beam entities
inline constexpr int k_fl_killme       = 1 << 30; // const.h:56 — marked for death
inline constexpr int k_fl_dormant      = static_cast<int>( 1U << 31 ); // no updates to client

// common/mod_local.h :34 — the world model always lives at precache slot 1.
inline constexpr int k_world_index = 1;

// common/const.h :97 — entvars_t.deadflag (DEAD_NO; alive)
inline constexpr int k_dead_no = 0;

// common/const.h :610/:614 — pfnEmitSound channels
inline constexpr int k_chan_auto = 0;
inline constexpr int k_chan_body = 4;

// common/const.h :133 — entvars_t.takedamage (DAMAGE_AIM; float field)
inline constexpr float k_damage_aim = 2.0f;

// common/const.h :689 — rendermode (kRenderNormal)
inline constexpr int k_render_normal = 0;

// common/const.h :109/:112/:113/:118 — entvars_t.effects bits
inline constexpr int k_ef_muzzleflash = 2;       // single-frame muzzle flash
inline constexpr int k_ef_invlight   = 16;      // get lighting from ceiling
inline constexpr int k_ef_nointerp   = 32;      // don't interpolate the next frame
inline constexpr int k_ef_fullbright = 1 << 27; // just get fullbright

// HLSDK trace type (low byte of the pfnTraceLine/SV_Move `type` argument;
// the high byte is the ignore-transparent flag)
inline constexpr int k_move_normal     = 0; // dont_ignore_monsters
inline constexpr int k_move_nomonsters = 1; // ignore_monsters
inline constexpr int k_move_missile    = 2; // ±15 expanded monster boxes

// engine/custom.h :47-49 — resource_t.ucFlags bits (wire + game-visible)
inline constexpr std::uint32_t k_res_fatalifmissing = 1u << 0; // disconnect if unavailable
inline constexpr std::uint32_t k_res_wasmissing     = 1u << 1;

} // namespace xash::abi
