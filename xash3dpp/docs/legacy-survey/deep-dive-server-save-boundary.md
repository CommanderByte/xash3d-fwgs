# Deep dive: save/restore boundary — `sv_save.c` (server-side view)

*Chunk 6 recon, 2026-07-04. Save/restore is its own later chunk (Chunk 8);
this brief maps the save system's BOUNDARY with the server core — what
Chunk 6 must expose and what ordering it must honour — not the file-format
internals (kept to a short summary). Full format recon happens at Chunk 8.*

> **Refreshed 2026-07-06 (as-built cross-ref).** As-built, the save seams are
> **stubbed behind the `ILevelChangeExecutor` interface**:
> `Server::exec_load_game` / `exec_change_level` carry
> `// XASH3DPP-STUB(chunk8)` markers, and the four save primitives
> (`SaveGameState`/`LoadGameState`/`LoadAdjacentEnts`/`ClearSaveDir`) are
> absent until Chunk 8. Chunk 6 keeps the `SV_ChangeLevel` orchestration point
> and the `pSaveData` pass-through as designed. See
> `docs/boundaries/server-boundary.md` §Satellite components (save row).

## 1. Responsibility

`engine/server/sv_save.c` (2493 lines) implements the GoldSrc-compatible
save/restore and landmark-changelevel system: it serializes engine-side
headers plus per-entity game-DLL data into per-level `.HL1/.HL2/.HL3` temp
files and a container `.sav`, and on restore recreates edicts and drives
the game DLL to repopulate them. It owns save-slot management (aging,
comments, latest-save lookup) and the entity-transfer half of smooth level
transitions. Crucially, it does **not** own the field codec: all
TYPEDESCRIPTION-based reading/writing — including the engine's own headers
— is delegated to the game DLL's `pfnSaveWriteFields`/`pfnSaveReadFields`.

## 2. Entry points from the rest of the engine

Public API (`engine/server/server.h:668-674`): `SV_SaveGame`,
`SV_LoadGame`, `SV_LoadGameState`, `SV_ChangeLevel`, `SV_GetLatestSave`,
`SV_InitSaveRestore`, `SV_ClearGameState`, plus `SV_GetSaveComment`
(`engine/common/common.h:783`).

Callers:

- **Console commands** (`engine/server/sv_cmds.c`): `save`/`SV_Save_f`:431
  → `SV_SaveGame` ("new" = scan save000..999); `load`/`SV_Load_f`:400 →
  `SV_LoadGame(save/<name>.sav)`; `savequick`/`loadquick` just enqueue
  `save quick` / `load quick` via Cbuf (:458, :420);
  `autosave`/`SV_AutoSave_f`:488 → `SV_SaveGame("autosave")` gated on
  `sv_autosave` cvar; `reload`/`SV_Reload_f`:522 →
  `SV_LoadGame(SV_GetLatestSave())` (death reload); `killsave`:469 deletes
  `.sav`+`.bmp`; `changelevel`/`changelevel2`:539/:557 →
  `SV_QueueChangeLevel`. Save commands are registered singleplayer-only
  (:1079-1080, removed at :1119).
- **Game DLL**: engine func `pfnChangeLevel` (`engine/server/sv_game.c:
  1401`) → `SV_QueueChangeLevel`; `trigger_autosave` reaches the
  `autosave` command via `SERVER_COMMAND`. Optional DLL export
  `SV_SaveGameComment` resolved at `SV_InitSaveRestore`
  (`sv_save.c:2489-2492`, called from `sv_game.c:5334` after DLL load).
- **Menu**: `SV_GetSaveComment` exported to the menu DLL as
  `pfnGetSaveComment` (`engine/client/dll_int/cl_gameui.c:1226`,
  `engine/menu_int.h:155`); menus otherwise drive saves/loads by issuing
  the console commands.
- **Host state machine** (`engine/common/host_state.c`): everything
  map-changing is deferred one frame through `GameState`.
  `SV_QueueChangeLevel` (`sv_game.c:721`) validates via `SV_MapIsValid`
  (landmark existence, `sv_validate_changelevel`, multiplayer forces
  non-smooth, `sv.framecount < 15` infinite-changelevel guard) then calls
  `COM_ChangeLevel` (host_state.c:109) → `STATE_CHANGELEVEL` →
  `SV_ExecChangeLevel` (`sv_init.c:1137`) → `SV_ChangeLevel`.
  `SV_LoadGame` ends with `COM_LoadGame(mapname)` (`sv_save.c:2189`) →
  `STATE_LOAD_GAME` → `SV_ExecLoadGame` (`sv_init.c:1120`) →
  `SV_SpawnServer` + `SV_LoadGameState` + `SV_ActivateServer(false)`.
