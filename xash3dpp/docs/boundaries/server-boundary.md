# Server Boundary Spec (Chunk 6)

> **Refreshed 2026-07-06 (as-built pass).** Chunk 6 is **Complete** — the
> dedicated-server milestone shipped: a real 32-bit `hl.dll` loads, `c0a0`
> spawns, and one map frame runs clean. `status_table.py` reports **30 TUs,
> tests ✅, Complete**; `compliance_scan.py server` is **clean** (74 files,
> 0 blocker/warning/note; 9 judgment areas reviewer-set). The subsystem is
> `cxx_std_23`. The pre-implementation spec below is retained verbatim as the
> design record; the new **§As-built reconciliation** and **§Extension axes
> (Q-21)** sections reconcile it against the shipped `xash3dpp/src/server/`
> tree (5 slices / 30 TUs) and record the headline door-keep verdicts. Nothing
> in the original spec is deleted; superseded items are marked inline.

*Status: pre-implementation spec, 2026-07-04. Legacy reference:
`engine/server/*.c|h` (~24.5k lines) plus the frozen ABI headers
`engine/eiface.h`, `engine/edict.h`, `engine/progdefs.h`,
`engine/physint.h`, `pm_shared/pm_defs.h`. Deep dives (exhaustive quirk
catalogues with file:line cites — read these before implementing an
area): `docs/legacy-survey/deep-dive-server-lifecycle.md`,
`deep-dive-server-game-dll-bridge.md`, `deep-dive-server-clients.md`,
`deep-dive-server-physics.md`, `deep-dive-server-world-frame.md`,
`deep-dive-server-save-boundary.md`, plus the pre-existing
`deep-dive-delta-encoder.md` and `deep-dive-trace-pvs.md`.*

## Responsibility

The server subsystem is the authoritative game-simulation host. It owns
the edict table and the game-DLL binding (loads `hl.dll`, implements the
full `enginefuncs_t` callback surface, drives `DLL_FUNCTIONS` callbacks);
runs the per-frame simulation (entity physics dispatch, thinks, pushers,
monster locomotion, the player-move bridge into the game DLL's
`pfnPM_Move`); manages client connections end-to-end (challenge/connect
handshake, resource/consistency distribution, usercmd execution,
delta-compressed snapshot streams); composes spatial queries over the
loaded world (areanode entity index, hull selection, multi-entity trace
merging, point contents, PVS/PHS multicast routing); and provides the
operator surface (console commands, rcon, A2S/legacy queries, ban
filters, HL-standard logging, master-server registration).

