# ABI Bridge

> **Defined in**: `private/server/engine_bridge.hpp`, `edict_arena.hpp`,
> `string_pool.hpp`, `game_dll.hpp`, `entity_view.hpp` /
> `src/server/abi/*.cpp`\
> **Namespace**: `xash::server`

## Overview

The ABI bridge is the widest and highest-risk surface in the rewrite: it is
where **unmodified Half-Life mod binaries** meet the C++ engine. Two frozen
tables cross here — the engine hands the DLL 159 `enginefuncs_t` slots
(engine→DLL) and ~30 `playermove_t` callbacks, and the DLL hands the engine 50
`DLL_FUNCTIONS` + 5 `NEW_DLL_FUNCTIONS` slots (DLL→engine) plus a
`LINK_ENTITY_FUNC` per entity classname. Every struct layout is byte-frozen,
vendored verbatim from `engine/eiface.h` / `edict.h` / `progdefs.h` /
`pm_defs.h`, and **never redeclared** — layout-parity tests include the real
legacy headers in a sealed namespace to prove it.

The 159 engine slots are plain C function pointers: they cannot capture state,
so — exactly like legacy's `svgame` reach-through — every slot implementation
reaches a single file-scope `EngineBridge` for the state it needs. This is the
**one** deliberate exception to the Q-2 no-globals rule. Underneath the bridge
sit the four ABI stores: `GameDll` (the library binding), `EdictArena` (the
Q-20 authoritative entity array), `StringPool` (the `string_t` arena), and the
`EntityView` accessor seam.

______________________________________________________________________

## `GameDll` — the load handshake

**Header**: `game_dll.hpp` · **Source**: `abi/game_dll.cpp`

Loads the game DLL and performs the export negotiation in the **ABI-visible
order** (shipped DLLs latch state per call):

1. Resolve `GetEntityAPI` / `GetEntityAPI2` / `GetNewDLLFunctions` — missing
   *both* EntityAPI exports is fatal (`MissingEntityApi`).
1. Resolve `GiveFnptrsToDll` — missing is fatal (`MissingGiveFnptrs`).
1. Call `GiveFnptrsToDll(engfuncs, globals)` **first**, before any `Get*API`.
1. `GetNewDLLFunctions` (optional; a version reject zeroes the table).
1. `GetEntityAPI2` with version 140 by pointer — accepted only when the echoed
   version still equals 140 (`extended_api() == true`).
1. Else fall back to `GetEntityAPI` (version by value, possibly the value the
   failed API2 negotiation wrote back — a preserved legacy quirk).

### Key operations — GameDll

- `load(path, table, globals)` → `bool`. **Pre**: the caller owns `table` and
  `globals` for the DLL's whole lifetime (the DLL keeps the raw pointers). On
  failure the library is freed and `last_error()` explains why.
- `unload()`. **Pre**: the lifecycle orchestrator has already run
  `pfnGameShutdown` and unwound game state.
- `entity_link(classname)` → `LINK_ENTITY_FUNC`. Per-classname spawn export
  resolved by raw name (`COM_GetProcAddress(hInstance, classname)`).
- `symbol(name)` → `void*`. Raw export lookup (backs `pfnFunctionFromName`).
- `query_hull_bounds(funcs)` → `HullBoundsTable`. Calls `pfnGetHullBounds` for
  hulls 0..3; the table starts **zeroed** and a slot is written only when the
  game returns nonzero (legacy parity: a game without hull *i* leaves that entry
  zero). Fed to `map_loader` `WorldLoadOptions::hull_bounds`.

The DLL is loaded **once** and survives map changes; `load()` on a loaded
instance early-returns `true`.

______________________________________________________________________

## `EngineBridge` — the state behind the slots

**Header**: `engine_bridge.hpp` · **Source**: `abi/engine_table.cpp`

`EngineBridge` is a plain aggregate of borrowed pointers and mirrored host
scalars. The slots reach it through the free functions `install_engine_bridge`
(stores the pointer) and `engine_bridge()` (reads it). Fields are wired in by
the slice that owns them and stay **null until then** — every slot degrades to
its documented legacy-safe default when its pointer is absent (precache slots
return 0, the pmove callbacks return a clear trace, `RegUserMsg` returns
`svc_bad`, etc.).

### Notable fields

