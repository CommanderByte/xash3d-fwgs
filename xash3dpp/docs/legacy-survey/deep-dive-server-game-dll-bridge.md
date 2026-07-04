# Deep dive: game DLL bridge — `sv_game.c`, `eiface.h`, `edict.h`, `progdefs.h`

*Chunk 6 recon, 2026-07-04. Narrow-and-exact behavioural reference for the
xash3dpp server rewrite. This is the highest-ABI-risk area of the entire
rewrite: everything in here is visible to unmodified HL game DLLs.*

File: `engine/server/sv_game.c` (5367 lines). Supporting: `engine/eiface.h`,
`engine/edict.h`, `engine/progdefs.h`, `engine/server/server.h`,
`engine/physint.h`.

## 1. Responsibility

`sv_game.c` is the **engine↔game-DLL boundary** on the server side:

- Loads/unloads the game DLL (`SV_LoadProgs`/`SV_UnloadProgs`,
  sv_game.c:5214/5171) and negotiates the three export families
  (`GetEntityAPI`/`GetEntityAPI2`, `GiveFnptrsToDll`,
  `GetNewDLLFunctions`) plus the Xash-only physics interface.
- Implements the entire `enginefuncs_t` callback table (`gEngfuncs`,
  sv_game.c:4705-4866) handed to the DLL.
- Owns edict lifecycle: alloc/free/reuse (`SV_AllocEdict`/`SV_FreeEdict`/
  `SV_InitEdict`), private-data alloc/free, entity-string parsing at map
  spawn (`SV_ParseEdict`/`SV_LoadFromFile`/`SV_SpawnEntities`).
- Owns the engine string pool (`string_t` ↔ `char*`: `SV_AllocString`/
  `SV_MakeString`/`SV_GetString`, plus the 64-bit `str64` machinery).
- Owns the user-message system (`pfnRegUserMsg`, `pfnMessageBegin/End`,
  `pfnWrite*`) and multicast routing (`SV_Multicast`).
- Misc: sound message building (`SV_BuildSoundMsg`, `SV_StartSound`), event
  playback (`SV_PlaybackEventFull`), PVS/PHS services, static
  entities/decals, changelevel validation (`SV_MapIsValid`,
  `SV_QueueChangeLevel`), `.ent` entity-patch support.

## 2. enginefuncs_t table (gEngfuncs, sv_game.c:4705-4866)

### Entity management

`SV_AllocEdict` (pfnCreateEntity), `pfnRemoveEntity`,
`pfnCreateNamedEntity`, `pfnMakeStatic`, `pfnSetSize`
(→`SV_SetMinMaxSize`), `SV_SetModel`, `pfnSetOrigin`,
`SV_FindEntityByString`, `pfnFindEntityInSphere`, `pfnFindEntityByVars`,
`pfnGetVarsOfEnt`, `pfnGetModelPtr`, `pfnNumberOfEntities`,
`pfnPvAllocEntPrivateData`, `pfnPvEntPrivateData`, `SV_FreePrivateData`,
`pfnPEntityOfEntOffset`, `pfnEntOffsetOfPEntity`, `pfnIndexOfEdict`,
`pfnPEntityOfEntIndexAllEntities` (both slot 78 and the tail "8279" slot),
`pfnEntIsOnFloor`, `pfnDropToFloor`, `pfnWalkMove`, `pfnMoveToOrigin`,
`pfnChangeYaw`, `pfnChangePitch`, `pfnGetAimVector`.

Non-obvious:

- **`pfnPEntityOfEntIndex`** — engine default is the *fixed*
  `pfnPEntityOfEntIndexAllEntities` (`allentities=true`); with
  `-bugcomp peoei` (`BUGCOMP_PENTITYOFENTINDEX_FLAG`) it is swapped at load
  time for `pfnPEntityOfEntIndexBroken` (`allentities=false`) —
  sv_game.c:5250-5251. Core check `SV_PEntityOfEntIndex` (sv_game.c:42-61):
  index must be `0..max_edicts-1`; index 0 or `ENGINE_QUAKE_COMPATIBLE`
  returns the raw array slot unconditionally; otherwise a valid edict
  **with private data** is required; world/clients are allowed without
  private data — but the player check is `iEntIndex <= svs.maxclients`
  (fixed) vs `iEntIndex < svs.maxclients` (broken GoldSrc behaviour: last
  player slot returns NULL when it has no private data).
- **`SV_FindEntityByString`** (1485): only fields in `gEntvarsDescription`
  (13 string/model/sound-name entvars, sv_game.c:71-86) are searchable;
  returns **world** (`svgame.edicts`), not NULL, on failure; skips string
  values whose pointer equals `pStringBase` (empty string); skips client
  edicts not in game (in MP requires `cs_spawned`, in SP any).
- **`pfnFindEntityInSphere`** (1569): box-distance test against
  `absmin/absmax` (not origin distance); final compare is strict `<` while
  the per-axis early-out uses `<=`.
- **`pfnEntitiesInPVS`** (1743): despite the name it chains entities whose
  **box** is in the PVS from the viewer's viewpoint via `SV_BoxInPVS`;
  `MOVETYPE_FOLLOW` entities test their `aiment`'s box; result chained
  through `ent->v.chain`, terminated at world.
- **`pfnMakeStatic`** (1828): fills `svs.static_entities[]` via game's
  `pfnCreateBaseline`, abuses `state->messagenum` to carry the model
  string_t, writes `svc_spawnstatic` into the signon, then flags the source
  edict `FL_KILLME` (freed at frame end).
- **`pfnDropToFloor`** (1868): traces down 256 units; returns -1 if
  allsolid, 0 if fraction==1, else moves entity, links, sets `FL_ONGROUND`
  + `groundentity`, returns 1. Honours `FL_MONSTERCLIP`.
- **`pfnWalkMove`** (1903): requires `FL_FLY|FL_SWIM|FL_ONGROUND`; unknown
  mode → `Host_Error`.
- **`pfnRemoveEntity`** (1795): refuses to free world or client edicts
  (index < maxclients+1).
- **`pfnPvAllocEntPrivateData`** (2946): frees old data, then
  `Mem_Calloc(svgame.mempool, (cb+15) & ~15)` — 16-byte size round-up is a
  deliberate hack for Poke646's off-the-end writes.
- **`pfnGetAimVector`** (2269): autoaim; `speed` param unused; 2048-unit
  trace; team check `takedamage == DAMAGE_AIM`; threshold seeded from
  `sv_aim.value` only when `sv_allow_autoaim` is set, else 0.

