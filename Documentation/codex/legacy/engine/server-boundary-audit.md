# Server Boundary Audit

Phase 52 audits `engine/server/` before moving more server internals into the
modern source tree. The goal is to separate stable compatibility surfaces from
implementation code that can be migrated behind them.

## Inventory

| File | Lines | Current ownership | Migration risk |
| --- | ---: | --- | --- |
| `server.h` | 602 | Server structs, globals, prototypes, inline helpers, game DLL-facing declarations. | High: central compatibility header. |
| `sv_main.c` | 978 | Server cvars, startup, shutdown, host frame, movevars, heartbeats, user-agent policy. | High: lifecycle and global state. |
| `sv_init.c` | 918 | Map spawn/activation/deactivation, precache indexes, server allocation. | High: map lifecycle and global state. |
| `sv_client.c` | 3,051 | Connections, challenges, rcon, client commands, connectionless packets, userinfo, resource upload. | High: protocol, clients, game DLL calls. |
| `sv_game.c` | 4,407 | Game DLL loading, exported engine callbacks, edicts, string pools, entity APIs, events. | Very high: ABI bridge to game DLLs. |
| `sv_save.c` | 2,057 | Save/restore format, level transitions, save headers, entity tables, client state. | Very high: binary/file compatibility. |
| `sv_phys.c` | 1,773 | Entity physics, collision response, custom physics API. | High: gameplay behavior. |
| `sv_world.c` | 1,353 | Area nodes, entity link/unlink, traces, point contents, light styles. | High: physics, game DLL callbacks. |
| `sv_pmove.c` | 834 | Server-side player movement and prediction bridge. | High: movement behavior and shared pmove state. |
| `sv_frame.c` | 812 | Client frame, deltas, snapshots, message sending. | High: protocol and client state. |
| `sv_cmds.c` | 925 | Server console commands and operator commands. | Medium/high: command names are stable, handlers are coupled. |
| `sv_custom.c` | 474 | Resource lists, consistency response, uploads/download resource flow. | Medium/high: resource protocol. |
| `sv_move.c` | 456 | NPC movement helpers and yaw/bottom checks. | Medium/high: gameplay behavior. |
| `sv_filter.c` | 570 | ID/IP ban filters, filter commands, `banned.cfg`, `listip.cfg`, embedded tests. | Low/medium: policy is mostly extractable. |
| `sv_log.c` | 208 | Multiplayer server event log, remote log address, log command. | Medium: small but sink/router ownership is not finished. |
| `sv_query.c` | 153 | Source/GoldSrc query response payloads. | Low/medium: packet builder can be isolated. |

## Compatibility Surfaces

### Public Server C Surface

The visible server surface is still C-style:

- `SV_*` functions declared in `engine/server/server.h` and selected declarations
  repeated in `engine/common/common.h`;
- `Log_*` server event logging functions;
- `sv`, `svs`, `svgame`, and `sv_areanodes` globals;
- `sv_client_t`, `server_t`, `server_static_t`, and `svgame_static_t` layout.

These names and layouts should remain stable while migration is in progress.
Modern code should sit behind them rather than replacing them in place.

### Game DLL And Physics ABI

`svgame_static_t` owns the loaded game DLL instance, exported DLL function
tables, new DLL function table, physics interface table, edict array, global
vars pointer, movement state, user message state, and server string pools.

High-risk compatibility points:

- `DLL_FUNCTIONS`, `NEW_DLL_FUNCTIONS`, `physics_interface_t`;
- `edict_t`, `entvars_t`, `globalvars_t`, and `string_t`;
- `GAME_EXPORT` functions called by game DLLs;
- save/restore field callbacks such as `pfnSaveReadFields`,
  `pfnSaveWriteFields`, `pfnSave`, and `pfnRestore`;
- user message registration and rewrite state.

Do not expose modern C++ types across this boundary. Use adapters that translate
plain C data into modern snapshots or policy objects.

### Network And Wire Formats

Server code writes directly to `sizebuf_t` via `MSG_*` and sends packets through
`NET_*` and `Netchan_*`. The following formats are compatibility-sensitive:

- connectionless commands and responses;
- Source/GoldSrc query payloads in `sv_query.c`;
- connect/challenge/rcon packet flow in `sv_client.c`;
- server-to-client `svc_*` messages;
- resource list and consistency messages;
- voice packets;
- save/restore handoff messages.

The safest first network migration is payload construction that returns bytes
or writes into a legacy buffer supplied by the adapter. Packet routing should
stay in legacy code until server networking has a broader design.

### Commands And Cvars

Server commands are registered from several places:

