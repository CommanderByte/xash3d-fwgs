# Server Command Lifecycle Baseline

Phase: 80

Legacy owner: `engine/server/sv_cmds.c`

## Scope

This baseline covers command-side lifecycle validation for map loading,
background maps, save/load aliases, restart/reload, and changelevel commands.
It does not cover the deeper load, save, or changelevel execution paths in
`sv_save.c`, `sv_game.c`, or host state transitions.

## Command Behavior

| Command | Legacy behavior |
| --- | --- |
| `map <mapname>` | Requires exactly one map argument, copies it into a `MAX_QPATH` buffer, strips the extension, validates the map, sets `sv_hostmap`, then calls `COM_LoadLevel(map, false)`. |
| `map_background <mapname>` | Dedicated servers reject immediately. Otherwise it requires exactly one map argument. If a normal game is already active, it prints an active-game error when the host is running frames and returns. Valid maps force single-player cvars before `COM_LoadLevel(map, true)`. |
| `load <savename>` | Requires exactly one save argument and calls `SV_LoadGame("save/<name>.sav")`. |
| `loadquick` | Appends `echo Quick Loading...; wait; load quick\n` to the command buffer. |
| `save` | With no save argument, calls `SV_SaveGame("new")`. |
| `save <savename>` | Calls `SV_SaveGame(<savename>)`. |
| `save` with too many args | Prints usage and performs no save. |
| `savequick` | Appends `echo Quick Saving...; wait; save quick\n` to the command buffer. |
| `autosave` | Requires no arguments and calls `SV_SaveGame("autosave")` only when `sv_autosave` is enabled. |
| `restart` | If the server is active, reloads the current map with the current background flag. Otherwise it silently returns. |
| `reload` | If the host is currently running frames, tries `SV_LoadGame(SV_GetLatestSave())`, falling back to `COM_LoadLevel(sv_hostmap, false)` on failure. Otherwise it silently returns. |
| `changelevel <mapname>` | Requires at least one argument and queues a classic changelevel. Extra arguments are ignored for compatibility. |
| `changelevel2 <mapname> [landmark]` | Requires at least one argument. With only a map it behaves like `changelevel`; with a landmark it queues smooth changelevel validation. |

## Map Validation

`SV_ValidateMap()` calls `SV_MapIsValid(map, NULL)` and classifies the returned
flags:

- `MAP_INVALID_VERSION`: print `map <name> is invalid or not supported`.
- Missing `MAP_IS_EXIST`: print `map <name> doesn't exist`.
- Otherwise: allow the caller to continue.

The phase 80 helper can classify these flags but must not call filesystem,
BSP, or entity parsing code.

## Extraction Boundary

Safe to extract:

- Argument-count decisions.
- Map-name extension stripping for `map` and `map_background`.
- Save/load path and alias text construction.
- Restart/reload no-op gates.
- Changelevel argument routing.
- Map-validation flag classification.

Kept legacy-owned:

- `Cmd_Argc()` and `Cmd_Argv()`.
- Console output text.
- `SV_MapIsValid()`, filesystem probes, and BSP/entity parsing.
- Cvar mutation.
- `COM_LoadLevel()`, `COM_NewGame()`, `SV_LoadGame()`, `SV_SaveGame()`,
  `SV_QueueChangeLevel()`, and command-buffer execution.
