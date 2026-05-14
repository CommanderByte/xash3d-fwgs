# Domain Consolidation Checkpoint

Phase 160 pauses after the post-146 server consolidation lane. The goal is to
decide whether the modern server layer is becoming easier to reason about, or
whether it is drifting into a very correct but very wordy pile of adapters,
policies, and one-off tests.

## Current Shape

Current scan after Phase 159:

| Area | Files | Lines | C++ sources | C++ headers | C sources | C headers |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/engine/server` | 67 | 9025 | 66 | 0 | 0 | 0 |
| `src/include/engine/server` | 67 | 4500 | 0 | 66 | 0 | 0 |
| `engine/server` | 124 | 26293 | 52 | 0 | 15 | 53 |
| `tests/engine` | 88 | 13619 | 86 | 2 | 0 | 0 |

The legacy server directory is now mostly adapters by file count:

| Legacy server group | Files | Lines |
| --- | ---: | ---: |
| Adapter-named files | 108 | 6258 |
| Non-adapter files | 16 | 20035 |

That is the expected shape for this migration method. The live runtime still
belongs to the old C files, while many small compatibility decisions are tested
and routed through modern C++ helpers.

## Modern Domain Split

The modern tree has four grouped domains and one remaining flat layer:

| Domain | Source files | Source lines | Header files | Header lines |
| --- | ---: | ---: | ---: | ---: |
| `client` | 9 | 791 | 9 | 383 |
| `game_dll` | 15 | 3027 | 15 | 1547 |
| `messaging` | 14 | 1391 | 14 | 872 |
| `resources` | 9 | 1114 | 9 | 423 |
| flat server layer | 20 | 2702 | 20 | 1275 |

The grouped domains are the right direction. The remaining flat layer is now
the main source of visual clutter:

- server-wide shared constraints: limits, lifecycle limits, group filters, map
  validation, visibility constraints;
- runtime shell helpers: filters, event-log formatting, lifecycle/operator
  command policy;
- world/physics helpers: movement constraints, physics routing, PMove bridge,
  world link policy, world trace policy;
- save helpers: save/restore format, value decisions, runtime fixtures;
- query helpers: source query and NetAPI payload builders.

These are not bad helpers. They are just not grouped enough to communicate the
architecture at a glance.

## Legacy Structure Comparison

The old layout is useful as a compatibility map, not as the final design:

| Legacy owner | Main responsibility today | Modern shape we want |
| --- | --- | --- |
| `sv_game.c` | Game DLL ABI, callbacks, entities, messages, resources, traces, movement, changelevel/save hooks. | `game_dll` domain with internal ABI, lifecycle, entities, messaging, resources, world-query, movement, and output concepts. |
| `sv_client.c` | Admission, sessions, commands, userinfo, transfers, voice, rcon, query responses, move parsing. | `client` domain plus resource, messaging, and future movement-owned helpers. |
| `sv_custom.c` | Custom resources, consistency, customization propagation, HPAK side effects. | `resources` domain, with payload writers remaining under `messaging` where appropriate. |
| `sv_frame.c` | Snapshot/entity deltas, event emission, datagrams, inactive client update bookkeeping. | Future `snapshot` or `messaging` subdomain once frame fixtures are stronger. |
| `sv_world.c`, `sv_phys.c`, `sv_move.c`, `sv_pmove.c` | Area links, collision, traces, entity physics, monster move, PMove bridge. | `world` domain, but only after fixture coverage catches up. |
| `sv_save.c` | Save/load streams, entity restore, filesystem mutation, game DLL field callbacks. | `save` domain, starting with fixtures and value objects, not stream ownership. |
| `sv_main.c`, `sv_init.c`, `sv_cmds.c`, `sv_log.c`, `sv_filter.c`, `sv_query.c` | Runtime shell, startup/shutdown, cvars, commands, logs, filters, query responses. | `runtime`, `client`, and `shared` concepts split by responsibility. |

The old `sv_*.c` file names should continue to guide adapter placement. They
should not force modern code to inherit the same mixed ownership.

## What Is Working

- The tests are doing their job. We now have focused tests for many server
  quirks that used to be hidden inside large C functions.
- The modern helpers mostly consume plain values and return explicit plans or
  classifications.
- The risky surfaces remain legacy-owned: `edict_t`, `sv_client_t`, packet
  buffers, `sizebuf_t`, game DLL ABI, filesystem effects, console output,
  exact traces, PMove callbacks, and save streams.
- The grouped domains under `resources`, `messaging`, `game_dll`, and `client`
  are easier to scan than the earlier flat helper layer.

## What Feels Too Ceremonial

The problem is not "too many tests." The problem is too many tiny concepts
when a domain has already proven itself.

Avoid this as a permanent style:

- one helper file for every branch condition;
- one adapter file for every plain-value translation when a domain adapter
  helper already exists;
- long names that restate the whole migration history;
- `Plan`, `Policy`, `Service`, or `Manager` suffixes when a plain value or
  free function would be clearer;
- aggregate tests that only repeat every narrow unit test without exercising a
  real domain flow.

The better style from here is:

- keep narrow tests where they protect compatibility;
- add aggregate tests where a domain has enough pieces to behave as one thing;
- group modern files by domain once the owner is clear;
- group adapter glue only when it removes repeated mechanical conversion;
- leave live legacy side effects obvious at the call site.

## Decision

Continue server consolidation for the next short lane. Do not shift the main
effort to client/render yet.

Reasoning:

- Phase 159 found good client/render candidates, but that lane starts from
  audit and fixture work.
- The server lane already has enough tested surface to do productive
  simplification now.
- The next server work should improve shape rather than extract many more tiny
  helpers.

Deferred areas remain deferred:

- allocator/memory modernization waits until more runtime state is C++-owned;
- non-Windows platform validation stays in the 800-series phases;
- licensing/provenance cleanup stays in the 1100-series phase;
- client/render work should start after a deliberate phase-list switch, not as
  a side effect of server cleanup.

## Simplification Pilot Chosen

Phase 160 chooses **README inventory cleanup** as the immediate pilot.

Why this pilot:

- it reduces stale documentation noise immediately;
- it does not alter runtime code;
- it forces us to name the current domains clearly before moving more files;
- it gives later agents a shorter map so they do not reproduce the old
  one-file-per-seam pattern by default.

What changed:

- `src/engine/server/README.md` now describes the current grouped domains and
  boundaries instead of listing every helper one by one.
- `src/include/engine/server/README.md` now describes header ownership rules
  and canonical include paths instead of repeating a long helper catalog.

## Recommended Next Server Shape

Keep the existing grouped directories:

```text
src/engine/server/
  client/
  game_dll/
  messaging/
  resources/
