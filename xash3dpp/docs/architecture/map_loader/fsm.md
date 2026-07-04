# MapLoader FSM and World Ownership

> **Defined in**: `map_loader/map_loader.hpp`, `src/map_loader/map_loader.cpp`,
> the Filesystem overload in `src/map_loader/world.cpp`\
> **Namespace**: `xash` (`MapLoader` is a top-level engine subsystem)\
> **Legacy reference**: `engine/common/host_state.c` (host-boundary OQ-2),
> `Mod_LoadEntities` `.ent` handling (`mod_bmodel.c:2356-2382`)

## Overview

`MapLoader` is the EngineContext-owned subsystem (Q-1) combining the
map-load state machine from host-boundary OQ-2 with ownership of the active
[`WorldData`](./world-data.md). Host drives it once per frame
(`run_frame_step`); console/server/client queue transitions
(`new_game`/`load_level`/`load_game`/`change_level`); client and server
observe via `IMapLoaderObserver`.

## Fields / members (Impl)

| Name | Type | Role |
|------|------|------|
| `filesystem` | `Filesystem*` | injected via `MapLoaderInitParams` (Q-4); `@lifetime: engine` |
| `world` | `std::optional<WorldData>` | the active immutable world |
| `state/next` | `MapLoadState` | FSM current/queued |
| `level_name/landmark_name` | `char[limits::map_qpath_max]` | fixed transition buffers |
| `observers` | 4-slot pointer table | attach/detach without heap |
| `pool` | `memory::PoolHandle` | created at init (scaffold-era convention) |

## Key operations

- **`load_world(mapname, opts)`** — resolves `maps/<name>.bsp` (a name
  containing `/` is used as-is; `.bsp` appended when missing), resets the
  previous world FIRST, then loads through the Filesystem overload of
  `load_world_data`. Failures are logged upstream (Q-5) and leave
  `world() == nullptr`.
- **`world()`** — borrowed pointer, valid until the next
  load/clear/shutdown. `clear_world()` releases explicitly.
- **`run_frame_step()`** — processes one queued transition:
  - **LoadLevel** loads synchronously (Chunk 5 scope decision): observers
    get `on_load_begin`, the load runs, the FSM returns to `RunFrame` in the
    SAME step, and `on_load_end(map, success)` reports the real outcome.
  - **GameShutdown** clears the world.
  - **LoadGame / ChangeLevel** remain transition-only stubs until Chunks
    8 / 6 respectively.

## The `.ent` entity patch

World loads through the Filesystem overload probe for `maps/<name>.ent`
(gamedironly lookup, exactly like legacy): if present and **not older than
the bsp** (`file_time` comparison; older ⇒ "Entity patch is older than bsp.
Ignored."), its content replaces the ENTITIES lump wholesale via
`WorldLoadOptions::entity_patch` before parsing. The map CRC is unaffected
(it never covers entities). Non-world loads and the span overload never
probe — callers of the span overload supply `entity_patch` themselves if
they want the behaviour.

## Threading model

Main-thread only (FSM, observers, world swap). Queries against a borrowed
`world()` are safe from any thread BETWEEN swaps; not holding the pointer
across a load is the host/server frame contract — see
[threading analysis](../../threading-analysis/map_loader-threading.md).

## Error handling

`init` is `[[nodiscard]] bool`; `load_world` returns false after logging
(no filesystem, empty name, or any loader error). Observer notification
happens on both success and failure paths.

## Edge cases and invariants

- A failing load CLEARS the previous world (reset-first) — callers must not
  assume the old world survives a failed reload.
- Stats: no hot path — stats-exempt (comment at the class declaration);
  revisit when the server adds per-frame query volume.

## See also

- [bsp-loading.md](./bsp-loading.md) — what `load_world` invokes
- host-boundary OQ-2 (FSM decision), OQ-11 (clock gate wired by the
  Chunk 6 server)
