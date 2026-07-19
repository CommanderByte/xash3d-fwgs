# Deep dive: server lifecycle — `sv_main.c`, `sv_init.c`, `sv_cmds.c`

*Chunk 6 recon, 2026-07-04. Narrow-and-exact behavioural reference for the
xash3dpp server rewrite. All paths relative to the repository root; line
numbers verified against the current tree.*

> **Refreshed 2026-07-06 (as-built cross-ref).** This recon shipped as
> Chunk 6 (Complete): the lifecycle bookends map to `src/server/lifecycle/`
> (`game_host.cpp` = `SV_Init`/`SV_Shutdown` + the `Host_ServerFrame` loop,
> `spawn.cpp` + `entity_parse.cpp` + `precache.cpp` = `SV_SpawnServer` →
> entity-string parse → `SV_ActivateServer`), driven by the map_loader FSM
> through `Server::exec_load_level` (the `ILevelChangeExecutor` seam,
> `server.hpp`). The zero-physics-frames early-return quirk and the spawn
> ordering (`sv.time = 1.0` epoch, `initialized` set early, baselines after
> settling) are reproduced as-built. As-built reconciliation + door-keep
> verdicts: `docs/boundaries/server-boundary.md` §As-built / §Extension axes.
> This narrative recon is **not** superseded — it remains the file:line quirk
> catalogue behind the shipped code.

## 1. Responsibility

- **engine/server/sv_main.c** — Server per-frame driver and lifecycle
  bookends: defines ~120 sv_* cvars, `SV_Init`/`SV_Shutdown`, the
  `Host_ServerFrame` loop (packet read → movevars → timeouts → physics →
  send → heartbeat), master-server info reply, useragent/input-device
  gating, final-message broadcast, timing/`sv_fps` decoupling.
- **engine/server/sv_init.c** — Server spawn/activate/deactivate state
  machine: owns the `sv`/`svs`/`svgame` globals, precache index registries
  (model/sound/event/generic), resource-list + baseline construction,
  `SV_SpawnServer`/`SV_ActivateServer`/`SV_DeactivateServer`, game-DLL init
  (`SV_InitGame`), maxclients (re)allocation, bandwidth test packet, and the
  three host-state entry points (`SV_ExecLoadLevel/LoadGame/ChangeLevel`).
- **engine/server/sv_cmds.c** — Console command surface: map/newgame/save/
  load/changelevel launchers (host commands, always available), operator
  commands (kick/status/serverinfo/...) registered per game-DLL load, plus
  text-broadcast helpers `SV_ClientPrintf`/`SV_BroadcastPrintf`/
  `SV_BroadcastCommand`.

## 2. External surface (called from outside engine/server/)

Declared in `engine/common/common.h` (:619-790) unless noted:

| Function | Signature | Callers (outside engine/server/) | Purpose |
|---|---|---|---|
| `SV_Init` | `void SV_Init(void)` | `common/host.c:1222` (Host_Main init) | One-time startup: cvars, netmsg buffer, filter, loads game DLL |
| `SV_Shutdown` | `void SV_Shutdown(const char *finalmsg)` | `common/host.c:317,736,1356` (Host_ShutdownServer / Host_Error / Host_Shutdown); `client/cl_main.c:1502` (connecting to remote server); `client/cl_demo.c:723` (demo playback start) | Full server teardown |
| `Host_ServerFrame` | `void Host_ServerFrame(void)` | `common/host.c:674` (`Host_Frame`, between Host_GetCommands and Host_ClientFrame) | Per-frame server tick |
| `SV_Active` | `qboolean SV_Active(void)` (`sv.state != ss_dead`) | ~20 sites: host.c, host_state.c, cvar.c:581 (FCVAR_LATCH gating), net_chan.c:1774, mod_bmodel.c:4354, cl_main.c, cl_demo.c, console.c:960, dll_int/cl_pmove.c:229, parse/cl_parse.c:2666 | "map loaded" predicate |
| `SV_Initialized` | `qboolean SV_Initialized(void)` (`svs.initialized`) | `common/common.h:871` (`Host_IsLocalClient` inline) | "SV_SpawnServer has run" predicate |
| `SV_GetMaxClients` | `int SV_GetMaxClients(void)` | `common.h:758` (`CL_GetMaxClients` inline), `common.h:863-864` (`Host_IsSinglePlayerGame`), `common/cmd.c:268,1294,1382` (privileged-command / stufftext filtering) | maxclients accessor |
| `SV_ShutdownGame` | `void SV_ShutdownGame(void)` | `common/host_state.c:66,88,137` (COM_NewGame/COM_LoadLevel/Host_ShutdownGame) | Graceful game exit (final reconnect msg, deactivate) |
| `SV_ExecLoadLevel` | `void SV_ExecLoadLevel(void)` | `common/host_state.c:209` (COM_Frame, STATE_LOAD_LEVEL) | State-machine map load |
| `SV_ExecLoadGame` | `void SV_ExecLoadGame(void)` | `common/host_state.c:213` (STATE_LOAD_GAME) | State-machine savegame load |
| `SV_ExecChangeLevel` | `void SV_ExecChangeLevel(void)` | `common/host_state.c:217` (STATE_CHANGELEVEL) | State-machine changelevel |
| `SV_BroadcastCommand` | `void SV_BroadcastCommand(const char *fmt, ...)` (common.h:769) | `common/cvar.c:150` (fullserverinfo rebroadcast on FCVAR_SERVER cvar change) | svc_stufftext to all |
| `SV_BroadcastPrintf` | `void SV_BroadcastPrintf(struct sv_client_s *ignore, const char *fmt, ...)` (common.h:770) | `common/cvar.c:171,176` (announce FCVAR_SERVER cvar change) | svc_print to all spawned |