It does **not**: load BSPs or own the trace kernel (map_loader, Chunk 5 —
the server composes per-entity traces on top of it per the "Chunk 6
contract" in `map_loader-boundary.md §2`); own sockets, netchan
reliability, or the delta codec (networking, Chunk 4); implement player
movement itself (the game DLL statically links pm_shared; the engine's
own prediction-side PM code is Chunk 11); serialize save games (Chunk 8 —
but the server core owns the changelevel orchestration and the seams
save/restore plugs into, see Satellites); or render anything.

## External ABI contracts

This chunk carries the highest ABI risk in the rewrite. Two distinct
frozen surfaces meet here: the **game DLL ABI** (C structs and
function-pointer tables shared with unmodified HL mod binaries) and the
**wire protocol** (already partly owned by networking).

### Game DLL ABI (byte-frozen; vendor verbatim, never redeclare)

| Surface | Contract |
|---|---|
| `engine/eiface.h` | `enginefuncs_t` — 159 slots, engine→DLL (the classic GoldSrc surface is the first 144 through `pfnGetPlayerAuthId`; this fork appends the Sequence/tutor block, `pfnQueryClientCvarValue{,2}`, `pfnCheckParm`, `pfnPEntityOfEntIndexAllEntities` — all filled by the engine); `DLL_FUNCTIONS` — 50 slots, DLL→engine; `NEW_DLL_FUNCTIONS` (version 1, 5 slots, all optional); `INTERFACE_VERSION` = 140 negotiated via `GetEntityAPI2` (by-pointer) with `GetEntityAPI` (by-value) fallback; `GiveFnptrsToDll` (`__stdcall` on Win32) is called **before** any GetEntityAPI. `TraceResult`, `KeyValueData`, `TYPEDESCRIPTION`/`FIELDTYPE`, `SAVERESTOREDATA`/`ENTITYTABLE`/`LEVELLIST` (save-side, Chunk 8 consumes; server core hands `globals->pSaveData` through). |
| `engine/edict.h` | `edict_t` header layout: `{free, serialnumber, area links, headnode, num_leafs, leafnums union (24×int32 QBSP2 / 48×int16), freetime, pvPrivateData, entvars_t v}`. Game code addresses entities by `&v` offsets and by byte offset from the edict array base (`pfnEntOffsetOfPEntity`), so the **array-of-edicts representation itself is ABI**. |
| `engine/progdefs.h` | `entvars_t` (123 fields, byte-exact) and `globalvars_t` (incl. `pStringBase`, `pSaveData`, the `trace_*` mirror block, `changelevel`). |
| `string_t` | `int` offset from `globals->pStringBase`. On 64-bit, offsets must stay in `int` range → on Linux/amd64 only, legacy mmaps the string pool within ±2 GB of the game DLL (`USE_MMAP`, sv_game.c:3046); on other 64-bit platforms **including Win64** it heap-allocates with no proximity guarantee and `SV_MakeString` falls back to `SV_AllocString` when the pointer difference overflows int (sv_game.c:3321-3328). Strategy for xash3dpp is an open question (OQ-6 below). |
| `engine/physint.h` | Xash extension: `SV_PHYSICS_INTERFACE_VERSION` = 6, export `Server_GetPhysicsInterface`, exchanging `server_physics_api_t` (engine→DLL) / `physics_interface_t` (DLL→engine). Optional; version-reject zeroes the table with only a warning. Hooks reach deep into physics, world linking, string pool, entity creation, save veto. |
| `pm_shared/pm_defs.h` | `playermove_t` + `physent_t` frozen. The engine owns the single static `playermove_t` object and fills its ~30 function pointers (trace family backed by the Chunk 5 kernel); the game DLL owns the simulation (`pfnPM_Init`/`pfnPM_Move`/`pfnPM_FindTextureType`). |
| Per-classname exports | `LINK_ENTITY_FUNC` (`__cdecl void(entvars_t*)`) resolved by raw classname string from the DLL at entity creation. |
| `trace_t` ↔ `pmtrace_t` | Legacy casts one to the other (sv_world.c:911); the layouts' common prefix is a de-facto ABI invariant the rewrite's trace types must respect at the boundary shim. |

Direct-symbol exports the game DLL links (`Host_Error`, etc.) go in
`xash3dpp/src/abi/engine_funcs.cpp` per host-boundary OQ-10 — every
`GAME_EXPORT` discovered in this chunk lands there.

### Wire protocol (shared with networking; PROTOCOL_VERSION 49)

- Connects accept protocol **49 only** — this fork has no GoldSrc-48
  server-side connect path (GoldSrc dialect handling in netchan/delta is
  client-side; the networking boundary's per-netchan dialect selection is
  unused by the Chunk 6 server milestone).
- svc_/clc_ opcode sets and their payload layouts (see the clients and
  world-frame deep dives §11/§10 for the exact lists and bit widths:
  entity number 13 bits, `LAST_EDICT` 8191, visible-packet count 11 bits,
  event index 10 bits, resource index 12 bits, etc.).
- Delta encoding is data-driven from `delta.lst` via networking's
  `net_encode` port (`deep-dive-delta-encoder.md`); the server registers
  game encoders via `pfnRegisterEncoders` → `Delta_AddEncoder` (the
  networking boundary §3 hook).
- Frozen shared structs on the wire: `entity_state_t`, `clientdata_t`,
  `weapon_data_t`, `usercmd_t`, `event_args_t`, `movevars_t`,
  `resource_t` (two wire forms: bit-packed in `svc_resourcelist`,
  byte-aligned in `svc_customization`). `customization_t` is ABI-frozen
  (handed to the game DLL via `pfnPlayerCustomization`) but in-memory
  only — it contains raw pointers and is never written to the wire.
- OOB text protocol: challenge/connect/bandwidth/info/netinfo/rcon/
  master-server (`S2M_INFO`)/A2S (`TSource Engine Query`, 'U', 'V') — no
  A2S challenge mechanism (replies never require one); the info reply is
  unconditional, but the 'U' players reply is suppressed entirely when
  `sv_expose_player_list` is 0 or a server password is set, and
  rules/players queries send **no reply at all** when the list would be
  empty (sv_query.c:106, 130-131, 158) — wire-observable parity
  behaviour.

### Host-side surface

In legacy, `engine/common/common.h:619-795` declares most of what
host/client call into the server (exception: `SV_GetPlayerCount` is
declared in `engine/server/server.h:543` — out-params for players and
bots — and called from `host.c:344`). In xash3dpp these become the
`server::Server` class API on the `EngineContext` slot (host-boundary
§Dependencies, `server` (Chunk 6) row; `SV_ShutdownFilter` /
`SV_Initialized` / `SV_GetMaxClients` / `SV_Serverinfo` are additions
sourced from common.h):
`SV_Init`, `SV_Shutdown(finalmsg)`, `SV_ShutdownFilter`, `SV_Active`,
`SV_Initialized`, `SV_GetMaxClients`, `SV_GetPlayerCount`,
`Host_ServerFrame`, `SV_ExecLoadLevel`, `SV_ExecLoadGame`,
`SV_ExecChangeLevel`, `SV_ShutdownGame`, `SV_Serverinfo`. Cross-calls
that need explicit seams (not part of the Server class API):

- `SV_ClipPMoveToEntity` — called back from the shared PM trace path for
  `SOLID_CUSTOM` physents (legacy `engine/common/pm_trace.c`); in
  xash3dpp the server supplies this as a callback when composing traces.
- Studio hulls — the cross-call runs server → studio code: the server's
  `SV_HullForStudioModel` calls `Mod_HullForStudio` (mod_studio.c). The
  default bone setup (`SV_StudioSetupBones`) is a *static inside
  mod_studio.c*, installed in the engine-default
  `sv_blending_interface_t` and overridable by the game DLL via
  `Server_GetBlendingInterface` (mod_studio.c:1262-1277). The whole
  surface folds into the studio-hull option seam (OQ-2).
- `SV_BroadcastPrintf` from `cvar.c` on FCVAR_SERVER changes (and
  `SV_BroadcastCommand("fullserverinfo ...")` on FCVAR_USERINFO changes
  when dedicated, cvar.c:142-151); FCVAR_MOVEVARS changes only set
  `host.movevars_changed`, which the server polls each frame — replaced
  by the server's `ICvarObserver` registration (cmd_cvar boundary D3;
  mask `FCVAR_SERVER`). Note: D3 assigns the FCVAR_MOVEVARS observer to
  the *host*; if the server observes FCVAR_MOVEVARS directly instead of
  polling a host flag, amend D3 explicitly rather than re-deciding here.
- `SV_GetSaveComment` (menu) — save satellite, Chunk 8.
- `SV_DrawDebugTriangles`/`SV_DrawOrthoTriangles` — physics-interface
  debug overlay, listen-server only; stub for the dedicated milestone.

## Interface (what the rest of the engine calls)

