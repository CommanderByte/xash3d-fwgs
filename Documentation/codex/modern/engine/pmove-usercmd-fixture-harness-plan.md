# PMove Usercmd Fixture Harness Plan

Phase: 145

## Purpose

`sv_pmove.c` and the movement path in `sv_client.c` are still runtime-owned
because PMove is a bridge between client packets, `sv_client_t`, `edict_t`,
`playermove_t`, world traces, physents, game DLL callbacks, and touch replay.
Moving it directly would combine protocol, physics, and game DLL ABI risk in
one change.

This phase defines fixture data first. The goal is to make future PMove work
testable with plain snapshots before attempting to route setup, finish,
command replay, or callback publication through modern code.

## Legacy Runtime Areas

| Area | Legacy owner | Fixture target |
| --- | --- | --- |
| Movement packet decoding | `SV_ParseClientMove()` | Usercmd arrays, backup count, dropped count, checksum admission, freeze/paused command normalization. |
| Timebase calculation | `SV_EstablishTimeBase()` | Plain command durations, last-command replay, backup-command replay, current-command replay. |
| Command execution | `SV_RunCmd()` | Command split/replay order, random seed selection, speed-hack ignore windows, fake-client exceptions. |
| PMove setup | `SV_SetupPMove()` | Comparable setup snapshot from player state, command, timebase, hull choice, water fields, and user fields. |
| PMove finish | `SV_FinishPMove()` | Comparable return snapshot for origin, velocity, view angles, old buttons, ground flag, and runfuncs reset. |
| Callback table | `SV_InitClientMove()` | Mock inventory for trace, contents, texture, model, file, sound, event, and console callbacks. |
| Unlag history | `SV_SetupMoveInterpolant()` / `SV_RestoreMoveInterpolant()` | Packet-history frame pairs, latency/lerp target timing, teleport/nointerp cases, and restore intent. |
| Touch replay | tail of `SV_RunCmd()` | `numtouch`, `touchindex`, PM trace conversion, impact ordering, custom touch callback, and freed entity cases. |

## Fixture Inputs

The first reusable fixture shape lives in modern C++ tests and covers:

- `PmoveFixtureUsercmd`: command duration, buttons, impulse, movement axes,
  lerp milliseconds, light level, and view angles;
- `PmoveFixtureRunPlan`: replay order for last-command recovery, backup
  commands, current commands, and random seeds;
- `PmoveFixturePlayerState`: plain player facts needed to build a safe PMove
  setup snapshot;
- `PmoveFixtureSetupSnapshot`: fields copied into `playermove_t` that can be
  compared without live edicts or callbacks;
- `PmoveFixtureMoveResult` and `PmoveFixtureFinishSnapshot`: fields copied
  back from PMove after movement;
- `PmoveFixtureCallbackMock`: a bitmask inventory of callbacks a future mock
  runtime must provide before PMove can move further;
- `PmoveFixtureUnlagHistory`: target-time inputs routed through the existing
  modern PMove unlag policy.

The first skeleton intentionally avoids `server.h`, `usercmd_t`,
`playermove_t`, `sv_client_t`, `edict_t`, `physent_t`, `pmtrace_t`, and live
globals. It depends only on `server_pmove_bridge_policy.hpp`.

## Harness Placement

Use modern C++ tests first:

- `tests/engine/pmove_usercmd_fixture_common.hpp`
- `tests/engine/pmove_usercmd_fixtures.cpp`

Legacy C tests should wait until a future phase needs direct comparison with
`SV_ParseClientMove()`, `SV_SetupPMove()`, or `SV_RunCmd()` side effects.
A shared generated harness can come later if both modern and legacy tests need
the same command streams.

## Callback Mock Inventory

Before PMove ownership moves, fixtures need mocks for:

- player hull traces: `PM_PlayerTrace`, `PM_PlayerTraceEx`, `PM_TraceLine`,
  and `PM_TraceLineEx`;
- position and contents checks: `PM_TestPlayerPosition`,
  `PM_TestPlayerPositionEx`, `PM_PointContents`, and
  `PM_TruePointContents`;
- model/surface queries: model type, bounds, BSP hull, trace model, trace
  surface, and trace texture;
- file and text helpers: `COM_FileSize`, `COM_LoadFile`, `COM_FreeFile`,
  `memfgets`, and `PM_Info_ValueForKey`;
- emitted effects: particles, sounds, and playback events;
- touch replay: `PM_StuckTouch`, custom `PM_PlayerTouch`, PM trace conversion,
  and `SV_Impact()` ordering.

The fixture currently records that inventory as a bitmask rather than
implementing callback behavior.

## Deferred Runtime Ownership

Keep these legacy-owned for now:

- packet bitstream decoding and command checksum validation;
- `SV_ParseClientMove()` dropped-command sequencing against live `net_drop`;
- `SV_RunCmd()` command splitting, speed-hack gates, game DLL callback order,
  PMove execution, and touch replay;
- `SV_SetupPMove()` / `SV_FinishPMove()` live `edict_t` and `playermove_t`
  copying;
- `SV_InitClientMove()` callback table publication;
- `SV_CopyEdictToPhysEnt()`, `SV_AddLinksToPmove()`, and
  `SV_AddLaddersToPmove()`;
- unlag relinking and restore mutation;
- `PM_Move()` and shared `pm_trace` hull traversal.

## Next Good Fixture Step

The next PMove step should extract only a plain command replay plan:

1. normalize frozen/paused commands;
2. decide replay order for dropped, backup, and current commands;
3. compute the timebase command-duration window;
4. leave bitstream reads, checksums, `SV_RunCmd()`, and live `lastcmd`
   mutation in `sv_client.c`.

That gives us useful coverage without pretending a synthetic harness can
execute PMove or validate trace callbacks.

## Validation

Focused validation target:

```text
.\waf.bat build --targets=test_engine_pmove_usercmd_fixtures,test_engine_server_pmove_bridge_policy
```
