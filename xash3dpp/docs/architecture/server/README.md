# server — Architecture Overview

> **Source**: `xash3dpp/src/server/`\
> **Public API**: `xash3dpp/include/xash3dpp/server/server.hpp`\
> **Private headers**: `xash3dpp/include/xash3dpp/private/server/`\
> **Legacy reference**: `engine/server/` (`sv_main.c`, `sv_game.c`,
> `sv_world.c`, `sv_phys.c`, `sv_pmove.c`, `sv_frame.c`, `sv_client.c`,
> `sv_init.c`, plus the `sv_query.c` / `sv_filter.c` / `sv_log.c` satellites)

## Purpose

The server module is the **authoritative game-simulation host** (Chunk 6, the
dedicated-server milestone). It loads the game DLL (`hl.dll`), implements the
full `enginefuncs_t` engine→DLL callback surface, drives the `DLL_FUNCTIONS`
callbacks, owns the edict table, runs the fixed-step per-frame simulation
(entity physics, thinks, pushers, the player-move bridge into the DLL's
`pfnPM_Move`), manages client connections end-to-end (challenge/connect
handshake, usercmd execution, delta-compressed snapshot streams), composes
spatial queries over the loaded world (areanode entity index, hull selection,
multi-entity trace merging, point contents, PVS/PHS multicast routing), and
provides the operator surface (A2S/legacy queries, ban filters, HL-standard
logging).

It does **not** load BSPs or own the trace kernel (that is `map_loader`,
Chunk 5 — the server composes per-entity traces on top of the edict-free
kernel); it does not own sockets, netchan reliability, or the delta codec
(`networking`, Chunk 2); it does not implement player movement itself (the game
DLL statically links `pm_shared`); it does not serialize save games (Chunk 8 —
the server keeps only the `SV_ChangeLevel` orchestration and the seams
save/restore plugs into); and it renders nothing.

## Milestone status

The subsystem is **feature-complete for the dedicated milestone**: it loads the
game DLL, spawns and activates a level, runs the fixed-step frame loop
(`sv_phys` movetypes/pushers + the pmove bridge P1–P4), and drives clients over
a bidirectional netchan loop. The remaining behaviour is **milestone-trimmed
under OQ-8** — every trim carries an inline `XASH3DPP-STUB(<tag>)` / `TODO(<tag>)`
marker naming the future chunk that owns it (S9 completion, Chunk 7 content,
Chunk 8 save, Chunk 9 sound, Chunk 11 pmove parity). These are documented
deferrals, **not** missing or broken code. The live inventory is
`stub_scan server` (149 markers at the S10 gate); the grouped backlog is the
"Chunk 6 (server) — deferred stub inventory" section of
`docs/implementation-plan.md`.

## Design goals

- **Frozen-ABI fidelity first.** The `enginefuncs_t` (159 slots), `DLL_FUNCTIONS`
  (50) / `NEW_DLL_FUNCTIONS` (5), `edict_t` / `entvars_t` layouts, `playermove_t`,
  and the wire structs are byte-frozen and vendored verbatim, never redeclared.
  Layout-parity tests include the real legacy headers in a sealed namespace.
- **No globals (Q-2).** The legacy `sv` / `svs` / `svgame` file-scope triple
  becomes one heap-owned `ServerRuntime` aggregate; lifecycle steps are free
  functions over it. The **one** deliberate exception is the engine-bridge
  install (the 159 C slots cannot capture state — see below).
- **Single authoritative edict store (Q-20).** The ABI-exact `edict_t` array
  *is* the entity state (no shadow copy, no projection); engine-internal code
  reads/writes entvars through the zero-cost `EntityView` facade, and raw
  `edict->v.` access is confined to `abi/`, the pmove bridge, and the Chunk 8
  save serializer.
- **Main-thread-only (OQ-9).** Every mutating entry point opens with
  `assert_thread_role(ThreadRole::Main)`; all shared state is safe-by-contract.
- **No exceptions, no RTTI** (`/EHs-c-`, `/GR-`); failure paths return `bool` /
  `nullptr` / a safe sentinel and route hard errors through an injected
  `HostErrorHook` (Q-5).
- **Behavioural parity over cleanliness.** Quake-lineage constants, bugs, and
  quirks (ClipVelocity snap-to-zero, the `1/(sv_fps-0.01)` fudge, stale-field
  edict reuse, the zero-physics-frames early return) are reproduced exactly;
  hardening is allowed only when it is behaviour-preserving.

## Key invariants

- **One live `Server` per process.** The engine bridge (`g_bridge`) is a single
  process-global the 159 ABI slots reach through; a second concurrent server
  would share it. Matches the legacy single-`svgame` assumption.
- **The edict array base never moves.** Game DLLs hold raw pointers and do
  byte-offset arithmetic against the base (`pfnEntOffsetOfPEntity`), so the
  array-of-edicts representation itself is ABI.
- **`load_progs` before any spawn.** The game DLL is loaded once and survives
  map changes; unload runs only at engine shutdown / game switch.
- **Spawn → activate ordering is load-bearing** (see
  [lifecycle.md](./lifecycle.md)): world load → submodel precache → entity-string
  parse → `ServerActivate` → string pool to dynamic mode → settle physics →
  **baselines built after settling** → `ss_active`.
