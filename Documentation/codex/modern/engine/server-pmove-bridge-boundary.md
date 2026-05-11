# Server PMove Bridge Boundary

Phase 133 audits server-side player movement in `engine/server/sv_pmove.c`.
PMove is a bridge rather than a self-contained subsystem: it adapts live server
clients, edicts, world links, model data, unlag history, game DLL callbacks,
and `pm_shared` code into one frame of player movement. The safe modernization
seam is therefore the plain unlag and interpolation policy, not `PM_Move()` or
physent ownership.

## Legacy Owners

| Area | Legacy owner | Why it stays legacy-owned |
| --- | --- | --- |
| PMove callback table | `SV_InitClientMove()` | Publishes `playermove_t` callbacks for particles, traces, contents, files, randoms, sounds, events, model queries, and PMove extension callbacks. This is an ABI surface for game DLL/player physics code. |
| Physent conversion | `SV_CopyEdictToPhysEnt()` | Reads `edict_t`, `model_t`, flags, player/bot identity, studio hitbox flags, custom solids, and unlag-adjusted origins before writing `physent_t`. |
| Physent population | `SV_AddLinksToPmove()` and `SV_AddLaddersToPmove()` | Traverses live area-node lists, applies group filters, skips owner/dead/monsterclip entities, checks bounds, consults model types, and appends to `physents`, `visents`, and `moveents`. |
| PMove setup and finish | `SV_SetupPMove()` and `SV_FinishPMove()` | Copies a large live `edict_t`/`sv_client_t` state surface into and out of `playermove_t`, updates hull choice, groundentity, angles, user fields, and min/max size. |
| Unlag state mutation | `SV_SetupMoveInterpolant()` and `SV_RestoreMoveInterpolant()` | Reads packet history, moves other clients to interpolated origins, relinks edicts, and later restores original positions. |
| Command execution | `SV_RunCmd()` | Owns speed-hack timing gates, command splitting, game DLL command callbacks, moving-ground velocity, pre/post-think, PMove execution, touch replay, and client timebase/cmdtime mutation. |
| Client packet parsing | `SV_ParseClientMove()` in `sv_client.c` | Decodes delta-compressed `usercmd_t`, handles packet loss/drop accounting, frozen players, backup commands, and calls `SV_RunCmd()`. |

## Modern Helper

Phase 133 adds:

- `src/include/engine/server/server_pmove_bridge_policy.hpp`
- `src/engine/server/server_pmove_bridge_policy.cpp`
- `engine/server/server_pmove_bridge_policy_adapter.*`
- `tests/engine/server_pmove_bridge_policy.cpp`

The helper owns only plain PMove bridge policy:

- decide whether unlag may run from multiplayer state, game DLL allowance,
  `sv_unlag`, client lag-compensation flag, and spawned state;
- validate player edict indexes against the current maxclient count;
- decide whether an active/moving interpolant should replace a player origin or
  bounds;
- preserve the strict 64-unit teleport threshold used to disable interpolation;
- clamp client latency to the legacy 1.5 second cap and optionally sanitize
  negative `sv_maxunlag` to zero;
- clamp lerp time to the legacy 100 ms cap and lower-bound it by the next
  message interval;
- compute the unlag target time with `sv_unlagpush` while preventing a future
  target;
- compute the frame interpolation fraction with the old zero-span and `[0, 1]`
  clamp behavior.

`sv_pmove.c` routes those decisions through the helper, but keeps all cvar
mutation, packet-history reads, edict movement, relinking, and PMove callbacks
in legacy code.

## Existing Coverage

Related modern helpers cover adjacent policy but not PMove ownership:

- `game_dll_movement_policy`: fake-client `pfnRunPlayerMove()` admission,
  timebase calculation, and callback command snapshots.
- `game_dll_visibility_trace_policy`: game DLL trace callback admission and
  result policy, not PMove tracing.
- `server_group_filter`: group predicates used while collecting PMove
  physents.
- `server_world_link_policy`: area-node traversal masks also used by PMove
  collection, without owning live list traversal.
- `server_physics_routing_policy`: server entity physics dispatch, not player
  command execution.

## Fixture Requirements

Before PMove can move further into `src/engine/server`, we need fixtures for:

- `sv_client_t` command timing: speed-hack ignore windows, warning counters,
  fake-client exceptions, command splitting, timebase, and cmdtime mutation.
- Delta-compressed `usercmd_t` packet parsing from `SV_ParseClientMove()`,
  including dropped packets, backups, frozen players, and `lastcmd` reuse.
- `playermove_t` setup/finish snapshots covering origin, velocity, view
  angles, flags, hull choice, user fields, water, ducking, oldbuttons,
  groundentity, and fixangle behavior.
- Physent/visent/moveent population with synthetic area nodes, world entity,
  players, bots, dead bodies, ladders, monsterclip brushes, owner missiles,
  transparent models, custom solids, and bounds intersections.
- PMove callback mocks for `PM_PlayerTrace`, `PM_TraceLine`,
  `PM_TestPlayerPosition`, `PM_PlaySound`, `PM_PlaybackEventFull`, model
  queries, and file callbacks.
- Unlag frame-history fixtures for packet entity rings, nointerp flags,
  teleports, missing second frames, expired history, and restore/relink order.
- Touch replay fixtures for `numtouch`, `touchindex`, trace conversion,
  `SV_Impact()` ordering, custom `PM_PlayerTouch`, `sv.playersonly`, and freed
  entities.

## Deferred Boundaries

Do not route these yet:

- `SV_CopyEdictToPhysEnt()`, `SV_AddLinksToPmove()`, or
  `SV_AddLaddersToPmove()`;
- `SV_SetupPMove()`, `SV_FinishPMove()`, or the `playermove_t` callback table;
- `SV_SetupMoveInterpolant()` and `SV_RestoreMoveInterpolant()` mutation beyond
  the pure policy already extracted;
- `SV_RunCmd()` command splitting, callback order, PMove execution, or touch
  replay;
- `SV_ParseClientMove()` packet decoding and dropped-command sequencing.

Those paths require deterministic command, physent, packet-history, trace, and
callback fixtures before route-through would be safer than the legacy owner.

## Validation

Phase 133 validation:

- `.\waf.bat build --targets=test_engine_server_pmove_bridge_policy` passed.
- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 129/129 tests.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.515 seconds and stopped with reason `command`.
