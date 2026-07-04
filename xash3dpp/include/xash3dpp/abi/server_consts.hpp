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

// common/const.h :76-82 — entvars_t.movetype (S8 vendors the full set)
inline constexpr int k_movetype_push     = 7;  // no clip to world, push and crush
inline constexpr int k_movetype_follow   = 12; // track movement of aiment
inline constexpr int k_movetype_pushstep = 13; // BSP model with physics/world collisions

// common/const.h — entvars_t.flags bits used by the clip filters
inline constexpr int k_fl_client       = 1 << 3;
inline constexpr int k_fl_monster      = 1 << 5;
inline constexpr int k_fl_onground     = 1 << 9;
inline constexpr int k_fl_fakeclient   = 1 << 13;
inline constexpr int k_fl_monsterclip  = 1 << 23;
inline constexpr int k_fl_worldbrush   = 1 << 25;
inline constexpr int k_fl_customentity = 1 << 29; // beam entities

// common/const.h :133 — entvars_t.takedamage (DAMAGE_AIM; float field)
inline constexpr float k_damage_aim = 2.0f;

// common/const.h :689 — rendermode (kRenderNormal)
inline constexpr int k_render_normal = 0;

// common/const.h :112/:118 — entvars_t.effects bits
inline constexpr int k_ef_invlight   = 16;      // get lighting from ceiling
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
