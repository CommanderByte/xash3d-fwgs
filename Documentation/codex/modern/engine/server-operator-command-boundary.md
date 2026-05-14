# Server Operator Command Boundary

Phase 128 audits `engine/server/sv_cmds.c`, the legacy owner for server console
commands. The file is intentionally still a compatibility boundary: it knows
about command registration, console output, filesystem probes, cvars, save/load
operations, game DLL state, client lookup, and live server mutation.

## Legacy Owners

| Area | Legacy ownership reason |
| --- | --- |
| Command registration | `SV_InitHostCommands()`, `SV_InitOperatorCommands()`, and `SV_KillOperatorCommands()` call the legacy `Cmd_*` registry with static C callbacks. |
| Map and save lifecycle | `map`, `map_background`, `load`, `save`, `reload`, `autosave`, and changelevel commands still call `COM_*`, `SV_LoadGame()`, `SV_SaveGame()`, filesystem probes, and cvar mutation. |
| Status and diagnostics | `status`, `edict_usage`, `entity_info`, `sv_list_messages`, `clientinfo`, and `clientuseragent` print directly from live server, client, edict, and game DLL state. |
| Operator effects | `kick`, `heartbeat`, `shutdownserver`, `playersonly`, `entpatch`, `redirect`, `log`, and `logaddress` mutate live runtime state or delegate to subsystem-specific owners. |
| Info mutation | `serverinfo` and `localinfo` mutate live info strings; `serverinfo` also mutates matching cvars and broadcasts `fullserverinfo`. |

## Selected Seam

The safest reusable seam is command argument policy, not command execution.
Phase 128 adds `server_operator_command_policy` for:

- `kick` usage detection and `#userid` versus name target classification;
- preserving the legacy requirement that every character after `#` must be a
  digit before treating the argument as a numeric user ID;
- `serverinfo` and `localinfo` print/usage/set decisions;
- preserving star-key rejection only for valid three-argument mutation forms.

`sv_cmds.c` still owns:

- all `Cmd_AddCommand()` / `Cmd_RemoveCommand()` calls;
- all command callbacks and descriptions;
- all `Con_Printf()`/`Msg()` output;
- client lookup and `SV_KickPlayer()`;
- cvar mutation, `Info_SetValueForStarKey()`, and `SV_BroadcastCommand()`;
- filesystem, save/load, map, and shutdown effects.

## Compatibility Notes

- `kick #12` still resolves by user ID, while `kick #12abc` remains a name
  lookup because the legacy `Q_isdigit(param + 1)` check required the whole
  suffix to be numeric.
- Missing `kick` target still prints usage before any lookup.
- `serverinfo` and `localinfo` with one argument still print current info.
- Two or more than three info arguments still print usage before star-key
  checks.
- `*` keys are still rejected only when the command otherwise has the right
  mutation form.

## Validation

Phase 128 validation:

- `.\waf.bat build --targets=test_engine_server_operator_command_policy`
  passed.
- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 125/125 tests.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.504 seconds and stopped with reason `command`.