- `SV_InitHostCommands()` registers map/load/save commands.
- `SV_InitOperatorCommands()` registers operator/server commands.
- `SV_InitFilter()` registers `addip`, `listip`, `removeip`, `writeip`,
  `banid`, `listid`, `removeid`, and `writeid`.
- `SV_ServerLog_f()` and `SV_SetLogAddress_f()` are registered as `log` and
  `logaddress`.

Server cvars are mostly defined and registered in `sv_main.c`. Command names,
cvar names, aliases such as `sv_allowupload`, and compatibility placeholders
such as `sv_allow_dlfile` must remain stable.

### File Formats And Persistence

Server code writes several compatibility-sensitive files:

- `banned.cfg` from ID ban filters;
- `listip.cfg` from IP filters;
- multiplayer log files under `logsdir`;
- savegame and level-transition files under `DEFAULT_SAVE_DIRECTORY`;
- entity patches from `entpatch`;
- resource/customization data through resource helpers.

`sv_filter.c` and `sv_query.c` are attractive because their file/wire surfaces
are small enough to baseline before implementation moves.

## Lifecycle

The high-level lifecycle is:

1. `Host_Init()` calls `SV_Init()`.
2. `SV_Init()` registers commands/cvars, initializes filters, clears game
   state, initializes the game DLL, and prepares network buffers.
3. `Host_ServerFrame()` runs server timing, networking, physics, and client
   messaging while active.
4. `SV_Shutdown()` sends final messages, shuts down network/public-server state,
   deactivates the server, drops the client, frees clients, and clears globals.
5. `Host_Shutdown()` calls `SV_ShutdownFilter()` after server shutdown.

This lifecycle is too central for an early rewrite. Modernization should first
move leaf policies that can be called from the existing lifecycle.

## Migration Candidate Ranking

| Rank | Candidate | Why it is suitable | Main caveat |
| ---: | --- | --- | --- |
| 1 | `sv_filter.c` policy | Mostly address/ID matching, formatting, linked-list state, and command wrappers; already has embedded tests. | Command handling, client dropping, file writes, and `host.realtime` must stay adapter-owned at first. |
| 2 | `sv_query.c` payload builder | Small file, clear request types, good fit for golden payload tests and modern network buffer primitives. | Reads live globals and sends packets directly; builder must consume snapshots. |
| 3 | user-agent/input-device policy in `sv_main.c` | Policy is relatively pure and guards client connection behavior. | Rejection routing and cvars are still legacy-owned. |
| 4 | `sv_log.c` formatting pieces | Small, isolated server-event format rules. | Full sink ownership waits on console/log router decisions. |
| 5 | server command registration table | Command definitions could become declarative. | Handlers are strongly coupled to save/map/client state. |
| 6 | resource list helpers in `sv_custom.c` | List manipulation may be separable. | Resource protocol and client state make it easy to change behavior accidentally. |
| Later | save/restore, game DLL bridge, physics/world, client connection flow | High payoff. | Too compatibility-sensitive until smaller server slices prove the adapter pattern. |

## Phase 53 Recommendation

Use `sv_filter.c` as the first server pilot.

The first implementation slice should extract pure rule behavior only:

- filter-address parse/match decisions already covered through
  `NET_StringToFilterAdr`;
- IP filter inclusion/removal policy;
- ID filter prefix matching and expiration behavior;
- formatting for human output versus config output.

Keep these legacy-owned initially:

- `Cmd_Argv`/`Cmd_Argc` command handlers;
- `Cmd_AddRestrictedCommand`/`Cmd_RemoveCommand` registration;
- `FS_Open`, `FS_Write`, `FS_Printf`, `FS_Close`;
- `SV_ClientPrintf`, `SV_DropClient`, `svs.clients`;
- `host.realtime` as a time source supplied to modern code.

## Phase 54 Recommendation

Use `sv_query.c` as the second server pilot.

The modern helper should consume an immutable `SourceQuerySnapshot` containing
hostname, map name, game folder, game description, version, max clients, player
counts, platform code, password/secure flags, cvar rules, and player rows. It
should build byte-stable responses without sending packets itself.

The legacy adapter should keep:

- request dispatch in `SV_SourceQuery_HandleConnnectionlessPacket`;
- `NET_SendPacket` routing;
- live reads from `sv`, `svs`, `svgame`, `GI`, cvars, and client arrays.

## Smoke Expectations

Phase 52 is documentation-only, so no runtime smoke is required.

For Phase 53 and Phase 54, run:

- focused modern tests under `tests/engine`;
- existing engine embedded tests through `.\waf.bat build --alltests`;
- a dedicated-server or regular runtime smoke after any route-through change;
- time-to-first-frame logging whenever the regular game smoke is used.
