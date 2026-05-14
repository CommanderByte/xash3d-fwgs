# Server Map Validation Policy

Phase 108 centralizes interpretation of `SV_MapIsValid()` flags while leaving
map probing, BSP/header validation, entity parsing, and landmark scanning in
legacy `sv_game.c`.

## Legacy Flags

`SV_MapIsValid()` returns a bitmask:

- `MAP_IS_EXIST`: the BSP file was opened.
- `MAP_HAS_LANDMARK`: a requested landmark targetname was found in the entity
  script.
- `MAP_INVALID_VERSION`: the BSP opened, but model/lump validation failed.

The modern helper mirrors those values through `server_limits.hpp` and exposes
typed classification in:

- `src/include/engine/server/server_map_validation.hpp`
- `src/engine/server/server_map_validation.cpp`

## Compatibility Results

The load-oriented classification keeps the old precedence:

1. `MAP_INVALID_VERSION` wins first.
2. Missing `MAP_IS_EXIST` is missing.
3. Otherwise the map is valid for the caller's next step.

This is used for command validation and save/load admission.

## Changelevel Landmark Quirk

Smooth changelevel requests are special. If a smooth changelevel asks for a
landmark and the target map exists but does not report `MAP_HAS_LANDMARK`, the
old code only disables smooth transition when `sv_validate_changelevel` is
enabled. With validation disabled, the request continues as smooth.

`BuildServerChangeLevelMapValidationDecision()` preserves that quirk:

- invalid or missing maps cannot continue;
- smooth requests without a landmark record `missingLandmarkForSmooth`;
- `disableSmooth` becomes true only when `sv_validate_changelevel` is enabled.

## Game DLL `pfnIsMapValid`

The game DLL callback historically returns true when `MAP_IS_EXIST` is set. It
does not reject `MAP_INVALID_VERSION` if both bits are present. That behavior is
preserved through `ServerMapExistsForGameDll()`.

## Route-Through Scope

Routed callers:

- `sv_game.c`: `SV_QueueChangeLevel()` and `pfnIsMapValid()`.
- `sv_save.c`: savegame load admission and save-comment map checks.
- `server_command_lifecycle.cpp`: command map validation classification now
  delegates to the shared helper while keeping the lifecycle public contract.

Legacy-owned concerns:

- `SV_MapIsValid()` and `SV_ReadEntityScript()`;
- filesystem opens, `.ent` override handling, and BSP lump validation;
- entity-script parsing for landmark targetnames;
- console messages, savegame state, and actual `COM_ChangeLevel()` execution.

## Tests

`tests/engine/server_map_validation.cpp` covers:

- flag decoding;
- invalid/missing/valid precedence;
- game DLL existence compatibility;
- smooth changelevel missing-landmark behavior with and without validation.
