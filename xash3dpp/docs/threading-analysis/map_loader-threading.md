# map_loader — Threading Analysis (Chunk 5)

> Refreshed 2026-07-06 (as-built pass). Re-scanned `src/map_loader/**`. The
> model is unchanged; the enforcement posture is now stronger than the
> 2026-07-04 authoring assumed — the FSM entry points carry real
> `assert_thread_role(Main)` calls (7 sites), and the shipped `PhsTable` is a
> second immutable-after-build Safe-RO surface. Both are folded into the
> hazard table below.

*2026-07-04. Decision refs: Q-6 (threading model: world queries
concurrent-read-safe after map load), decisions-architecture.md §Q-6;
threading-model.md `WorldData` immutable-after-activation rule.*

## Model

- **Load/activate = main thread.** `load_world_data`, `MapLoader::init/
  shutdown/load_world/clear_world/run_frame_step` and observer notification
  all run on the main thread (legacy `Mod_LoadBrushModel` likewise). As built,
  `map_loader.cpp` **enforces** this: 7 `assert_thread_role(Main)` sites guard
  `init`, `shutdown`, `load_level`, `load_game`, `run_frame_step`,
  `set_level_executor` and `load_world` (was "enforced by convention" in the
  original draft). `PhsTable::build_phs` runs on the same load path.
- **Queries = concurrent-read-safe after activation.** Every query takes
  `const WorldData&` (or a non-owning `TraceHull` view) and touches no
  mutable state: `point_leaf`, `leaf_compressed_pvs`, `pvs_for_point`,
  `box_leafnums`, `box_visible`, `fat_pvs`, `hull_point_contents`,
  `recursive_hull_check`, `trace_hull`, `finalize_trace`, `world_hull`,
  `hull_for_bsp`, plus the PHS queries `fat_phs` / `headnode_visible` over a
  `const PhsTable&`. Output buffers are caller-supplied; scratch state is
  function-local (the legacy static `g_visdata` row became a local). No query
  asserts a thread role — that is the point: they are role-agnostic reads.

## Hazard table

| State | Writers | Readers | Rule |
|---|---|---|---|
| `WorldData` contents | `load_world_data` (before return only) | all PVS/PHS/trace queries | Immutable after return — no synchronization needed for reads (Q-6). Safe-RO published snapshot (P-2). |
| `PhsTable` contents | `build_phs` (before return only) | `fat_phs` / `headnode_visible` | Immutable after build (Q-6) — second Safe-RO surface; built at load, empty when the map has no visdata. |
| `MapLoader::Impl::world` (the `optional<WorldData>`) | `load_world` / `clear_world` / `shutdown` / `run_frame_step(LoadLevel/GameShutdown)` | `world()` borrowers | Swap is NOT synchronized: callers must not hold or use a `world()` pointer across a load/clear. The write paths that flow through `load_world` / `shutdown` / `run_frame_step` now assert `ThreadRole::Main`; `clear_world` is a **gap** (not yet asserted). Frame-boundary ownership (compute/commit) is the host/server contract. |
| FSM state/observers | main-thread transitions + `run_frame_step` | `state()`/`current_map()` | Main-thread only (matches host-boundary OQ-2). `init`/`shutdown`/`load_level`/`load_game`/`run_frame_step`/`set_level_executor`/`load_world` assert Main; **`new_game` and `change_level` are gaps** — not yet guarded (parallels the host `RunFrame` asymmetry). |
| `ILevelChangeExecutor` callback | `set_level_executor` (Main, asserted) | FSM delegation on the load path (Main) | Registered once by the server; invoked only on the Main-thread load path. Absent → inline `load_world` fallback. |
| `BoxHull` | `set_bounds` | trace calls on its `hull()` | Per-instance mutable — one `BoxHull` per thread/callsite (legacy used a shared static `pm_boxhull`; the value type removes that global hazard). |

## Notes

- `decompress_pvs`/`fat_pvs`/`fat_phs` write only into caller buffers/locals —
  the legacy shared `g_visdata` static (a real cross-thread hazard in the C
  engine) has no equivalent here. Zero file-scope mutable state in the whole
  subsystem (no statics, no atomics — confirmed by scan).
- **Enforcement gaps (as-built).** ~~`new_game`, `change_level` and
  `clear_world` mutate FSM/world state but do not yet `assert_thread_role`.~~
  **CLOSED 2026-07-19 (consolidation audit, HB-3):** `new_game`,
  `change_level`, `clear_world`, `attach_observer` and `detach_observer` now
  assert `ThreadRole::Main` at entry — every mutating entry point is guarded
  (12 sites; the hazard-table "gap" notes below are superseded). The same
  pass added the `MapLoaderStats` value-snapshot counters (the former
  stats exemption's recorded revisit trigger had fired).
  Low risk today (all callers are Main), but they should join the other 7 for
  P-8 conformance when the server chunk wires the transition callers — mirror
  of the host `RunFrame`/`RequestShutdown` finding.
- No stats tier yet: the subsystem currently exposes no always-on counters;
  map loads are cold-path. Revisit when the server chunk adds per-frame
  query volume worth counting.
