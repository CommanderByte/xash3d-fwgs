# map_loader — Threading Analysis (Chunk 5)

*2026-07-04. Decision refs: Q-6 (threading model: world queries
concurrent-read-safe after map load), decisions-architecture.md §Q-6;
threading-model.md `WorldData` immutable-after-activation rule.*

## Model

- **Load/activate = main thread.** `load_world_data`, `MapLoader::init/
  shutdown/load_world/clear_world/run_frame_step` and observer notification
  all run on the main thread (legacy `Mod_LoadBrushModel` likewise).
- **Queries = concurrent-read-safe after activation.** Every query takes
  `const WorldData&` (or a non-owning `TraceHull` view) and touches no
  mutable state: `point_leaf`, `leaf_compressed_pvs`, `pvs_for_point`,
  `box_leafnums`, `box_visible`, `fat_pvs`, `hull_point_contents`,
  `recursive_hull_check`, `trace_hull`, `finalize_trace`, `world_hull`,
  `hull_for_bsp`. Output buffers are caller-supplied; scratch state is
  function-local (the legacy static `g_visdata` row became a local).

## Hazard table

| State | Writers | Readers | Rule |
|---|---|---|---|
| `WorldData` contents | `load_world_data` (before return only) | all queries | Immutable after return — no synchronization needed for reads (Q-6). |
| `MapLoader::Impl::world` (the `optional<WorldData>`) | `load_world` / `clear_world` / `shutdown` / `run_frame_step(LoadLevel/GameShutdown)` | `world()` borrowers | Swap is NOT synchronized: callers must not hold or use a `world()` pointer across a load/clear. Frame-boundary ownership (compute/commit) is the host/server contract; enforced by convention until the server chunk lands `assert_thread_role` at these entry points. |
| FSM state/observers | main-thread transitions + `run_frame_step` | `state()`/`current_map()` | Main-thread only (matches host-boundary OQ-2). |
| `BoxHull` | `set_bounds` | trace calls on its `hull()` | Per-instance mutable — one `BoxHull` per thread/callsite (legacy used a shared static `pm_boxhull`; the value type removes that global hazard). |

## Notes

- `decompress_pvs`/`fat_pvs` write only into caller buffers/locals — the
  legacy shared `g_visdata` static (a real cross-thread hazard in the C
  engine) has no equivalent here.
- No stats tier yet: the subsystem currently exposes no always-on counters;
  map loads are cold-path. Revisit when the server chunk adds per-frame
  query volume worth counting.