| Entry point | Caller | Purpose / notes |
|---|---|---|
| `Server::init()` (`SV_Init`) | Host startup | Register cvars/commands, init filter, load game DLL (`silent` unless single-player-only gameinfo). Also wires `Clock::set_frame_rate_gate` (host OQ-11). The server *implements* the `ITrustOracle` answer, but the oracle object is injected at `CmdCvarContext` init per cmd_cvar D2 — before `Server::init()` runs — so the host binds an indirection at construction (or cmd_cvar D2 gets an explicit post-init setter amendment). |
| `Server::shutdown(finalmsg)` (`SV_Shutdown`) | Host, client connect/demo | Final message ×2, master shutdown, deactivate, free clients/ring/testpacket, close log. |
| `Server::frame()` (`Host_ServerFrame`) | Host `RunFrame` | Order: check cmd-time window → read packets → update movevars → request missing resources (customization-upload completion sweep) → check timeouts → run game frame (fixed-step physics loop) → **early-return if zero physics frames ran** → send client messages → `pfnPrepWorldFrame` → master heartbeat (sv_main.c:693-719). The early return skips sends/heartbeat that host frame — behavioural quirk to keep. Caveat: `sv_fps` is registered only on non-dedicated builds (sv_main.c:893-895); dedicated always runs the `sv_fps == 0` path (`sv.frametime = host.frametime`, one physics step per host frame, early return unreachable). |
| `exec_load_level / exec_load_game / exec_change_level` | MapLoader GameState FSM | The MapLoader-owned GameState FSM (host-boundary OQ-2; legacy `host_state.c`), driven from `Host::RunFrame`, owns level-change sequencing (STATE_LOAD_LEVEL etc.); the server never changes maps outside these three entry points. Spawn/activate ordering in Invariants below. |
| `shutdown_game` | Host | Always sends the reconnect final message (`SV_FinalMessage("", true)`); then `Host_EndGame` when `GameState->newGame`, else stop sounds + deactivate. Skips `SV_ClearGameState` when a load is pending (`GameState->loadGame`). |
| `active() / initialized() / max_clients() / player_count()` | Host, client, cmd privilege checks | `active` = map loaded (`state != dead`); `initialized` = a server session has begun — set early in `SV_SpawnServer` right after game-DLL/client init succeeds (before world load and entity spawn; the load-game path sets it before the save is even read), cleared only by `SV_Shutdown`. |
| `serverinfo()` | cvar layer | Serverinfo buffer access (legacy `SV_Serverinfo`); becomes part of the cvar-observer seam. |
| Trace/query surface (internal to engine) | sv peers; Chunk 8 (save: `sv_save.c` uses `SV_Move`/`SV_PointContents`/`SV_SetLightStyle`) | `SV_Move`, `SV_MoveNoEnts`, `SV_ClipMoveToEntity` (per-entity hull clip; the game-DLL-facing `pfnTraceHull` wraps it), `SV_PointContents`, `SV_LinkEdict`, `SV_LightForEntity`, `SV_SetLightStyle`, lightstyles. The shared-PM path's only server dependency is the `SV_ClipPMoveToEntity` seam above. |
| `IMasterListConfig` impl | networking master_list | Heartbeat gating (`public_server \|\| sv_nat`, `maxclients > 1`), challenge validation, S2M_INFO reply content. |
| `IMapLoaderObserver` impl | map_loader FSM | `on_load_begin`/`on_load_end` around world swaps (host boundary OQ-2). |

Game DLL entry points the server *drives* (the other half of the
interface): the 50 `DLL_FUNCTIONS` + 5 `NEW_DLL_FUNCTIONS` callbacks.
Load-bearing call-order contracts: `GameInit` once at DLL load;
`pfnGetHullBounds` ×4 at game-DLL load fills `host.player_mins/maxs`
(server-written, host-stored — consumed later by `svc_serverdata`,
`CreateBaseline`, and the pmove bridge);
`Spawn/KeyValue` during entity-string parse; `ServerActivate` after
entities spawn but **before** baselines; per-frame `StartFrame` before
entity physics; `AddToFullPack`/`SetupVisibility`/`UpdateClientData`/
`GetWeaponData` per client snapshot — `AddToFullPack` does **both** the
visibility test and the entire `entity_state_t` fill (there is no
engine-side state fill); `SetAbsBox` owns absmin/absmax expansion (engine
has no fallback); `ClientConnect/PutInServer/Disconnect/ClientCommand/
ClientUserInfoChanged` around the client state machine; `CmdStart →
PlayerPreThink → think → PM_Move → touches → PlayerPostThink → CmdEnd`
per usercmd.

## Dependencies (what this module calls)

