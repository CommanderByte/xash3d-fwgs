# Save Boundary Spec (Chunk 8)

> Draft assembled 2026-07-19 from A1 recon fragments V8.1 (seam
> re-verification), V8.2 (byte-level format recon), V8.3 (xash3dpp seam
> inventory) against `engine/server/sv_save.c` (2,492/2,493 lines, HEAD
> ec7f4d2a) and `engine/eiface.h`. Companion deep-dive:
> `legacy-survey/deep-dive-server-save-boundary.md` (+ its 2026-07-19 format
> internals append). Not yet implemented — save/sound/input = 0 TUs (A0 fact
> base).

## Responsibility

`sv_save.c` implements the GoldSrc-compatible save/restore and
landmark-changelevel system: it serializes engine-side headers plus
per-entity game-DLL data into per-level `.HL1/.HL2/.HL3` temp files and a
container `.sav`, and on restore recreates edicts and drives the game DLL to
repopulate them. It owns save-slot management (aging, comments, latest-save
lookup) and the entity-transfer half of smooth level transitions. It does
**not** own the field codec: all `TYPEDESCRIPTION`-based reading/writing —
including the engine's own headers — is delegated to the game DLL's
`pfnSaveWriteFields`/`pfnSaveReadFields` (A0 fact base; V8.3 Dependencies).
It does not own file I/O primitives (filesystem, sibling-scope), the
changelevel/loadgame *orchestration* (server-core, `ILevelChangeExecutor` —
sibling-scope), or the svc_restore wire format (networking, chunk 9).

## External ABI contracts

### Vendored SDK structures (`abi/eiface.hpp:131-211`)

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| eiface.h:318-324 | `LEVELLIST` — landmark array entry (mapname, landmarkname, landmark edict pointer, origin offset for coordinate rebase) | eiface.hpp:131-137: `struct LEVELLIST { mapName[32], landmarkName[32], pentLandmark, vecLandmarkOrigin }` | high |
| eiface.h:331-336 | `ENTITYTABLE` — per-entity save metadata row (id, pent, location, size, flags, classname); **`pent` is excluded from the wire format** (V8.2) | eiface.hpp:139-147: `struct ENTITYTABLE { id, pent, location, size, flags, classname }` | high |
| eiface.h:313-327 | `SAVERESTOREDATA` — the save-buffer-manager aggregate (base pointer, cursor, size/capacity, token table, entity table array, landmark list) | eiface.hpp:158-179: `struct SAVERESTOREDATA { pBaseData, pCurrentData, size, bufferSize, tokenSize, tokenCount, pTokens, currentIndex, tableCount, connectionCount, pTable[], levelList[] }` | high |
| eiface.h:348-370 | `FIELDTYPE` enum — serializer type codes (FLOAT, STRING, ENTITY, EHANDLE, VECTOR, …); `FIELD_FUNCTION` selects a game-DLL callback | eiface.hpp:182-203: `enum FIELDTYPE { … FIELD_TYPECOUNT }` | high |
| eiface.h:392-399 | `TYPEDESCRIPTION` — field-descriptor table row (type, name, offset, count, flags); `k_ftypedesc_save` (0x0002) marks a saveable field | eiface.hpp:211-215 + line 206 | high |

### `DLL_FUNCTIONS` save callbacks (game-DLL-owned)

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| eiface.h:429-437 | Seven save-related slots (8-14): `pfnSave`, `pfnRestore`, `pfnSaveWriteFields`, `pfnSaveReadFields`, `pfnSaveGlobalState`, `pfnRestoreGlobalState`, `pfnResetGlobalState` | eiface.hpp:429-437 | high |
| eiface.h:462-468 | `pfnChangeLevel` — game DLL requests a landmark transition; engine queues a `changelevel` console command that `ILevelChangeExecutor::exec_change_level` dispatches | eiface.hpp:244 + engine_table.cpp:464-467 | high |
| sv_save.c:2489-2491 | `SV_InitSaveRestore` resolves the *optional* DLL export `SV_SaveGameComment` into a file-static fn-ptr at load time | `pfnSaveGameComment = COM_GetProcAddress( svgame.hInstance, "SV_SaveGameComment" );` | high |
| sv_save.c:1417-1419 | Physint veto/override hook `pfnCreateEntitiesInRestoreList` wholesale-replaces the engine edict-recreation loop when set | `if( svgame.physFuncs.pfnCreateEntitiesInRestoreList != NULL ) { svgame.physFuncs.pfnCreateEntitiesInRestoreList( pSaveData, levelMask, create_world );` | high |
| sv_save.c:533-540 | Physint veto hook `SV_AllowSaveGame` — save can be refused entirely | `if( svgame.physFuncs.SV_AllowSaveGame != NULL ) { if( !svgame.physFuncs.SV_AllowSaveGame( )) { Con_Printf( "Savegame is not allowed.\n" );` | high |

### On-disk format as a compat surface