- **Pool teardown order.** `snapshot_shutdown` / `reset_external_cvars` / string
  and edict pool frees must run **before** `game_pool` destruction — the memory
  pool asserts on outstanding allocations.
- **All entry points are main-thread.** A future threaded loader or listen-server
  client that wants to touch server state must marshal onto the Main thread.

## Relationship to legacy code

The rewrite preserves the legacy behaviour and every ABI surface but reshapes
the ownership model:

- The `sv` / `svs` / `svgame` globals collapse into `ServerRuntime`, owned by
  `Server::Impl` (pimpl). Lifecycle logic stays split across free functions that
  mirror the legacy `sv_init.c` / `sv_game.c` division.
- The edict store is the single ABI array behind the `EntityView` accessor seam
  (Q-20), replacing scattered `ent->v.` access. Raw access is confined and
  compliance-scanned.
- The 64-bit string pool ships the legacy-Windows-x64 heap-arena path only
  (Q-20/OQ-6 baseline); the Linux mmap near-module probing is deliberately not
  ported.
- The map/world is owned by `map_loader` (Q-6); the server borrows `WorldData`
  and composes per-entity traces over the edict-free kernel rather than owning
  clipnodes.
- The single-threaded assumption is now *enforced* (86 `assert_thread_role`
  calls) rather than merely assumed.

## Architecture at a glance

The server is a stack of source-level layers inside one static library
(`xash3dpp_server`). The **ABI bridge** is the widest interface: the game DLL
calls into 159 `enginefuncs_t` slots (plus ~30 `playermove_t` callbacks), which
are context-free C function pointers that reach a file-scope `EngineBridge` for
state. Everything above the bridge is ordinary C++ over the `ServerRuntime`
aggregate.

```text
 ┌───────────────────────────────────────────────────────────────┐
 │  Game DLL (hl.dll) — unmodified HL mod binary                  │
 │    exports: GiveFnptrsToDll, GetEntityAPI2, DLL_FUNCTIONS,     │
 │             LINK_ENTITY_FUNC per classname, pfnPM_Move         │
 └───────▲───────────────────────────────────────────┬───────────┘
   159 enginefuncs_t + ~30 playermove_t callbacks     │ 50+5 DLL_FUNCTIONS
   (engine→DLL)                                        │ (DLL→engine, driven)
 ┌───────┴───────────────────────────────────────────▼───────────┐
 │  abi/      EngineBridge (g_bridge)  ·  build_engine_table       │
 │            GameDll loader/handshake ·  EdictArena (Q-20 store)  │
 │            StringPool  ·  EntityView accessor seam              │
 └───────┬────────────────────────────────────────────────────────┘
         │ reaches for state
 ┌───────▼──────────┬──────────────┬───────────────┬──────────────┐
 │  lifecycle/      │  world/      │  physics/     │  clients/     │
 │  load_progs      │  areanodes   │  sv_physics   │  state machine│
 │  spawn/activate  │  link/move   │  fixed-step   │  snapshots    │
 │  entity parse    │  clip/trace  │  frame loop   │  messaging    │
 │  precache        │  contents    │  pmove bridge │  net I/O      │
 │  changelevel seam│  lightstyles │  PM_* traces  │  query/filter │
 └───────┬──────────┴──────┬───────┴───────┬───────┴──────┬────────┘
         │                  │               │              │
 ┌───────▼──────────────────▼───────────────▼──────────────▼────────┐
 │  ServerRuntime  (Q-2 aggregate: sv + svs + svgame)                │
 └───────┬──────────────────────────────────────────────────────────┘
         │ depends on
 ┌───────▼───────┬────────────┬───────────┬───────────┬─────────────┐
 │ map_loader    │ networking │ cmd_cvar  │ memory /  │ platform /  │
 │ (world+trace  │ (netchan,  │ (cvars,   │ filesystem│ utilities / │
 │  +PVS/PHS)    │  delta)    │  observer)│           │ core        │
 └───────────────┴────────────┴───────────┴───────────┴─────────────┘
```

## Index of concepts

- [index.md](./index.md) — full header / source / type / CMake index
- [abi-bridge.md](./abi-bridge.md) — game-DLL ABI shim: `GameDll` handshake,
  `EngineBridge` + the 159-slot `enginefuncs_t`, `EdictArena` (Q-20 store),
  `StringPool`, the `EntityView` accessor seam
- [lifecycle.md](./lifecycle.md) — `load_progs`/`unload_progs`, spawn / activate /
  deactivate, the entity-string parse, precache tables, the model resolver, and
  the `Server` class + `ILevelChangeExecutor` seam
- [world-interaction.md](./world-interaction.md) — areanodes + `link_edict`, the
  `move` / `clip_move_to_entity` trace composition, point contents, hull
  selection, and lightstyles
- [physics-and-pmove.md](./physics-and-pmove.md) — `sv_physics` movetypes/pushers,
  the fixed-step frame loop, movevars, and the pmove bridge (`SetupPMove` /
  `FinishPMove` / the `PM_*` trace family / `SV_RunCmd`)
- [clients-and-messaging.md](./clients-and-messaging.md) — the connection state
  machine, snapshot / delta pipeline, multicast + user messages, netchan demux,
  and the query / filter / log satellites
- [threading-and-invariants.md](./threading-and-invariants.md) — the OQ-9
  main-thread-only posture, the Q-20 raw-access confinement, and the
  cross-cutting invariants
