# Server Save/Restore Runtime Ownership

Phase: 134

## Purpose

This audit revisits runtime save/restore after the Phase 83 and Phase 84
format fixtures. The fixture parser is useful, but it is only a read-only view
of the save byte stream. `engine/server/sv_save.c` still owns the live runtime
because it coordinates engine state, game DLL callbacks, temporary files,
client renderer/audio state, and map transitions.

The goal for this checkpoint is to separate concepts that can become modern
value objects from side effects that must remain legacy-owned until a later,
heavier migration phase.

## Current Runtime Owners

| Area | Current owner | Why it stays legacy-owned now |
| --- | --- | --- |
| Save admission | `IsValidSave()` / `SV_SaveGame()` | Reads server/client globals, UI credits state, physics extension veto, local client state, maxclients, player edict health, and prints user-facing reasons. |
| Save comments | `SaveBuildComment()` / `SV_GetSaveComment()` | Calls optional game DLL `SV_SaveGameComment`, falls back to hardcoded map title aliases, worldspawn message strings, file timestamps, and map validation. |
| Save buffer lifetime | `SaveInit()`, `SaveClear()`, `SaveFinish()` | Allocates from `host.mempool`, owns `SAVERESTOREDATA`, publishes `svgame.globals->pSaveData`, and exposes raw mutable pointers to game DLL callbacks. |
| Token table build/rebase | `StoreHashTable()` / `BuildHashTable()` | Depends on `SAVERESTOREDATA` pointer rebasing and sparse `pTokens` semantics that game DLL field serialization expects. |
| Level save stream | `SaveGameState()` / `LoadSaveData()` / `ParseSaveTables()` | Writes and reads `.HL1`, builds `ENTITYTABLE`, calls game DLL field serializers, serializes lightstyles, and mutates server sky/time/skill state. |
| Client save stream | `SaveClientState()` / `LoadClientState()` | Reads renderer decals, dynamic sounds, music state, camera view entity, static entities, water cvars, and writes signon messages on restore. |
| Entity patches | `EntityPatchWrite()` / `EntityPatchRead()` | Writes `.HL3` removed-entity lists and mutates live `ENTITYTABLE` flags. Phase 84 only documents the safe parser target. |
| Entity restore | `CreateEntitiesInRestoreList()`, `LoadGameState()`, `CreateEntityTransitionList()` | Calls physics extension hooks, creates edicts, restores game DLL private data, merges global entities, kills invalid entities, and frees old entities. |
| Landmark transitions | `SV_ChangeLevel()` / `LoadAdjacentEnts()` | Saves current level, deactivates and respawns the server, computes landmark offsets, restores adjacent entities/decals, and rewrites patch files. |
| Outer save archive | `SaveGameSlot()` / `SaveReadHeader()` / `DirectoryCopy()` / `DirectoryExtract()` | Bundles and extracts `.HL?` files, ages quick/autosave slots, triggers `saveshot`, validates maps, restores global state, and mutates save directory files. |
| Public save load entry points | `SV_LoadGame()`, `SV_SaveGame()`, `SV_ClearGameState()`, `SV_GetLatestSave()` | Own filesystem probes, cvar mutation, `COM_LoadGame()`, temp-file cleanup, global-state reset, and save-slot lookup. |

## Value Objects Worth Extracting Later

These objects can be represented with plain data and tested without changing
the save file format.