```

Add the remaining directories only when a mechanical move or grouped test
needs them:

```text
src/engine/server/
  runtime/
  save/
  shared/
  world/
```

Likely mapping:

- `shared`: limits, lifecycle limits, group filters, map validation,
  visibility constraints;
- `runtime`: filters, event log formatting, lifecycle/operator command policy;
- `save`: save/restore format, value, and runtime fixture helpers;
- `world`: movement constraints, physics routing, PMove bridge, world link,
  and world trace helpers;
- `client`: source query and NetAPI can move here if the domain accepts query
  response ownership.

Do not add a `Server` class or a catch-all `server_runtime` facade yet. That
would hide the same mixed ownership that made the legacy C files hard to split.

## Adapter Strategy

Keep adapters boring.

Good adapter consolidation:

- shared helper functions for repeated value conversion;
- small grouped adapter utilities inside an already-tested domain;
- adapter files that mirror a real legacy owner or real modern domain.

Bad adapter consolidation:

- a giant `server_adapter.cpp`;
- a giant `game_dll_adapter.cpp` that hides callback publication order;
- moving world/physics/PMove/save stream mutation into C++ before fixtures
  exercise the dangerous paths.

The adapter count is less important than adapter honesty. A separate adapter is
fine when it keeps a risky side effect visible.

## Validation

Phase 160 validation should run the full automation script because this is a
checkpoint phase:

```powershell
.\scripts\run-phase-validation.ps1 -SkipFocused -CopyLauncher -StopRunningXash -AllowSmokeNonZeroExit
```

The smoke result should be recorded in `Documentation/codex/tasks.md` so we
keep the first-frame timing breadcrumb trail.

Validation result:

- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 141/141 tests.
- Runtime binaries were refreshed in `run-win32`.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.498 seconds and stopped with reason `command`.