The `.sav` container format is itself an ABI contract — it is read/written
across engine versions and by external tools. Full byte-level map is in the
deep-dive append; the FORMAT constants below are load-bearing:

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sv_save.c:31-34 | Two on-disk magics/versions: `.HL1` uses `SAVEFILE_HEADER` = "VALV"; `.sav`/`.HL2` use `SAVEGAME_HEADER` = "JSAV". `SAVEGAME_VERSION` 0x0071 (`.sav`/`.HL1`), `CLIENT_SAVEGAME_VERSION` 0x0067 (`.HL2`) | `#define SAVEFILE_HEADER 'VALV' … #define SAVEGAME_HEADER 'JSAV' … #define SAVEGAME_VERSION 0x0071 … #define CLIENT_SAVEGAME_VERSION 0x0067` | high |
| sv_save.c:36-37 | Fixed capacity constants baked into the format: `SAVE_HEAPSIZE` 4 MiB (0x400000) working buffer, `SAVE_HASHSTRINGS` 4095 (0xFFF) max unique tokens | `#define SAVE_HEAPSIZE 0x400000 … #define SAVE_HASHSTRINGS 0xFFF` | high |
| sv_save.c:2317-2347 | Version compat is **exact-match only** — no forward/backward tolerance band; `tag < SAVEGAME_VERSION` → `"<old version>"`, `tag > SAVEGAME_VERSION` → `"<invalid version>"` | `if( tag < SAVEGAME_VERSION ) { … "<old version>" …} if( tag > SAVEGAME_VERSION ) { … "<invalid version>" …}` | high |

### Compat scope (Q-12)

Per `decisions-architecture.md` Q-12 (per-subsystem `ICompatPolicy`), save
owns a narrow compat surface, not an engine-wide table:

- **Legacy saves must be loadable in both directions**: a `.sav` written by
  the legacy engine loads in xash3dpp, and vice versa, for the frozen
  `SAVEGAME_VERSION 0x0071` / `CLIENT_SAVEGAME_VERSION 0x0067` tags. The
  version-exact-match gate (sv_save.c:2317-2347) is preserved verbatim — no
  new tolerance band is introduced by the rewrite.
- **Native-endian, little-endian only, recorded (not solved)**: the legacy
  format has no byte-order tag; all field reads/writes are raw
  `memcpy`-shaped host-order copies (V8.2 field-encoding rows). xash3dpp
  targets MSVC x64/x86, both LE — this spec **records** the LE-only
  assumption as a compat-scope boundary rather than adding a portability
  layer. Cross-endian save exchange is out of scope.
- **Compat carrier**: `save::ICompatPolicy` (naming per Q-12), selected at
  link time, holding exactly the version-tag/magic table above — no
  cross-subsystem compat router.

## Interface (what the rest of the engine calls)

Per the §8.2 "cleanest cut" (V8.1, re-verified against current source with
**no drift**): the save module exposes exactly four primitives, all
file-static in legacy `sv_save.c`; `SV_ChangeLevel` stays the server-core
orchestrator and is **not** one of the four.

| Function | Legacy site | Purpose |
|---|---|---|
| `SaveGameState` | sv_save.c:1469 `static SAVERESTOREDATA *SaveGameState( int changelevel )` | Serialize current level state (full save, or landmark-transition snapshot when `changelevel` is set) |
| `LoadGameState` | sv_save.c:1628 `static int LoadGameState( char const *level, qboolean changelevel )` | Deserialize a level's saved state and drive entity recreation |
| `LoadAdjacentEnts` | sv_save.c:1931 `static void LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName )` | Merge/transfer entities carried across a landmark transition |
| `ClearSaveDir` | sv_save.c:499 `static void ClearSaveDir( void )` | Delete `*.HL?` temp files from the save directory |
| `SV_GetSaveComment` | sv_save.c:2261 (called from `common.h:783`, exported to menu DLL as `pfnGetSaveComment`) | Hand-parse a `.sav` header to produce a UI comment string, without invoking the game DLL |

`SV_ChangeLevel` (sv_save.c:2049-2117, server-core orchestrator, called by
`ILevelChangeExecutor::exec_change_level`) drives the four primitives plus
the server lifecycle API inline (V8.1):

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sv_save.c:2078,2100,2102,2104,2107 | orchestrator body calls all four primitives | `pSaveData = SaveGameState( true );` … `if( !LoadGameState( level, true ))` … `LoadAdjacentEnts( oldlevel, startspot );` … `ClearSaveDir();` | high |
| sv_save.c:2090-2094,2108,2113-2115 | orchestrator calls server-lifecycle API (owned by server, not save) | `SV_InactivateClients (); SV_FinalMessage( "", true ); SV_DeactivateServer ();` … `if( !SV_SpawnServer( level, startspot, background ))` … `SV_ActivateServer( false );` | high |

### xash3dpp orchestration seam (server-core, ILevelChangeExecutor)

The four primitives + `SV_GetSaveComment` sit **behind**
`ILevelChangeExecutor`, which server-core owns (sibling-scope rule):

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| map_loader.hpp:65-80 | `ILevelChangeExecutor` — three virtual methods: `exec_load_level`, `exec_load_game`, `exec_change_level`; Server is the registered implementation | `ILevelChangeExecutor` interface, `exec_load_game` (L73), `exec_change_level` (L77) | high |
| server.cpp:147-154 | `exec_load_game` stub: names SV_LoadGame staging + spawn/activate settle-frame path as the Chunk 8 body | `XASH3DPP-STUB(chunk8): savegame restore (SV_LoadGame staging + the spawn/activate(false) settle-frame path)` | high |
| server.cpp:156-164 | `exec_change_level` stub: names landmark transition + CHANGE_LEVEL fixups as the Chunk 8 body | `XASH3DPP-STUB(chunk8): landmark transition (adjacent-level save staging, CHANGE_LEVEL fixups)` | high |
| engine_table.cpp:1436-1444 | `pfnFunctionFromName`/`pfnNameForFunction` (symbol↔ordinal table for `FIELD_FUNCTION`) stubbed, marked Chunk 8 | `XASH3DPP-STUB(chunk6): the save/restore symbol↔ordinal table (COM_FunctionFromName_SR) lands with Chunk 8` | med |
| game_host.cpp:150-151 | `SV_InitSaveRestore` (grabs `SV_SaveGameComment`) stubbed, marked Chunk 8 | `XASH3DPP-STUB(chunk6): SV_InitSaveRestore (SV_SaveGameComment grab) — Chunk 8 save seam` | med |