### Tracing

`pfnTraceLine`, `pfnTraceToss`, `pfnTraceMonsterHull`, `pfnTraceHull`,
`pfnTraceModel`, `pfnTraceTexture`, `pfnTraceSphere` (**no-op stub**, 2258),
`SV_PointContents`.

- All convert engine `trace_t` → ABI `TraceResult` via `SV_ConvertTrace`
  (276), which also **resets `svgame.globals->trace_flags = 0`** as a side
  effect (same in `SV_CopyTraceToGlobal`, 195-211, which maps trace into
  the `globalvars_t` trace_* fields and substitutes world for invalid
  `trace_ent`).
- `pfnTraceLine` (2121) rewrites invalid hit ent to world before
  converting.
- `pfnTraceHull`/`pfnTraceModel` clamp hullNumber to 0..3 (out of range →
  0).
- `pfnTraceModel` (2194): `SOLID_CUSTOM` goes through
  `SV_CustomClipMoveToEntity` even if callbacks aren't initialised; for
  brush models it temporarily forces `movetype=MOVETYPE_PUSH,
  solid=SOLID_BSP` around the clip and restores afterwards.
- `pfnTraceMonsterHull` returns true if `allsolid || fraction != 1.0`.

### Messaging / network

`pfnMessageBegin`, `pfnMessageEnd`, `pfnWriteByte/Char/Short/Long/Angle/
Coord/String/Entity`, `pfnRegUserMsg`, `SV_Multicast` (internal),
`pfnBuildSoundMsg` (wraps Begin+`SV_BuildSoundMsg`+End, 2933),
`SV_StartSound`/`pfnEmitAmbientSound`, `SV_PlaybackEventFull`
(pfnPlaybackEvent), `pfnClientCommand` (stufftext), `pfnClientPrintf`,
`pfnServerPrint`, `pfnCrosshairAngle`, `pfnSetView`, `pfnFadeClientVolume`,
`pfnQueryClientCvarValue`, `pfnQueryClientCvarValue2`, `pfnParticleEffect`,
`pfnLightStyle`, `pfnStaticDecal`.

Notables (details in §5, §8):

- `pfnWriteByte` (2740): `-1` is converted to `0xFF`.
- `pfnWriteEntity` (2837): `Host_Error` if value outside
  `0..numEntities-1`.
- `pfnWriteAngle` (2794): 8-bit encode `((int)(v * 256/360) & 255)`.
- `pfnWriteString`: realsize accounting `strlen+1`; escape processing
  happens in `SV_ProcessString` only for alloc'd strings, not here.
- `pfnClientPrintf` (3591): `print_center` → `svc_centerprint` direct to
  netchan; console/chat → `SV_ClientPrintf`; non-client target just logs.
- `pfnCrosshairAngle` (3650): wraps angles to ±180 then writes
  `char(angle*5)` — ±25.5° effective range.
- `pfnSetView` (3677): view ent == client resets to self; fakeclients
  update state but skip the `svc_setview` message.
- `pfnQueryClientCvarValue/2` (4596/4623): on non-client target calls the
  game's `pfnCvarValue`/`pfnCvarValue2` callback with literal string
  `"Bad Player"`.
- `pfnParticleEffect` (2406): needs 16 bytes left in `sv.datagram` else
  silently dropped; dir components encoded as `bound(-128, dir*16, 127)`.

### Precache / resources

`pfnPrecacheModel`, `SV_SoundIndex` (pfnPrecacheSound), `SV_GenericIndex`
(pfnPrecacheGeneric), `pfnPrecacheEvent`, `pfnModelIndex`,
`pfnModelFrames`, `pfnDecalIndex`, `pfnForceUnmodified`,
`pfnCreateInstancedBaseline`, `pfnIsMapValid`.

- `pfnPrecacheModel` (1280): empty/NULL name returns 0 (world) with a
  warning — **GoldSrc does `Host_Error` here**, deviation documented in
  comment. Leading `'!'` marks model optional (skips `RES_FATALIFMISSING`).
