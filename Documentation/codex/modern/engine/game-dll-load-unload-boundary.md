# Game DLL Load And Unload Boundary

Phase 100 models the high-level game DLL load/unload decisions against fake
symbol tables. It deliberately does not move real DLL lifetime, callback table
publication, edict allocation, or server shutdown side effects out of the
legacy server.

## Legacy Baseline

- `SV_InitGame()` resets the library error state, resolves the game server
  library path, calls `SV_LoadProgs()`, and reports failures through
  `Sys_Warn()` or `Con_Printf()` depending on the silent flag.
- `SV_LoadProgs()` returns success immediately if a game DLL is already
  loaded. Otherwise it assigns static `playermove_t` and `globalvars_t`
  storage, allocates the server-game memory pool, and loads the library.
- Library-load failure frees the memory pool and returns false.
- Required exports are `GetEntityAPI` or `GetEntityAPI2` plus
  `GiveFnptrsToDll`. Missing entity API exports push the library error
  `"missing GetEntityAPI and GetEntityAPI2 exports"`. Missing
  `GiveFnptrsToDll` pushes `"missing GiveFnptrsToDll export"`. Both paths
  free the library, clear `svgame.hInstance`, free the memory pool, and return
  false.
- `GiveFnptrsToDll()` is called before any entity API initialization, after
  copying `gEngfuncs` to a local table and applying the
  `BUGCOMP_PENTITYOFENTINDEX_FLAG` compatibility override.
- `GetNewDLLFunctions` is optional. If present and it returns false, the
  extended function table is cleared. A version warning is printed only on
  failure when the returned version differs from `NEW_DLL_FUNCTIONS_VERSION`.
  A successful return is accepted without a separate version check.
- `GetEntityAPI2` is preferred. It initializes the entity API only when the
  call succeeds and the returned version matches `INTERFACE_VERSION`. On a
  successful-but-mismatched extended API, the engine warns and falls back to
  `GetEntityAPI()` if available, passing along the mutated version value.
  Failed extended API calls may also leave a mutated version for the legacy
  fallback.
- If neither entity API path initializes successfully, the engine pushes
  `"can't init entity API"`, frees the library, clears `svgame.hInstance`,
  frees the memory pool, and returns false.
- `SV_InitPhysicsAPI()` is optional. Missing physics exports validate zero
  engine features and return success. A rejected physics interface clears
  `svgame.physFuncs`, validates zero features, returns false to trigger a
  warning, but does not abort `SV_LoadProgs()`. Accepted physics interfaces
  optionally validate features through `SV_CheckFeatures`.
- After entity API success, the legacy runtime still initializes operator
  commands, studio API, physics API, save/restore hooks, globals, edicts,
  baselines, `host_gameloaded`, string pools, game init, client movement,
  delta tables, and custom encoders.
- `SV_UnloadProgs()` returns immediately when no library is loaded. Otherwise
  it deactivates the server, shuts down delta state, prepares extended-DLL
  cvars for unlink, optionally calls `pfnGameShutdown`, sets
  `host_gameloaded` to `0`, frees static entities and baselines, removes
  server operator commands, unlinks pending cvars and server-DLL commands,
  frees the string pool, resets studio API, frees the library, frees the
  server-game memory pool, and clears `svgame`.

## Modern Boundary

`src/engine/server/game_dll/game_dll_load_policy.cpp` owns pure plans for:

- required symbol admission and load-failure cleanup;
- preferred `GetEntityAPI2` versus legacy `GetEntityAPI` selection, including
  version-warning and mutated fallback-version behavior;
- optional `GetNewDLLFunctions` acceptance, rejection, warning, and table
  clearing;
- optional physics API acceptance, rejection, feature-validation, and
  non-fatal load continuation;
- unload cleanup intent, including command/cvar unlink, string-pool cleanup,
  memory-pool cleanup, and optional `pfnGameShutdown`.

The legacy server still owns:

- `COM_LoadLibrary()`, `COM_GetProcAddress()`, `COM_FreeLibrary()`, and real
  library error storage;
- `GiveFnptrsToDll()`, `gEngfuncs`, `globalvars_t`, `DLL_FUNCTIONS`,
  `NEW_DLL_FUNCTIONS`, and `physics_interface_t` table publication;
- edict, baseline, static entity, string-pool, and memory-pool allocation;
- operator command registration/removal, command/cvar unlinking, delta/studio
  shutdown, save/restore hooks, and game callback invocation.

No route-through is added in Phase 100. The helper is a fake-symbol
compatibility model for a later DLL lifecycle facade, after the bridge has
loaded-DLL fixtures and a deliberate ABI publication plan.
