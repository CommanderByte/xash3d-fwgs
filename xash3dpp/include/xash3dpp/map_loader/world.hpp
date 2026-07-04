#pragma once
// xash3dpp — immutable world model loaded from a BSP file.
// Legacy reference: engine/common/mod_bmodel.c, common/com_model.h
// Deep dives: docs/legacy-survey/deep-dive-bsp-loader.md,
//             docs/legacy-survey/deep-dive-trace-pvs.md
//
// Chunk 5 build-up: C3 introduces BspVersion (header/quirk detection);
// WorldData and load_world_data() land in C4.

#include <cstdint>

namespace xash::map_loader {

// Detected BSP flavour.  All format variance (record widths, extended
// clipnodes, Blue-Shift lump swap) is resolved at load time; queries never
// branch on this — it is informational (mapstats, diagnostics, CRC rules).
enum class BspVersion : std::uint8_t
{
    Quake1,      // version 29
    HalfLife,    // version 30
    HalfLifeExt, // version 30 + 'XASH' extra header (BSP30ext large-map support)
    Bsp2,        // 'BSP2' fourcc (32-bit records everywhere)
};

} // namespace xash::map_loader