| Dependency | Used for |
|---|---|
| `xash3dpp_networking` | Netchan per client (setup/transmit/fragments/file fragments, LZSS flag, frag-size callback), `MSG_*` bitbuffer, the delta codec (`MSG_WriteDeltaEntity`, clientdata/weapondata/usercmd/event/movevars deltas, `Delta_Init` per **spawn** — inside `SV_SpawnServer` before the entity-string parse, plus once at game-DLL load — `Delta_AddEncoder`), OOB packets, master_list. |
| `xash3dpp_map_loader` | `WorldData` (via MapLoader FSM) and the MapLoader-owned GameState FSM (host-boundary OQ-2) that invokes the three `exec_*` entry points; PVS queries, clip-hull trace kernel, `hull_for_bsp`/`BoxHull` composition per the Chunk 6 contract; `WorldData::checksum()` → `sv.worldmapCRC`. **New work this chunk adds on top:** PHS (`Mod_CalcPHS` port + the phs path of fat-PVS), `Mod_HeadnodeVisible`, entity leaf caching, rotated-entity transform path, the traversal visited-budget hardening follow-up. Placement is OQ-1. |
| `xash3dpp_filesystem` | Game DLL path resolution (gameinfo `game_dll`), resource lists (`<map>.res`), downloads, logs, ban-list persistence, `.ent` patches. |
| `xash3dpp_cmd_cvar` | ~120 sv_* cvars, host + operator command registration (operator set lives only while the game DLL is loaded), `ICvarObserver` (mask `FCVAR_SERVER` per D3; movevars arrive via the host's FCVAR_MOVEVARS observer / changed-flag unless D3 is amended), `ITrustOracle` implementation, latch/READ_ONLY dances (sv_cheats unlock, sv_maxclients latch). |
| `xash3dpp_host` (core) | `Host_Error` policy, feature flags (`host.features`: ENGINE_QUAKE_COMPATIBLE, ENGINE_PHYSICS_PUSHER_EXT, ENGINE_COMPENSATE_QUAKE_BUG), bugcomp flags (`peoei`, `gsmrf`, `get_game_dir_full` — route through a server-scoped `ICompatPolicy` per Q-12), realtime/frametime. Player hull dims are **not** host state in xash3dpp (host-boundary moves `player_mins/maxs` out): the server fills them from `pfnGetHullBounds` at DLL load and feeds the trace kernel via `WorldLoadOptions::hull_bounds`; Chunk 6 owns them until Chunk 11 takes the pm_shared side. |
| `xash3dpp_memory` | Server pool (edicts + DLL private data), string pool, per-client allocations, snapshot ring. |
| `xash3dpp_platform` | DLL load/unload + `GetProcAddress` (game DLL, per-classname exports), time, `Platform_SetStatus`. |
| `xash3dpp_utilities` | MD5 (challenges, consistency, customizations), CRC32 (map CRC, usercmd checksum via `CRC32_BlockSequence`), Info strings, math/Matrix4x4 (pushers, rotated clips), tokenizer. |
| Game DLL (runtime) | Everything in `DLL_FUNCTIONS`/`NEW_DLL_FUNCTIONS`/`physics_interface_t`. |
| **Not depended on** | `ref/`(renderer), `engine/client` — the legacy server calls a small CL_*/S_*/SCR_* set on listen builds only (dedicated stubs them via `common.h` inlines); xash3dpp models these as an injected listen-server capability interface, absent for the dedicated milestone (OQ-4). |
| Deferred/cross-chunk | Studio hitbox hulls (`Mod_HullForStudio`) and studio bone setup → Chunk 7 content; stubbed behind an option seam this chunk (OQ-2). HPAK custom-resource archive → OQ-3. |

## Owned state

The legacy triple, target-internal in xash3dpp (never externally linked;
the legacy `RENAME_SYMBOL` aliases `sv_`/`svs_`/`SV_DropClient_` exist to
hide these internals from mod frameworks like AMXModX that locate them by
exported symbol name — deliberate symbol hiding, not collisions):

- **`sv` (`server_t`)** — per-level, wiped every spawn: state
  (dead/loading/active), `time` (double, epoch **1.0**), `frametime`,
  `time_residual`, framecount, hostflags, `worldmapCRC`, map name /
  startspot, precache tables (models/sounds/events/generic + flags),
  `model_t*` cache, lightstyles, consistency + resource lists, instanced
  baselines, five sizebufs (datagram, reliable_datagram, multicast,
  signon, spec_datagram), worldmodel, pause/simulating flags, overflow
  counters.
- **`svs` (`server_static_t`)** — persists across maps: `initialized`,
  maxclients, `spawncount`, the client array (`sv_client_t[maxclients]`:
  netchan, frames ring, resource lists, customizations, userinfo/
  physinfo, timing/penalty state, view entities, event queue), snapshot
  ring (`packet_entities` = maxclients × SV_UPDATE_BACKUP × 256 states,
  `next_client_entities` cursor), `baselines[max_edicts]`,
  `static_entities[3096]`, serverinfo/localinfo buffers, 16-entry
  challenge salt, pregenerated bandwidth testpacket, log state, group
  mask/op.
- **`svgame` (`svgame_static_t`)** — game-DLL binding: DLL handle, the
  three function tables, `edicts` array + numEntities, `globals`
  (`globalvars_t`, handed to the DLL by pointer), movevars/oldmovevars,
  the `playermove_t` *pointer* (the object itself is the function-local
  static `gpMove` in `SV_LoadProgs`, sv_game.c:5223), user-message
  registry + in-flight message state, `pushed[256]` stack,
  `interp[MAX_CLIENTS]` unlag snapshots, edict mempool + strings pool.
- **World-interaction statics**: `sv_areanodes[32]` + count, box-hull
  scratch, touch-links semaphore, fat PVS/PHS buffers, `frame_ents`
  snapshot scratch (~2048 `entity_state_t`).
- **Module statics to keep an eye on** (Q-2 no-globals rule forces these
  into the Server impl): monotonic `g_userid`, ban filter lists, str64
  bookkeeping, rcon redirect buffer, the `gpMove`/`gpGlobals`/
  `gpEngfuncs` function-local statics in `SV_LoadProgs`,
  `SV_UPDATE_BACKUP` runtime variable (16 SP / 64 MP — sizes both the
  frames ring and the delta mask).

## Quirks and invariants

The exhaustive catalogues (150+ items, each with file:line) live in the
six deep dives; the boundary-level ones that shape the design:

**Lifecycle ordering (must be reproduced exactly):**

1. Spawn: `spawncount++` → challenge salt regen → `memset(sv)` →
   `sv.time = 1.0` → buffers init → `ss_loading` → world load + map CRC
   (MP blend flag = `maxclients > 1`) → submodel precache (`*N`) → client
   downgrade to cs_connected → `SV_ClearWorld` → entity-string parse
   (precaches only legal during `ss_loading`; late precache = broadcast +
   warning, table overflow = fatal) → activate: `ServerActivate` →
   string-pool switch to dynamic mode → settle physics (SP 2 / MP 8
   frames @ 0.1 s, or 1 × 0.001 s when restoring) → **baselines built
   after settling** → resource + consistency lists → per-client netchan
   clear + `delta_sequence = -1` → `ss_active`. Late precache (outside
   `ss_loading`): broadcast to connected clients for all four index
   tables, but only models/sounds also log the "late precache" console
   warning (events/generic broadcast silently).
2. The game DLL is loaded **once** and survives map changes; unload only
   on engine shutdown / game switch. Operator commands exist only while
   loaded.
3. Changelevel is GameState-FSM-driven; on the landmark path the old
   level's state is captured (`SaveGameState`) **while the old level is
   still live** — before inactivate/deactivate (deactivation frees the
   edicts, so saving later is impossible) — then deactivate → spawn new
   level → `SaveFinish` + `LoadGameState`/`LoadAdjacentEnts` → activate.
   The orchestration (`SV_ChangeLevel`) is server-core even though
   Chunk 8 owns serialization.
4. Frame: fixed-step physics accumulator (`frametime =
   1/(sv_fps − 0.01)` — keep the 0.01 fudge; listen builds only —
   dedicated has no `sv_fps` cvar and always runs one physics step per
   host frame at `host.frametime`); players simulate via usercmds, not
   the entity loop; `svc_time` + clientdata + entities + per-client
   datagram compose each snapshot packet; reliable fan-out happens
   before per-client sends.

**Game DLL bridge:**

<!-- pyml disable-next-line ol-prefix -->
5. Edict lifecycle rules are load-bearing for mods: reuse quarantine
   (`freetime < 2.0 || sv.time − freetime > 0.5`), `serialnumber++` on
   free (EHANDLE invalidation), `SV_FreeEdict` scrubs only a curated
   entvars subset (stale fields persist into reuse by design),
   `SV_InitEdict` centers bone controllers at 0x7F, private-data size is
   rounded up to the next 16-byte multiple (`(cb + 15) & ~15`, Poke646
   workaround), slot 0 = world, 1..maxclients = clients.
6. `string_t` offsets are relative to `pStringBase` and must fit in int;
   static-vs-dynamic pool mode flips at activate; escape processing on
   alloc (`\n\r\t` — wider than GoldSrc's `\n`).
7. User messages: strict Begin/End pairing (`Host_Error` on nesting or
   unregistered), fixed-size mismatch drops the message with a console
   S_ERROR print ("expected N bytes, it written M. Ignored." — no
   Host_Error, but not silent), variable-size patches a reserved word,
   dest clamped then routed through multicast; `MSG_INIT` appends to
   signon during load else becomes reliable-ALL.
8. Bug-compat toggles arrive as host feature bits and `-bugcomp` flags
   (`peoei` broken PEntityOfEntIndex, `gsmrf` GoldSrc message rewrite,
   `get_game_dir_full`) — route via a server `ICompatPolicy` (Q-12), not
   scattered ifs.

**World / snapshots:**

<!-- pyml disable-next-line ol-prefix -->
9. `pfnSetAbsBox` (game) owns absbox expansion; `pfnAddToFullPack` (game)
   owns both per-entity visibility and the whole `entity_state_t` fill.
   The engine's snapshot job is: candidate loop + PVS/PHS mask selection
   (`EF_REQUEST_PHS`), portal-camera merging (`EF_MERGE_VISIBILITY`,
   `SVF_MERGE_VISIBILITY` re-entry), the circular state ring, delta
   emission against acked frames, and baseline selection (instanced →
   best-baseline search → static baseline).
10. Edicts cache leaf **clusters** (24×int32 / 48×int16 union) with a
    headnode fallback on overflow; `pfnCheckVisibility` mutates the edict
    it tests (rolling leaf cache). Multicast dest semantics (PAS = fat
    PVS machinery over PHS rows, radius 8; SP forces fullvis for PAS) and
    the per-client visibility check ("NULL mask → visible") are wire
    behaviour.
11. Trace composition: world clip first, then entity pass with the
    fraction *multiplied* (entity fractions are relative to the
    world-clipped segment); filter order in the clip-links walk is
    observable (owner symmetry, monsterclip, ignoretrans high-byte flag,
    trigger-in-solid-list = fatal, allsolid aborts the walk); hull
    selection thresholds 8/36/36 (HL) vs 3/32 (Quake maps) with the
    point-hull offset asymmetry.
12. Areanodes: depth 4, X/Y splits only, three lists (trigger/solid/
    portal); water volumes are `SOLID_NOT` brushes whose skin is a
    contents value **below** `CONTENTS_EMPTY` (skin < −1, e.g.
    `CONTENTS_WATER` = −3) that *do* link into solid lists for
    `SV_WaterLinks` contents composition — `SOLID_NOT` entities are
    skipped only when `skin >= CONTENTS_EMPTY` (sv_world.c:677; coding
    this as `skin < 0` diverges for skin == −1).

**Clients / security:**

<!-- pyml disable-next-line ol-prefix -->
13. Challenges are stateless (MD5 over ip‖salt‖5-second-window, previous
    window accepted); connect requires protocol 49 exactly; reject sends
    three OOB packets; reconnect matches base-addr + (qport or port);
    slot wipe preserves `physinfo`/`pViewEntity`.
14. Signon delivery contract: `serverdata` (+ delta descriptions +
    movevars + user-message regs + lightstyles) at `new`; resource +
    consistency lists at `sendres`; the **verbatim signon buffer**
    (baselines/static ents/ambients) + setview + signonnum at `spawn`;
    spawncount staleness re-runs `new`.
15. Anti-abuse behaviours are part of parity: cmd-time speedhack window +
    warn/kick counters, usercmd count cap (≥63 = drop), a second
    clc_move in one packet aborts processing of the packet's entire
    remainder (immediate return, not skip-and-continue), voice payload
    > 4096 = drop, userinfo spam penalties, upload size caps, fullupdate
    rate limit. Known-broken
    bits stay broken: `SV_CheckRate` no-op, master-info inverted
    `password` key, rcon plain-strcmp validation (a hardening pass may
    strengthen rcon *behind* a default-off option only).

**Physics:**

<!-- pyml disable-next-line ol-prefix -->
16. Quake-lineage constants and bugs are behavioural contract:
    whole-vector maxvelocity clamp, ClipVelocity snap-to-zero at ±1.0,
    4-bump FlyMove, `SV_AddGravity`'s basevelocity fold, pusher ltime
    clock + ±3600 angle wrap, chase-dir `215.0f` typo, drown
    `dmg<15→10`, friction consumed-then-reset-to-1.0, `pushed[256]`
    without bounds check (rewrite may bound-check + log, but must not
    change behaviour below the cap).
17. The pmove bridge fills `playermove_t` from entvars with exact rules
    (MP forces `onground = -1`, `waterjumptime` ↔ `teleport_time`
    aliasing, pitch = −v_angle/3 on copy-back, physent gather box ±256,
    caps 600/64) and runs unlag rewind/restore around each command.

## Satellite components

Q-11 test applied (≥2 "separate" criteria → separate target):

| Candidate feature | Score (0-5) | Verdict |
|-------------------|-------------|---------|
| Save/restore (`sv_save.c` serialization + save-slot mgmt) | 2 (own on-disk format/state machine; narrow primitive interface `SaveGameState`/`LoadGameState`/`LoadAdjacentEnts`/`ClearSaveDir` once orchestration stays server-core — `SV_ChangeLevel` invokes `ClearSaveDir` on `sv_newunit`) | **Separate target, own chunk** (Chunk 8 `xash3dpp_save`, per plan). Chunk 6 keeps `SV_ChangeLevel` orchestration + the seams (`pSaveData` pass-through, `sv.loadgame` gates, `svc_restore` handshake) and stubs the four save primitives. |
| Game-DLL ABI shim (`sv_game.c` tables + `src/abi/engine_funcs.cpp`) | 0-1 (lives/dies with the server; no own protocol; no extra deps) | **Same target**; source-level isolation only (`src/server/abi/` + the host-boundary OQ-10 `src/abi/` extern-C surface). |
| Server physics (`sv_phys/sv_move/sv_pmove`) | 0 (shares edicts, frame loop, trace composition) | **Same target**. The server-side pmove bridge (`sv_pmove.c`) ships as core Chunk 6; Chunk 11 then covers the client-prediction path (`cl_pmove.c`) and the both-paths determinism test. Note: this narrows implementation-plan.md's Chunk 11 legacy-reference line (which lists `sv_pmove.c`) — update the plan when this spec is accepted. |
| A2S / legacy query responders (`sv_query.c`, `SV_Info`, netinfo) | 1 (own tiny text protocol, but reads live server state and is useless without it) | **Same target** (`query.cpp`), mirroring the master_list-stays-in-networking precedent. |
| Ban filters (`sv_filter.c`) | 1 (own persistence files; trivial state) | **Same target** (`filter.cpp`). |
| Server log (`sv_log.c`) | 0-1 (UDP log via networking OOB; format tied to server events) | **Same target** (`log.cpp`). |
| HPAK custom-resource archive (legacy `engine/common/hpak.c`) | 2-3 (own file format; useful to client too; only touches server via 4 calls) | **Separate small target or content-side module** — decision deferred to OQ-3; Chunk 6 can stub customization upload storage behind an interface. |
| Master-server reporting | — | Already decided (networking boundary Q-11 table): protocol lives in `xash3dpp_networking` `master_list`; server implements `IMasterListConfig`. |

## Known Deviations (intentional; parity-audit reviewed)

- **S4 `EdictArena::free_private`** frees `pvPrivateData` unconditionally;
  legacy gates on `Mem_IsAllocatedExt(svgame.mempool, …)` and silently
  skips foreign pointers (sv_game.c:970). The xash3dpp memory API has no
  ownership probe — `pvPrivateData` must come from `alloc_private`
  (precondition). Revisit if a real mod assigns its own block.
- **S4 arena exhaustion** returns `nullptr` instead of calling
  `Host_Error` directly (Q-5 error model); the game-DLL bridge maps it to
  the host error policy — behaviour identical at the ABI surface.
- **S4 string pool** implements the legacy-Windows-x64 heap-arena path
  only (OQ-6 baseline); the Linux mmap near-module probing is not ported.
  The `physFuncs.pfnAllocString/pfnMakeString/pfnGetString` overrides are
  a deferred S8 seam (physics interface) — tracked, absent until then.

______________________________________________________________________

## As-built reconciliation (2026-07-06)

The shipped subsystem is **`xash3dpp_server`, one CMake target, 30 TUs** across
five source slices (plus the public `Server` facade). The legacy triple
(`sv`/`svs`/`svgame`) folded into a single heap-owned `ServerRuntime`
aggregate reached only through Main-thread entry points — the Q-2 no-globals
rule held, with one documented carve-out (`g_bridge`, below).

| Slice (`src/server/…`) | TUs | Shipped responsibility |
|------------------------|-----|------------------------|
| `abi/` | 4 | `engine_table.cpp` (the 159-slot `enginefuncs_t` shims + `g_bridge`), `edict_arena.cpp` (Q-20 store), `game_dll.cpp` (GiveFnptrsToDll / GetEntityAPI2 negotiation), `string_pool.cpp` (`string_t` arena) |
| `lifecycle/` | 6 | `game_host.cpp` (spawn/activate/deactivate orchestration + `Host_ServerFrame`), `spawn.cpp`, `entity_parse.cpp`, `precache.cpp`, `model_resolver.cpp`, `world_hooks.cpp` |
| `clients/` | 8 | `client_state.cpp`, `net_io.cpp`, `messages.cpp`, `snapshot.cpp` (the delta/PVS/PHS snapshot pipeline), `info_string.cpp`, `query.cpp` (A2S/legacy), `filter.cpp` (bans), `log.cpp` |
| `physics/` | 6 | `physics.cpp` (MOVETYPE dispatch/pushers), `pmove.cpp` + `init_client_move.cpp` + `pm_trace.cpp` + `run_cmd.cpp` (the pmove bridge + ~30 `playermove_t` callbacks), `movevars.cpp` |
| `world/` | 5 | `clip.cpp` (trace composition), `links.cpp` (areanodes), `hulls.cpp`, `contents.cpp`, `light.cpp` |

**Reconciled against the spec:**

- **Edict store (Q-20 / OQ-5, decided):** as-built. `EdictArena`
  (`abi/edict_arena.cpp`) is the single authoritative store — free-list,
  `serialnumber` bump on free, `freetime` grace, curated stale-field reuse,
  16-byte private-data rounding. Engine-internal code reads/writes entvars
  through the **`EntityView`** zero-cost facade
  (`include/.../server/entity_view.hpp`); raw `edict_t*`/`entvars_t`
  access is confined to `abi/`, the pmove bridge, and the (stubbed) Chunk 8
  save serializer, exactly per Q-20. `EntityView` is the shipped `EntityView`
  the north star names — value-semantic accessors that compile to direct
  array loads/stores.
- **Game-DLL bridge (`g_bridge`):** the 159 `enginefuncs_t` slots are plain C
  function pointers that cannot capture state, so — like legacy `svgame` —
  they reach engine state through **one file-scope singleton**
  `EngineBridge *g_bridge` (`abi/engine_table.cpp:54`), carrying the
  `compliance-allow(mutable-global, di-global-ref)` carve-out annotation. It
  is written once at bridge install (`load_progs`), nulled at `unload_progs`,
  Main-thread-only in between. This is the deliberate Q-20 ABI-slot exception,
  not a regression of Q-2.
- **`ILevelChangeExecutor` seam:** the `Server` class **is** an
  `::xash::ILevelChangeExecutor` (`server.hpp`), registered on the MapLoader
  FSM via `MapLoader::set_level_executor` (wired in host
  `engine_context.cpp`). `exec_load_level` runs the full `SV_SpawnServer` →
  `spawn_entities` → `SV_ActivateServer` chain; `exec_load_game` /
  `exec_change_level` are Chunk 8 stubs behind the seam. This resolves the
  original "MapLoader-owned GameState FSM invokes the three `exec_*` entry
  points" contract as-built.
- **Injected deps (Q-4):** `ServerInitParams` takes non-owning
  `CmdCvarContext*` / `Filesystem*` / `MapLoader*` / `NetworkContext*` plus the
  `host_error` hook — a default-constructed (all-null) params yields an inert
  server the scaffold lifecycle test relies on. **Still TODO** (marked in
  `server.hpp`): the `ITrustOracle` seam (cmd_cvar D2), host feature flags +
  `ICompatPolicy` (Q-12), and the frame-rate gate (host OQ-11) — Chunk 7
  backlog, not milestone-blocking.
- **PHS (Q-19 / OQ-1, decided):** shipped in `map_loader` as an immutable
  load-time query module (`PhsTable`); the server consumes it read-only via
  `EngineBridge::phs` (`const PhsTable*`). No PHS build code lives in the
  server. See `map_loader-boundary.md §9`.
- **Stats:** `ServerStats` ships the Tier-1 always-on `frames_run`
  (`std::atomic<uint64_t>`); Tier-2/3 are compile-gated TODOs. `stats()` is the
  documented any-thread read surface (debug-stats seam).
- **OQ-8 milestone trims:** honoured. Voice fan-out, HLTV datagram, bandwidth
  testpacket, NAT punch, and the full A2S responder set carry
  `// XASH3DPP-STUB(chunk6)` markers; `query.cpp` ships the info-reply skeleton.
  The two RNG statics (`s_rng_state` / `s_pm_rng`) are explicit
  `XASH3DPP-STUB(chunk6)` idtech-RNG-parity follow-ups (see Threading).
- **`strnicmp` / `string_view` over-read:** **ABSENT** (see the note under
  Extension axes). All server string comparisons are over NUL-terminated
  C-strings — the `Info_ValueForKey` static-buffer key parser
  (`clients/info_string.cpp`) and the `abi/` slot bodies are C-string
  `strcmp`/`strncpy`, not `string_view.data()` over-reads.

**Deferred to Chunk 7+ (tracked, not milestone-blocking):** the studio-hitbox
trace loop + LRU (OQ-2 geometric core done, trace parity gated on hl.dll
goldens), HPAK custom-resource archive (OQ-3), the listen-server capability
seam (OQ-4, shaped null-only), the Chunk 8 save serializer behind the four
stubbed primitives, and the aspirational cvar/command registration.

______________________________________________________________________

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`. **Server is the centre of
the extension roadmap** — G-1 (MCP) reads server state and marshals mutations
here; G-2 (Game ABI v2) replaces exactly the context-less GoldSrc slots, the
global `pmove_t`, and `gpGlobals` that live in this subsystem; G-3 (debug
thread) reads server snapshots off-Main; and Q-20 (EDICT_STORE) is the
confinement seam all three depend on. These are the most consequential
door-keep verdicts in the whole rewrite.

> **As-built confirmation 2026-07-06.** The verdicts below are not
> forward-looking: the shipped code *keeps every door open by construction*.
> The single load-bearing constraint is the **frozen game-DLL ABI** —
> `eiface.h` / `edict.h` / `progdefs.h` / `pm_defs.h` slot signatures, the
> array-of-edicts representation, and the `gpGlobals` / `pmove_t` shapes are a
> byte-frozen behavioural contract shared with unmodified HL mod binaries and
> must never change. G-2 does not *edit* them — it adds a v2 flavor
> **alongside** them behind the Q-20 seam (one game DLL per process). Full
> threading analysis in `docs/threading-analysis/server-threading.md`.

| Goal / primitive | Applies? | Required seam or door — door-keep verdict |
|------------------|----------|-------------------------------------------|
| **Q-20** edict store confinement | **Yes — headline, ✅ met** | `EdictArena` is the single ABI-exact store; `EntityView` is the zero-cost typed access seam; raw `edict_t*`/`entvars_t` access is confined to `abi/` + pmove bridge + save serializer. This is the seam G-1 reads through and G-2 rebinds behind. **Keep the confinement — it is the whole game.** No new work owed; the compliance rule that pins raw access to those three areas is the guard. |
| **G-1** in-engine MCP service | **Yes — headline, doors OPEN** | Reads: `EntityView` (typed entity surface) + `ServerStats` Tier-1 atomics + the cvar layer are the query substrate — G-1 serves them from **published snapshots (P-2)**, never live sim state. Mutations: marshalled to Main via the P-1 inbox, then executed through the **existing command buffer gated by `ITrustOracle`** (cmd_cvar ships the oracle; the server's `ITrustOracle` answer is a `server.hpp` TODO to wire, cmd_cvar D2). Verdict: **no new seam owed** — the three doors (EntityView reads, command-buffer mutation, ITrustOracle gate) already exist; the owed work is P-2 snapshot publication + wiring the oracle. |
| **G-2** Game ABI v2 | **Yes — headline door-keep, ✅ confined** | The v2 target set is precisely this subsystem's frozen surface: the **context-less `enginefuncs_t` slots** (state via `g_bridge`/`gpGlobals`), the **single global `playermove_t`** (`EngineBridge::pmove`), and the **non-reentrant think/callback model**. The door is kept open by three shipped facts: (1) the edict store sits behind `EntityView` (Q-20) so a v2 arena/handle flavor swaps behind the seam; (2) `EngineBridge` already localises the slot state to one struct — v2 slots carry a context handle to it instead of reaching the global; (3) the pmove working set is a single `playermove_t*` on the bridge, the exact object v2 makes per-player. **Never edit the frozen slots — add a v2 sibling flavor.** No v2 design owed now (needs its own brief, extension-goals §5). |
| **G-3** dedicated debug thread | **Yes — door OPEN, needs P-2** | Off-Main reads of published server snapshots (entity dumps, perf counters). `ServerStats` counters are already `std::atomic` (any-thread read). Live entity/world reads must go through a **published P-2 snapshot**, never a live `EngineBridge`/`ServerRuntime` ref (Main-mutable for its whole lifetime — threading Safe-by-contract rows). The snapshot double-buffer is the bring-up work; a new `ThreadRole` value + the cvar `shared_mutex` retrofit are the shared cost G-3 triggers. |
| **P-1** main-thread inbox | **Yes — door, host-owned drain** | The server has no inbox of its own; it drains at `Host_ServerFrame` (`lifecycle/game_host.cpp`) — the host owns the `RunFrame` pump (host-boundary P-1). G-1/G-3 mutations land in the host inbox and execute on Main inside the frame. Server owes nothing beyond staying Main-only-mutating. |
| **P-2** published-snapshot reads | Partial — **door identified** | The snapshot pipeline (`clients/snapshot.cpp`) already builds per-client `entity_state_t` frames, but those are the **wire** snapshots (delta-compressed, consumed by netchan). A G-1/G-3 introspection snapshot (typed entity/cvar/world dump) is a *new* published surface — the P-2 bring-up work, not yet built. `EntityView` is the value type it publishes. |
| **P-3** context-first, no new file-scope state | **✅ met, one documented exception** | `ServerRuntime` is heap-owned, not static; the module statics the spec flagged (`g_userid`, ban lists, rcon buffer) live inside the runtime. The **one** file-scope mutable global is `g_bridge` — the deliberate ABI-slot carve-out (the 159 C slots have no userdata parameter), annotated `compliance-allow`. The two RNG statics are tracked `XASH3DPP-STUB` follow-ups. |
| **P-4** typed introspection | **✅ door OPEN** | `EntityView` is the shipped typed read surface (the north star names it directly) — no `extern edict_t*` array poke in engine-internal code, no raw `->v.` offset access outside the confined areas. This is the one G-1/G-3/G-4 query layer. |
| **Q-12** compat scope | **Door, deferred** | `ICompatPolicy` for `peoei` / `gsmrf` / `get_game_dir_full` + the `HACKS_RELATED_HLMODS` set (OQ-7 proposal) is a `server.hpp` TODO; `peoei_broken` currently rides in as a plain `ServerInitParams` bool. The policy seam is owed at Chunk 7, not milestone-blocking. |

**Net verdict:** the server owes **no new extension seam** at the milestone —
every G-1/G-2/G-3 door is either open by construction (`EntityView`, Q-20 store,
atomic stats, host-owned P-1 drain) or a *named, not-yet-built* published-snapshot
surface (P-2). The job is **preservation**: keep raw edict access confined, keep
the frozen slots un-edited, keep `g_bridge` the only file-scope global, and wire
the already-designed `ITrustOracle` when G-1 has a consumer.

## Open questions

- **OQ-1 — PHS placement.** ✅ **Decided 2026-07-04 → Q-19
  (PHS_PLACEMENT)**: option (a) — `xash3dpp_map_loader` gains a `phs`
  query module (built at world load, immutable after; reuses the existing
  fat-vis walk; server consumes via the query API only). Byte-parity
  golden fixture against GoldSrc dumps (mod_bmodel.c:3845-3859 parity
  note) gates it. Lands as Chunk 6 scope.
- **OQ-2 — Studio hitbox hulls.** *Geometric core done 2026-07-06 (Chunk 7):*
  `SV_StudioSetupBones` + `pfnGetBonePosition` / `pfnGetAttachment` / `pfnGetModelPtr`
  are unstubbed (the `content` bone solver reached via `IModelResolver::studio_bytes`,
  a lazy `ModelCache`), and the hitbox hull geometry is implemented + tested —
  `content::studio_hitbox_hulls` (`Mod_SetStudioHullPlane`) + the oriented-box trace
  hull (`map_loader BoxHull::set_planes`). *Still open (gated on the hl.dll smoke):*
  the `SV_ClipMoveToEntity` per-hitbox trace loop + `SV_HullForStudioModel` gating
  (trace-size scaling, `sv_clienttrace` scale, player-blend, CS shield-skip) + the
  `pm_trace.cpp` mirror + the 16-entry LRU — their parity needs verbatim-legacy
  trace goldens. The null-provider bbox fallback stays until then (stock HL
  degrades: player hitboxes are boxes).
- **OQ-3 — HPAK.** Where does the custom.hpk archive live
  (content/filesystem/server satellite)? Needed for player-decal
  customizations; the dedicated milestone can stub uploads off
  (`sv_allow_upload 0` semantics) if deferred.
- **OQ-4 — Listen-server capability seam.** Legacy stubs CL_*/S_*/SCR_*
  via `#if XASH_DEDICATED` inlines. xash3dpp: an injected
  `IListenClientHooks` (null for dedicated) covering
  save-preview/decal-list/sound-snapshot/visibility-disable/credits.
  Shape it now, implement null-only.
- **OQ-5 — `entvars_t` internal representation.** ✅ **Decided 2026-07-04
  → Q-20 (EDICT_STORE)**: the ABI-exact edict array is the *single*
  authoritative store (no projection); one arena class owns lifecycle
  (free-list, serialnumbers, freetime grace, stale-field reuse); engine
  internals use zero-cost inline typed accessors, with raw
  `entvars_t`/`edict_t` access confined to the ABI shim, pmove bridge,
  and Chunk 8 save serializer (compliance-scan rule when the scaffold
  lands). Handleization / a future ABI flavor is a post-parity load-time
  choice behind that seam.
- **OQ-6 — 64-bit string pool strategy.** Legacy's mmap-within-±2 GB
  trick is Linux/amd64-only; **legacy Windows x64 — the milestone
  platform — already uses a plain heap arena** with the `SV_MakeString`
  INT-range fallback. So the bounded arena + fallback is the
  legacy-Windows parity baseline; near-module VirtualAlloc probing would
  be *new* behaviour beyond parity (nice-to-have for unpatched 64-bit
  game DLLs that stash raw `char*` into string_t). Decide which to ship.
- **OQ-7 — Compat routing.** Which quirks go behind the server
  `ICompatPolicy` (Q-12) vs stay unconditional: bugcomp flags (peoei,
  gsmrf, get_game_dir_full), `HACKS_RELATED_HLMODS` (landmark-space
  truncation, Invasion "test", ce08_02 origin hack), `ENGINE_*` feature
  bits. Proposal: bugcomp + HLMODS hacks → ICompatPolicy; `ENGINE_*`
  feature bits stay host-feature-driven (they are game-requested, not
  compat).
- **OQ-8 — Milestone scope trims.** The dedicated milestone ("loads
  hl.dll, runs a single map frame") does not need: voice fan-out, HLTV
  spectator datagram, bandwidth testpacket, NAT punch, A2S responders,
  enttools. All are parity-required *eventually*; propose implementing
  the state/wire scaffolding (flags, buffers) now but allowing stub
  behaviour behind explicit `// XASH3DPP-STUB(chunk6)` markers tracked in
  the implementation plan, so finish-subsystem can gate on the list.
- **OQ-9 — Threading posture.** Legacy is single-threaded throughout;
  `Mod_CalcPHS` uses OpenMP. The snapshot scratch (`frame_ents`), fat-vis
  buffers, and the pmove static are single-thread assumptions. Spec
  assumption: all server entry points main-thread (matches host
  boundary); PHS build may parallelize internally at load time only.
  `/analyse-threading` pass due after implementation.
