# Game DLL Client Info Policy

Phase 93 extracts game-DLL-facing client info, query, and identity decisions
without moving `Info_*` storage or live client ownership out of the legacy
server.

## Legacy Baseline

- `pfnGetInfoKeyBuffer()` returns `localinfo` for null or invalid edicts,
  `serverinfo` for world, client `userinfo` for client edicts, and an empty
  string for other valid non-client edicts.
- `pfnSetValueForKey()` only mutates `localinfo` and `serverinfo`; attempts to
  mutate client keys through that callback print an error.
- `pfnSetClientKeyValue()` ignores `localinfo` and `serverinfo`, rejects invalid
  one-based client indices, skips unchanged values, then mutates the supplied
  userinfo buffer and marks the client for userinfo resend.
- Physics info callbacks require a client edict. Bad players print an error;
  getters return an empty string and setters skip mutation.
- `pfnGetPlayerUserId()` returns `-1` for bad players. Player stats default
  ping and packet loss to zero before optional client lookup.
- `pfnQueryClientCvarValue()` and `pfnQueryClientCvarValue2()` ignore null or
  empty cvar names. Bad players call the game DLL cvar callback with
  `"Bad Player"` when available and print an error.
- `pfnGetGameDir()` normally returns the game folder. With
  `BUGCOMP_GET_GAME_DIR_FULL_PATH`, it attempts `root/gamefolder` and falls
  back to the folder name if the root directory lookup or 256-byte formatting
  fails.

## Modern Boundary

`src/engine/server/game_dll_client_info_policy.cpp` owns pure decisions for:

- info-buffer routing;
- local/server versus client set-value admission;
- one-based client index validation;
- unchanged userinfo skips and resend planning;
- bad-client string, mutation, userid, and stats fallbacks;
- cvar-query admission and bad-player routing;
- full-path game-dir fallback selection.

`engine/server/game_dll_client_info_policy_adapter.cpp` exposes those decisions
to the legacy server. The legacy side still owns:

- `svs.localinfo`, `svs.serverinfo`, client `userinfo`, and `physinfo`;
- `Info_ValueForKey()`, `Info_SetValueForKey()`, and
  `Info_SetValueForStarKey()`;
- live `sv_client_t` fields and `FCL_RESEND_USERINFO`;
- query-cvar network message writes;
- `SV_GetClientIDString()` and filesystem root directory lookup.

This gives the game DLL bridge a testable policy layer while keeping external
ABI behavior and mutable engine state in place.
