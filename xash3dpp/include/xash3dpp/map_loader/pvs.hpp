#pragma once
// xash3dpp — PVS (potentially visible set) queries over a loaded WorldData.
// Legacy reference: engine/common/mod_bmodel.c — Mod_DecompressPVS (:1059),
// Mod_PointInLeaf (:1122), Mod_GetPVSForPoint (:1145), Mod_FatPVS (:1211),
// Mod_BoxLeafnums/Mod_BoxVisible (:1250-1341); engine/common/mod_local.h
// (radii, CHECKVISBIT).
// Deep dive: docs/legacy-survey/deep-dive-trace-pvs.md §3.
//
// All functions are pure queries over const WorldData& — concurrent-read-
// safe after load (Q-6).  Server-side PHS machinery (Mod_CalcPHS, the phs
// path of Mod_FatPVS) belongs to Chunk 6 and is not implemented here.
//
// NOTE the tie-break asymmetry, preserved from legacy: the point-in-leaf
// walk sends an exactly-on-plane point to the BACK child (PlaneDiff <= 0),
// while the clip-hull walkers (trace.hpp) send it to the FRONT child
// (PlaneDiff < 0).

#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <cstddef>
#include <span>

namespace xash::map_loader {

// Buffer cap routed through limits.hpp (override: XASH_LIMIT_MAP_BOX_LEAFS_MAX).
inline constexpr std::size_t k_max_box_leafs = ::xash::limits::map_box_leafs_max;
inline constexpr float       k_fatpvs_radius = 8.0f; // legacy FATPVS_RADIUS
inline constexpr float       k_fatphs_radius = 8.0f; // legacy FATPHS_RADIUS

// Legacy CHECKVISBIT: false for negative cluster numbers.
[[nodiscard]] inline bool check_vis_bit( std::span<const std::byte> vis, int cluster ) noexcept
{
    if ( cluster < 0 || static_cast<std::size_t>( cluster ) >= vis.size() * 8 )
        return false;
    return ( static_cast<unsigned char>( vis[static_cast<std::size_t>( cluster ) >> 3] ) &
             ( 1u << ( static_cast<unsigned>( cluster ) & 7u ))) != 0;
}

// Mod_DecompressPVS: classic zero-RLE — a nonzero byte copies through, a
// zero byte is followed by a zero-run length.  Empty `in` fills with 0xFF
// (legacy NULL input == "all visible").  Runs are clamped to the output;
// if `in` is exhausted before `visbytes` are produced the remainder is
// zero-filled (hardening: legacy reads past the buffer).
void decompress_pvs( std::span<const std::byte> in, std::size_t visbytes,
                     std::span<std::byte> out ) noexcept;

// Mod_PointInLeaf: walks the draw-node tree from node 0 and returns the
// LEAF INDEX containing p.  On-plane points go to the back child (<= 0).
// Pre: w.nodes() is non-empty (guaranteed by load_world_data).
[[nodiscard]] int point_leaf( const WorldData &w,
                              const ::xash::utilities::Vec3 &p ) noexcept;

// Raw compressed PVS run for a leaf: empty when visofs == -1, out of range
// (hardening; legacy computes the pointer unchecked), or the leaf index is
// invalid — decompress_pvs treats empty as full visibility, the legacy
// NULL-pointer convention.  Otherwise visdata from visofs to the end
// (legacy pointer semantics — decompression reads the same byte stream).
[[nodiscard]] std::span<const std::byte>
leaf_compressed_pvs( const WorldData &w, int leaf ) noexcept;

// Mod_GetPVSForPoint: decompresses the point-leaf's PVS into `out` and
// returns true; returns false (out untouched) when the leaf has no cluster
// — legacy returns NULL there and callers treat it as full visibility.
// Pre: out.size() >= w.visbytes().
[[nodiscard]] bool pvs_for_point( const WorldData &w,
                                  const ::xash::utilities::Vec3 &p,
                                  std::span<std::byte> out ) noexcept;

// Mod_BoxLeafnums: collects the CLUSTER numbers of non-solid leafs the box
// touches (legacy stores leaf->cluster, not leaf indices).  Recursion stops
// when `list` is full (legacy overflowed flag).  `topnode`, when non-null,
// receives the first straddling node (-1 if none).
[[nodiscard]] std::size_t box_leafnums( const WorldData &w,
                                        const ::xash::utilities::Vec3 &mins,
                                        const ::xash::utilities::Vec3 &maxs,
                                        std::span<int> list,
                                        int *topnode ) noexcept;

// Mod_BoxVisible: true when any cluster the box touches is set in visbits.
// Empty visbits → true (legacy NULL check).
[[nodiscard]] bool box_visible( const WorldData &w,
                                const ::xash::utilities::Vec3 &mins,
                                const ::xash::utilities::Vec3 &maxs,
                                std::span<const std::byte> visbits ) noexcept;

// Mod_FatPVS (PVS path only): ORs the PVS of every leaf whose bbox is
// within `radius` of org into `visbuffer` and returns the byte count
// (min(w.visbytes(), visbuffer.size()), matching the legacy Q_min).  Full
// visibility (0xFF) when `fullvis` is set, the map has no visdata, or the
// point sits in a clusterless leaf.  `merge` accumulates into the existing
// buffer contents instead of clearing first.
[[nodiscard]] std::size_t fat_pvs( const WorldData &w,
                                   const ::xash::utilities::Vec3 &org,
                                   float radius, std::span<std::byte> visbuffer,
                                   bool merge, bool fullvis ) noexcept;

} // namespace xash::map_loader