- `SV_ClearGameState` from `SV_Init` (`sv_main.c:1014`) and
  `SV_ShutdownGame` (`sv_init.c:765-766`, skipped when a load is
  pending).

### Changelevel-with-landmark flow and ordering (`SV_ChangeLevel`, sv_save.c:2049-2117)

1. Requires `sv.state == ss_active`. Smooth path sets
   `svgame.globals->changelevel = true`.
2. **Old server still live:** `SaveGameState(true)` (:1469) — calls
   `pfnParmsChangeLevel` (game DLL fills
   `SAVERESTOREDATA.levelList[]`/`connectionCount` via its LEVELLIST
   logic), writes `<oldmap>.HL1` (ETABLE + header + adjacency +
   lightstyles + all entity data), `<oldmap>.HL3` (removed-entity patch),
   `<oldmap>.HL2` (client state: decals/static ents; sounds skipped on
   changelevel :1212). Failure aborts changelevel with `Sys_Warn`, server
   keeps running (:2080-2087).
3. `SV_InactivateClients` → `SV_FinalMessage` → `SV_DeactivateServer`.
4. `SV_SpawnServer(level, startspot, background)` — wipes `sv`,
   `sv.time = 1.0` (`sv_init.c:982-983`), loads map, world precache.
5. `LoadGameState(level, changelevel=true)` (:1628) restores the new
   level from its `.HL1` if previously visited; **fallback** to fresh
   `SV_SpawnEntities` if not (:2102-2103). Then
   `LoadAdjacentEnts(oldlevel, landmark)` (:1931): re-runs
   `pfnParmsChangeLevel` for the *new* level's connection list, and for
   each connected map loads its `.HL1`, computes the landmark offset,
   builds the transfer mask, and runs `CreateEntityTransitionList`
   (:1836) to pull entities (incl. the player from the previous map,
   `FENTTABLE_PLAYER` bit :1980-1981). Missing back-connection ⇒
   `Host_Error` (:2013-2014).
6. `sv_newunit` ⇒ `ClearSaveDir()`; finally `SV_ActivateServer(false)` (no
   physics settling: 1 frame at 0.001 vs 2-8 frames — `sv_init.c:606-615`).
   Classic (non-landmark) path instead: `pfnResetGlobalState` →
   `SV_SpawnEntities` → `SV_ActivateServer(true)`.
7. For a full **load game**, `pfnRestoreGlobalState` happens *earlier
   still*: inside `SaveReadHeader` (:1822) during `SV_LoadGame`, i.e.
   before the server is even spawned; `SV_LoadGame` also extracts all
   `.HL?` files from the `.sav` container (`DirectoryExtract`), validates
   the map, and forces `maxplayers 1`, `deathmatch 0`, `coop 0`
   (:2186-2188). Dedicated servers refuse `SV_LoadGame` entirely (:2131).

## 3. What it needs FROM the server core

**State read/written:**

- `sv`: `sv.name` (written on load before DLL calls! :1640), `sv.time`
  (restored `sv.time = header.time` :1692, after SpawnServer reset it to
  1.0), `sv.lightstyles[]` (saved :1513-1539; zeroed + rebuilt via
  `SV_SetLightStyle` on restore :996-1003), `sv.signon` sizebuf (restored
  decals/static ents/sounds are written as svc messages into signon
  :1141-1176, 1368), `sv.num_static_entities` + `svs.static_entities`,
  `sv.loadgame`/`sv.paused` (both set true :1647, :1941; cleared when the
  client spawns, `sv_client.c:1389-1390`), `sv.state`, `sv.background`,
  `sv.framecount`, `sv.hostflags`.
- `svs`: `initialized`, `maxclients` (==1 enforced), `clients[0]`
  (viewentity save :1226-1227, `pViewEntity` restore :1394-1395),
  `groupmask` (:486).
- `svgame`: `numEntities`, `edicts`, `globals->time` (save-time base),
  `globals->mapname` (`SV_MakeString`), `globals->pSaveData` (**shared
  pointer into the DLL**, set/cleared in SaveInit/SaveClear/SaveFinish
  :730/:753/:781 and in `LoadAdjacentEnts` :1940/:2011),
  `globals->changelevel`, `skill`/sky cvars,
  `GI->quicksave_aged_count`/`autosave_aged_count`.
