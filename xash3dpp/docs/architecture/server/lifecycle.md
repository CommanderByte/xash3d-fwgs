# Lifecycle

> **Defined in**: `private/server/lifecycle.hpp`, `precache.hpp`,
> `model_resolver.hpp`, `server/server.hpp` /
> `src/server/lifecycle/*.cpp`, `server.cpp`\
> **Namespace**: `xash::server`

## Overview

Lifecycle owns the `ServerRuntime` aggregate and the ordered state machine that
takes the server from "game DLL not loaded" to "level active and simulating".
It mirrors the legacy `sv_init.c` / `sv_game.c` split as free functions over
`ServerRuntime` (Q-2: no globals). The `Server` class is the public facade — a
pimpl holding the runtime, and the MapLoader FSM's `ILevelChangeExecutor`.

The five phases are: **load** (`load_progs`, once, survives map changes) →
**setup clients** (`setup_clients`, latches maxclients) → **spawn**
(`spawn_server` + `spawn_entities`, per level, wipes `sv`) → **activate**
(`activate_server`, settle frames then baselines) → **deactivate**
(`deactivate_server`) / **unload** (`unload_progs`, at shutdown).

______________________________________________________________________

## `ServerRuntime` — the aggregate

**Header**: `lifecycle.hpp`

The single struct replacing the legacy `sv` / `svs` / `svgame` triple, owned by
`Server::Impl`. It holds the injected deps (`cvars`/`fs`/`maps`/`net`), the game
binding (`game_pool`, `GameDll game`, `EdictArena arena`, `StringPool strings`,
`globalvars_t globals`, `enginefuncs_t engine_table`, `DeltaTables delta`), the
movevars, the pool-owned `playermove_t pmove`, the lightstyle table, the
`pushed[256]` pusher stack, the `EngineBridge bridge`, the `PrecacheTables`, the
world-interaction instances (`ModelResolver`/`WorldLinks`/`GameWorldHooks`/
`MoveEnv`/`LinkEnv`, rebound each spawn), the split `LevelState level` /
`PersistentState persistent`, and the S9 `ClientMachinery clients` /
`SnapshotState snapshot` / signon buffer.

`LevelState` is the `server_t` subset (wiped every spawn): `state`, `time`
(spawn epoch **1.0**), `frametime`, `framecount`, `hostflags`, map name, CRCs,
pause/simulate flags. `PersistentState` is the `server_static_t` subset
(persists across maps): `initialized`, `maxclients`, `spawncount`, the 16-entry
challenge salt.

`ServerState` (`Dead=0`/`Loading=1`/`Active=2`) values are behavioural contract —
they are mirrored verbatim into the read-only `host_serverstate` cvar.

______________________________________________________________________

## `load_progs` / `unload_progs`

**Source**: `lifecycle/game_host.cpp`

`load_progs(rt, dll_path)` runs the full legacy `SV_LoadProgs` order: create the
game pool → install the engine bridge → `build_engine_table` → `GameDll::load`
handshake → register operator commands → init the string pool → set up globals →
init the edict arena → `host_gameloaded` → `pfnGameInit` → query hull bounds →
`Delta_Init` → `pfnRegisterEncoders`. It is **idempotent** (early-returns `true`
when already loaded). It also allocates `rt.pmove` and calls
`sv_init_client_move` (the `PM_*` callback table), and `snapshot_alloc_baselines`.

`unload_progs(rt)` runs the reverse unwind: `deactivate_server` → delta shutdown
→ cvar prepare-to-unlink → `pfnGameShutdown` (gated on `game_initialized` so the
Q-5 OOM-unwind path never delivers an unpaired Shutdown) → `host_gameloaded 0` →
kill operator commands → unlink cvars/commands → `reset_external_cvars` →
`snapshot_shutdown` → free the string pool → free the library → destroy the pool.
Teardown order matters: everything pool-allocated must be freed **before**
`game_pool` destruction (the pool asserts on leaks).

`deactivate_server(rt)` keeps the quirk that the disconnect-cfg execs are queued
**before** the initialized/dead guard.

______________________________________________________________________

## Spawn and activate

**Source**: `lifecycle/spawn.cpp`

`setup_clients(rt)` (`SV_SetupClients`) latches `svs.maxclients` from the
`sv_maxclients` cvar (acting only on a real change), clamps it (dedicated
`bound(4, ·, MAX_CLIENTS)` / listen `bound(1, ·, ·)`), reconciles
deathmatch/coop, and updates the arena reserved floor (`maxclients + 1`) and the
snapshot ring (`snapshot_alloc_ring`).

`spawn_server(rt, mapname, startspot, background)` (`SV_SpawnServer`):
`setup_clients` → ensure progs loaded → reset per-level state → `ss_loading` →
world load through MapLoader + submodel precache (`*N`) → client-slot
`init_edict` → install the world-interaction bridge (`SV_ClearWorld`: rebuild
areanodes, reset lightstyles, bind `MoveEnv`/`LinkEnv`/`ModelResolver`/hooks). It
does **not** run the entity lump — the caller drives `spawn_entities` next.
Returns `false` on load failure (routed through the host-error hook).

`activate_server(rt, run_physics)` (`SV_ActivateServer`): free old entities →
`pfnServerActivate` → string pool → dynamic mode (**after** activate) → the
settle frames (SP 2 / MP 8 at `SV_SPAWN_TIME`; the restore path is a single
0.001 s frame) → **baselines built after settling** → `ss_active`.
`run_physics == false` is the save-restore path.

