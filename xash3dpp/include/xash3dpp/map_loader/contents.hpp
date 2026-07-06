#pragma once
// xash3dpp — BSP leaf/brush contents constants
// Legacy reference: common/const.h:586-603 (CONTENTS_*),
//                   engine/common/world.h:23 (CONTENTS_NONE)
//
// Values are format- and ABI-frozen: they appear in BSP leafs/clipnodes on
// disk and cross the game-DLL ABI (pmtrace_t, pfnPointContents).  `int` per
// the GoldSrc-ABI integer policy (QG).
//
// @thread-safety: stateless header — compile-time constants only; safe to read from any thread.

namespace xash::map_loader {

inline constexpr int k_contents_none        = 0;   // sentinel: no custom contents
inline constexpr int k_contents_empty       = -1;
inline constexpr int k_contents_solid       = -2;
inline constexpr int k_contents_water       = -3;
inline constexpr int k_contents_slime       = -4;
inline constexpr int k_contents_lava        = -5;
inline constexpr int k_contents_sky         = -6;
inline constexpr int k_contents_origin      = -7;  // removed at csg time
inline constexpr int k_contents_clip        = -8;  // changed to CONTENTS_SOLID
inline constexpr int k_contents_current_0   = -9;
inline constexpr int k_contents_current_90  = -10;
inline constexpr int k_contents_current_180 = -11;
inline constexpr int k_contents_current_270 = -12;
inline constexpr int k_contents_current_up  = -13;
inline constexpr int k_contents_current_down = -14;
inline constexpr int k_contents_translucent = -15;
inline constexpr int k_contents_ladder      = -16;

} // namespace xash::map_loader