- **Server-core services called:** `SV_EdictNum`, `SV_InitEdict`,
  `SV_CreateNamedEntity`, `SV_FindGlobalEntity`, `SV_FreeOldEntities`,
  `SV_IsValidEdict`, `NUM_FOR_EDICT`, `SV_PointContents`, `SV_Move`
  (decal re-trace :1129), `SV_CreateDecal`, `SV_CreateStaticEntity`,
  `SV_BuildSoundMsg`, `SV_SetLightStyle`, `pfnDecalIndex`,
  `SV_MapIsValid`, `SV_InitGame`, `SV_SpawnServer`/`SV_SpawnEntities`/
  `SV_ActivateServer`/`SV_DeactivateServer`/`SV_InactivateClients`/
  `SV_FinalMessage`, string pool (`SV_GetString`/`SV_MakeString`/
  `SV_AllocString`).
- **Outside the server:** FS_* + `host.mempool`/Mem_*, Cvar_*,
  `Cbuf_AddTextf("saveshot ...")` (:1753, client renders the `.bmp`
  preview), and on listen builds `ref.dllFuncs.R_CreateDecalList`,
  `S_GetCurrentDynamicSounds`, `S_StreamGetCurrentState`, `GL_FreeImage`,
  `CL_Active`, `CL_IsIntermission`, `UI_CreditsActive` (:1204-1222,
  :521-583).

**Game DLL callbacks driven:** `pfnSave` (:1555), `pfnRestore(ent, data,
globalEntity)` (:1674, :1879, :1894), `pfnSaveWriteFields`/
`pfnSaveReadFields` (used for *every* engine header — GAME_HEADER,
SAVE_HEADER, ETABLE, ADJACENCY, LIGHTSTYLE, ClientHeader, DECALLIST,
STATICENTITY, SOUNDLIST, temp ENTVARS), `pfnSaveGlobalState` (:1730),
`pfnRestoreGlobalState` (:1822), `pfnResetGlobalState` (:2040, :2113),
`pfnParmsChangeLevel` (:1495, :1944, and again per connecting client in
`SV_PutClientInServer` `sv_client.c:1362-1382`; its absence disables
saving entirely :1483). Optional: DLL export `SV_SaveGameComment` (comment
text; `pfnGetGameDescription` is *not* part of the save path — the
fallback is a hardcoded HL/OF/BS map-title table :229-304),
physics-interface hooks `SV_AllowSaveGame` (`physint.h:133`, veto at
:533-540) and `pfnCreateEntitiesInRestoreList` (`physint.h:154`, overrides
engine recreation loop :1417-1419). `SV_RestoreCustomDecal`
(`sv_game.c:483`) lets the game DLL claim decal restoration.

**Client-side handshake:** on first spawn after a restore, the server
sends `svc_restore` with the `.HL2` filename + connection list
(`sv_client.c:1372-1379`); the client (`cl_parse.c:1315 CL_ParseRestore`)
re-reads that file for decals. `svc_restoresound` carries sample-position
extradata (:1173-1176).

## 4. ABI surfaces

