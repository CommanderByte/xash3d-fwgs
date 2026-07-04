#pragma once
// xash3dpp — server snapshot / baseline pipeline (Chunk 6, S9 completion)
// Legacy reference: engine/server/sv_init.c:459 SV_CreateBaseline,
// engine/server/sv_frame.c (SV_WriteEntitiesToClient / SV_EmitPacketEntities —
// later sub-slices), engine/server/sv_game.c pfnCreateInstancedBaseline (:4425).
// Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md §5.
//
// The byte-exact entity_state_t delta codec already lives in networking
// (DeltaTables::write_delta_entity); this module only orchestrates baseline
// data + (later) the per-client visible-entity gather and frames ring.  The
// game DLL fills each baseline via pfnCreateBaseline — there is no engine-side
// SV_FillEntityState (deep dive §5).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/eiface.hpp>          // entity_state_t, string_t, edict_t
#include <xash3dpp/abi/entity_state.hpp>

#include <cstddef>

namespace xash::server {

struct ServerRuntime; // fwd

// sv.instanced[] cap (server.h MAX_CUSTOM_BASELINES); the signed 7-bit wire
// baseline offset (-i-1) references these, so the count is bounded to 63 usable
// (index 0..62) with the 6-bit num_instanced field (sv_init.c:538).
inline constexpr int k_max_custom_baselines = 64;

// One instanced baseline: a classname-keyed template state shared by every
// entity of that class (pfnCreateInstancedBaseline, sv_game.c:4425).
struct InstancedBaseline
{
    ::xash::abi::string_t       classname = 0;  // into the server string pool
    ::xash::abi::entity_state_t baseline  = {};
};

// svs.baselines + sv.instanced (+ the packet_entities ring / frames land in the
// next sub-slices).  Owned by ServerRuntime.  `baselines` is pool-allocated
// from game_pool at load_progs (sized max_edicts), zeroed each spawn, and freed
// wholesale when game_pool dies at unload.
struct SnapshotState
{
    ::xash::abi::entity_state_t *baselines      = nullptr; // [baseline_count]
    int                          baseline_count = 0;       // == GI->max_edicts

    InstancedBaseline instanced[k_max_custom_baselines] = {}; // sv.instanced
    int               num_instanced       = 0; // sv.num_instanced
    int               last_valid_baseline = 0; // sv.last_valid_baseline
};

// SV_LoadProgs baseline alloc (sv_game.c:5342, Z_Calloc entity_state_t *
// max_edicts).  Idempotent — no-op when already sized.  Returns false (logged)
// on allocation failure.
[[nodiscard]] bool snapshot_alloc_baselines( ServerRuntime &rt ) noexcept;

// SV_SpawnServer reset (sv_init.c:995 memset baselines; instanced counters
// cleared by the per-level sv memset).
void snapshot_reset( ServerRuntime &rt ) noexcept;

// SV_CreateBaseline fill half (sv_init.c:459-508): per valid edict set
// number/entityType, call the game DLL's pfnCreateBaseline to fill the state,
// track last_valid_baseline; then pfnCreateInstancedBaselines.  The signon
// serialization half (sv_init.c:510-544) lands with the signon buffer in the
// send sub-slice; the SP/MP voice-codec write is an OQ-8 stub.
void create_baselines( ServerRuntime &rt ) noexcept;

// pfnCreateInstancedBaseline callback (engine_table, eiface.hpp:369): append a
// classname-keyed template to sv.instanced[]; returns the new index, or the cap
// when full (legacy silently ignores past MAX_CUSTOM_BASELINES).
int create_instanced_baseline( ServerRuntime &rt, ::xash::abi::string_t classname,
                               const ::xash::abi::entity_state_t *baseline ) noexcept;

} // namespace xash::server