______________________________________________________________________

## Entity-string parse

**Source**: `lifecycle/entity_parse.cpp`

`spawn_entities(rt, world)` (`SV_SpawnEntities`): reset sky/water cvars, stamp
the world edict (model/modelindex/solid/movetype) + globals
(maxEntities/mapname/startspot/time), then `load_from_file` over the world's
entity lump.

`load_from_file(rt, world, entities)` (`SV_LoadFromFile`): the `{`-delimited
loop — world edict is slot 0 (already initialised), the rest are `alloc_edict`;
`pfnSpawn == -1` without `FL_KILLME` frees + counts the entity as inhibited;
world origin/angles are cleared afterwards.

`parse_edict(rt, world, cursor, ent)` (`SV_ParseEdict`): pull one `{ … }`
dictionary, applying the classname-first / `angle`→`angles` / custom-entity /
trailing-space quirks. Returns `false` when inhibited (no classname, or
`alloc_private_data` rejected the edict).

______________________________________________________________________

## Precache tables

**Header**: `precache.hpp` · **Source**: `lifecycle/precache.cpp`

`PrecacheTables` holds four 1-based name registries (model/sound/event/generic)
living in `sv` — `clear()` runs at every spawn. Registration quirks preserved
exactly:

- Dedup is case-insensitive; the scan stops at the first empty slot (holes never
  form because slots fill in order).
- model/sound strip **one** leading `/` or `\`; event/generic do not; all four
  run `COM_FixSlashes`.
- sound rejects sentence names (`!` prefix) with a warning.
- Table overflow is a hard `Host_Error` (routed through the error hook).
- An index registered while **not** in `ss_loading` is a "late precache": all
  four notify the sink (S9 wires the `svc_resource` broadcast), but only
  model/sound also log the console warning.

`find_model` (backing `pfnModelIndex`) looks up **without** registration, logs
"not precached" and returns 0 on a miss. `model_flags` tracks the `RES_*` bits.

______________________________________________________________________

## Model resolver

**Header**: `model_resolver.hpp` · **Source**: `lifecycle/model_resolver.cpp`

`ModelResolver` is the production `IModelResolver` the trace hull selector
consumes. It derives brush identity **lazily** from the precache name + the
loaded `WorldData` (no parallel `sv.models[]` cache): the world is precache slot
1 (submodel 0) and the `*N` inline submodels. Non-brush names (studio/sprite)
resolve to `nullopt` (`is_studio` → true, the bbox-fallback path) until the
Chunk 7 content pipeline lands real model loading. Bound each `spawn_server` to
the fresh world + precache tables, cleared on deactivate.

______________________________________________________________________

## The `Server` facade

**Header**: `server/server.hpp` · **Source**: `server.cpp`

`Server` is a move-only pimpl (`Impl` holds `ServerRuntime rt` + `ServerStats`).
`init(params)` wires the injected deps into `rt.cfg` / `rt` and returns — the
game DLL loads lazily at the first spawn (legacy `SV_InitGame`). `shutdown()`
calls `unload_progs` (idempotent). `active()` / `initialized()` read
`level.state == Active` / `persistent.initialized`.

As `ILevelChangeExecutor` (driven by the MapLoader FSM):

- `exec_load_level(map, background)` — the full chain: `spawn_server` →
  `spawn_entities` → `activate_server(run_physics=true)`.
- `exec_load_game(map)` / `exec_change_level(map, landmark, background)` —
  **Chunk 8 stubs** behind the seam (return `false`). The `SV_ChangeLevel`
  landmark orchestration is server-core scope, but the save serialization it
  drives belongs to `xash3dpp_save`.

`frame(host_frametime)` delegates to `host_server_frame` (see
[physics-and-pmove.md](./physics-and-pmove.md)).

______________________________________________________________________

## Threading model

Main-thread only (OQ-9). Every public entry — `init`, `shutdown`,
`exec_load_level`, `frame`, and the lifecycle free functions
(`load_progs`/`unload_progs`/`spawn_server`/`activate_server`/`deactivate_server`)
— opens with `assert_thread_role(ThreadRole::Main)`. `ServerRuntime` is
heap-owned by the one `Server` per process and reached only through these
asserted entry points; it is mutable for the whole active lifetime but only ever
from one thread. See
[docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md).

## Error handling

`spawn_server` returns `false` on world-load failure and routes fatal conditions
through the injected `HostErrorHook` (Q-5); pool/allocation failures are logged
and surfaced as `false`. `unload_progs` is idempotent — a never-loaded server
early-returns. No exceptions.

## Edge cases and invariants

- `initialized` is set **early** in `spawn_server` (right after game-DLL/client
  init succeeds, before world load), and cleared only by `shutdown` — the
  boundary Interface-table timing quirk.
- Spawn epoch is `sv.time = 1.0`, not 0.
- The MP world-blend flag is `maxclients > 1`; settle frames are SP 2 / MP 8.
- Baselines are built **after** the settle frames, not before.
- The game DLL is loaded once and survives map changes; operator commands exist
  only while it is loaded.

## See also

- [abi-bridge.md](./abi-bridge.md) — the stores `load_progs` wires and the
  bridge it installs
- [world-interaction.md](./world-interaction.md) — what `SV_ClearWorld` /
  `install_world_bridge` sets up
- [clients-and-messaging.md](./clients-and-messaging.md) — the S9 machinery the
  runtime aggregates
- `docs/legacy-survey/deep-dive-server-lifecycle.md`