All in `engine/eiface.h`, part of the frozen interface (banner "INTERFACE
VERSION IS FROZEN AT 138", eiface.h:286):

- `LEVELLIST` (:298-304): mapName[32], landmarkName[32], `edict_t
  *pentLandmark`, vec3 origin. `MAX_LEVEL_CONNECTIONS = 16` (:318) —
  connection bits live in the low 16 bits of ENTITYTABLE.flags.
- `ENTITYTABLE` (:306-316): id, `edict_t *pent`, location, size, flags,
  string_t classname. Flag bits `FENTTABLE_PLAYER/REMOVED/MOVEABLE/GLOBAL`
  = top 4 bits (:320-323). Serialized to disk via `gEntityTable`
  TYPEDESCRIPTION (sv_save.c:130-137) — `pent` is not written; classname
  goes through the token table as FIELD_STRING.
- `SAVERESTOREDATA` (:325-346): buffer cursor pair, size/bufferSize, token
  table (`pTokens`/tokenCount/tokenSize), `currentIndex`,
  `pTable`/tableCount, inline `levelList[16]`, landmark fields
  (`fUseLandmark`, `szLandmarkName[20]`, `vecLandmarkOffset`, `time`,
  `szCurrentMapName[32]`). Handed to the DLL both as function argument and
  via `globalvars_t.pSaveData` (`engine/progdefs.h`).
- `TYPEDESCRIPTION` (:392-399) + `FIELDTYPE` enum (:348-370,
  FIELD_TIME/FIELD_POSITION_VECTOR are the rebased ones) + `DEFINE_FIELD`
  macros (:380-390).
- **Token hash system:** engine allocates `pTokens` (4095 entries,
  `SAVE_HASHSTRINGS`, sv_save.c:37) and a 4 MB data buffer
  (`SAVE_HEAPSIZE`, :36); the game DLL's `CSaveRestoreBuffer::TokenHash`
  populates it during field writes; engine flattens it with
  `StoreHashTable` (:792) and rebuilds pointers with `BuildHashTable`
  (:823). On-disk field encoding (short size, short token index, payload)
  is a de-facto ABI: `SV_GetSaveComment` parses it by hand (:2390-2436).
- `physint.h`: `SV_AllowSaveGame` (:133), `pfnCreateEntitiesInRestoreList`
  (:154). Note the latter is also abused outside save context as a generic
  entity factory in `SV_EntCreate_f` (`sv_client.c:2925-2942`, sentinel
  `flags = 1337`).

## 5. Owned state (statics/globals in sv_save.c)

Almost stateless between operations:

- `pfnSaveGameComment` function pointer (:86-89), set once per game DLL
  load.
- Const TYPEDESCRIPTION tables `gGameHeader/gSaveHeader/gAdjacency/
  gLightStyle/gEntityTable/gSaveClient/gDecalEntry/gStaticEntry/
  gSoundEntry/gTempEntvars` (:91-227) and `gTitleComments[]` (:229-304).
- `static char savename[]` in `SV_GetLatestSave` (:2263) — returned
  pointer into static buffer.
- Working `SAVERESTOREDATA` is heap-allocated from `host.mempool` per
  operation and freed by `SaveFinish`; the only cross-module leak of it is
  `svgame.globals->pSaveData` while an operation is in flight.

## 6. Format summary (brief — Chunk 8 does the full recon)

- Container `save/<name>.sav`: magic `JSAV` (`SAVEGAME_HEADER`), version
  `0x0071` (:31-33); GAME_HEADER (mapName/comment/mapCount) +
  `pfnSaveGlobalState` blob + token table, then verbatim concatenation of
  all `save/*.HL?` temp files (`DirectoryCopy` :647).
- Per-level `<map>.HL1`: magic `VALV` (`SAVEFILE_HEADER`) + version,
  counts (size/tableCount/tokenCount/tokenSize), token table, ETABLE
  array, then SAVE_HEADER + adjacency LEVELLIST entries + lightstyles +
  per-entity blobs.
- `<map>.HL2`: client state (version `0x0067`): SAVE_CLIENT header, decal
  list, static entities, dynamic sounds, music track/position, viewentity,
  wateralpha/wateramp.
- `<map>.HL3`: entity patch — plain int count + int indices of entities
  that left the level (FENTTABLE_REMOVED).
- No compression, little-endian raw structs; field encoding is (short
  size, short nameToken, bytes), all produced by the game DLL codec.
- Screenshot `save/<name>.bmp` taken asynchronously by the client via the
  `saveshot` command; slot aging renames both `.sav` and `.bmp`
  (`AgeSaveList` :592, counts from gameinfo).
- Fixed budgets: 4 MB buffer, 4095 tokens; `SV_GetSaveComment`
  sanity-checks against both (:2358-2370).

## 7. Boundary-relevant quirks

- **Time rebasing:** FIELD_TIME rebasing is done by the game DLL relative
  to `pSaveData->time`. Engine sets `pSaveData->time = 0` just around
  writing SAVE_HEADER so `header.time` itself isn't rebased (:1520-1522);
  on restore `pSaveData->time = header.time` (:989); for adjacent-map
  transfer `pSaveData->time = sv.time` (:1970); full restore ends with
  `sv.time = header.time` (:1692) *after* SpawnServer reset it to 1.0.
  `pfnLightStyle` refuses lightstyle writes while `sv.loadgame` so the
  map's worldspawn can't clobber restored styles
  (`sv_game.c:2443-2444`).
- **Edict recreation/remapping:** ETABLE is read first, all `pent` nulled
  (:978), then `CreateEntitiesInRestoreList` (:1410) recreates edicts by
  classname: id 0 = worldspawn reuses edict 0; ids 1..maxclients must be
  FENTTABLE_PLAYER and reuse client edicts; everything else
  `SV_CreateNamedEntity(NULL,...)` — so *indices are not preserved* for
  non-player entities; pointer↔index conversion during restore goes
  exclusively through the table (`EdictFromTable` clamps out-of-range
  :438). `pfnRestore < 0` ⇒ FL_KILLME (:1674-1678).
- **Landmark offset:** `vecLandmarkOffset = landmark(new) − landmark(old)`
  computed in `LoadAdjacentEnts` (:1976-1978); FIELD_POSITION_VECTOR fixup
  is DLL-side; engine applies it itself only to decals flagged
  FDECAL_USE_LANDMARK (subtract on save :1239-1240, add on load
  :1349-1350).
- **Transfer mask:** which entities cross is decided by
  `ENTITYTABLE.flags` connection bits set by the game DLL at save time;
  loader rebuilds the mask by finding the new map in the old level's
  connection list (`EntryInTable` loop :1983-1988), OR-ing
  `FENTTABLE_PLAYER` only for the immediately-previous map.
- **Global entities:** FENTTABLE_GLOBAL entities are *merged*, not
  spawned: engine pre-reads classname/globalname (gTempEntvars), resets
  the cursor, calls `pfnRestore(..., 1)`, and on failure repoints the
  table entry at the already-existing entity so decals still resolve
  (:1858-1888).
- **Post-transfer pruning:** transferred non-player entity whose center is
  CONTENTS_SOLID gets FL_KILLME (:1900-1906; MOVETYPE_FOLLOW-on-client
  exempt :482); successfully moved entities are marked FENTTABLE_REMOVED
  and the source level's `.HL3` is rewritten (:1993-2001).
- **Pause-until-spawn:** restore sets `sv.loadgame = sv.paused = true`
  (:1647/:1941); cleared only in `SV_PutClientInServer`, which also
  re-runs `pfnParmsChangeLevel` just to send the connection list to the
  client (`sv_client.c:1352-1391`).
- **Ordering hazard:** `sv.name`/`globals->mapname` must be set before any
  DLL restore call (:1639-1641); global state restore happens at
  `.sav`-header time, a whole state-machine step before the map exists
  (:1822).

## 8. Separation verdict input (save/restore as separate build target)

Feasible; legacy already keeps it in one file with a narrow inward API (8
functions in server.h). The interface a split would need to expose:

1. **From server core to save module** (services consumed): edict table
   access + lifecycle (`EdictNum`, `InitEdict`, `CreateNamedEntity`,
   `FindGlobalEntity`, `FreeOldEntities`, validity/index conversion),
   string-pool alloc/get, lightstyle get/set, static-entity list,
   signon-buffer message writers (decal/static/sound emit), world queries
   (`PointContents`, `Move`), `MapIsValid`, the sv/svs fields listed in §3
   (name, time, paused/loadgame, background, clients[0], maxclients), and
   the whole game-DLL dispatch table (`pfnSave/pfnRestore/pfnSave*Fields/
   pfn*GlobalState/pfnParmsChangeLevel` + physint hooks).
2. **From save module to server core** (calls out): the changelevel
   orchestration currently *inside* sv_save.c calls `SpawnServer/
   SpawnEntities/ActivateServer/DeactivateServer/InactivateClients/
   FinalMessage` — either the orchestrator (`SV_ChangeLevel`) stays
   server-core and the save module exposes only `SaveGameState/
   LoadGameState/LoadAdjacentEnts/ClearSaveDir`-grade primitives (cleanest
   cut), or the save target needs the server lifecycle API.
3. **Cross-cutting couplings that resist separation:**
   `svgame.globals->pSaveData` sharing (ABI-mandated), `sv.loadgame`
   checks sprinkled in sv_game.c/sv_client.c (lightstyle guard,
   PutClientInServer restore handshake, svc_restore),
   `SV_PutClientInServer`'s own `pfnParmsChangeLevel` invocation,
   `SV_EntCreate_f`'s reuse of `pfnCreateEntitiesInRestoreList`, and
   non-server dependencies (renderer decal list, sound engine snapshot,
   client UI queries) that a dedicated-server build already #ifdefs out —
   a rewrite boundary should model those as injected capabilities.
4. **Host-level ordering constraint:** the load/changelevel sequencing
   lives in the host state machine (`host_state.c` + `SV_Exec*` in
   sv_init.c), not in sv_save.c; the boundary spec should treat "when
   save/restore runs relative to spawn/activate" as a server-core contract
   with the save module purely passive.
5. ABI freeze: SAVERESTOREDATA/ENTITYTABLE/LEVELLIST/TYPEDESCRIPTION
   layouts and the token/field wire encoding are game-DLL-visible and must
   be vendored verbatim (eiface.h:298-399, physint.h:133/154, progdefs.h
   pSaveData).