## Dependencies

| Dependency | Used for | Evidence |
|---|---|---|
| `filesystem/` (sibling-scope, owns file I/O) | `.sav`/`.HL1-3` read/write; save-directory `search`/`remove` glob surface | V8.3 Dependencies: "Save/load delegates to filesystem subsystem for .sav file I/O" |
| `server/` core (sibling-scope, owns changelevel/loadgame orchestration) | `ILevelChangeExecutor` dispatch, `SV_ChangeLevel`/`SV_SpawnServer`/`SV_ActivateServer`/`SV_InactivateClients`/`SV_FinalMessage`/`SV_DeactivateServer` lifecycle calls, the edict arena | V8.1 Interface; V8.3 "MapLoader FSM State" |
| `networking/` (sibling-scope, owns svc_* wire formats) | `svc_restore` message during loadgame client-spawn append; depends on Chunk 9 | client_state.cpp:478-479 (V8.3) — **uncertain**, no xash3dpp wire format exists yet |
| Game DLL (`DLL_FUNCTIONS`) | field codec (`pfnSaveWriteFields`/`pfnSaveReadFields`), per-entity save/restore (`pfnSave`/`pfnRestore`), global state (`pfnSaveGlobalState`/`pfnRestoreGlobalState`/`pfnResetGlobalState`), symbol resolution for `FIELD_FUNCTION` | eiface.h:429-437; A0 fact base "field codec is game-DLL-owned" |
| Physint (`svgame.physFuncs`) | `SV_AllowSaveGame` veto, `pfnCreateEntitiesInRestoreList` override | sv_save.c:533-540, 1417-1419 |
| `cmd_cvar/` | `sv_autosave`, `sv_newunit`, save-related console commands (`save`/`load`/`savequick`/`loadquick`/`autosave`/`reload`/`killsave`) | deep-dive §2 |
| `server/` physics | `rt.globals.changelevel` freeze check during restore | physics.cpp:721,820 (V8.3) |

## Owned state

| Name | Type/shape | Description | Legacy site |
|---|---|---|---|
| `pfnSaveGameComment` | file-static fn-ptr | Optional DLL-provided comment formatter; write-once at DLL load, read-only after | sv_save.c:86-89 |
| `savename` | `static char[MAX_QPATH]` | `SV_GetLatestSave`'s return buffer — classic static-return-buffer pattern | sv_save.c:2263 |
| `SAVERESTOREDATA` working buffer | heap, up to `SAVE_HEAPSIZE` (4 MiB) | The in-flight save/restore buffer allocated per save/load operation; not persistent state between operations | sv_save.c:36 |
| `globalvars_t::pSaveData` | ABI-shared `void*` (cast `SAVERESTOREDATA*`) | Set to the working buffer for the duration of `LoadAdjacentEnts`, cleared to NULL after — an ABI-mandated engine↔game-DLL sharing window | sv_save.c:1940,2011; edict.hpp:265 |
| `globalvars_t::changelevel` | `int` flag | Set true during a landmark transition; read by movement/physics/entity-state code to freeze updates | edict.hpp:259; V8.3 |

## Quirks and invariants

### Ordering (server-core orchestration, preserved by SV_ChangeLevel)

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sv_save.c:2057-2116 | Changelevel-with-landmark step sequence: guard `ss_active` → `SaveGameState(true)` (abort keeps server running on fail) → inactivate/final/deactivate → SpawnServer → SaveFinish → `LoadGameState` (fallback `SpawnEntities`) → `LoadAdjacentEnts` → optional `ClearSaveDir` → `ActivateServer(false)`; classic load path = ResetGlobalState/SpawnEntities/ActivateServer(true) | `if( sv.state != ss_active )` … `pSaveData = SaveGameState( true );` … `if( !LoadGameState( level, true )) SV_SpawnEntities( level ); LoadAdjacentEnts( oldlevel, startspot );` | high |
| sv_save.c:2080-2087 | Save-failure aborts the changelevel via `Sys_Warn` (**not** `Host_Error`) — the server keeps running | `Sys_Warn( "Can't write save file for performaing change level; check permissions" ); svgame.globals->changelevel = false; return;` | high |
| sv_save.c:1640-1641 | `sv.name`/`globals->mapname` must be set before any DLL restore call | `// must set mapname before calling into DLL` | high |
| sv_save.c:1941,1647 | Restore pauses until clients connect: `sv.loadgame = sv.paused = true` | `sv.loadgame = sv.paused = true;` | high |
| sv_save.c:2013-2014 | Missing back-connection to the previous map is a **hard fail** (`Host_Error`) | `if( !foundprevious ) Host_Error( "Level transition ERROR\nCan't find connection to %s from %s\n", pOldLevel, sv.name );` | high |
| sv_save.c:1822 | `pfnRestoreGlobalState` fires **before** map spawn — inside `SaveReadHeader`, at `.sav`-header time | `svgame.dllFuncs.pfnRestoreGlobalState( pSaveData );` | high |

