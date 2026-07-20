#pragma once
// xash3dpp — pmove trace family: the PM_* callbacks the game DLL's PM_Move
// invokes to sweep the player hull against the gathered physent list.
// Legacy reference: engine/common/pm_trace.c — PM_PlayerTraceExt (:325),
// PM_TestPlayerPosition (:535), PM_TruePointContents (:665), PM_PointContents
// (:685), PM_TraceModel (:743), PM_TraceLine (:791) / PM_TraceLineEx (:814),
// PM_PointContentsPmove (:860), PM_StuckTouch (:872), PM_HullForBsp (:146).
// Deep dive: docs/legacy-survey/deep-dive-server-physics.md §4/§7.
//
// This is the physent-list analogue of the S6 world composition
// (world_trace.hpp): it reuses the SAME edict-free map_loader kernel
// (recursive_hull_check / hull_point_contents / world_hull / BoxHull /
// hull_for_bsp) and the SAME rotated-brush transforms exposed from clip.cpp,
// but sources its hulls from the gathered `physents[]` (usehull-indexed player
// bounds) rather than the areanode edict store, and merges nearest-fraction
// across the list recording the winning physent index in `pmtrace_t::ent`.
//
// Group (c) — the surface/texture trace family (PM_TraceSurface /
// PM_TraceTexture / PM_RecursiveSurfCheck) — is NOT here: it needs facet-bevel
// + miptex original-buffer data WorldData does not carry until the Chunk 7
// content pipeline, so it is stubbed at the fn-ptr table (P3b), exactly like
// the studio hitbox hulls fall back to the bbox here (Chunk 7 / OQ-2).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/pm_defs.hpp>
#include <xash3dpp/map_loader/world.hpp> // HullBoundsTable
#include <xash3dpp/utilities/math.hpp>

#include <array>
#include <span>

namespace xash::cmd_cvar { class CmdCvarContext; }

namespace xash::map_loader {
struct WorldData;
}

namespace xash::world { struct IModelResolver; } // world/trace.hpp

namespace xash::server {

// Filter callback the DLL may pass (pm_defs.h pfnIgnore): non-null overrides
// the ignore_pe index; a physent it returns non-zero for is skipped.
using PmIgnore = int ( * )( ::xash::abi::physent_t *pe );

// Server-owned snapshots aligned with playermove_t's three frozen physent
// arrays. The ABI arrays stay untouched; only each list's valid num* prefix is
// observable by the shared trace kernel.
struct PmTraceModelIndices
{
    std::array<int, ::xash::abi::k_max_physents> physents {};
    std::array<int, ::xash::abi::k_max_physents> visents {};
    std::array<int, ::xash::abi::k_max_moveents> moveents {};
};

struct PmPhysentView
{
    std::span<::xash::abi::physent_t> entities;
    std::span<const int>               model_indices;
};

// Per-call environment for the pmove trace family — the world + resolver the
// physent hulls resolve against, the aligned model-index snapshots, the player
// hull-bounds table (usehull index), and the pusher-ext toggle. Mirrors MoveEnv,
// sourced from the physent list.
struct PmTraceEnv
{
    const ::xash::map_loader::WorldData *world  = nullptr; // @lifetime: engine
    ::xash::world::IModelResolver       *models = nullptr; // @lifetime: engine
    const PmTraceModelIndices *model_indices = nullptr; // @lifetime: role owner
    const ::xash::map_loader::HullBoundsTable *player_bounds = nullptr; // @lifetime: engine
    bool pusher_ext = false; // ENGINE_PHYSICS_PUSHER_EXT (transform_bbox path)
    // OQ-2: mod_studiocache gate for the studio hull provider (PM has no
    // sv_clienttrace/FTRACE gating — PM_AllowHitBoxTrace is flag||usehull==2).
    ::xash::cmd_cvar::CmdCvarContext *cvars = nullptr; // @lifetime: engine
};

// PM_PlayerTraceExt (:325): sweep [start,end] against `ents[0..numents)` with
// the player hull for `pm.usehull`, returning the nearest impact (ent = the
// winning physent index, -1 = clear).  `flags` are k_pm_* selectors; a
// non-null `filter` overrides `ignore_pe`.
[[nodiscard]] ::xash::abi::pmtrace_t
pm_player_trace_ext( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                     const ::xash::utilities::Vec3 &start,
                     const ::xash::utilities::Vec3 &end, int flags,
                     PmPhysentView ents, int ignore_pe,
                     PmIgnore filter ) noexcept;

// PM_TestPlayerPosition (:535): point-in-solid test of `pos` against every
// physent's hull; returns the hit physent index or -1.  `ptrace` (when
// non-null) receives an origin->origin trace (legacy quirk).
[[nodiscard]] int
pm_test_player_position( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                         const ::xash::utilities::Vec3 &pos,
                         ::xash::abi::pmtrace_t *ptrace, PmIgnore filter ) noexcept;

// PM_TraceModel (:743): single-entity BSP sweep forcing usehull 2; returns the
// trace (its `.fraction` is the legacy float return).
[[nodiscard]] ::xash::abi::pmtrace_t
pm_trace_model( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                ::xash::abi::physent_t *pe, int model_index,
                const ::xash::utilities::Vec3 &start,
                const ::xash::utilities::Vec3 &end ) noexcept;

// PM_TraceLine (:791) / PM_TraceLineEx (:814): usehull-swapping traceline over
// the physent (PHYSENTSONLY) or visent (ANYVISIBLE) list.
[[nodiscard]] ::xash::abi::pmtrace_t
pm_trace_line( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
               const ::xash::utilities::Vec3 &start,
               const ::xash::utilities::Vec3 &end, int flags, int usehull,
               int ignore_pe ) noexcept;
[[nodiscard]] ::xash::abi::pmtrace_t
pm_trace_line_ex( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                  const ::xash::utilities::Vec3 &start,
                  const ::xash::utilities::Vec3 &end, int flags, int usehull,
                  PmIgnore filter ) noexcept;

// PM_TruePointContents (:665): world hull-0 contents at `p` (no water merge).
[[nodiscard]] int
pm_true_point_contents( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                        const ::xash::utilities::Vec3 &p ) noexcept;

// PM_PointContents (:685): world hull-0 merged with SOLID_NOT water bmodels
// from the physent list (highest RankForContents wins; rotation-aware).
[[nodiscard]] int
pm_point_contents( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                   const ::xash::utilities::Vec3 &p ) noexcept;

// PM_PointContentsPmove (:860): pm_point_contents folding CURRENT_* -> WATER;
// `truecontents` (when non-null) receives the unfolded value.
[[nodiscard]] int
pm_point_contents_pmove( const PmTraceEnv &env, ::xash::abi::playermove_t &pm,
                         const ::xash::utilities::Vec3 &p,
                         int *truecontents ) noexcept;

// PM_StuckTouch (:872): dedup + append a touch record (stamping the current
// pmove velocity as deltavelocity) to pm.touchindex.
void pm_stuck_touch( ::xash::abi::playermove_t &pm, int hitent,
                     ::xash::abi::pmtrace_t *tr ) noexcept;

} // namespace xash::server
