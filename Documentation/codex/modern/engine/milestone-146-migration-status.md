# Milestone 146 Migration Status

Phase: 146

## Purpose

Phase 146 is a pause-and-check checkpoint after the server post-134
consolidation lane. The goal is to answer a practical question before adding
more phases: are we ready to move larger pieces, or are we still building the
fixtures and vocabulary needed to avoid breaking compatibility?

Short answer: we are ready to consolidate selected modern domains, but not
ready to move the live server runtime wholesale.

## Current Position

### Modern Tree

Current scan at this checkpoint:

- `src/`: 359 files;
- `src/engine/`: 87 files;
- `src/engine/server/`: 65 implementation files;
- `src/include/engine/server/`: 112 header files.

The server modern tree is already split into meaningful subdomains:

- `client/`: 9 implementation files;
- `game_dll/`: 15 implementation files;
- `messaging/`: 14 implementation files;
- `resources/`: 9 implementation files;
- flat server/shared layer: 18 implementation files.

This is no longer only a scratchpad. It contains reusable policy and value
objects with focused tests.

### Legacy Runtime Tree

`engine/server/` still contains 120 files. The largest remaining runtime
owners are:

- `sv_game.c`: game DLL ABI, callback tables, entity lifecycle, messaging,
  resources, string pool, trace/visibility, and changelevel/save interactions;
- `sv_client.c`: client admission, sessions, userinfo, commands, transfers,
  voice, rcon, packet parsing, and movement packet handling;
- `sv_save.c`: runtime save/load streams, entity field parsing, filesystem
  mutation, callback reads, and console output;
- `sv_phys.c`, `sv_world.c`, `sv_pmove.c`, and `sv_move.c`: live world,
  physics, traces, PMove, hull traversal, and touch/impact behavior;
- `sv_frame.c`: frame snapshots, entity deltas, event emission, datagrams, and
  client update bookkeeping;
- `sv_main.c`, `sv_init.c`, and `sv_cmds.c`: runtime shell, startup/shutdown,
  cvar registration, lifecycle commands, operator commands, and server frame
  driving.

The adapter files in `engine/server/*_adapter.*` are still the compatibility
cost of routing narrow decisions through modern code while leaving the live C
runtime intact.

## What Is Already In Good Shape

### Filesystem

The filesystem lane is the most mature modernization pass. Most meaningful
archive/backend behavior has moved into `src/filesystem` and the old
`filesystem/` directory is now mostly compatibility/export glue. Full removal
of legacy ABI surfaces is intentionally deferred because the engine still
expects legacy filesystem exports, memory hooks, and console behavior.

### Launcher

The launcher is now under `src/launcher` with platform-specific concerns
separated under `src/launcher/platform`. The old `game_launch` structure has
effectively been retired.

### Shared Utilities

Modern utilities now exist for debugging, JSON writing, registries, checksum
and hash helpers, path/text helpers, build numbers, and network-buffer-style
value operations. These are useful foundations for later real modules.

### Engine Server Policy Layer

The server has strong coverage for:

- game DLL bridge policy and value objects;
- resource identity, catalogs, consistency, downloads, uploads, hot resources,
  and customization payloads;
- message payload builders and recipient policies;
- client admission/session/userinfo/command/admin/query surfaces;
- frame/datagram and packet-entity cursor policies;
- server limits, group filters, map validation, lifecycle limits, visibility
  constraints, and movement constraints;
- world link policy, PMove bridge policy, and fixture harnesses for world
  trace and usercmd/PMove preparation;
- save/restore format and value-object fixtures.

This is a good compatibility safety net. It is not yet full runtime ownership.

## Main Remaining Gap

The biggest gap is live state ownership.

Much of the modern layer is still:

1. read plain facts from legacy structs;
2. call a modern helper;
3. write the result back through legacy code.

That was the right first method, but it should not become the final design.
The next phases should now group and test modern domains as concepts, then
reduce adapter duplication only where a real domain owner exists.

## Runtime Smoke Checkpoint

Phase 146 validation used the post-144 and post-145 fixture targets as the
focused consolidation check:

```text
.\scripts\run-phase-validation.ps1 -FocusedTarget "test_engine_world_trace_fixtures,test_engine_pmove_usercmd_fixtures,test_engine_server_world_link_policy,test_engine_server_pmove_bridge_policy" -StopRunningXash -CopyLauncher
```

Results:

- focused tests passed;
- `.\waf.bat build --targets=xash` passed;
- `.\waf.bat build --alltests` passed 134/134;
- runtime binaries were refreshed in `run-win32`;
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.502 seconds and stopped with reason `command`;
- `scripts/run-game.ps1 -StopRunningXash -CopyLauncher` launched the game
  successfully for manual runtime validation.

Recent server-lane first-frame timings were approximately:

- 0.501 seconds;
- 0.497 seconds;
- 0.508 seconds;
- 0.494 seconds;
- 0.481 seconds;
- 0.502 seconds at this checkpoint.

The current timing is inside the recent range. There is no obvious startup
regression to investigate from this checkpoint alone.

## Recommendation

The next lane should be domain consolidation rather than another long stretch
of isolated helper extraction.

Preferred order:

1. resource domain aggregate tests;
2. resource adapter shrink/consolidation where safe;
3. messaging domain aggregate tests;
4. messaging adapter shrink/consolidation where safe;
5. game DLL bridge domain consolidation;
6. client/session domain consolidation;
7. world/PMove fixture expansion before moving live runtime;
8. save runtime fixture expansion before moving stream ownership;
9. a broader client/render/audio/menu audit after the server domains stop
   producing easy wins.

Avoid for now:

- a giant `Server` class;
- moving `edict_t`, `sv_client_t`, `playermove_t`, or `trace_t` ownership
  without fixtures;
- merging adapters solely to reduce file count;
- touching allocator ownership before more code is C++-owned;
- changing game DLL ABI, filesystem ABI, or platform exports without a
  dedicated compatibility phase.

## Practical Migration Meaning

At this point we are roughly in the middle stage:

- **done:** mapping, tests, policy extraction, low-risk value objects, and
  first domain grouping;
- **in progress:** turning modern helper islands into coherent domain modules;
- **not done:** moving the live server, client, renderer, audio, allocator, and
  game DLL ABI runtime ownership into modern C++ concepts.

The work has not stalled, but the next step should be more architectural than
another pile of tiny one-function wrappers.
