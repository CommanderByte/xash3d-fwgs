#pragma once
// xash3dpp — PVS (potentially visible set) queries over a loaded WorldData.
// Legacy reference: engine/common/mod_bmodel.c — Mod_DecompressPVS (:1059),
// Mod_PointInLeaf (:1122), Mod_GetPVSForPoint (:1145), Mod_FatPVS (:1211),
// Mod_BoxLeafnums/Mod_BoxVisible (:1250-1341).
// Deep dive: docs/legacy-survey/deep-dive-trace-pvs.md §3.
//
// All functions are pure queries over const WorldData& — concurrent-read-
// safe after load (Q-6).  Chunk 5 build-up: C6 introduces decompress_pvs
// (the loader's water-alpha probe needs it); the remaining query surface
// lands in C7.

#include <xash3dpp/map_loader/world.hpp>

#include <cstddef>
#include <span>

namespace xash::map_loader {

// Mod_DecompressPVS: classic zero-RLE — a nonzero byte copies through, a
// zero byte is followed by a zero-run length.  Empty `in` fills with 0xFF
// (legacy NULL input == "all visible").  Runs are clamped to the output;
// if `in` is exhausted before `visbytes` are produced the remainder is
// zero-filled (hardening: legacy reads past the buffer).
void decompress_pvs( std::span<const std::byte> in, std::size_t visbytes,
                     std::span<std::byte> out ) noexcept;

} // namespace xash::map_loader