| Field | Wired by | Role |
|-------|----------|------|
| `arena` / `strings` / `globals` / `game` | S4/S6 | The four ABI stores + the `globalvars_t` handed to the DLL |
| `move_env` / `links` / `link_env` / `lightstyles` / `phs` | S5 (per spawn) | World-interaction state (null until a map is loaded) |
| `precache` | S7 | The four index registries |
| `runtime` | `load_progs` | Back-pointer to the owning `ServerRuntime`; only the rare full-orchestration slots reach it (`pfnRunPlayerMove` → `sv_run_cmd`) |
| `pmove` / `player_bounds` | `sv_init_client_move` | The single `playermove_t` working set + the player-hull table the `PM_*` callbacks index |
| `clients` / `snapshot` / `delta` | S9 | Client machinery, baseline/instanced state, delta tables |
| `misc_pool` | `load_progs` | The svgame-mempool equivalent for ABI-forced allocations |
| `sv_time` / `max_clients` / `server_state` / `dedicated` / … | per frame | Host scalars mirrored for the slots (edict reuse, `SV_SetModel` ss_active guard, autoaim, group mask) |
| `external_cvars` / `cvar_string_allocs` | game DLL | The game's own `cvar_t` chain + engine-owned replacement strings |
| `host_error` / `host_error_ctx` | host | The Q-5 error surface slots hard-error through |

### Key operations — EngineBridge

- `build_engine_table(peoei_broken)` → `enginefuncs_t`. Builds a **fresh copy**
  of the table (legacy `gpEngfuncs` local-copy semantics: the caller's copy goes
  to the DLL so a rogue `bots.dll` cannot corrupt the master table). Every slot
  is populated; milestone-deferred slots are `XASH3DPP-STUB(chunk6)`-marked
  no-ops. `peoei_broken` applies the `BUGCOMP_PENTITYOFENTINDEX` patch (the
  broken GoldSrc player-range `pfnPEntityOfEntIndex`).
- `alloc_private_data(ent, className, customentity)` → `edict_t*`. Re-init/alloc
  the edict, stamp its classname, resolve the `LINK_ENTITY` spawn export by raw
  name and run it; `customentity` reports the "custom" fallback path. Shared with
  the lifecycle entity-parse path so LINK dispatch has one implementation.
- `reset_external_cvars(bridge)`. The `SV_UnloadProgs` counterpart: unlink the
  game's `cvar_t` structs and free every engine-owned replacement string. Must
  run **before** `misc_pool` is destroyed (the pool asserts on leaks).

______________________________________________________________________

## `EdictArena` — the Q-20 authoritative store

**Header**: `edict_arena.hpp` · **Source**: `abi/edict_arena.cpp`

The ABI-exact `edict_t` array **is** the entity state — no shadow copies, no
projection (Q-20). The base pointer is allocated once from the game pool and
**never moves** (game DLLs hold raw pointers and do byte-offset arithmetic
against it). `reserved` = worldspawn + client slots (legacy `maxclients + 1`):
`alloc_edict` never scans below it and `num_entities` starts there.

Load-bearing legacy semantics preserved exactly:

- `free_edict` scrubs only a **specific entvars subset** and leaves the rest
  stale while the edict is free (games read freed edicts through the peoei
  bugcomp path), stamps `freetime`, and **increments `serialnumber`** (EHANDLE
  invalidation).
- `alloc_edict` reuse policy: a freed slot is reusable when its `freetime` is
  inside the first-seconds relax window **or** older than the grace period.
  Returns `nullptr` on exhaustion (legacy `Host_Error`'s — the bridge maps this
  to the host error policy, Q-5).
- `init_edict` releases private data, zeroes all entvars, self-links
  `pContainingEntity`, sets `controller[0..3] = 0x7F`, `free = false`.
- `private_data_size(cb)` rounds every request up to the next 16-byte multiple
  (`(cb + 15) & ~15`) — a deliberate over-allocation because shipped binaries
  (Poke646 et al.) write past the end of their requested block.

Index / byte-offset contract: `edict_num` / `index_of` / `offset_of` /
`ent_of_offset` back `pfnPEntityOfEntIndex` / `pfnEntOffsetOfPEntity` etc.

**Known deviation** (parity-reviewed): `free_private` frees `pvPrivateData`
unconditionally; legacy gates on `Mem_IsAllocatedExt` and silently skips foreign
pointers. The xash3dpp memory API has no ownership probe, so `pvPrivateData`
must come from `alloc_private` (precondition). Revisit if a real mod assigns its
own block.

______________________________________________________________________

## `StringPool` — the `string_t` arena

**Header**: `string_pool.hpp` · **Source**: `abi/string_pool.cpp`

