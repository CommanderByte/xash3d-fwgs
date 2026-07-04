#pragma once
// xash3dpp — PHS (potentially hearable set): build + queries (Chunk 6).
// Legacy reference: engine/common/mod_bmodel.c — Mod_CalcPHS (:3730),
// Mod_CompressPVS (:1088), the phs path of Mod_FatPVS (:1168-1241);
// engine/server/sv_game.c — Mod_HeadnodeVisible (:4299).
// Deep dive: docs/legacy-survey/deep-dive-trace-pvs.md.
// Decision ref: Q-19 PHS_PLACEMENT (raised as server-boundary#OQ-1) —
// BSP-derived immutable query data lives here, not in the server.
//
// The PHS is derived from the PVS at load time ("audible if anything you
// can see can see it"): row i = PVS row i OR'd with the PVS row of every
// cluster set in it.  Rows are zero-RLE compressed and indexed by an
// offset table, exactly like legacy world.compressed_phs / world.phsofs.
// Legacy builds it only for multiplayer servers (SV_Active &&
// maxclients > 1, mod_bmodel.c:4353-4354): the server calls build_phs()
// during spawn; the table is immutable afterwards (Q-6).

#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace xash::map_loader {

// ---------------------------------------------------------------------------
// PhsTable
// ---------------------------------------------------------------------------

class PhsTable
{
public:
    PhsTable() = default; // empty — "no PHS" (legacy NULL compressed_phs)

    [[nodiscard]] bool        empty() const noexcept { return offsets_.empty(); }
    [[nodiscard]] std::size_t row_count() const noexcept { return offsets_.size(); }

    // Compressed zero-RLE run for row i.  Row index == leaf index (row 0
    // is the solid leaf; the row for cluster c is c + 1).  To-end span,
    // mirroring the legacy pointer semantics; empty when i is out of
    // range (hardening — legacy indexes phsofs unchecked).
    [[nodiscard]] std::span<const std::byte> compressed_row( std::size_t i ) const noexcept;

private:
    friend PhsTable build_phs( const WorldData &w );

    std::vector<std::byte>   blob_;    // legacy world.compressed_phs
    std::vector<std::size_t> offsets_; // legacy world.phsofs
};

// ---------------------------------------------------------------------------
// Build + codec
// ---------------------------------------------------------------------------

// Mod_CompressPVS: zero-RLE — nonzero bytes copy through; a zero byte is
// followed by the length of the zero run (max 255 per pair).  Returns the
// compressed size.  Pre: out.size() >= 2 * in.size() (worst case).
[[nodiscard]] std::size_t compress_pvs( std::span<const std::byte> in,
                                        std::span<std::byte> out ) noexcept;

// Mod_CalcPHS: builds the PHS table from the world's PVS.  Returns an
// empty table when the map has no visdata (legacy early-return).  Rows
// are built rowbytes = align4(visbytes) wide with zero padding, matching
// the legacy 32-bit row alignment.  Single-threaded port of the legacy
// OpenMP build (load-time only; parallelising internally is an allowed
// follow-up per the server-boundary OQ-9 threading posture).
[[nodiscard]] PhsTable build_phs( const WorldData &w );

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

// Mod_FatPVS (PHS path): ORs the PHS row of every leaf within `radius` of
// org into `visbuffer`; same fullvis fallbacks as fat_pvs PLUS the legacy
// "requested PHS but we don't have PHS" rule — an empty `phs` yields full
// visibility (mod_bmodel.c:1230-1234).  Returns the byte count written.
[[nodiscard]] std::size_t fat_phs( const WorldData &w, const PhsTable &phs,
                                   const ::xash::utilities::Vec3 &org,
                                   float radius, std::span<std::byte> visbuffer,
                                   bool merge, bool fullvis ) noexcept;

// Mod_HeadnodeVisible: true when any non-solid leaf under the subtree at
// node index `headnode` has its cluster set in `visbits`; that cluster is
// written to *lastleaf (legacy stores the CLUSTER, which is what edict
// leafnums hold).  Child order (front first) is preserved — the first
// visible leaf in legacy traversal order wins.  Out-of-range headnode →
// false (hardening; callers pass ent->headnode >= 0).
[[nodiscard]] bool headnode_visible( const WorldData &w, int headnode,
                                     std::span<const std::byte> visbits,
                                     int *lastleaf ) noexcept;

} // namespace xash::map_loader
