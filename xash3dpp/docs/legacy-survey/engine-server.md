# engine/server

## Purpose

The server module implements the Half-Life dedicated server runtime: entity/edict management, world physics simulation, client connection handling, game DLL bridge (`eiface.h`/`edict.h` ABI), networking (multicast & PVS filtering), save/restore, master-server reporting, and frame processing. It coordinates the game DLL, physics, and networking subsystems.

## Source Files

- **sv_main.c** — server loop, CVars, initialization
- **sv_init.c, sv_game.c** — server startup, map load, DLL setup; `sv_game.c` is the **game DLL bridge** implementing `enginefuncs_t` callbacks
- **sv_client.c, sv_cmds.c, sv_custom.c** — client connections, user commands, resource downloads
- **sv_phys.c, sv_pmove.c, sv_move.c** — physics dispatch (`MOVETYPE_*`), player prediction, monster pathfinding
- **sv_world.c** — spatial queries, entity linking, collision/trace, PVS/PAS
- **sv_frame.c** — world snapshots, delta-encoded entity updates
- **sv_save.c** — save/restore serialization (binary file format `0x71`)
- **sv_filter.c, sv_log.c, sv_query.c** — IP filtering, event logging, master-server query

## Key Data Structures and Globals

- `sv` (`server_t`) — frame-local state: time, entities, precaches, message buffers
- `svs` (`server_static_t`) — persistent state: client slots, baselines, packet entities, spawncount
- `svgame` (`svgame_static_t`) — game DLL handle, edict array, `globalvars`, function pointers (`dllFuncs`, `dllFuncs2`, `physFuncs`)
- `edict_t` — entity header: free flag, serialnumber, leaf links, `pvPrivateData` pointer, `entvars_t`
- `entvars_t` — ABI-locked entity fields (model, origin, angles, velocity, health, …)

## Public Surface to Other Subsystems

**Game DLL ABI (`enginefuncs_t` from `eiface.h`)**:

- `sv_game.c` implements ~100+ callbacks: entity queries, tracing, messaging, precaching, model/sound registration, CVar access, file I/O, entity private data allocation
- DLL-exported `pfnSpawn`, `pfnThink`, `pfnTouch`, `pfnSetAbsBox` called from `sv_world.c` (touch/think), `sv_phys.c` (physics), `sv_pmove.c` (player move)

**Outward dependencies**:

- **filesystem/** — asset precaching, save file I/O
- **public/** — memory pools, CRC, string hashing
- **common/, pm_shared/** — shared constants, edict/entvars/pmove definitions
- **platform/** — dedicated server flag, system calls

Does **not** depend on `ref/` or `engine/client/` even in dedicated mode.

## Coupling and Risks

1. **Rigid edict layout** — entity fields baked into `entvars_t`; game DLLs assume fixed offsets. Reordering breaks the ABI.
1. **Monolithic DLL interface** — `eiface.h` is ~150 function pointers; adding/removing breaks compatibility. No version negotiation.
1. **Save format brittle** — binary serialization (0x71) assumes field ordering & sizes; mismatches crash.
1. **Physics/world coupling** — `sv_world.c` directly manipulates edict state; physics callbacks (`svgame.physFuncs`) optional but deeply threaded.
1. **String pool bottleneck** — entity names/models cached in global string pool; collisions on large maps.

## Modernization Opportunities

- **ABI versioning layer** — wrap `eiface` callbacks in a version-negotiated shim; allow evolution without breaking old DLLs
- **Entity handle abstraction** — replace raw edict pointers with versioned handles; enable internal entity layout redesign
- **Modular physics** — extract physics dispatch into plugin interface; decouple movetype-specific logic
- **Save format v2** — struct-aware serialization (field map) + version tags; backward compat
- **Decouple spatial queries** — move BSP/collision out of `sv_world.c` into a separate library; query API independent of edict representation
