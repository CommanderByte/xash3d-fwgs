# Game DLL Changelevel And Save Boundary

Phase 97 models changelevel and entity-patch decisions that can be tested
without moving runtime save/load streams or game DLL field serialization out of
legacy code.

## Legacy Baseline

- `pfnChangeLevel()` ignores empty level names and inactive servers.
- `pfnChangeLevel()` suppresses duplicate requests by remembering the last
  `svs.spawncount` it accepted.
- With Half-Life mod hacks enabled, landmarks passed through
  `pfnChangeLevel()` are truncated at the first space.
- `SV_QueueChangeLevel()` strips the requested level extension before map
  validation and queuing.
- Invalid BSP versions and missing maps reject the changelevel.
- A requested landmark enables smooth transition. If validation is enabled and
  the map lacks that landmark, the engine warns and falls back to classic
  non-smooth changelevel.
- Multiplayer forces classic non-smooth changelevel.
- Smooth changelevel to the same map is rejected.
- During the first 15 frames, validation can reject changelevel requests to
  avoid infinite transition loops.
- Queued smooth changes call `COM_ChangeLevel(map, landmark, background)`;
  classic changes call `COM_ChangeLevel(map, NULL, background)`.
- `.HL3` entity patch writes always write a patch count followed by removed
  entity indexes. Even zero removed entities produce a valid patch file if it
  can be opened.

## Save/Restore Callback Sequence

Runtime save/load remains legacy-owned, but the critical callback order is:

1. `SaveGameState(changelevel)` refuses to continue if
   `pfnParmsChangeLevel` is missing.
2. It initializes `SAVERESTOREDATA`, builds the entity table, then calls
   `pfnParmsChangeLevel()` to populate adjacency data.
3. It writes the save header with `pSaveData->time` temporarily forced to
   zero, restores header time, writes adjacency rows and lightstyles, then
   calls `pfnSave()` for each valid edict.
4. It writes `ETABLE`, token data, `.HL1`, `.HL3` patch data, and `.HL2`
   client state.
5. `LoadGameState()` loads `.HL1`, sets the current map name before calling
   the DLL, parses tables, reads `.HL3`, creates restore-list entities, then
   calls `pfnRestore()` for each table entry.
6. Smooth `SV_ChangeLevel()` sets `globals->changelevel`, saves the old level,
   deactivates the server, spawns the new map, loads new level state if
   available, then loads adjacent entities and client state before activation.
7. Classic changelevel resets global state, spawns entities from the map, and
   activates the server.

## Modern Boundary

`src/engine/server/game_dll/game_dll_changelevel_policy.cpp` owns pure plans for:

- `pfnChangeLevel()` request admission and duplicate spawncount suppression;
- optional landmark truncation;
- queued changelevel validation, smooth fallback, same-map rejection, early
  frame-loop rejection, and classic versus smooth queue decisions;
- `.HL3` entity-patch write intent and removed-count normalization.

The legacy server still owns:

- `SV_MapIsValid()`, `COM_ChangeLevel()`, `SV_SkipUpdates()`, and all console
  output;
- `SaveGameState()`, `LoadGameState()`, `LoadAdjacentEnts()`, and
  `SV_ChangeLevel()` runtime ordering;
- `SAVERESTOREDATA`, token tables, entity tables, `.HL1`/`.HL2`/`.HL3` file
  I/O, and field serializer callbacks;
- all calls to `pfnParmsChangeLevel()`, `pfnSave()`, `pfnRestore()`,
  `pfnSaveWriteFields()`, `pfnSaveReadFields()`, and global-state callbacks.

No route-through is added in Phase 97. The helper is a compatibility model for
future changelevel/save bridge slices after broader save fixtures exist.