### FIELD_TIME rebasing

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sv_save.c:1970 | Adjacent-map transfer: `pSaveData->time = sv.time` (not rebased against header) | `pSaveData->time = sv.time; // - header.time;` | high |
| sv_save.c:1692 | Full restore: `sv.time = header.time`, restored **after** `SpawnServer` reset it | `sv.time = header.time;` | high |
| sv_save.c:1816 | `SaveReadHeader` zeroes `pSaveData->time` so the header's own time isn't double-rebased | `pSaveData->fUseLandmark = false; pSaveData->time = 0.0f;` | high |

### Edict recreation / entity transfer

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sv_save.c:1434-1453 | Edict indices are **not preserved** for non-players: id 0 reuses edict 0, ids 1..maxclients reuse client edicts (must be `FENTTABLE_PLAYER`), everything else via `SV_CreateNamedEntity(NULL, …)` | `if( pTable->id == 0 && create_world ) …` … `else if( active ) { pent = SV_CreateNamedEntity( NULL, pTable->classname );` | high |
| sv_save.c:1674-1678 | `pfnRestore < 0` ⇒ `FL_KILLME` set, table pointer nulled | `if( svgame.dllFuncs.pfnRestore( pent, pSaveData, 0 ) < 0 ) { SetBits( pent->v.flags, FL_KILLME ); pTable->pent = NULL;` | high |
| sv_save.c:1858-1888 | Global-entity **merge**: pre-read classname/globalname, `SV_FindGlobalEntity`, `pfnRestore(...,1)`; on failure repoint the table at the existing entity + `FL_KILLME` | `pNewEnt = SV_FindGlobalEntity( tmpVars.classname, tmpVars.globalname ); … if( svgame.dllFuncs.pfnRestore( pent, pSaveData, 1 ) > 0 ) … else { … SetBits( pent->v.flags, FL_KILLME );` | high |
| sv_save.c:1900-1910 | Post-transfer pruning: transferred non-player-in-solid ⇒ `FL_KILLME`; moved entity marked `FENTTABLE_REMOVED` | `if( !FBitSet( pTable->flags, FENTTABLE_PLAYER ) && EntityInSolid( pent )) … SetBits( pent->v.flags, FL_KILLME ); … else { pTable->flags = FENTTABLE_REMOVED; movedCount++;` | high |

- **S8.3 writer dependency** (sv_save.c:1541-1559, the `SaveGameState` row
  loop that builds `ENTITYTABLE` rows before calling `pfnSave`): the loop
  reads `pTable->pent` directly — `SV_IsValidEdict( pTable->pent )` gates
  the `pfnSave` call, and `FBitSet( pTable->pent->v.flags, FL_CLIENT )` sets
  `FENTTABLE_PLAYER`. The S8.3 writer must therefore populate `row(i).pent`
  from the edict arena before invoking `pfnSave` (both FENTTABLE_PLAYER
  tagging and validity screening depend on it); `EntityTable::init()`
  leaves `pent` null, matching the read side.

### Format-level quirks (compat-relevant)

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sv_save.c:64-76 | `SAVE_CLIENT.viewentity` is a `short` but is serialized via `FIELD_CHARACTER, sizeof(short)` (2 raw bytes), **not `FIELD_SHORT`** — legacy comment says mods based on HLU SDK disallow `FIELD_SHORT` | `short viewentity; // Xash3D added` / `// mods based on HLU SDK disallow usage of FIELD_SHORT` | high |
| sv_save.c:1384 | `bound(0, (word)header.viewentity, tableCount)` casts a signed `short` to unsigned before bounding — a negative viewentity silently degrades to "last table index" rather than being rejected | `edict_t *pent = pSaveData->pTable[bound( 0, (word)header.viewentity, pSaveData->tableCount )].pent;` | high |
| sv_save.c:1226-1227,1394-1395,1440-1449 | Client/player state assumes exactly one client (`svs.clients` slot 0, no loop) — a wire-format-level single-player invariant, not just a UX gate | `sv_client_t *cl = svs.clients;` | high |
| sv_save.c:1739-1742 vs 2470-2473 | Quicksave/autosave rotation naming (`Q_stricmp` exact-stem, save-time) is a **different predicate** than the comment-prefix classification (`Q_strstr` substring, comment-time) — a save merely containing "quick" as a substring gets `[quick]` prefixed without triggering rotation | `if( !Q_stricmp( pSaveName, "quick" ))` vs `if( Q_strstr( savename, "quick" ))` | med |

### Adjudicated deviations from legacy (rewrite closes/changes behaviour)