`string_t` is an `int` offset from `globals->pStringBase`. The pool is one block
of `2 × arena_size` bytes: the first half is the **dynamic** arena (per-level
strings, reset on level change), the second half the **static** arena (strings
allocated before spawn finishes — survive level changes). Offset 0 is the empty
string (the zeroed first byte). `set_dynamic(bool)` flips the active arena at
activate; the switch resets the cursor into the selected arena.

### Key operations — StringPool

- `alloc_string(value)` — escape-process (`\n`, plus the Xash extension `\r`/`\t`
  that GoldSrc leaves alone), dedup against the active arena, append. On
  exhaustion the cursor **wraps** and overwrites old strings (the legacy
  `numoverflows` quirk — stale `string_t` values then read newer text;
  preserved).
- `make_string(value)` — a pointer already within `int` range of `base` becomes
  a direct offset; anything else (e.g. a game-DLL static on x64) falls back to
  `alloc_string`.
- `get_string(s)` → `base + s`.
- `process_string(dst, src)` (static) — the escape expansion; `dst == nullptr`
  measures.

Default sizing is the legacy `65536 * ceil(max_edicts / 1024)`. The Linux mmap
near-module probing is deliberately **not ported** — the heap arena +
`make_string` INT-range fallback is the legacy-Windows-x64 parity baseline
(Q-20/OQ-6).

______________________________________________________________________

## `EntityView` — the accessor seam

**Header**: `entity_view.hpp` (header-only)

The zero-cost typed facade over the edict store. Engine-**internal** server
code reads/writes entvars through `EntityView`, never via `->v.` directly; raw
access is confined to `abi/`, the pmove bridge, and the Chunk 8 save serializer
(a compliance-scan rule enforces this). Accessors are **value-semantic** (they
return copies, e.g. `origin()` → `Vec3`, not a reference into the store) so a
future ABI flavor can swap the backing arena behind the seam without touching
callers. Entity cross-links stay `edict_t*` — the pointer identity **is** the
ABI handle. Every accessor compiles to a direct load/store on the array. It also
exposes `valid()` (`SV_IsValidEdict`: non-null and not freed) and `raw()` (the
sanctioned escape hatch).

______________________________________________________________________

## Threading model

Main-thread only (OQ-9). The bridge is the one surface that does **not**
self-assert: the 159 slots and ~30 pmove callbacks are plain C pointers the game
DLL calls **synchronously from inside an already-asserted engine call**
(`pfnTouch` inside `sv_impact`, `pfnKeyValue` inside `spawn_entities`,
`pfnPM_Move` inside `sv_run_cmd`), so they inherit the Main-thread context of
their caller transitively. Three items are safe **only** by that contract and
would race under concurrent entry:

- **`g_bridge`** — written by `install_engine_bridge` (at `load_progs`), nulled
  at `unload_progs`, read by every slot in between. Process-global, so it also
  assumes a single live server.
- **ABI static return buffers** — `s_value[256]` / `s_empty` / the several slots
  that return a static `""` (`pfnInfoKeyValue`, `pfnGetInfoKeyBuffer`, …). The
  frozen slot contract only requires the returned pointer to survive the call;
  a second concurrent caller would clobber the first's result. **Do not retain
  the pointer across another ABI call** (the legacy contract, unchanged).
- **`s_rng_state`** — the `COM_RandomLong/Float` xorshift state (a tracked
  RNG-unification stub).

See [threading-and-invariants.md](./threading-and-invariants.md) and
[docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md).

## Error handling

Fatal ABI conditions (edict exhaustion, bad `WriteEntity`, missing exports)
route through the injected `HostErrorHook` (Q-5); the slot still returns a safe
value afterwards because the hook makes no control-flow guarantee. `GameDll`
reports load failures via `last_error()` (`LoadError` enum) rather than
aborting. No exceptions.

## Edge cases and invariants

- Slot 0 is world; 1..maxclients are clients; the arena never scans below
  `reserved`.
- `build_engine_table` hands the DLL a **copy**, not the master table.
- The string pool's overflow wrap is intentional parity, not a bug — stale
  `string_t` values reading newer text is legacy-observable.
- Arena exhaustion returns `nullptr` (Q-5 error model) instead of calling
  `Host_Error` directly; the bridge maps it — behaviour is identical at the ABI
  surface.

## See also

- [lifecycle.md](./lifecycle.md) — who installs the bridge and owns the stores
- [physics-and-pmove.md](./physics-and-pmove.md) — the `playermove_t` callback
  table the bridge also backs
- `docs/boundaries/server-boundary.md` — the frozen ABI contract table
- `docs/legacy-survey/deep-dive-server-game-dll-bridge.md` — the exhaustive
  quirk catalogue with file:line cites