- `pfnModelIndex` (1315): strips leading `\` or `/`, `COM_FixSlashes`,
  case-insensitive linear search of `sv.model_precache[1..]`;
  not-precached → error print + 0.
- `pfnDecalIndex` (2456): searches `host.draw_decals`, returns **-1** on
  failure (not 0).
- `pfnForceUnmodified` (4497): during `ss_loading` appends to
  `sv.consistency_list` (dup-checked); table full → `Host_Error
  "MAX_MODELS limit exceeded"`; post-load it only verifies presence.
- `pfnCreateInstancedBaseline` (4425): returns `sv.num_instanced` **after**
  increment, i.e. 1-based id; 0 on failure; capped at
  `MAX_CUSTOM_BASELINES` (64, netchan.h:80).

### Cvar / command

`pfnCvar_RegisterServerVariable` (2853, sets `FCVAR_EXTDLL` before
registering), `Cvar_VariableValue`, `Cvar_VariableString`,
`Cvar_SetValue`, `Cvar_Set`, `pfnCVarGetPointer`,
`pfnCvar_RegisterEngineVariable` (3765, registers **without**
`FCVAR_EXTDLL` → survives DLL unload, per comment), `Cvar_DirectSet`,
`pfnServerCommand` (validated: must end in `;` or `\n`, `SV_IsValidCmd`
1229), `pfnServerExecute` (`Cbuf_Execute`), `Cmd_Args`/`Cmd_Argv`/
`Cmd_Argc`, `Cmd_AddServerCommand` (4486, tagged `CMD_SERVERDLL`).

### File / system

`COM_LoadFileForMe`, `COM_FreeFile`, `pfnCompareFileTime`, `COM_FileSize`
(pfnGetFileSize), `Sound_GetApproxWavePlayLen`, `pfnGetGameDir` (4680; with
`BUGCOMP_GET_GAME_DIR_FULL_PATH` emulates GoldSrc pre-1.1.1.1 full path
into a 256-byte buffer, falling back to gamefolder on overflow),
`pfnIsDedicatedServer`, `Sys_FloatTime` (pfnTime), `COM_CheckParm`,
`pfnEndSection` (4444: `"oem_end_credits"` → `Host_Credits()`, anything
else → stuffs `disconnect`).

### String pool

`SV_GetString` (pfnSzFromIndex), `SV_AllocString` (pfnAllocString). See §4.

### Info strings / player state

`pfnGetInfoKeyBuffer` (3884: invalid edict → `svs.localinfo`, world →
`svs.serverinfo`, client → userinfo, else `""`), `Info_ValueForKey`,
`pfnSetValueForKey` (3909: only localinfo/serverinfo accepted),
`pfnSetClientKeyValue` (3924: 1-based index, skips no-op changes, sets
`FCL_RESEND_USERINFO` + immediate resend), `Info_RemoveKey`,
`pfnGetPhysicsKeyValue`/`pfnSetPhysicsKeyValue`/`pfnGetPhysicsInfoString`,
`pfnGetPlayerUserId`, `pfnGetPlayerStats` (4472: ping = `latency*1000`),
`pfnGetPlayerAuthId`, `pfnSetClientMaxspeed` (3800: clamps to
`±svgame.movevars.maxspeed` — **GoldSrc doesn't clamp**, per comment;
writes unused `"maxspd"` physinfo key), `SV_FakeConnect`
(pfnCreateFakeClient), `pfnRunPlayerMove` (3823: fakeclients only; swaps
`sv.current_client`, timebase = `sv.time + frametime - msec/1000`, random
seed `COM_RandomLong(0, 0x7fffffff)`),
`pfnVoice_GetClientListening`/`SetClientListening` (4544/4561: 1-based,
bitmask `cl->listeners`).

### Visibility / PVS

`pfnFindClientInPVS` (1684), `pfnSetFatPVS` (4255), `pfnSetFatPAS` (4279),
`pfnCheckVisibility` (4329), `pfnCanSkipPlayer` (4397: `FCL_LOCAL_WEAPONS`
flag), `SV_LightForEntity` (pfnGetEntityIllum), `pfnSetGroupMask` (4413 →
`svs.groupmask/groupop`), `pfnGetCurrentPlayer` (4238:
`sv.current_client - svs.clients`, -1 if out of range).

- `pfnFindClientInPVS`: round-robin client check cached 0.1 s
  (`sv.lastchecktime`); merges portal-camera PVS only for `FL_MONSTER`
  callers; with `ENGINE_PHYSICS_PUSHER_EXT` uses bmodel center as PVS
  origin — comment warns this **breaks the "radiation tick"** in stock HL
  (feature-gated for that reason).
- `pfnSetFatPVS/PAS`: fullvis when no visdata / `sv_novis` / NULL org /
  `CL_DisableVisibility()`; merge mode when `SVF_MERGE_VISIBILITY` (portal
  pass); radius 8.0 (`FATPVS_RADIUS`/`FATPHS_RADIUS`, mod_local.h:31-32);
  PAS shares the file-static `fatphs` buffer with `SV_Multicast`'s MSG_PAS
  path.
- `pfnCheckVisibility`: see §8.

### Delta encoding

`Delta_SetField`, `Delta_UnsetField`, `Delta_AddEncoder`,
`Delta_FindField`, `Delta_SetFieldByIndex`, `Delta_UnsetFieldByIndex`
(implemented in net_encode.c, exposed through this table).

### Studio / model helpers

`pfnGetBonePosition` (3556), `pfnGetAttachment` (3637),
`pfnFunctionFromName`/`pfnNameForFunction` (3569/3580, symbol↔address for
save/restore of function pointers), CRC32 quartet
(`CRC32_Init/ProcessBuffer/ProcessByte/Final`),
`COM_RandomLong`/`COM_RandomFloat`, `pfnMakeVectors`/`AngleVectors`,
`pfnVecToYaw`, `VectorAngles`, `pfnSequenceGet`,
`pfnSequencePickSentence`, `pfnIsCareerMatch`.

### Stubs / deprecated no-ops

- `pfnGetSpawnParms` (1411), `pfnSaveSpawnParms` (1422) — empty
  ("OBSOLETE, UNUSED").
- `pfnTraceSphere` (2258) — empty.
- `pfnEngineFprintf` (2922) — empty.
- `pfnAnimationAutomove` (3546) — empty.
- `pfnGetPlayerWONId` (3736) — returns `(uint)-1`.
- `pfnGetLocalizedStringLength` (4651) — returns 0.
- `pfnRegisterTutorMessageShown` (4664) — empty;
  `pfnGetTimesTutorMessageShown` (4675) — returns 0 ("only exists in
  PlayStation version"); `pfnProcessTutorMessageDecayBuffer` /
  `pfnConstructTutorMessageDecayBuffer` / `pfnResetTutorMessageDecayData`
  are external stubs referenced in the table.
- `pfnChangeLevel` obsolete-guard: no-op unless `sv.state == ss_active`.

## 3. Game DLL lifecycle

**Location**: `SV_InitGame` (sv_init.c:731) →
`COM_GetCommonLibraryPath(LIBRARY_SERVER)` (lib_common.c:210):
`host.gamedll` override (`-dll`, `@name` = generate) else
`COM_GenerateServerLibraryPath` from gameinfo `GI->game_dll` (win32) /
`game_dll_linux` / `game_dll_osx`; on Linux x86 strips extension and Intel
`_i?86` suffix (smarter than GoldSrc, which strips everything after `_`,
lib_common.c:164-167). Then `SV_LoadProgs(dllpath)` →
`COM_LoadLibrary(name, true, false)` (sv_game.c:5235).

**`SV_LoadProgs` sequence** (sv_game.c:5214-5366), order matters:

1. Early-return `true` if `svgame.hInstance` already set — **the DLL
   persists across map changes**; `SV_UnloadProgs` runs only on
   shutdown/game switch (called from host/game-change paths, not per
   level).
2. `svgame.pmove = &gpMove; svgame.globals = &gpGlobals` (function-local
   statics!); alloc `svgame.mempool` ("Server Edicts Zone").
3. Load library; zero `dllFuncs2` and `physFuncs`.
4. If `BUGCOMP_PENTITYOFENTINDEX_FLAG`: patch
   `gEngfuncs.pfnPEntityOfEntIndex = pfnPEntityOfEntIndexBroken` (5250).
5. `gpEngfuncs = gEngfuncs` — **local static copy so bots.dll etc. can't
   corrupt the master table** (5254).
6. Resolve `GetEntityAPI`, `GetEntityAPI2`, `GetNewDLLFunctions`. Missing
   both EntityAPI exports → fail. Missing `GiveFnptrsToDll` → fail (both
   paths free the library, free the mempool, push a library error).
7. **`GiveFnptrsToDll(&gpEngfuncs, svgame.globals)` is called first**
   (5282), before any Get*API.
8. `GetNewDLLFunctions` (optional): pass `version =
   NEW_DLL_FUNCTIONS_VERSION` (1); on failure warn if version mismatch and
   zero `dllFuncs2` (all NEW_DLL_FUNCTIONS pointers then treated as
   absent).
9. Entity API negotiation: try `GetEntityAPI2(&svgame.dllFuncs, &version)`
   with `version = INTERFACE_VERSION` (140); accept only if returned
   version still equals 140 ("extended EntityAPI"). Else fall back to
   `GetEntityAPI(&svgame.dllFuncs, version)` (version by value, legacy).
   Neither → fail/unload.
10. `SV_InitOperatorCommands()`, `Mod_InitStudioAPI()`.
11. `SV_InitPhysicsAPI()` (sv_phys.c:2136): optional export
    `Server_GetPhysicsInterface(SV_PHYSICS_INTERFACE_VERSION=6,
    &gPhysicsAPI, &svgame.physFuncs)`; on version-reject, `physFuncs`
    zeroed and only a warning; if present,
    `svgame.physFuncs.SV_CheckFeatures()` feeds
    `Host_ValidateEngineFeatures`.
12. `SV_InitSaveRestore()`; `svgame.globals->pStringBase = ""`; set
    `globals->maxEntities/maxClients`; alloc `svgame.edicts =
    Mem_Calloc(mempool, sizeof(edict_t) * GI->max_edicts)`;
    `svs.static_entities` (`MAX_STATIC_ENTITIES` = 3096, common.h:124) and
    `svs.baselines` (`max_edicts`) Z_Calloc'd; `numEntities = maxclients +
    1` (world + client slots pre-reserved); **all edicts marked
    `free = true`**.
13. `host_gameloaded = 1`; `SV_AllocStringPool()`; print
    `pfnGetGameDescription()`; **`pfnGameInit()`**; `SV_InitClientMove()`
    (pm_shared); `Delta_Init()`; **`pfnRegisterEncoders()`**.

**`SV_UnloadProgs`** (5171): `SV_DeactivateServer` → `Delta_Shutdown` →
`Cvar_PrepareToUnlink(FCVAR_EXTDLL)` → `dllFuncs2.pfnGameShutdown()` (if
present) → `host_gameloaded=0` → free `static_entities`/`baselines` →
`SV_KillOperatorCommands` → unlink pending cvars + `Cmd_Unlink(CMD_SERVERDLL)`
→ `SV_FreeStringPool` → `Mod_ResetStudioAPI` → `COM_FreeLibrary` → free
mempool → `memset(&svgame, 0, ...)`. (Note the ordering: game cvars are
captured *before* GameShutdown, unlinked after.)

**`SV_SysError`** (95): forwards fatal engine errors to
`dllFuncs.pfnSys_Error` if DLL is loaded.

## 4. Edict management

- Storage: flat array `svgame.edicts` of `GI->max_edicts`, alloc'd once in
  `SV_LoadProgs`; `svgame.numEntities` = high-water count.
  `SV_EdictNum(n)` (server.h:649) bounds-checks 0..max_edicts and returns
  NULL outside. `NUM_FOR_EDICT` = pointer subtraction (server.h:57).
  `SV_IsValidEdict` = `SV_CheckEdict` (server.h:635): NULL→false,
  out-of-range→console spam ("bad entity %i (called at file:line)"), else
  `!e->free`.
- Slot layout: 0 = world, 1..maxclients = clients, game entities from
  `maxclients+1`.
- **`SV_AllocEdict`** (1041): scans from `maxclients+1` for a free slot
  with the **reuse policy** `e->freetime < 2.0f || (sv.time - e->freetime)
  > 0.5f` (first ~2 s of server time reuse is unrestricted; afterwards a
  slot is quarantined 0.5 s). Exhaustion → `Host_Error "no free edicts"`.
- **`SV_InitEdict`** (983): frees private data, zeroes entvars,
  `pContainingEntity = self`, **bone controllers preset to 0x7F**
  (centered), `free = false`. `serialnumber` is intentionally *not* reset
  here.
- **`SV_FreeEdict`** (1004): idempotent; unlink from world; free private
  data; `freetime = sv.time`; **`serialnumber++` invalidates EHANDLEs**;
  scrubs a specific subset of entvars (solid=SOLID_NOT, flags=0, model=0,
  takedamage, modelindex, colormap, frame, scale, gravity, skin all 0,
  `nextthink = -1`, origin/angles cleared) — everything else left stale;
  `free = true`.
- **Private data**: allocated by the game via `pfnPvAllocEntPrivateData` or
  implicitly by the classname export. `SV_AllocPrivateData` (1092):
  resolves the classname's `LINK_ENTITY_FUNC` via
  `COM_GetProcAddress(svgame.hInstance, classname)`; if absent, tries Xash
  extension `physFuncs.SV_CreateEntity(ent, classname)` (-1 = reject),
  then the `"custom"` export (flagging `customentity`), else error "No
  spawn function" + `SV_FreeEdict` + NULL. Calls `SpawnEdict(&ent->v)` —
  the DLL allocates pvPrivateData inside. `SV_FreePrivateData` (961):
  calls optional `dllFuncs2.pfnOnFreeEntPrivateData` first, frees only if
  `Mem_IsAllocatedExt(svgame.mempool, ...)` (tolerates DLL-owned or
  already-freed pointers).
- **Entity string parsing**: `SV_SpawnEntities` (5136) resets sky/water
  cvars, initialises world edict (model_precache[1], `WORLD_INDEX`,
  SOLID_BSP, MOVETYPE_PUSH), sets
  `globals->maxEntities/mapname/startspot/time`, then `SV_LoadFromFile`
  (5079): optional `physFuncs.SV_LoadEntities` full override; else
  `{`-token loop, first entity is edict 0, others `SV_AllocEdict`;
  `pfnSpawn(ent) == -1` without `FL_KILLME` → `SV_FreeEdict` +
  inhibited++. World origin/angles cleared after load "for some reason".
  `SV_ParseEdict` (4885) details in §8.
- **string_t handling**: `string_t` = `int` (common/const.h:724), an
  **offset from `svgame.globals->pStringBase`**.
  - 32-bit: `pStringBase = ""` (SV_AllocStringPool 3141); `SV_AllocString`
    = copy into `svgame.stringspool` after escape processing, return
    `ptr - pStringBase` (a raw pointer diff — works because 32-bit
    pointers fit in int); `SV_MakeString` = `szValue - pStringBase` with
    **no allocation** (only valid for stable strings).
  - 64-bit (`XASH_64BIT`): `str64` pool (2976-2991): default size
    `65536 * ceil(max_edicts/1024)` (override `-str64alloc`, min 1024); on
    Linux/amd64 mmap'd within ±2 GB of the game library so offsets fit in
    int (search down from `hInstance`, then up, sv_game.c:3078-3130);
    split into dynamic half + static half; `SV_SetStringArrayMode(dynamic)`
    switches after server spawn so map-static strings survive; dedup scan
    unless `-str64dup`; **pool overflow silently wraps**
    (`str64.numoverflows++`, old strings clobbered, 3263-3268);
    `SV_MakeString` falls back to `SV_AllocString` when ptrdiff exceeds
    INT range (3321-3328). `SV_EmptyStringPool` resets on server stop.
  - All three can be overridden by the physics interface
    (`physFuncs.pfnAllocString/pfnMakeString/pfnGetString`, checked first
    at 3241/3319/3342).
  - `SV_ProcessString` (3173): converts `\n` **and also `\r`, `\t`**
    escapes (comment: GoldSrc only does `\n`; the `\r\t` behaviour comes
    from an old Xash pfnWriteString hack).

## 5. User message system

State in `svgame` (server.h:303-315): `msg_name`,
`msg[MAX_USER_MESSAGES]` (197, protocol.h:128) of `sv_user_message_t
{name[32], number, size}`, `msg_size_index`, `msg_realsize`, `msg_index`,
`msg_dest`, `msg_rewrite_index`, `msg_rewrite_pos`, `msg_started`,
`msg_ent`, `msg_org`, `msg_trace`.

- **`pfnRegUserMsg`** (3488): NULL/empty name → `svc_bad`; name ≥ 32 chars
  → error `svc_bad`; `iSize > MAX_USERMSG_LENGTH` (2048, common.h:136) →
  error `svc_bad`; size clamped to `[-1, 2048]` (-1 = variable-length);
  re-registration by name returns existing number; slot 0 reserved
  (svc_bad); full table → `svc_bad`. Number = `svc_lastmsg (59) + i`. If
  registered while `ss_active`, immediately broadcasts `svc_usermessage`
  (byte num, word size, string name — `SV_SendUserReg` 3474).
- **`pfnMessageBegin`** (2534): re-entry (`msg_started`) → `Host_Error`.
  `msg_num = bound(svc_bad, msg_num, 255)`. System messages
  (`<= svc_lastmsg`): `msg_index = -msg_num`; `svc_temp_entity` gets
  variable size (-1), all other system msgs size 0 (unchecked). GoldSrc
  rewrite facility (bugcomp `gsmrf`): `svc_goldsrc_spawnstaticsound` (a
  GoldSrc opcode colliding with Xash numbering) is recorded
  (`msg_rewrite_index/pos = MSG_TellBit`) for later rewrite into Xash
  `svc_sound`. User messages: looked up by number; unregistered →
  `Host_Error`. Writes the cmd byte into `sv.multicast`; variable-size
  messages reserve a **word** at `msg_size_index` (byte position). Saves
  `msg_org` (or zero), `msg_dest`, `msg_ent`; `msg_realsize = 0`. Tracing
  (`sv_trace_messages`) only for user msgs, excluding `"ReqState"`.
- **`pfnWrite*`**: each write increments `msg_realsize` by its wire size;
  no per-write bounds check (multicast buffer overflow caught at End).
- **`pfnMessageEnd`** (2620): no active message → `Host_Error`. Buffer
  overflow → error + `MSG_Clear`, message dropped. Rewrite path re-parses
  and re-emits the sound message (`SV_RewriteMessage` 2487; failure or
  post-rewrite overflow drops). Size validation: system+variable →
  `0 <= realsize <= 2048`, patch reserved word (little-endian memcpy into
  `sv.multicast.pData[msg_size_index]`); fixed-size user msg →
  `expsize == realsize` else **message dropped entirely** ("expected %i
  bytes, it written %i. Ignored."); variable user msg → same 0..2048
  patch. Impossible fall-through → drop. Compat shim: empty
  `svc_finale`/`svc_cutscene` get a NUL byte appended (2720-2724).
  `msg_dest` clamped to `[MSG_BROADCAST, MSG_SPEC]` (silently!), then
  `SV_Multicast(dest, org-if-nonzero, msg_ent, usermessage=true,
  filter=false)`.
- **`SV_Multicast`** (354): dead server → clear+0. `MSG_INIT` during
  `ss_loading` appends to `sv.signon` bit-exactly, else **falls through to
  MSG_ALL reliable**. `MSG_PAS(_R)`: NULL origin → 0; FatPVS-as-PHS
  (radius 8, PHS merge disabled in singleplayer per GoldSrc).
  `MSG_PVS(_R)`: point PVS. `MSG_ONE(_UNRELIABLE)`: ent must be valid and
  index in `[1, maxclients]`. `MSG_SPEC` → `sv.spec_datagram`, HLTV
  proxies only. Unknown dest → `Host_Error` (only reachable from internal
  callers because MessageEnd clamps). Per-client skips: free/zombie;
  not-spawned unless (reliable && !usermessage); fakeclients always;
  predicted-movement filter (step-sound suppression, FIXME comment 437);
  `groupinfo` AND/NAND ops; `SV_CheckClientVisiblity` (NULL mask → true,
  "GoldSrc rules", 308; checks view entity origin, then all portal cameras
  `cl->viewentity[]`). Reliable → `cl->netchan.message`, else
  `cl->datagram`. Always clears `sv.multicast`. Returns numsends.

## 6. Owned state

| Symbol | Type / loc | Purpose |
|---|---|---|
| `svgame` | `svgame_static_t` (server.h:301-337), extern global | msg_* state, `hInstance`, `edicts` + `numEntities`, `movevars`/`oldmovevars`, `pmove`, `interp[MAX_CLIENTS]`, `pushed[MAX_PUSHED_ENTS=256]`, `globals`, `dllFuncs` (DLL_FUNCTIONS), `dllFuncs2` (NEW_DLL_FUNCTIONS), `physFuncs` (physics_interface_t), `mempool` (edicts+private data), `stringspool` (32-bit strings) |
| `fatphs` | `static byte [(MAX_MAP_LEAFS+7)/8]` sv_game.c:31 | shared PHS buffer (Multicast PAS + pfnSetFatPAS + event playback) |
| `clientpvs` | `static byte [...]` sv_game.c:32 | pfnFindClientInPVS merged PVS |
| `fatpvs` | `static byte [...]` inside `pfnSetFatPVS` 4257 | buffer returned to game DLL |
| `gEntvarsDescription` | `static TYPEDESCRIPTION[13]` :71 | FindEntityByString field table |
| `str64` | `static struct str64_s` :2976 | 64-bit string pool bookkeeping |
| `gEngfuncs` | `static enginefuncs_t` :4705 | master callback table (mutable: bugcomp patch) |
| `last_spawncount` | function-static in `pfnChangeLevel` :1374 | double-changelevel guard |
| statics in `SV_LoadProgs` :5217-5223 | `GetEntityAPI(2)`, `GiveFnptrsToDll`, `GiveNewDllFuncs`, `gpEngfuncs`, `gpGlobals`, `gpMove` | the actual `globalvars_t` and engfuncs copy handed to the DLL live here |

Related but externally owned: `sv` (multicast/signon/datagram buffers,
precache tables, static ent counters), `svs` (clients, static_entities,
baselines, groupmask/groupop, serverinfo/localinfo).

## 7. Dependencies

- **World/physics (sv_world.c, sv_move.c, sv_phys.c)**: `SV_LinkEdict`,
  `SV_UnlinkEdict`, `SV_Move`, `SV_MoveNoEnts`, `SV_MoveToss`,
  `SV_ClipMoveToEntity`, `SV_CustomClipMoveToEntity`, `SV_TraceTexture`,
  `SV_PointContents`, `SV_CheckBottom`, `SV_MoveStep`, `SV_MoveTest`,
  `SV_MoveToOrigin`, `SV_VecToYaw`, `SV_LightForEntity`,
  `SV_SetLightStyle`, `SV_InitPhysicsAPI`.
- **Model/BSP (mod_bmodel.c, ref)**: `Mod_PointInLeaf`,
  `Mod_GetPVSForPoint`, `Mod_FatPVS`, `Mod_BoxVisible`, `Mod_ForName`,
  `Mod_TestBmodelLumps`, `Mod_StudioExtradata`, `Mod_GetBonePosition`,
  `Mod_StudioGetAttachment`, `Mod_InitStudioAPI`/`Mod_ResetStudioAPI`,
  `node_child`, `ref.dllFuncs.R_CreateDecalList/R_ClearAllDecals` (demo
  decal restart only).
- **Network/protocol (net_buffer, net_encode, netchan)**: `MSG_*`
  (writes/reads/seek/overflow), `MSG_BeginServerCmd`,
  `MSG_WriteDeltaEntity`, `MSG_WriteDeltaEvent`, `Delta_Init/Shutdown` +
  the six Delta_* table exports, `svc_*`/`svc_goldsrc_*` opcodes,
  `svc_strings` tables.
- **Server peers**: `SV_ModelIndex`/`SV_SoundIndex`/`SV_EventIndex`/
  `SV_GenericIndex` (sv_init.c precache), `SV_ModelHandle`,
  `SV_FindBestBaseline`, `SV_ClientPrintf`, `SV_BroadcastPrintf`,
  `SV_FakeConnect`, `SV_RunCmd`, `SV_InitClientMove`, `SV_SkipUpdates`,
  `SV_DeactivateServer`, `SV_MapIsValid` consumers,
  `SV_InitOperatorCommands`/`SV_KillOperatorCommands`,
  `SV_InitSaveRestore`, `SV_GetClientIDString`, `Log_Printf`.
- **Common/host**: `COM_LoadLibrary`/`COM_FreeLibrary`/
  `COM_GetProcAddress`/`COM_FunctionFromName_SR`/`COM_NameForFunction`
  (library.c), `COM_ParseFile`, `COM_ParseVector`,
  `copystring`/`Mem_*`/`Z_*` pools, `Cvar_*`, `Cmd_*`, `Cbuf_*`, `Info_*`,
  `FS_*`/`g_fsapi`, `Host_Error`, `Host_Credits`, `Host_IsDedicated`,
  `host.features`/`host.bugcomp`/`host_developer`, `COM_ChangeLevel`,
  `CRC32_*`, `COM_RandomLong/Float`, `Sys_GetParmFromCmdLine`, `va`.
- **Client-side (listen server only, `!XASH_DEDICATED`)**:
  `CL_ClearStaticEntities`, `S_GetCurrentStaticSounds`, `S_StopSound`,
  `S_StreamGetCurrentState`, `CL_DisableVisibility`.

## 8. Quirks & invariants (bug-compat catalogue)

1. **pfnPEntityOfEntIndex dual behaviour** — sv_game.c:42-61, 3406-3421,
   5250. Broken variant treats player index range as `< maxclients`
   (GoldSrc off-by-one); fixed uses `<=`. Entities without private data
   return NULL (except world/players/index-0/Quake-compat).
2. **`ENGINE_QUAKE_COMPATIBLE`** bypasses private-data checks in
   PEntityOfEntIndex (:49), forces `MSG_ALL` sounds (:2075), makes
   `pfnServerPrint` broadcast to clients (:3626).
3. **Landmark-with-space bug emulation** — `pfnChangeLevel` :1387-1397
   under `HACKS_RELATED_HLMODS`: truncates landmark at first space to
   mimic `Cmd_TokenizeString` behaviour some maps rely on.
4. **Double-changelevel guard** via static `last_spawncount` :1374-1384;
   **infinite-changelevel guard**: `sv.framecount < 15` +
   `sv_validate_changelevel` → refuse (:768-775); multiplayer never does
   landmark transitions (:758).
5. **Edict reuse quarantine**: `freetime < 2.0 || sv.time - freetime > 0.5`
   :1051 (verbatim Quake/GoldSrc policy).
6. **`SV_InitEdict` sets `controller[0..3] = 0x7F`** :990-993 (bone
   controllers centered — netcode baseline assumption).
7. **`SV_FreeEdict` sets `nextthink = -1`** and only a curated subset of
   fields; stale entvars persist into reuse (games depend on this)
   :1015-1031.
8. **Private data 16-byte round-up** for Poke646 memory corruption
   :2954-2955.
9. **`SV_ParseEdict`** (:4885-5065): classname must be `fHandled` by the
   game or `Host_Error` (:4943, stricter than GoldSrc); trailing spaces
   stripped from key names *after* classname handling (GoldSrc parity,
   :4951-4955); `"wad"` key dropped; `_`-prefixed keys dropped only when
   `FWORLD_SKYSPHERE`; `"angle"` → `"angles"` with `-1` → `"-90 0 0"` (up)
   and `-2` → `"90 0 0"` (down) (:5021-5038); custom entities get a
   synthetic `{"custom","customclass",classname}` KVD with **no fHandled
   check** (GoldSrc behaviour, :4989-5000); disabled `#if 0` GoldSrc bug
   that skipped kv whose value equals classname (:5016);
   `ce08_02`/`info_player_start_force` origin -16 z hack for Chemical
   Existence (:5002-5009). **Bounds oddity**: `pkvd[256]` overflow check
   is `numpairs > ARRAYSIZE(pkvd)` *after* increment (:4964) — the 257th
   pair is written out of bounds before the break triggers.
10. **Message size mismatch drops the whole message** rather than erroring
    the game (:2687-2690); recursive MessageBegin and unregistered user
    messages are hard `Host_Error`s (:2538, 2579).
11. **`pfnWriteByte(-1)` → 0xFF** :2742 (GoldSrc char/byte convention).
12. **svc_finale/svc_cutscene empty-body NUL append** :2720-2724 (format
    changed; keeps old clients happy).
13. **GoldSrc Message Rewrite Facility** (bugcomp `gsmrf`):
    GoldSrc-numbered `svc_spawnstaticsound` written by the game is
    transparently re-encoded as Xash `svc_sound` at MessageEnd
    (:2472-2526).
14. **`SV_BuildSoundMsg`** clamps vol/attn/chan/pitch with error spam
    instead of rejecting; `!N`/`#N` sentence encodings with
    `MAX_SOUNDS_NONSENTENCE` split; `*` prefix hijacks channel to
    `CHAN_STREAM`; `SND_RESTORE_POSITION` selects `svc_restoresound` and
    is stripped from wire flags; attenuation wire format
    `min(attn*64, 255)` (:1951-2051).
15. **`pfnPrecacheModel` NULL → warn + return 0 (world)** where GoldSrc
    `Host_Error`s — deliberate divergence to not break Xash games
    (:1285-1290).
16. **`pfnCheckVisibility`** (:4329-4389): beams (`FL_CUSTOMENTITY`) with
    client owner test the owner instead; `headnode >= 0` path checks up to
    `MAX_ENT_LEAFS(large)` cached leafs then recursive headnode walk, and
    on success **mutates the const edict**, caching the found leaf with
    wrap-around `num_leafs = (num_leafs + 1) % MAX_ENT_LEAFS` and
    returning 2 (vs 1 for leaf hit). QBSP2 worlds use 32-bit leafnums (24
    entries) vs 16-bit (48 entries).
17. **`SV_CheckClientVisiblity` NULL mask → visible** ("GoldSrc rules")
    :308; view origin taken from `pViewEntity` if set (Invasion mod camera
    fix) :310-314.
18. **FEV event playback** (:4026-4230): `FEV_CLIENT` flag silently
    ignored ("someone stupid joke"); index bounds 1..MAX_EVENTS + precache
    required; non-global events without any origin dropped;
    FEV_NOTHOST/FEV_HOSTONLY cleared with warning when invoker isn't a
    client; negative delays clamped to 0; `FEV_NOTHOST` skip condition
    checks `cl == sv.current_client || cl->edict == pInvoker` —
    documented divergence from GoldSrc's never-nulled `host_client`
    (:4161-4170); reliable events bypass the queue straight into netchan;
    `SV_PlaybackReliableEvent` encodes delay as `word(delay * 100)` :1200.
19. **`pfnFindClientInPVS` 0.1 s cache** (`sv.lastchecktime`),
    PVS-origin-for-bmodels only under `ENGINE_PHYSICS_PUSHER_EXT` because
    it breaks HL's radiation tick (:1717-1723).
20. **`pfnSetClientMaxspeed` clamps to movevars.maxspeed (GoldSrc
    doesn't)** and writes a vestigial `"maxspd"` physinfo key
    (:3808-3812).
21. **`pfnGetGameDir` bugcomp**: default returns bare gamefolder;
    `get_game_dir_full` returns GoldSrc-pre-1.1.1.1 full path, falling
    back if > 256 chars (:4680-4702).
22. **String escape processing**: `SV_AllocString` converts `\n`, `\r`,
    `\t`; GoldSrc converts only `\n` (comment :3189-3192).
23. **str64 pool overflow silently recycles the array** (numoverflows
    counter; stale string_t values then dangle) :3263-3268; dedup on by
    default; mmap-near-library trick to keep 32-bit offsets valid for
    unpatched game DLLs :3078-3130.
24. **`SV_Multicast` MSG_INIT fallthrough** to MSG_ALL when not loading;
    PHS not used in singleplayer; step-sound prediction filter with a
    FIXME (:372-439).
25. **`SV_CreateDecal`** requires 20 bytes left, scale sent as
    `word(scale*4096)`; static decals only into signon during loading
    (:501-522). `SV_CreateStaticEntity` requires 50 bytes and caps at
    `MAX_STATIC_ENTITIES - 1` with one-shot warning + ignored counter
    (:531-554).
26. **`pfnRegUserMsg` live re-registration** broadcasts immediately when
    `ss_active` (:3529-3534); comment in server.h:272 notes GoldSrc max
    name length is 12 vs 32 here.
27. **`pfnLightStyle`** hard `Host_Error` on style ≥ MAX_LIGHTSTYLES;
    ignored entirely during `sv.loadgame` to protect restored styles
    (:2437-2444).
28. **`pfnAlertMessage`**: `at_logged` in MP → server log; `at_aiconsole`
    requires `developer >= DEV_EXTENDED` because "some mods have wrong
    aiconsole messages that crash the engine" (:2874-2888).
29. `pfnEndSection("oem_end_credits")` → credits; anything else →
    `disconnect` (:4444-4449).
30. `pfnCreateInstancedBaseline` returns count (1-based index) and copies
    the classname string because "must sure that classname is really
    allocated" (:4430-4435).
31. `.ent` entity patch overrides BSP entity lump only if file mtime is
    newer than the BSP; entity lump must be ≥ 32 bytes
    ("{ classname worldspawn }") (:874-894).
32. `SV_AngleMod` is the classic Quake anglemod stepping toward ideal at
    `speed` deg/frame (:122-156).

## 9. ABI touchpoints (frozen surfaces)

- **`INTERFACE_VERSION` = 140** (eiface.h:22; 001 for HLDEMO_BUILD).
  `GetEntityAPI2` negotiates by pointer, `GetEntityAPI` by value. Comment
  eiface.h:286: "ONLY ADD NEW FUNCTIONS TO THE END OF THIS STRUCT.
  INTERFACE VERSION IS FROZEN AT 138".
- **`NEW_DLL_FUNCTIONS_VERSION` = 1** (eiface.h:498); `NEW_DLL_FUNCTIONS` =
  { pfnOnFreeEntPrivateData, pfnGameShutdown, pfnShouldCollide,
  pfnCvarValue, pfnCvarValue2 } — all optional.
- **`SV_PHYSICS_INTERFACE_VERSION` = 6** (physint.h:21), export name
  `Server_GetPhysicsInterface`, exchanging `server_physics_api_t`
  (engine→game) and `physics_interface_t` (game→engine); both tables
  frozen-at-6 with append-only comment. Members consumed by sv_game.c:
  `SV_CreateEntity`, `SV_LoadEntities`,
  `pfnAllocString/pfnMakeString/pfnGetString`, `pfnRestoreDecal`,
  `SV_CheckFeatures`.
- **Export signatures**: `GiveFnptrsToDll` is
  `void (DLLEXPORT *)(enginefuncs_t*, globalvars_t*)` with
  `DLLEXPORT = __stdcall` on Win32 (eiface.h:37-41, sv_game.c:40);
  `LINK_ENTITY_FUNC` is `__cdecl void(entvars_t*)` on Win32
  (sv_game.c:35-39); per-classname entity exports resolved by raw name.
- **`enginefuncs_t`**: 159 function-pointer slots (eiface.h:104-285;
  counted — the classic GoldSrc surface is the first 144 through
  `pfnGetPlayerAuthId`) ending at `pfnPEntityOfEntIndexAllEntities`
  ("added in 8279", eiface.h:284). Note
  `pfnPvAllocEntPrivateData` takes `long cb` (LLP64 hazard, fine on Win64
  vs LP64 divergence), `pfnFunctionFromName` returns `unsigned long` in
  the header but the engine casts through `void*` in the table (:4785).
- **`DLL_FUNCTIONS`**: 50 slots, `pfnGameInit` … `pfnAllowLagCompensation`
  (eiface.h:411-493; counted). Called from this file: GameInit, Spawn, KeyValue,
  CreateBaseline, RegisterEncoders, GetGameDescription, Sys_Error, plus
  dllFuncs2 OnFreeEntPrivateData/GameShutdown/CvarValue/CvarValue2.
- **`edict_t`** (edict.h:25-46): `{qboolean free; int serialnumber; link_t
  area; int headnode; int num_leafs; union{int leafnums32[24]; short
  leafnums16[48]}; float freetime; void *pvPrivateData; entvars_t v;}`.
  FWGS deviation from GoldSrc: leafnums is a union sized 96 bytes either
  way (GoldSrc: `short leafnums[MAX_ENT_LEAFS=48]`); `MAX_ENT_LEAFS_32 =
  24` "Originally was 16". `pvPrivateData` precedes `v`; game code offsets
  from `&v` (`pfnGetVarsOfEnt`, `PEntityOfEntOffset` = byte offset from
  `svgame.edicts` base).
- **`entvars_t`** (progdefs.h:57-218): full frozen layout, 123 fields ending
  in the `iuser/fuser/vuser/euser1-4` mod block; contains `string_t
  classname/globalname/model/target/targetname/netname/message/noise0-3`,
  `edict_t*` cross-links (chain, dmg_inflictor, enemy, aiment, owner,
  groundentity, pContainingEntity, euser1-4), `byte
  controller[4]/blending[2]`.
- **`globalvars_t`** (progdefs.h:21-55): trace_* block mirrored by
  `SV_CopyTraceToGlobal`; **`pStringBase`** is the string_t offset base;
  `pSaveData` (SAVERESTOREDATA*), `vecLandmarkOffset`, `changelevel`
  ("transition in progress… was msg_entity"), `cdAudioTrack`,
  `maxClients`, `maxEntities`, `mapname`/`startspot` as string_t. Engine
  hands the DLL a pointer to a function-local static (`gpGlobals` in
  SV_LoadProgs).
- **`string_t` = int** (common/const.h:724), offset from `pStringBase`.
- **`TraceResult`** (eiface.h:71-83): int flags + float fraction + vec3
  endpos + plane dist/normal + `edict_t *pHit` + int iHitgroup — filled by
  `SV_ConvertTrace`.
- **`KeyValueData`** (eiface.h:289-295):
  szClassName/szKeyName/szValue/fHandled — engine allocates key/value
  copies and frees them after `pfnKeyValue` returns (game must not keep
  pointers).
- **`TYPEDESCRIPTION`** (eiface.h:392-399) + `FIELDTYPE` enum (18 values) +
  `FTYPEDESC_*` flags + `DEFINE_ENTITY_FIELD` macros — used here for
  `gEntvarsDescription`; also **`SAVERESTOREDATA`/`ENTITYTABLE`/
  `LEVELLIST`** (eiface.h:298-346, `MAX_LEVEL_CONNECTIONS` 16,
  `FENTTABLE_PLAYER/REMOVED/MOVEABLE/GLOBAL` =
  0x80000000/0x40000000/0x20000000/0x10000000) — declared in eiface.h,
  consumed by sv_save.c and `physFuncs.pfnCreateEntitiesInRestoreList`,
  not directly in sv_game.c.
- **`customization_t`** — referenced via `pfnPlayerCustomization` slot in
  DLL_FUNCTIONS and `sv_client_t.customdata`; not manipulated in this
  file.
- **Wire-adjacent constants**: `svc_lastmsg` = 59 (protocol.h:82),
  `MAX_USER_MESSAGES` = 197, `MAX_USERMSG_LENGTH` = 2048, `MAX_MULTICAST`
  = 8192 (net_ws.h:37), `MAX_STATIC_ENTITIES` = 3096 (common.h:124),
  `MAX_CUSTOM_BASELINES` = 64, `FATPVS_RADIUS`/`FATPHS_RADIUS` = 8.0f
  (mod_local.h:31-32). User message wire reg: `svc_usermessage{byte num,
  word size, string name}`.