| legacy file:line | claim | verbatim evidence | confidence | disposition |
|---|---|---|---|---|
| sv_save.c:2094-2095 | **`SV_SpawnServer` failure `pSaveData` leak**: on the smooth-transition path, if `SV_SpawnServer` fails, the function returns with the just-saved `pSaveData` never `SaveFinish`'d — legacy marks this `// ???` itself | `if( !SV_SpawnServer( level, startspot, background )) return; // ???` | med | **Deviation from legacy (bug fix)**: the rewrite orchestrator CLOSES this leak — the buffer must be released on every early-return path, not replicated as a leak. Recorded as an intentional deviation, not a parity requirement. |
| sv_save.c:679-706, 2155-2156 | **Unconditional-extract extension-blindness**: `DirectoryExtract` writes every embedded record verbatim by whatever name/extension is embedded (`fileCount` = attacker/corruption-controlled `gameHeader.mapCount`), with zero extension awareness or allowlist check | `for( i = 0; i < fileCount; i++ ) { FS_Read( pFile, szName, MAX_OSPATH ); … pCopy = FS_Open( fileName, "wb", true ); … FS_FileCopy( pCopy, pFile, fileSize ); }` | high | **Preserved quirk, load-bearing for SAV-OQ-1**: an unrecognized embedded record is extracted, not rejected — the extension door (SAV-OQ-1) must be designed so its side-block namespace is invisible to this loop by construction, not by adding a check here. |
| sv_save.c:499-512 (`ClearSaveDir`) vs 679-706 (`DirectoryExtract`) | **`*.HL?` cleanup-glob asymmetry**: the only post-extraction cleanup pass globs `*.HL?` (single-char wildcard, HL1/HL2/HL3 only); a record extracted with an unrecognized extension/name is never matched by this glob and is **orphaned on disk indefinitely** | `t = FS_Search( DEFAULT_SAVE_DIRECTORY "*.HL?", true, true ); … for( i = 0; i < t->numfilenames; i++ ) FS_Delete( t->filenames[i] );` | high | **Preserved quirk, load-bearing for SAV-OQ-1**: any new "extra embedded file" convention must independently match this glob (e.g. `.HLX`) so it is BOTH cleaned up by legacy tooling AND ignored by legacy exact-name readers — see SAV-OQ-1. |
| sv_save.c:2317-2347, 2358-2370, 2390-2436 | **`SV_GetSaveComment` locale/localtime pins**: the comment hand-parse formats a save timestamp via the C locale/`localtime` machinery (platform-owned, not re-derived here) and hand-decodes the token/field-block wire format independently of the game DLL | `// short, short (size, index of field name)` (hand-parse comment) | high | **Preserved quirk**: `SV_GetSaveComment` must remain a standalone hand-parse (no game-DLL dependency) so the save-list UI works without loading a level; the locale-dependent date/time formatting is out of save's compat scope (platform-owned) and is not re-specified here. |
| sv_save.c:1479-1480, 1502-1503, 1534 + public/crtlib.h:232-252 | **Zero-filled FIELD_CHARACTER tails (deterministic canonicalization)**: legacy writes SAVE_HEADER/SAVE_LIGHTSTYLE from UNINITIALIZED stack structs via non-padding `Q_strncpy` — every byte past a string's NUL is stack garbage, so no single legacy byte image even exists. The rewrite zero-fills. Structural consequence: an EMPTY variable-width char field (e.g. `skyName` with `sv_skyname ""`) is deterministically DataEmpty-skipped by the rewrite (field count 3, no `skyName` token), where legacy's garbage-dependent bytes usually force a written record (count 4 + interned token). | `Q_strncpy` = strlcpy (no zero-pad); `SAVE_HEADER header;` uninitialized | high | **Deviation (canonicalization, S8.3 parity audit 2026-07-19)**: round-trip-safe both directions (HL-SDK ReadFields pre-zeroes omitted fields; char arrays are consumed as C strings stopping at NUL). Recorded here per the deviation-table rule; no code change. |
| eiface.hpp: `FENTTABLE_*`/`FTYPEDESC_*` | **Deliberate non-wrap note**: `ENTITYTABLE.flags` (`FENTTABLE_PLAYER`/`GLOBAL`/`REMOVED`) and `TYPEDESCRIPTION.flags` (`FTYPEDESC_*`, incl. `k_ftypedesc_save`) are raw bit-flag `int`s in the vendored ABI structs, deliberately **not** wrapped in an enum-class/typed-flags helper at the ABI boundary | eiface.hpp:139-147 (`ENTITYTABLE.flags` is `int`), :211-215 (`TYPEDESCRIPTION` flags) | high | **Not a deviation — recorded as-is**: these are frozen wire-adjacent ABI fields (SetBits/FBitSet macro idiom throughout sv_save.c); wrapping them would require an adapter layer at every game-DLL call site for no behavioural benefit. The engine-internal `SAVERESTOREDATA`/`ENTITYTABLE` handling code may still use typed helpers internally as long as the ABI-facing struct layout is untouched. |

## Satellite components

Q-11 test (`decisions-architecture.md` §Q-11) applied to **save as a
candidate separate CMake target** from `server`:

| Criterion | Save (this candidate) |
|---|---|
| Shares the parent's wire/format protocol (lives or dies with it) | Partially — the `.sav` format is self-contained, but `SV_ChangeLevel` orchestration and the edict arena are server-core |
| Has its own independent protocol/state machine | **Yes** — `SAVERESTOREDATA` buffer lifecycle, token table, entity table are entirely save-local |
| Pulls in a different external dependency the parent does not need | No — same filesystem/memory dependencies as server |
| Consumer surface fits in a small interface the parent exposes | **Yes** — exactly four primitives + `SV_GetSaveComment`, all called through `ILevelChangeExecutor` |
| Useful without the parent at runtime | No — save has no meaning without a live server/edict arena to serialize |