**Globals reached from outside engine/server/:** `svs.maxclients` read
directly by `common/masterlist.c:245` (skip heartbeat if
`(!public_server.value && !sv_nat.value) || svs.maxclients == 1`) and
`common/mod_bmodel.c:4354`. Cvars `public_server`, `sv_nat`
(masterlist.c:245), `sv_lan`, `sv_lan_rate` (net_chan.c:1774-1775, extern in
`common/netchan.h:248-249`), `sv_cheats` (net_ws.c:1132 gates net_fakelag).
`svgame` is read by common/ (model.c physFuncs hooks, mod_studio.c blending
iface, lib_common.c, dedicated.c); its definition is sv_init.c:28.

## 3. Intra-subsystem surface

Key exports consumed by other engine/server/*.c:

- `SV_InitGame(qboolean silent)` — sv_init.c:731. Loads game DLL once
  (`if (svgame.hInstance) return true` — DLL is **kept loaded across maps**,
  sv_init.c:735-736). Called by SV_Init (sv_main.c:1015), SV_SpawnServer
  (sv_init.c:943), sv_save.c:2145 (save loading).
- `SV_SpawnServer(mapname, startspot, background)` — sv_init.c:935. Called
  by SV_ExecLoadLevel/LoadGame (sv_init.c:1106,1122) and SV_ChangeLevel
  (sv_save.c:2094).
- `SV_ActivateServer(int runPhysics)` — sv_init.c:579. Called by
  SV_ExecLoadLevel (runPhysics=1), SV_ExecLoadGame (0), SV_ChangeLevel
  (sv_save.c:2108 loadsave→0, :2115 fresh→1).
- `SV_DeactivateServer()` — sv_init.c:682. Called by SV_Shutdown,
  SV_ShutdownGame, SV_ChangeLevel (sv_save.c:2092), sv_game.c:5178
  (SV_UnloadProgs).
- `SV_FinalMessage(msg, reconnect)` — sv_main.c:1028. Called by SV_Shutdown,
  SV_ShutdownGame, sv_save.c:2091, sv_frame.c:652 (delta-outdated restart).
- `SV_ModelIndex/SV_SoundIndex/SV_EventIndex/SV_GenericIndex` —
  sv_init.c:103/148/199/241. Called from sv_game.c pfnPrecache* /
  pfnSetModel / SV_BuildSoundMsg (sv_game.c:242,1298,2013,4017).
- `SV_FreeOldEntities` — sv_init.c:554, also sv_save.c:1917.
- `SV_UpdateMovevars(initialize)` — sv_main.c:189; per-frame (sv_main.c:700)
  and at spawn (sv_init.c:1070).
- `SV_ClientPrintf/SV_BroadcastPrintf` — sv_cmds.c:26/49; used pervasively
  (sv_client.c, sv_game.c:3608,3627, sv_filter.c:443, sv_custom.c:186,
  sv_frame.c:866).
- `SV_InitOperatorCommands`/`SV_KillOperatorCommands` — sv_cmds.c:1053/1093;
  registered in SV_LoadProgs (sv_game.c:5325), removed in SV_UnloadProgs
  (sv_game.c:5198).
- `SV_AddToMaster`, `SV_ProcessUserAgent` — sv_main.c:730/787; called from
  sv_client.c:3211 (connectionless "s" packet) and :344 (connect handshake).
- `SV_FreeTestPacket` — sv_init.c:837; SV_Shutdown (sv_main.c:1142).

### Map-load / changelevel sequence

**Fresh map (`map foo`):** `SV_Map_f` (sv_cmds.c:199) → validate →
`Cvar_DirectSet(sv_hostmap)` → `COM_LoadLevel` (host_state.c:69) → sets
`GameState->nextstate = STATE_LOAD_LEVEL` + `SV_ShutdownGame()` → COM_Frame
loop: `Host_RunFrame` sees nextstate → SCR_BeginLoadingPlaque →
STATE_GAME_SHUTDOWN → `Host_ShutdownGame` → `SV_ShutdownGame` again →
STATE_LOAD_LEVEL → **`SV_ExecLoadLevel`** (sv_init.c:1103):

1. `SV_SetStringArrayMode(false)` — static string array *before* spawn.
2. **`SV_SpawnServer`** (sv_init.c:935), in order: `SV_SetupClients`
   (realloc if maxclients changed; full `SV_Shutdown` on change,
   sv_init.c:801-802) → `SV_InitGame(false)` → `Delta_Init()` → unlock
   sv_cheats READ_ONLY bit (:949) → `svs.initialized = true`, `Log_Open` →
   `svs.spawncount++` (:957, restarts partially-connected clients) →
   regenerate 16 `challenge_salt` randoms (:959-960) → queue
   `exec mapchangecfgfile` + `exec maps/<map>_load.cfg` (:962-967) → default
   hostname from `pfnGetGameDescription()` (:970-971) → `memset(&sv, 0)`,
   **`sv.time = globals->time = 1.0`** (:982-983) → MSG_Init 5 buffers
   (signon/multicast/datagram/reliable/spec) → clear static_entities +
   baselines → coop⇒deathmatch=0, skill rounded & clamped [0,3] (:998-1001)
   → `HPAK_CheckSize` → copy gamemode into globals → force
   sv_background/cl_background READ_ONLY (:1016-1017) → `sv.name` = stripped
   mapname → **`Host_SetServerState(ss_loading)`** (:1027) →
   `model_precache[1] = maps/<name>.bsp` + `Mod_LoadWorld` +
   `CRC32_MapFile(&sv.worldmapCRC, ..., maxclients > 1)` (:1033-1036) →
   Quake progsCRC (:1038-1044) → submodels precached as `"*%i"`
   (:1046-1051) → client slots: state > cs_connected downgraded to
   cs_connected, edicts assigned + `SV_InitEdict` (:1054-1064) →
   `NET_MasterClear()` → `SV_UpdateMovevars(true)` → `SV_ClearWorld()` →
   `SV_GenerateTestPacket()`.
3. `SV_SpawnEntities` (sv_game.c:5136) — entity string parse, game-DLL spawn
   callbacks, `ss_loading` still in effect so precaches are silent.
4. **`SV_ActivateServer(true)`** (sv_init.c:579), in order: clear
   `sv_newunit` → `SV_FreeOldEntities` → `globals->time = sv.time`;
   **`pfnServerActivate(edicts, numEntities, maxclients)`** (:599) →
   `SV_SetStringArrayMode(true)` (dynamic string array *after* activate,
   :601) → `SV_CreateGenericResources` (`<map>.res`, `reslist.txt`, used
   wads) → run settle physics: runPhysics ? (SP 2 frames : MP 8 frames) @
   `SV_SPAWN_TIME`=0.1 : 1 frame @ 0.001s (:606-619) → **`SV_CreateBaseline`**
   (after settle frames — baselines capture post-settle state) →
   `SV_CreateResourceList` → `SV_TransferConsistencyInfo` → per connected
   client: `Netchan_Clear`, `delta_sequence = -1`, bump `connection_started`
   (:631-641) → zero `oldmovevars`, `globals->changelevel = false` →
   `hostflags = 0` → `HPAK_FlushHostQueue` → dedicated: `Mod_FreeUnused` →
   `movevars_changed = true` → **`Host_SetServerState(ss_active)`** (:664).

**Changelevel:** game DLL `pfnChangeLevel` → `SV_QueueChangeLevel`
(sv_game.c:721) or console `changelevel[2]` (sv_cmds.c:539/557) →
`COM_ChangeLevel` (host_state.c:109; landmark present ⇒ `loadGame = true`)
→ STATE_CHANGELEVEL → `SV_ExecChangeLevel` (sv_init.c:1137) →
`SV_ChangeLevel` (sv_save.c:2049): save transition data →
`SV_FinalMessage("", true)` (reconnect) → `SV_DeactivateServer` →
`SV_SpawnServer(level, startspot, background)` → landmark path:
`SV_LoadGameState` + `SV_ActivateServer(false)`; classic path:
`SV_SpawnEntities` + `SV_ActivateServer(true)`.

**Deactivate** (sv_init.c:682): queue `exec disconcfgfile` +
`exec maps/<map>_unload.cfg` → `SV_InactivateClients` →
`globals->time = sv.time`; `pfnServerDeactivate()` →
`Host_SetServerState(ss_dead)` → `SV_FreeEdicts` → `PM_ClearPhysEnts` →
`SV_EmptyStringPool(true)` + empty stringspool → free per-client frames →
reset globals (`numEntities = maxclients + 1`, mapname/startspot = 0).

## 4. Dependencies (called from these three files)

- **Host:** `Host_Error`, `Host_IsDedicated`, `Host_IsSinglePlayerGame`,
  `Host_EndGame`, `host.realtime/frametime/features/movevars_changed/rd/
  type/mempool/draw_decals/player_mins`, `host_developer`,
  `host_limitlocal`, `GameState` + `COM_LoadLevel/COM_NewGame`
  (host_state.c), `Host_SetServerState` → `host_serverstate` cvar.
- **Cvar:** `Cvar_RegisterVariable`, `Cvar_Get/Getf`, `Cvar_FullSet`,
  `Cvar_DirectSet`, `Cvar_DirectFullSet`, `Cvar_SetValue/Set`,
  `Cvar_VariableString/Integer`, `Cvar_FindVar` (+ direct `var->string`
  mutation in SV_ServerInfo_f, sv_cmds.c:794-800).
- **Cmd/Cbuf:** `Cmd_AddCommand`, `Cmd_AddRestrictedCommand`,
  `Cmd_RemoveCommand`, `Cmd_Argc/Argv/Args`, `Cmd_ForwardToServer`,
  `Cmd_ListMaps`, `Cbuf_AddText(f)`.
- **NET/Netchan:** `NET_GetPacket`, `NET_SendPacket`, `NET_Config`,
  `NET_CompareBaseAdr`, `NET_IsLocalAddress`, `NET_BaseAdrToString/
  AdrToString`, `NET_MasterHeartbeat`, `NET_MasterClear`,
  `NET_MasterShutdown`, `NET_GetMaster`, `net_clockwindow`;
  `Netchan_Process`, `Netchan_IncomingReady`,
  `Netchan_CopyNormalFragments/CopyFileFragments`, `Netchan_TransmitBits`,
  `Netchan_Clear`.
- **MSG / delta (net_buffer/net_encode):** `MSG_Init/Clear/ReadLong/
  ReadShort/ReadDword/WriteString/WriteOneBit/WriteLong/WriteDword/
  WriteByte/WriteUBitLong/BeginServerCmd/GetData/...`,
  `MSG_WriteDeltaMovevars`, `MSG_WriteDeltaEntity`, `Delta_Init`.
- **Model/world:** `Mod_LoadWorld`, `Mod_ForName`, `Mod_FreeAll`,
  `Mod_FreeUnused`, `world.wadcount/wadlist`, `SV_ClearWorld` (sv_world.c).
- **FS:** `FS_LoadFile`, `FS_FileSize`, `FS_FileExists`,
  `FS_Open/Read/Seek/Close/FileLength`, `FS_Search`, `FS_Delete`,
  `FS_Title`.
- **COM/crt:** `COM_ParseFile`, `COM_FixSlashes`, `COM_StripExtension`,
  `COM_FileExtension/FileBase`, `COM_ReplaceExtension`, `COM_HexConvert`,
  `COM_RandomLong`, `COM_IsSafeFileToDownload`, `COM_CreateCustomization`,
  `COM_GetCommonLibraryPath`/`COM_ResetLibraryError`/`COM_GetLibraryError`
  (library.h), `CRC32_MapFile`, `CRC32_ProcessByte`,
  `Q_buildos/buildarch/buildnum`, `Info_*` string family.
- **Log:** `Log_Open/Close/Printf/PrintServerVars` (sv_log.c).
- **HPAK:** `HPAK_AddLump`, `HPAK_FlushHostQueue`, `HPAK_CheckSize`,
  `hpk_custom_file`.
- **Client (listen-server coupling):** `CL_Active`, `CL_IsInGame`,
  `CL_IsInConsole`, `CL_Drop`, `CL_IsPlaybackDemo`, `CL_StopPlayback`,
  `CL_HudMessage`, `S_StopBackgroundTrack`, `S_StopAllSounds`, `SCR_*`
  (indirect), `con_gamemaps`.
- **Platform:** `Platform_DoubleTime`, `Platform_SetStatus`.
- **Sound util:** `Sound_SupportedFileFormat`.
- **Voice:** `VOICE_DEFAULT_CODEC` (voice.h).
- **Master:** via NET_Master* + `SV_AddToMaster` info reply (S2M_INFO
  header, protocol.h).

## 5. Owned state

Defined in sv_init.c:24-28:

- `server_t sv` — per-level state (wiped each SV_SpawnServer): `state`
  (ss_dead/loading/active), `background`, `loadgame`, `time` (double,
  starts at 1.0), `time_residual`, `frametime`, `current_client`,
  `hostflags`, `worldmapCRC`, `progsCRC`, `name`, `startspot`, precache
  tables (`model_precache[MAX_MODELS][MAX_QPATH]` + flags,
  `sound_precache[MAX_SOUNDS]`, `files_precache[MAX_CUSTOM]`,
  `event_precache[MAX_EVENTS]`), `models[]`, lightstyles,
  `consistency_list`/`resources` + counts,
  `instanced[MAX_CUSTOM_BASELINES]`/`num_instanced`/`last_valid_baseline`,
  5 sizebufs (datagram, reliable_datagram, multicast, signon[MAX_INIT_MSG],
  spec_datagram), `worldmodel`, `playersonly`, `simulating`, `paused`,
  overflow counters (server.h:115-185).
- `server_static_t svs` — persists across maps: `initialized`, `timestart`,
  `maxclients`, `groupmask/groupop`, `log`,
  `serverinfo[MAX_SERVERINFO_STRING]`, `localinfo[32768]`, `spawncount`,
  `clients` (heap), `num/next_client_entities`, `packet_entities` (heap,
  `maxclients * SV_UPDATE_BACKUP * NUM_PACKET_ENTITIES`), `baselines`
  ([GI->max_edicts]), `static_entities`, `challenge_salt[16]`, testpacket
  state (server.h:339-371).
- `svgame_static_t svgame` — game-DLL binding: hInstance, dllFuncs/
  dllFuncs2/physFuncs, edicts/numEntities, globals, movevars/oldmovevars,
  pmove, msg registry, pools (server.h:301-337).
- `int SV_UPDATE_BACKUP` (sv_init.c:24, mutable global; SINGLEPLAYER_BACKUP
  or MULTIPLAYER_BACKUP set in SV_SetupClients:822).
- Statics: `lastreset` (SV_CheckCmdTimes, sv_main.c:252), `lasttime`
  (SV_UpdateStatusLine:638).

**Cvars registered (sv_main.c), grouped:**

- *Networking/rates:* sv_lan(0), sv_lan_rate(20000), sv_nat(0),
  sv_minrate(5000), sv_maxrate(0), sv_minupdaterate(10),
  sv_maxupdaterate(60), sv_failuretime(0.5), sv_timeout(65),
  sv_connect_timeout(60), sv_connect_timeout_ban(1),
  sv_connect_timeout_ban_time(2), sv_allow_testpacket(1),
  sv_log_outofband(0).
- *Auth/admin:* rcon_password(""), rcon_enable(1), sv_password(""),
  sv_cheats(0), public_server("public",0), sv_expose_player_list(1),
  sv_speedhack_kick(10), sv_userinfo_* penalty family,
  sv_fullupdate_penalty_time(1).
- *Resources/downloads:* sv_allow_upload(1,"sv_allowupload"),
  sv_allow_download(1,"sv_allowdownload"), sv_allow_dlfile(1, **no-op
  compat**), sv_uploadmax(0.5), sv_upload_penalty_time(60),
  sv_downloadurl(""), sv_send_logos(1), sv_send_resources(1),
  sv_consistency("mp_consistency",1), sv_instancedbaseline(1).
- *Gameplay/mode:* deathmatch(0), coop(0), teamplay(0), skill(1), temp1(0),
  sv_aim(1), sv_allow_autoaim(0, HL25 compat), sv_unlag(1),
  sv_maxunlag(0.5), sv_unlagpush(0), sv_unlagsamples(1), sv_clienttrace(1),
  sv_newunit(0), sv_autosave(1), sv_maxclients("maxplayers",1,LATCH),
  sv_pausable("pausable",1), sv_hostmap("hostmap", default GI->startmap),
  sv_background_freeze(1), _sv_override_scientist_mdl("").
- *Movevars (FCVAR_MOVEVARS):* sv_gravity(800), sv_stopspeed(100),
  sv_maxspeed(320), sv_spectatormaxspeed(500), sv_accelerate(10),
  sv_airaccelerate(10), sv_wateraccelerate(10), sv_friction(4),
  sv_edgefriction("edgefriction",2), sv_waterfriction(1), sv_bounce(1),
  sv_stepsize(18), sv_maxvelocity(2000), sv_zmax(4096), sv_wateramp(0),
  sv_footsteps("mp_footsteps",1), sv_skyname("desert"), sv_rollangle(0),
  sv_rollspeed(200), sv_skycolor_r/g/b(0), sv_skyvec_x/y/z(0),
  sv_wateralpha(1).
- *Logging:* mp_logecho(1), mp_logfile(1), sv_log_singleplayer(0),
  sv_log_onefile(0), logsdir("logs"), sv_trace_messages(0,LATCH).
- *Files:* mapcyclefile("mapcycle.txt"), motdfile("motd.txt"),
  bannedcfgfile("banned.cfg"), listipcfgfile("listip.cfg"),
  mapchangecfgfile(""), disconcfgfile("").
- *Voice:* sv_voiceenable(1), sv_voicequality(3), sv_voice_singleplayer(0).
- *Gore:* violence_hblood/ablood/hgibs/agibs(1).
- *Enttools:* sv_enttools_enable(0), sv_enttools_maxfire(5).
- *Input gating:* sv_allow_joystick/mouse/touch/vr/noinputdevices(1).
- *Misc:* sv_fps(0.0, **not registered on dedicated**), hostname(""),
  sv_version(READ_ONLY, build string), sv_novis(0), sv_check_errors(0),
  sv_validate_changelevel(0), showtriggers(0,LATCH|TEMPORARY),
  sv_airmove(1, obsolete), sv_contact(""), sv_master_response_timeout(4),
  sv_allow_PhysX/sv_precache_meshes (mod compat, Cvar_Get),
  suitvolume(0.25), protocol (READ_ONLY = PROTOCOL_VERSION), gamedir
  (READ_ONLY), sv_alltalk(legacy), servercfgfile/lservercfgfile.

## 6. Quirks & invariants (exhaustive)

**Timing / fps:**

- `sv.time` starts at **1.0** every spawn, not 0 (sv_init.c:983, "server
  spawn time it's always 1.0 second").
- sv_fps decoupling: frame length is `1.0 / (sv_fps - 0.01)` — deliberate
  0.01 subtraction "FP issues" (sv_main.c:611). Residual accumulator loop;
  `SV_RunGameFrame` returns false if zero frames ran, which makes
  `Host_ServerFrame` **return early, skipping SV_SendClientMessages,
  SV_PrepWorldFrame and NET_MasterHeartbeat** for that host frame
  (sv_main.c:709).
- `sv.time_residual` accumulates only when
  `sv.simulating || sv.state != ss_active` (sv_main.c:686-687); with
  sv_fps==0, `sv.frametime = host.frametime` every frame (:689-690).
- sv_fps clamped [MIN_FPS=20, MAX_FPS_HARD=1000] (sv_main.c:256-263;
  constants common.h:105-107). sv_fps cvar registered only
  `#if !XASH_DEDICATED` (sv_main.c:893-895) — dedicated always runs at host
  frametime.
- `svgame.globals->frametime/time` are re-stamped after every
  `SV_ExecuteClientMessage` (sv_main.c:425-426, 444-445) because client
  message handlers may run game code with different globals.
- Cmd-time anti-speedhack window: reset check runs at most once per 1.0s
  realtime (static `lastreset`, sv_main.c:268-271); skipped entirely in
  single-player (:265); positive drift > net_clockwindow sets
  `ignorecmdtime = net_clockwindow + realtime` penalty, negative drift only
  resnaps cmdtime (:283-294).

**State transitions & ordering:**

- `sv.state` transitions: memset(ss_dead) → `ss_loading` set in
  SV_SpawnServer **before** world load/precaches (sv_init.c:1027) →
  `ss_active` at end of SV_ActivateServer (:664) → `ss_dead` in
  SV_DeactivateServer (:700). Mirrored into read-only cvar
  `host_serverstate` (sv_init.c:35-39).
- Precache indexes registered while `sv.state != ss_loading` trigger an
  **immediate svc_resource broadcast + "late precache" warning** instead of
  being load-time (sv_init.c:131-137, 182-187, 225-229, 267-271). Sentence
  names (`!name`) are rejected from sound precache (:156-159). Leading `/`
  or `\` stripped from model/sound names (:111-112, 162-163). Dedup is
  case-insensitive `Q_stricmp`. Overflow of any table is a hard
  `Host_Error`.
- `svs.spawncount++` happens in SV_SpawnServer (sv_init.c:957) *before*
  clients are downgraded to cs_connected — late spawns from the previous
  count are detectable.
- Challenge salt (16 random uints) regenerated **per spawn**
  (sv_init.c:959-960) — outstanding challenges die on map change.
- World CRC: `CRC32_MapFile(&sv.worldmapCRC, ..., svs.maxclients > 1)` — the
  "blend" third arg only in multiplayer (sv_init.c:1036). Quake-compat
  `progsCRC` read from progs.dat at offset sizeof(int) (:1038-1044).
- Baselines built in SV_ActivateServer **after** ServerActivate and after
  settle physics frames (SP=2/MP=8 frames of 0.1s when runPhysics, else one
  0.001s frame; sv_init.c:606-622) — baseline captures the settled state.
  Voice codec (svc_voiceinit) written into signon *before* baselines if MP
  or sv_voice_singleplayer (:466-467). Player entities
  (0 < entnum <= maxclients) use DELTA_PLAYER; others skipped when
  `modelindex == 0` ("invisible"); FL_CUSTOMENTITY ⇒ ENTITY_BEAM
  (:482-501). Baseline stream terminated by `LAST_EDICT` in MAX_ENTITY_BITS
  then `num_instanced` in **6 bits** (:537-538).
- String-pool mode dance: `SV_SetStringArrayMode(false)` before
  SV_SpawnServer in ExecLoadLevel (sv_init.c:1105),
  `SV_SetStringArrayMode(true)` inside SV_ActivateServer right after
  pfnServerActivate (:601). Note ExecLoadGame does **not** call
  SetStringArrayMode(false).
- Game DLL loaded once and kept across map changes (sv_init.c:735-736);
  operator commands exist only while DLL loaded (sv_game.c:5325/5198).
- SV_DeactivateServer queues `exec disconcfgfile` and
  `exec maps/<map>_unload.cfg` **before** the `!svs.initialized || ss_dead`
  early-return (sv_init.c:685-694) — cfg exec is queued even when the rest
  of deactivation is skipped.
- SV_SetupClients only acts when maxclients actually changed; a change
  triggers **full SV_Shutdown** (sv_init.c:801-802). Dedicated clamps
  maxclients to [4, MAX_CLIENTS], listen [1, MAX_CLIENTS] (:808-810).
  maxclients==1 forces deathmatch=0 else deathmatch=1, then coop forces
  deathmatch=0 (:812-817). "maxplayers" written back with FCVAR_LATCH
  (:820). `FCVAR_CHANGED` cleared on sv_maxclients (:833).
- `sv_clienttrace` forced to 1 for SP **twice** — duplicated code lines
  (sv_init.c:1007-1008 and 1019-1020).
- Client slots on respawn: any state > cs_connected downgraded to
  cs_connected ("needs to reconnect"), edict = SV_EdictNum(i+1),
  pViewEntity = NULL (sv_init.c:1054-1064).
- SV_ActivateServer bumps every connected client's `connection_started` and
  sets `delta_sequence = -1` + Netchan_Clear (sv_init.c:631-641) — prevents
  timeout during load and forces uncompressed first delta.
- `sv_newunit` cleared at every activate (sv_init.c:592);
  `svgame.globals->changelevel = false` (:645); `sv.hostflags = 0` (:648).
- SV_FreeOldEntities only scans from `svs.maxclients + 1` (players never
  FL_KILLME-reaped) and shrinks numEntities from the top while trailing
  edicts are free (sv_init.c:560-569).

**Pausing / simulation:**

- `SV_IsSimulating` (sv_main.c:572-595): dedicated always simulates;
  background map simulates while `SV_Active && CL_Active` unless console is
  open; no spawned players ⇒ no simulation; SP + `sv.playersonly` ⇒ frozen;
  otherwise `!sv.paused && CL_IsInGame()`.
- MP auto-unpause when last non-spectator/fake player leaves
  (sv_main.c:535-539). `playersonly` requires sv_cheats and is XOR-toggled
  (sv_cmds.c:908-915).

**Networking / packets:**

- Connectionless packets need >= 4 bytes and `*(int *)data == -1`
  (sv_main.c:386).
- qport read from bytes 8-9 of the sequenced header to survive NAT
  rewrites; if base address matches but port changed, the client's stored
  port is updated (sv_main.c:392-414).
- `FCL_SEND_NET_MESSAGE` set only when
  `(maxclients == 1 && !host_limitlocal) || state != cs_spawned`
  (sv_main.c:418-419, 437-438) — spawned MP clients reply on the send
  schedule instead.
- Message processed only if `cl->frames != NULL && state != cs_zombie`
  (sv_main.c:422, 441). Fragment reassembly is handled *after* the normal
  payload of the same packet (:431-453); file fragments feed
  SV_ProcessFile.
- `sv.current_client` is set during the read loop and NULLed after
  (sv_main.c:400, 461).
- Customization uploads: filename must start with `!`; MD5 parsed from
  chars [4..36) of the filename (sv_main.c:313-319); size must equal the
  advertised `nDownloadSize` (:335-339); stored via HPAK; duplicate MD5
  silently ignored (:348-365).
- Timeouts: zombie state freed immediately with "FIXME: get rid of the
  zombie state" (sv_main.c:511-514); connecting clients dropped after
  sv_connect_timeout (60s) with optional automatic `addip <mins> <ip>` ban
  (:464-474, 519-520); spawned clients after sv_timeout (65s) of netchan
  silence; **local addresses never time out** (:517, 524).
- SV_FinalMessage: SP reconnect uses `svc_changing` + one bit
  `GameState->loadGame`; MP uses SV_BuildReconnect; the message is
  transmitted **twice**, staggered, direct Netchan_TransmitBits
  (sv_main.c:1043-1065).
- Master info reply (sv_main.c:730-778): validates challenge2 against
  stored heartbeat challenge and `sv_master_response_timeout` window;
  hardcoded keys `os="w"` (always Windows), `secure="0"`, `lan="0"`,
  `region="255"`; **`password` key is inverted**: sends "0" when a password
  IS set, "1" when not (:768) — opposite of the connectionless `info` reply
  in sv_client.c:896 which sends "1" when set. Bug-compat candidate;
  preserve as-is.
- Test packet (sv_init.c:857-925): only MP, non-low-memory,
  sv_allow_testpacket; content is the first FRAGMENT_MAX_SIZE bytes of
  `gfx.wad`; per-byte CRC32 lookup table built with `crc = 0` and
  **intentionally no CRC32_Init** "because of the client" (:907); ~300 KB
  kept resident until SV_Shutdown.
- SV_ProcessUserAgent: uuid must be exactly 32 chars, each `[0-9a-f]`
  (lowercase only) (sv_main.c:793-810); input-device bitfield gating with
  per-device reject messages; empty device list rejected only if
  `sv_allow_noinputdevices == 0`.

**Movevars:**

- sv_zmax clamped to [256, 16777216 (2^24)] with the Natural Selection
  ns_machina rationale in comments (sv_main.c:197-205). Delta-sent to
  clients only when `host.movevars_changed`; `initialize` pass fills the
  struct but skips the wire write (:236-241). `entgravity` hard-coded 1.0
  (:234); `features = host.features` copied "just in case" (:233).

**Shutdown:**

- SV_Shutdown on a non-initialized server still calls `CL_Drop()` if a demo
  is playing (sv_main.c:1107-1113). Order: reset sv_background cvar →
  SV_EndRedirect → FinalMessage → NET_MasterShutdown (only if
  `public_server && maxclients != 1`) → NET_Config(false) →
  SV_DeactivateServer → CL_Drop → memset sv → free clients/packet_entities
  → free test packet → **Mod_FreeAll** → HPAK_FlushHostQueue → Log_Close
  (:1117-1151).
- SV_ShutdownGame: `SV_ClearGameState` skipped when loading a save;
  `newGame` path ends with `Host_EndGame` instead of deactivate
  (sv_init.c:763-781).
- host_developer forces `sv_cheats 1` at SV_Init (sv_main.c:1004). SV_Init
  loads the game DLL at engine startup with
  `silent = (GI->gamemode != GAME_SINGLEPLAYER_ONLY)` (:1015).

**Commands:**

- SV_SetPlayer: with maxclients==1 or no argument returns client slot 0
  unconditionally; background map ⇒ NULL (sv_cmds.c:112-161).
- SV_NextMap_f searches `"maps\\*.bsp"` (backslash) first, then
  `"maps/*.bsp"` (sv_cmds.c:316-317); wraps alphabetically modulo list
  length.
- SV_HazardCourse_f: Gunman Chronicles compat — if `media/<trainmap>.avi`
  exists, plays movie + Host_EndGame instead of loading the map
  (sv_cmds.c:386-390).
- SV_Save_f pops "GAMESAVED" HUD title only when not
  ENGINE_QUAKE_COMPATIBLE (sv_cmds.c:448-449). quicksave/quickload go
  through Cbuf with `wait` (:422, 460).
- SV_Restart_f only when `ss_active` and reuses `sv.name`/`sv.background`
  (sv_cmds.c:507-513); SV_Reload_f only when nextstate == STATE_RUNFRAME,
  falls back to `COM_LoadLevel(sv_hostmap)` (:522-530).
- `changelevel2` with a single arg silently behaves like `changelevel`;
  both accept extra args "for compatibility" (sv_cmds.c:541, 559-567).
- SV_ConSay_f strips surrounding quotes into `text`, logs the stripped
  version, but broadcasts `hostname: <original unstripped p>`
  (sv_cmds.c:739-749) — stripped copy is effectively discarded for
  broadcast. Bug-compat.
- SV_ServerInfo_f mutates a matching cvar's `string/value` **directly**
  (freestring/copystring), bypassing Cvar_Set callbacks/flags, then
  broadcasts `fullserverinfo` (sv_cmds.c:793-803). Star-keys immutable in
  both serverinfo and localinfo.
- SV_Status_f forwards to the remote server when running as a connected
  client with no local server (sv_cmds.c:638-644); background map reports
  "no server running".
- `sv_list_messages` is registered in SV_InitOperatorCommands
  (sv_cmds.c:1074) but **never removed** in SV_KillOperatorCommands
  (:1093-1125) — asymmetric registry.
- Host commands (`map`, `newgame`, `load`, ...) are permanent from SV_Init;
  operator commands live only while the game DLL is loaded.
  `map`/`newgame`/etc. use `Cmd_AddRestrictedCommand` (privileged-only).
- Background maps: dedicated rejects them; forces `maxplayers 1`,
  `deathmatch 0`, `coop 0` latched before load (sv_cmds.c:268-299); can't
  be set while a foreground game is active.

**Magic numbers:** settle frames SP=2/MP=8 @ 0.1s and 0.001s fallback
(sv_init.c:606-615); sv.time epoch 1.0; sv_fps `-0.01`; zmax [256, 2^24];
uuid length 32; MD5 filename offset 4/length 32; instanced-baseline count 6
bits; final message sent 2x; status line refresh 0.5s; cmd-time reset window
1.0s; rcon redirect default 2000 lines; dedicated maxclients floor 4.

## 7. ABI touchpoints

- **engine/eiface.h `DLL_FUNCTIONS`:** `pfnServerActivate` (eiface.h:438;
  sv_init.c:599), `pfnServerDeactivate` (:439; sv_init.c:699),
  `pfnGetGameDescription` (:448; sv_init.c:971), `pfnCreateBaseline` (:467;
  sv_init.c:503 — passes `host.player_mins[0]/player_maxs[0]`),
  `pfnCreateInstancedBaselines` (:482; sv_init.c:508).
  `physics_interface_t::pfnPrepWorldFrame` (Xash extension;
  sv_main.c:563-564). `globalvars_t` fields written: `time`, `frametime`,
  `deathmatch`, `coop`, `maxClients`, `maxEntities`, `startspot`,
  `mapname`, `changelevel` (sv_init.c:598, 645, 698, 717-721, 1011-1013;
  sv_main.c:425-426, 691).
- **engine/edict.h / common/progdefs.h entvars:** `ent->v.flags`
  (FL_KILLME sv_init.c:564, FL_CUSTOMENTITY :499, FL_SPECTATOR|FL_FAKECLIENT
  sv_main.c:501), `v.effects` (EF_MUZZLEFLASH|EF_NOINTERP cleared per frame,
  sv_main.c:560), `v.modelindex` (baseline visibility, sv_init.c:488),
  `v.frags` (status, sv_cmds.c:713),
  `v.origin/classname/globalname/targetname/target/model` (entity_info,
  sv_cmds.c:961-976).
- **common/ wire structs:** `entity_state_t`
  (baselines/packet_entities/static_entities; delta-encoded via
  `MSG_WriteDeltaEntity` with DELTA_PLAYER/DELTA_ENTITY,
  ENTITY_NORMAL/ENTITY_BEAM), `movevars_t` (common/pmove.h:82; copied
  field-by-field from cvars sv_main.c:207-234, delta via
  `MSG_WriteDeltaMovevars`), `resource_t`/`customization_t`
  (customentity/custom.h; RES_FATALIFMISSING, RES_WASMISSING, FCUST_*
  flags), `usercmd_t` (via sv_client_t), `netadr_t`/`sizebuf_t`. Protocol
  constants: `PROTOCOL_VERSION`,
  `svc_print/stufftext/disconnect/changing/resource/voiceinit/
  spawnbaseline`, `S2M_INFO`, `S2C_BANDWIDTHTEST`, `LAST_EDICT`,
  `MAX_ENTITY_BITS`, `FRAGMENT_MAX_SIZE`.
- **pm_shared:** `playermove_t` via `svgame.pmove`;
  `PM_ClearPhysEnts(svgame.pmove)` at deactivate (sv_init.c:704,
  pm_local.h). Movevars struct is the pm-shared-visible one consumed by
  shared player movement.
- `GAME_EXPORT` calling-convention tags on `SV_SoundIndex`/`SV_GenericIndex`
  (sv_init.c:148, 241) — these are handed to the game DLL through the
  engine funcs table.

## 8. Console commands

**SV_InitHostCommands (sv_cmds.c:1030, permanent, from SV_Init):**

| Cmd | Handler | Purpose |
|---|---|---|
| map (restricted) | SV_Map_f | validate + COM_LoadLevel, sets sv_hostmap |
| maps | SV_Maps_f | list maps matching substring |
| *(HOST_NORMAL only)* newgame, hazardcourse, map_background, load, loadquick, reload, killsave, nextmap (all restricted) | SV_NewGame_f / SV_HazardCourse_f / SV_MapBackground_f / SV_Load_f / SV_QuickLoad_f / SV_Reload_f / SV_DeleteSave_f / SV_NextMap_f | new game from GI->startmap or arg; trainmap (avi special-case); background map; load save; cbuf quick-load; latest-save reload; delete .sav+.bmp; alphabetical next map |

**SV_InitOperatorCommands (sv_cmds.c:1053, alive while game DLL loaded):**
heartbeat→SV_Heartbeat_f (NET_MasterClear); kick→SV_Kick_f (#id or name);
status→SV_Status_f; localinfo→SV_LocalInfo_f; serverinfo→SV_ServerInfo_f;
clientinfo→SV_ClientInfo_f; clientuseragent→SV_ClientUserAgent_f;
playersonly→SV_PlayersOnly_f (cheats-gated freeze); restart→SV_Restart_f;
entpatch→SV_EntPatch_f (SV_WriteEntityPatch); edict_usage→SV_EdictUsage_f;
entity_info→SV_EntityInfo_f; shutdownserver→SV_KillServer_f (SV_Shutdown);
changelevel→SV_ChangeLevel_f; changelevel2→SV_ChangeLevel2_f;
redirect→Rcon_Redirect_f (rcon-only, default 2000 lines);
logaddress→SV_SetLogAddress_f (sv_log.c); log→SV_ServerLog_f (sv_log.c);
str64stats→SV_PrintStr64Stats_f (sv_game.c); sv_list_messages→
SV_ListMessages_f (never unregistered). HOST_NORMAL adds
save/savequick/autosave; HOST_DEDICATED adds say (SV_ConSay_f).
