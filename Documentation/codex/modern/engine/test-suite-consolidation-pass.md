# Test Suite Consolidation Pass

Phase 164 follows the implementation collapses from Phase 163 and reduces
test target noise only where the same domain is now intentionally implemented
together.

## Consolidated Targets

| New target | Replaces | Reason |
| --- | --- | --- |
| `test_engine_server_query_responses` | `test_engine_source_query`, `test_engine_netapi_info` | Source-query and NetAPI responses now share one client/query implementation unit. Their byte/string expectations remain in one focused query-response test. |
| `test_engine_game_dll_user_messages` | `test_engine_game_dll_message_bridge`, `test_engine_game_dll_user_message_registry` | User-message registry planning and message bridge behavior are now one Game API user-message implementation unit. |
| `test_engine_server_shared_rules` | `test_engine_server_group_filter`, `test_engine_server_map_validation`, `test_engine_server_visibility_constraints` | Shared server predicates are small compatibility rules with no live state. |
| `test_engine_server_world_policies` | `test_engine_server_movement_constraints`, `test_engine_server_physics_routing_policy`, `test_engine_server_pmove_bridge_policy`, `test_engine_server_world_link_policy`, `test_engine_server_world_trace_policy` | World/simulation policy helpers are now grouped under one implementation unit while keeping fixture-backed world and PMove tests separate. |
| `test_engine_server_runtime_commands` | `test_engine_server_command_lifecycle`, `test_engine_server_operator_command_policy` | Runtime command normalization and operator command planning are now one runtime command implementation unit. |
| `test_engine_server_consistency` | `test_engine_server_consistency_list`, `test_engine_server_consistency_policy` | Consistency packet list writes and consistency reservation/check policy now share one resource consistency implementation unit. |
| `test_engine_server_resource_flow` | `test_engine_server_resource_catalog`, `test_engine_server_download_policy`, `test_engine_server_upload_queue`, `test_engine_server_hot_resource`, `test_engine_server_reslist_policy` | Resource catalog, download, upload, hot-resource, and reslist rules now share one resource-flow implementation unit. |

The first pass reduced `tests/engine/*.cpp` from 86 to 77 and removed nine
engine test targets. The bis pass reduces it again from 77 to 72 and removes
five more engine test targets without dropping behavior coverage.

## Shared Test Support

`tests/engine/game_dll_user_message_test_support.hpp` now owns duplicated
Game DLL user-message slot, registration request, and C-string read helpers
used by both:

- `tests/engine/game_dll_user_messages.cpp`;
- `tests/engine/game_dll_bridge_domain.cpp`.

## Kept Focused

The following tests intentionally remain separate because they protect
diagnostics for fragile behavior:

- ABI and table-order coverage, such as `game_dll_enginefuncs`;
- packet byte layout coverage, such as sound, static, spawn, datagram, packet
  entity, resource, userinfo, and customization messages;
- resource transfer manifest and resource message byte layouts;
- save format/runtime/value coverage;
- fixture-heavy world/PMove behavior in `world_trace_fixtures` and
  `pmove_usercmd_fixtures`;
- domain aggregate tests that intentionally exercise cross-module behavior.

## Validation

Focused consolidation targets:

```powershell
.\waf.bat build --targets=test_engine_server_runtime_commands,test_engine_server_world_policies,test_engine_server_shared_rules,test_engine_server_query_responses,test_engine_game_dll_user_messages,test_engine_game_dll_bridge_domain
```

Result: passed 6/6.

Standard validation:

```powershell
.\scripts\run-phase-validation.ps1 -FocusedTarget "test_engine_server_runtime_commands,test_engine_server_world_policies,test_engine_server_shared_rules,test_engine_server_query_responses,test_engine_game_dll_user_messages,test_engine_game_dll_bridge_domain" -StopRunningXash -CopyLauncher
```

Result:

- `.\waf.bat build --targets=xash` passed;
- `.\waf.bat build --alltests` passed 132/132;
- runtime smoke reached first frame in 0.540 seconds and stopped with reason
  `command`.

Bis pass focused targets:

```powershell
.\scripts\run-phase-validation.ps1 -FocusedTarget "test_engine_server_resource_flow,test_engine_server_consistency,test_engine_resource_domain,test_engine_resource_transfer_manifest" -StopRunningXash -CopyLauncher
```

Result:

- focused resource targets passed;
- `.\waf.bat build --targets=xash` passed;
- `.\waf.bat build --alltests` passed 127/127;
- runtime smoke reached first frame in 0.541 seconds and stopped with reason
  `command`.