**Score: 2/5 ("Has its own independent state machine", "small consumer
interface") → separate target** by the letter of the ≥2 rule.

**Verdict with the codec-testability argument (overriding the raw score):**
**separate `xash3dpp_save` target**, alongside `xash3dpp_server`. The
deciding factor beyond the raw Q-11 score is **codec testability**: the
byte-level format (token table, ETABLE, field-record framing — deep-dive
append) is pure state→bytes machinery that can and should be unit-tested
against golden `.sav`/`.HL1-3` fixtures *without* spinning up a live server,
edict arena, or game DLL. A same-target design would force every format
test through `xash3dpp_server`'s full dependency graph. This mirrors the
Chunk 8 extension-goals hook (§4): "the field-map serializer is state→bytes
machinery; **consider** shaping it for reuse by debug dumps / snapshots" —
a separate target with a narrow, typed `IFieldSink` boundary (SAV-OQ-2) is
the shape that makes that reuse possible later without contorting the
primary parity path. The four primitives + `SV_GetSaveComment` remain the
entire public surface `server` calls through `ILevelChangeExecutor`; the
`.sav` codec internals (token table, ETABLE, DirectoryCopy/Extract, field
framing) are `xash3dpp_save`-private.

## Extension axes (Q-21)

Evaluated against `xash3dpp/docs/design/extension-goals.md` §2/§3 (G-1..G-5,
P-1..P-8) as read at draft time.

| Goal / primitive | Applies? | Required seam or door |
|---|---|---|
| **P-2** published-snapshot reads | **Yes** — via `IFieldSink` door | The field-map serializer is state→bytes machinery (Chunk 8 hook, §4). Splitting the writer behind an `IFieldSink` interface (SAV-OQ-2) means a future debug-dump/snapshot consumer gets a second `IFieldSink` implementation rather than a bespoke walker over `SAVERESTOREDATA`. Door-keep, not built now. |
| **P-4** typed introspection surfaces | **Yes** — via `IFieldSink` door | Same seam as P-2 from the read side: the load path (SAV-OQ-2, "load path as pure parse helpers") exposes typed field records rather than raw buffer offsets, so a future typed query surface extends the parse helpers instead of re-deriving byte offsets. |
| **G-2** Game ABI v2 | Via SAV-OQ-2 | `IFieldSink` is a context-carrying seam by construction (P-3-conformant) — a v2 ABI's save path would bind to the same interface rather than the raw `SAVERESTOREDATA*` the legacy ABI hands across the boundary. Door-keep only; no v2 design here. |
| **G-5** scripting runtime | Via SAV-OQ-2 | A future scripted save-inspection tool (debug automation) is a natural `IFieldSink` consumer for read-only dumps — no save-specific work needed beyond keeping the seam typed. Not built now. |
| **P-3** context-first entry points | **Yes — held** | All four primitives and `SV_GetSaveComment` are candidates for context-carrying signatures (no new file-scope save state beyond the two documented statics in Owned state) as they move off `file-static` legacy shape into `xash3dpp_save` — the working `SAVERESTOREDATA` buffer becomes an owned object/parameter, not implicit state. |
| **Stats row** | **Yes** | `SaveStats` (save/load counts, bytes written/read, entity-transfer counts — shape TBD at implementation) as **always-on Tier-1 atomics**, per the three-tier stats model — the P-2/G-3 any-thread read surface for a future debug-thread or MCP consumer, consistent with the networking-boundary `NetworkingStats` precedent. |
| **G-1** in-engine MCP service | Consumer only | Reads the `SaveStats` atomics (stats row above) and the typed `IFieldSink` read surface (P-4); no save-specific action surface — save mutations stay behind `ILevelChangeExecutor`, gated by the existing `ITrustOracle` at the command-dispatch layer (owned by cmd_cvar, not save). |
| **G-3** dedicated debug thread | Consumer only | Same `SaveStats` atomics; must never touch the live `SAVERESTOREDATA` working buffer (T_Main-only, see Threading). |
| **G-4** expanded in-game debugging | Consumer only | A save-list/format overlay extends the typed `IFieldSink` read surface (P-4 rule); no private backdoor into the token table. |
| **P-1** main-thread service inbox | None | Save has no off-main mutation surface to marshal — it is entirely T_Main by contract (Threading section) with no producer thread that would need P-1. Reasoned "none": nothing in save runs off-main, so there is nothing to inbox. |
| **P-5** narrowest-state signatures | None beyond the general P-3 note | Save's internal helpers (token-table flatten, ETABLE walk, DirectoryCopy/Extract) already operate on narrow local structures (`SAVERESTOREDATA*`, a single file handle) rather than a whole-engine aggregate; no wide-signature refactor is anticipated. Reasoned "none": the legacy shape is already narrow at the primitive level. |
| **P-6** services are satellites | Addressed above | See Satellite components — `xash3dpp_save` itself is the Q-11-scored satellite of `server`, not a service satellite of save. |
| **P-7** pool-owned RAII lifecycle | Applies generically | The `SAVERESTOREDATA` working buffer becomes a pool-owned RAII object (`create_save_buffer` factory + `pool_new`) per the P-7 idiom rather than the legacy `Mem_Calloc` + manual free; no save-specific deviation from the standard pattern. |
| **P-8** annotation discipline | Applies generically | Standard `@thread-safety`/`@lifetime` annotation on the T_Main-confined types (Threading section); no save-specific nuance beyond Appendix-A duty (see Threading). |

## Threading

Save/restore runs entirely on **T_Main** in legacy — driven from the host
state machine and console commands — and remains **T_Main-only** under the
ratified xash3dpp thread model (A0 fact base: "input/save = T_Main"; no
save-specific thread role is introduced).

| Shared state | Hazard class | Legacy thread | xash3dpp thread | Notes |
|---|---|---|---|---|
| `pfnSaveGameComment` (file-static fn-ptr) | **Race-lazy-init** in principle (write-once at DLL load, then read-only) | T_Main only | T_Main only | Both models are T_Main-only, so this is effectively Safe-RO after init; the classification is recorded for completeness per the Step-2 rubric, not because a real race exists today. sv_save.c:86-89. |
| `savename` (`static char[MAX_QPATH]` return buffer, `SV_GetLatestSave`) | **Race-static-buf** | T_Main only | T_Main only | Legacy static-return-buffer pattern; safe only because every caller is T_Main. A pool-owned/caller-supplied buffer (P-7-adjacent) removes the hazard class entirely in the rewrite rather than merely re-confining it. sv_save.c:2263. |
| `SAVERESTOREDATA` working buffer, token table, `pSaveData` cursor | **Race-shared** (shared, mutable, unsynchronized) if ever touched off-main | T_Main only | T_Main only | Entire buffer lifecycle (alloc → fill → write/parse → free) happens within one T_Main call chain per save/load operation; no cross-thread handoff exists in legacy or in the ratified model. |
| `globalvars_t::pSaveData` (ABI-shared pointer window) | **Race-shared** in principle (raw pointer visible to game DLL) | T_Main only (game DLL calls happen synchronously from T_Main) | T_Main only | Set/cleared within `LoadAdjacentEnts`; the game DLL callback that dereferences it runs synchronously on the same thread — no concurrent access, but the pointer itself is a documented ABI exception per P-3's file-scope-state door rule (server owns the exception listing, sibling-scope). |

### Entry-point assertion contract

Every public entry point (the four primitives, `SV_GetSaveComment`, and the
`ILevelChangeExecutor::exec_load_game`/`exec_change_level` dispatch) asserts
`ThreadRole::Main` on entry — save has no async/off-main path in either
legacy or the ratified model, so there is no `compliance-allow(thread-assert)`
carve-out needed here (contrast networking's T_NetIO-ready posture): the
assert is wired in immediately, not deferred to a future thread split. All
game-DLL dispatch (the seven `DLL_FUNCTIONS` save callbacks) is called
synchronously from within the T_Main-asserted entry points.

### Annotation duty (Appendix-A / P-8)

The two file-scope statics (`pfnSaveGameComment`, `savename`) are the only
candidates for the P-3 "documented ABI exception" table; `savename` should
be eliminated (see Race-static-buf row) rather than merely annotated, per
the general rewrite policy of removing static-return-buffer hazards where a
caller-owned buffer is a drop-in replacement. `@thread-safety: T_Main-only,
asserted` is the standard annotation on `SaveContext`/`ISaveCodec`-shaped
types once implementation names them.

## Open questions

### SAV-OQ-1 — Format extension door (reserved embedded-file namespace)

**Question**: how does a future xash3dpp-only save feature (e.g. an extra
embedded debug/metadata block) get shipped inside a `.sav` container without
breaking round-trip compatibility with legacy readers/writers?

**Evidence for the constraint** (V8.2 "CRITICAL: extension-tolerance
verdict"): the legacy loader (`DirectoryExtract`) extracts every embedded
record blindly by whatever name is stored in the record — it has **zero
extension awareness** and never chokes on an unrecognized name. But the
**write-side** glob (`SaveGameSlot`'s `*.HL?`) and the **cleanup-side** glob
(`ClearSaveDir`'s `*.HL?`) are both hardcoded to the single-char wildcard
`*.HL?`, matching only `.HL1`/`.HL2`/`.HL3`. A new side-block extension must
independently satisfy two properties simultaneously:

1. **Cleaned up** by the existing `*.HL?` glob (so it doesn't orphan on
   disk indefinitely, per the confirmed orphan-file quirk above) — which
   means it must be a 3-character extension starting with `HL` (the `?`
   wildcard is single-character exact, per V8.2's own caveat about
   filesystem-owned glob semantics not being re-derived here).
2. **Ignored** by legacy exact-name readers (`GetClientDataSize`,
   `LoadSaveData`, `EntityPatchWrite/Read` all open by hand-built exact
   filename, never by directory scan) — automatically satisfied for any
   name legacy doesn't construct itself.

**Recommended shape**: reserve `.HLX` (matches `*.HL?`, distinct from
`HL1`/`HL2`/`HL3`) as the side-block extension. Each side block is
self-describing: magic + version + size header (mirroring the
`GAME_HEADER`/`SAVE_HEADER` pattern), so a foreign reader can skip an
unknown block without crashing. **No producer ships now** — this is a
door-keep only: the extension-tolerance property (skip logic) and a
foreign-block round-trip test (write a `.sav` with one legacy-unaware
`.HLX` side block; confirm a build without the SAV-OQ-1 feature still loads
the save, ignoring the side block) are the Chunk 8 deliverable. No
xash3dpp-specific side-block *content* is designed here.

**Blocks classification**: **blocks-scaffold** — the extension namespace
(`.HLX`) and the skip-logic contract must be decided before `xash3dpp_save`
is scaffolded, so the container writer/reader shape doesn't need
retrofitting later (cf. the networking source-folder-layout "lesson
learned" precedent). The side-block *content* design does not block
scaffolding.

### SAV-OQ-2 — IFieldSink adoption

**Question**: does the rewrite introduce an `IFieldSink`-shaped seam for the
field-map serializer (writer split; load path as pure parse helpers), and
if so, at what granularity?

**Recommended shape**: `IFieldSink` is a typed interface with (at minimum)
`write_field(name, type, span<byte>)` / a corresponding read-side parse
helper that returns typed field records rather than raw offsets — mirroring
how `SV_GetSaveComment`'s hand-parse (deep-dive append) already independently
proves the wire format is `short size, short token-idx, payload` triplets.
The writer split lets a debug-dump consumer (P-2/P-4, Extension axes above)
implement a second `IFieldSink` without touching the primary save-write
path; the load path as pure parse helpers keeps `SV_GetSaveComment`-style
standalone parsing (no game-DLL dependency) as a first-class supported mode
rather than an ad hoc hand-roll. Since the underlying field *codec*
(`pfnSaveWriteFields`/`pfnSaveReadFields`) remains game-DLL-owned per the A0
fact base, `IFieldSink` is an engine-side **framing** seam (record
boundaries, token-table indirection) — it does not reach into or reimplement
the game-DLL's field-value encoding.

**Blocks classification**: **blocks-scaffold** for the interface shape
(affects whether `xash3dpp_save` is structured writer/reader-split from day
one); the debug-dump/MCP *consumer* is not blocking (door-keep, no consumer
scheduled per extension-goals §6).

### SAV-OQ-3 — FIELD_FUNCTION reverse symbol lookup (platform capability)

**Question**: `COM_FunctionFromName_SR` (symbol→ordinal) and
`COM_NameForFunction` (ordinal→name, reverse lookup) resolve `FIELD_FUNCTION`
callbacks by name for the game-DLL field codec (V8.3: engine_table.cpp:1436-
1444, stubbed, named as landing with Chunk 8). Reverse symbol lookup
(address→exported-name) is fundamentally a **platform/dynlib capability**
(symbol table walk over the loaded game-DLL module), not save-specific
logic — does it belong in `platform/` (sibling-scope: "platform owns …
dynlib …") with save only consuming it, or does save own a thin adapter?

**Recommended shape**: the underlying reverse-symbol-walk primitive belongs
in `platform/` per the sibling-scope rule (platform already owns dynlib);
save owns only the `FIELD_FUNCTION`-specific glue — the `TYPEDESCRIPTION`
walk that decides *when* a field needs symbol resolution and the ordinal
table that caches results for the current game DLL's lifetime. This keeps
the platform capability reusable (e.g. for future debug-symbol features)
while save stays focused on the save-file framing concern.

**Blocks classification**: **blocks-implementation** (not blocks-scaffold)
— `xash3dpp_save`'s target/interface shape does not depend on this
resolution; only the Chunk 8 body implementing `FIELD_FUNCTION` support
needs the platform-vs-save ownership question settled, and it can be
decided after scaffolding once `platform/`'s dynlib surface is checked for
an existing reverse-lookup primitive.

## Uncertainties (carried from A1 fragments, not resolved by assembly)

- The exact byte layout `pfnSaveWriteFields`/`pfnSaveReadFields` implement
  is game-DLL-owned code not present in this repo; the "short size, short
  token idx, payload" claim is derived with high confidence from
  `SV_GetSaveComment`'s independent hand-parse, not cross-checked against an
  actual HL SDK `save.cpp` (V8.2 Uncertainties).
- `SV_GetSaveComment`'s potential NULL-deref on a corrupted first
  field-block-name token (V8.2 Quirks row, sv_save.c:1751-1753):
  **adjudicated 2026-07-19 as a deviation (bug-fix class)** — corrupted
  input must reject gracefully via the typed `SaveError` path (the chunk's
  reject-gracefully deliverable), same disposition as the SV_SpawnServer
  leak above; a crash on hostile input is never a parity requirement.
- The `svc_restore` wire format dependency on Networking Chunk 9 is
  unresolved at the wire-message level (no xash3dpp enum/wire format exists
  yet) — this spec records the dependency direction only; the message
  itself belongs in `networking-boundary.md` once Chunk 9 lands.
- S8.3 parity-audit latent notes (recorded, not triggerable yet):
  `field_element_size(FIELD_POINTER/FIELD_FUNCTION)` returns `sizeof(void*)`
  vs the HL-SDK 32-bit gSizes value of 4 — never invoked by the engine-owned
  blocks and rejected by the codec's default case; re-adjudicate if the
  descriptor codec is ever reused for those types on x64. `FIELD_EDICT`
  DataEmpty tests the low 4 bytes of an 8-byte pointer on x64 (NULL-safe;
  mirrors gSizes[FIELD_EDICT]==sizeof(int)) — live once real landmark edicts
  flow. ETABLE classname DataEmpty keys on text emptiness vs legacy's 4-byte
  string_t (equivalent except a non-zero string_t resolving to ""). Writer
  goldens pin small token tables, not the 4095-slot production image — the
  production byte-parity witness is the S8.8 legacy-fixture tier.
- The Chunk 6 "SV_InitSaveRestore body" stub (game_host.cpp:150) and the
  exact `FIELD_FUNCTION` callback invocation site are not yet located in
  xash3dpp code (V8.3 Uncertainties) — expected to land as part of the
  Chunk 8 implementation, not a spec gap.
