#pragma once
// xash3dpp — vendored frozen SDK constants used across the game-DLL ABI
// Legacy reference: common/const.h (values are wire/ABI-frozen — game
// DLLs compare entvars fields against these numerals).
//
// Only the constants consumed by implemented slices are vendored; each
// later slice adds its set verbatim (values never change).

namespace xash::abi {

// common/const.h :88-94 — entvars_t.solid
inline constexpr int k_solid_not      = 0; // no interaction with other objects
inline constexpr int k_solid_trigger  = 1; // touch on edge, but not blocking
inline constexpr int k_solid_bbox     = 2; // touch on edge, block
inline constexpr int k_solid_slidebox = 3; // touch on edge, but not an onground
inline constexpr int k_solid_bsp      = 4; // bsp clip, touch on edge, block
inline constexpr int k_solid_custom   = 5; // call external callbacks for tracing
inline constexpr int k_solid_portal   = 6; // borrowed from FTE

// common/const.h :81 — entvars_t.movetype (S8 vendors the full set)
inline constexpr int k_movetype_follow = 12; // track movement of aiment

} // namespace xash::abi