| Candidate | Proposed responsibility | Notes |
| --- | --- | --- |
| `SaveAdmissionSnapshot` | Input facts for save eligibility: initialized, active, background, credits, physics veto, client active, intermission, maxclients, spawned client, player alive. | This could eventually replace the hardcoded decision tree in `IsValidSave()`, while console output and callback reads stay outside the helper. |
| `SaveSlotPlan` | Resolve `"new"` to the first free `saveNNN`, classify `quick` / `autosave` aging, and normalize visible save names. | Requires a filesystem-provided existence snapshot, not direct `FS_FileExists()` calls. |
| `SaveCommentPlan` | Select comment source: game DLL override, title-table alias, world message, map name, and formatted elapsed time. | The optional game DLL callback and string lookups remain legacy-owned; the fallback decision can be pure. |
| `SaveVersionStatus` | Classify `SAVEGAME_HEADER` versions for menu comments: corrupted, old unsupported `0x0065`, old version, invalid newer version, or valid. | Complements `save_restore_format` and can make `SV_GetSaveComment()` safer without parsing fields through raw pointer casts. |
| `SaveRestoreTokenTableView` | Represent token count, token bytes, consumed token bytes, rebased payload offset, and sparse empty-token slots. | Already partly covered by `ParseSaveRestoreTokenTable()`. Runtime use should wait until game DLL field callback compatibility is proven. |
| `SaveHeaderSnapshot` | Plain representation of skill, entity count, adjacency count, lightstyle count, map, sky, and time. | It must preserve the legacy `pSaveData->time = 0.0f` header-write quirk before moving runtime writes. |
| `ClientSaveHeaderSnapshot` | Plain representation of decal/static/sound counts, tracks, view entity, water alpha, and water amplitude. | Renderer/audio reads and signon restore remain live effects. |
| `LandmarkConnection` / `LandmarkTransitionPlan` | Plain map/landmark/origin rows plus offset computation and adjacent-map flags. | This can reduce duplication in `LandmarkOrigin()` / `LoadAdjacentEnts()`, but entity movement remains live. |
| `EntityTableRow` | Plain `ENTITYTABLE` row facts: id, location, size, flags, classname, player/global/removed flags. | Pointer ownership stays legacy. This can support safer patch and transition tests. |
| `EntityPatchPlan` | List removed entity indexes to write and validation status for indexes read from `.HL3`. | The fixture parser already models invalid indexes; runtime should not apply malformed indexes directly in the modern target. |
| `BundledSaveFileEntry` / `SaveArchiveManifest` | File name, size, and byte range entries for `.HL?` files inside `.sav`. | Existing parser covers one bundled file. A manifest view would let tests validate outer archive contents without extracting files. |

## Legacy-Owned Side Effects

The following should not move during the next save/restore pass:

- `SAVERESTOREDATA` layout, pointer rebasing, and publication through
  `svgame.globals->pSaveData`;
- game DLL `pfnSave()`, `pfnRestore()`, `pfnSaveWriteFields()`,
  `pfnSaveReadFields()`, `pfnSaveGlobalState()`, `pfnRestoreGlobalState()`,
  `pfnResetGlobalState()`, and `pfnParmsChangeLevel()` ordering;
- physics extension `pfnCreateEntitiesInRestoreList()` and decal restore
  callbacks;
- edict allocation, private-data restore, `FL_KILLME` mutation, global-entity
  merging, and `SV_FreeOldEntities()`;
- renderer decal list reads, dynamic sound reads, stream music reads, signon
  decal/static/sound writes, and `svc_restore` client messages;
- filesystem creation, deletion, rename, copy, extraction, and quick/autosave
  screenshot side effects;
- map validation probes, `COM_LoadGame()`, `SV_SpawnServer()`,
  `SV_ActivateServer()`, `SV_DeactivateServer()`, and cvar mutation;
- console output and `Host_Error()` / `Sys_Warn()` behavior.

## Recommended Next Checkpoint

Phases 115 through 134 have mapped the major live server owners after the
resource, messaging, game DLL bridge, client/session, runtime command,
frame/snapshot, world/physics/PMove, and save/restore audits.

The next checkpoint should not be another broad server sweep. The best next
step is a **server consolidation planning checkpoint** that turns these audits
into a small set of module directories and explicitly chooses which helpers are
ready to move as groups.

Suggested checkpoint questions:

1. Which helpers are stable enough to group under future directories such as
   `server/resources`, `server/messaging`, `server/game_dll`,
   `server/client`, `server/world`, and `server/save`?
2. Which adapters are temporary and can be merged only when a real domain
   facade exists?
3. Which live owners still need fixture harnesses before any further
   route-through?
4. Which pure value-object lanes should come before runtime movement?

For save/restore specifically, the next implementation phase should be small:
extract save admission or save-comment/version classification first. Runtime
stream replacement should wait until real-save fixtures exist and a controlled
game DLL field serialization harness can prove callback ordering.

## Validation

Phase 134 is documentation-only. Validation should be `git diff --check` plus
`scripts/phase-status.ps1 -PhaseNumber 134`.
