# Flat Server Module Rehome

Phase 162 physically moves the remaining flat modern server helpers into
engine-shaped modules. This is a mechanical rehome, not a behavior rewrite.

## Move Map

| From | To | Reason | Focused tests |
| --- | --- | --- | --- |
| `server_limits` | `shared/` | Cross-domain server constants and compatibility roles. | `test_engine_server_limits` |
| `server_lifecycle_limits` | `shared/` | Spawn/update/client capacity rules consumed across runtime and lifecycle code. | `test_engine_server_lifecycle_limits` |
| `server_group_filter` | `shared/` | Group-mask policy is used by world, event, PMove, and game-DLL facing paths. | `test_engine_server_group_filter` |
| `server_map_validation` | `shared/` | Map/load validation is used by runtime, command, save, and changelevel paths. | `test_engine_server_map_validation` |
| `server_visibility_constraints` | `shared/` | Visibility capacities cross world, frame, and game-DLL callback paths. | `test_engine_server_visibility_constraints` |
| `server_filter` | `runtime/` | Ban/filter policy belongs to the server runtime shell. | `test_engine_server_filter` |
| `server_event_log` | `runtime/` | Log line formatting is runtime shell behavior, while sinks stay legacy-owned. | `test_engine_server_event_log` |
| `server_command_lifecycle` | `runtime/` | Lifecycle command normalization belongs with the server shell. | `test_engine_server_command_lifecycle` |
| `server_operator_command_policy` | `runtime/` | Operator command argument policy belongs with the server shell. | `test_engine_server_operator_command_policy` |
| `save_restore_format` | `save/` | Savegame format fixture parser. | `test_engine_save_restore_format` |
| `save_restore_values` | `save/` | Savegame value decisions and comment classification. | `test_engine_save_restore_values` |
| `save_restore_runtime` | `save/` | Savegame runtime fixture model without stream ownership. | `test_engine_save_restore_runtime` |
| `server_movement_constraints` | `world/` | Movement constants and mode classification are simulation policy. | `test_engine_server_movement_constraints` |
| `server_physics_routing_policy` | `world/` | Server entity physics routing is simulation policy. | `test_engine_server_physics_routing_policy` |
| `server_pmove_bridge_policy` | `world/` | PMove bridge decisions are simulation-facing. | `test_engine_server_pmove_bridge_policy` |
| `server_world_link_policy` | `world/` | Area-node and link traversal policy belongs to world/simulation. | `test_engine_server_world_link_policy` |
| `server_world_trace_policy` | `world/` | Trace setup policy belongs to world/simulation. | `test_engine_server_world_trace_policy` |
| `source_query` | `client/` | Query payloads are session-facing client response behavior. | `test_engine_source_query` |
| `netapi_info` | `client/` | NetAPI query payloads are session-facing client response behavior. | `test_engine_netapi_info` |

## What Stayed Legacy-Owned

This phase does not move live runtime ownership:

- `sv`, `svs`, `svgame`, `sv_client_t`, `edict_t`, and `server_t` storage;
- packet buffers, netchan sends, signon buffers, and `MSG_*` mutation;
- game DLL and physics extension ABI publication;
- filesystem/HPAK/save stream mutation;
- exact world trace traversal and PMove callbacks;
- command registration, cvar mutation, and console output.

The query helpers moved to `client/` because their concrete consumers are
session-facing query/NetAPI responses. If later work grows a broader network
query module, they can move again as a purely mechanical rehome.

## Reference Scan

The phase updated include paths from the former flat locations to canonical
module paths such as:

- `engine/server/shared/server_limits.hpp`;
- `engine/server/runtime/server_filter.hpp`;
- `engine/server/save/save_restore_format.hpp`;
- `engine/server/world/server_world_trace_policy.hpp`;
- `engine/server/client/source_query.hpp`.

Post-rewrite search found no remaining flat include paths in `src`, `engine`,
or `tests`.

## Validation

Focused affected targets passed 22/22:

```powershell
.\waf.bat build --targets=test_engine_server_limits,test_engine_server_lifecycle_limits,test_engine_server_group_filter,test_engine_server_map_validation,test_engine_server_visibility_constraints,test_engine_server_filter,test_engine_server_event_log,test_engine_server_command_lifecycle,test_engine_server_operator_command_policy,test_engine_save_restore_format,test_engine_save_restore_values,test_engine_save_restore_runtime,test_engine_server_movement_constraints,test_engine_server_physics_routing_policy,test_engine_server_pmove_bridge_policy,test_engine_server_world_link_policy,test_engine_server_world_trace_policy,test_engine_world_trace_fixtures,test_engine_pmove_usercmd_fixtures,test_engine_source_query,test_engine_netapi_info,test_engine_client_session_domain
```

Full validation and smoke timing are recorded in `Documentation/codex/tasks.md`.

Full phase validation passed:

- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 141/141 tests.
- Runtime binaries were refreshed in `run-win32`.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.508 seconds and stopped with reason `command`.
