# Submodule Collapse Pass

Phase 163 is the first aggressive cleanup after the server tree was grouped
into domain folders. The goal is not to erase compatibility seams. The goal is
to stop carrying one implementation file per tiny branch once the module has
enough test coverage to protect a cleaner grouping.

## Starting Point

The Phase 162 layout had clear directories but still had many tiny source
files:

| Module | Sources before | Main fragmentation |
| --- | ---: | --- |
| `client` | 11 | Source query and NetAPI were separate response builders. |
| `resources` | 9 | Consistency list and consistency policy lived apart. |
| `messaging` | 14 | Text, service, userinfo, resource, and customization payload writers were tiny one-off files. |
| `game_dll` | 15 | User-message registry and registration bridge were split while the message session state machine stayed large. |

After the first pass the same modules had 10, 8, 11, and 14 source files
respectively. The total modern server source count went from 66 to 60.

A second iteration then collapsed the remaining tiny `shared` and `world`
implementation files. After both iterations the modern server source count is
53 `.cpp` files, while the 66 public modern headers remain intact as
conceptual boundaries for adapters and tests.

A third iteration collapsed cohesive resource-flow, save/restore, and runtime
command implementations. After all three iterations the modern server source
count is 45 `.cpp` files, while the 66 public modern headers remain intact as
the adapter/test include map.

## Collapses Made

### Client Query Responses

`source_query.cpp` and `netapi_info.cpp` are now implemented together by
`src/engine/server/client/server_query_responses.cpp`.

The public headers remain separate:

- `src/include/engine/server/client/source_query.hpp`
- `src/include/engine/server/client/netapi_info.hpp`

Reasoning:

- both implementations build connectionless query responses;
- `netapi_info` already used Source-query protected-value behavior;
- admission, challenge, remote admin, timeout, and user-agent helpers still
  model distinct server decisions, so they remain separate.

### Resource Consistency

`server_consistency_list.cpp` and `server_consistency_policy.cpp` are now
implemented together by `src/engine/server/resources/server_consistency.cpp`.

The public headers remain separate:

- `src/include/engine/server/resources/server_consistency_list.hpp`
- `src/include/engine/server/resources/server_consistency_policy.hpp`

Reasoning:

- the list writer and response/setup policy describe the same consistency
  feature;
- aggregate resource tests already exercise consistency reserved data feeding
  both the resource message row and consistency list writer;
- catalog, manifest, upload, download, hot-resource, and reslist helpers still
  have different ownership and side effects, so they remain separate.

### Messaging Payload Writers

Text and service protocol emitters are now implemented together by
`src/engine/server/messaging/server_basic_messages.cpp`.

Userinfo, resource-row, and customization payload emitters are now implemented
together by `src/engine/server/messaging/server_state_payloads.cpp`.

The public headers remain separate:

- `server_text_messages.hpp`
- `server_service_messages.hpp`
- `server_userinfo_message.hpp`
- `server_resource_message.hpp`
- `server_customization_message.hpp`

Shared byte/string primitives now live in `server_message_envelope`:

- `WriteServerMessageByte`
- `WriteServerMessageCommand`
- `WriteServerMessageString`
- `WriteServerMessageBytes`

Reasoning:

- these payload writers are byte-layout emitters, not independent subsystems;
- keeping headers separate preserves call-site meaning and adapter includes;
- frame datagrams, multicast, event playback, spawn handshake, sound, static
  entity, voice, and packet-entity delta helpers remain separate because they
  carry larger policy or snapshot-adjacent behavior.

### Game DLL User Messages

`game_dll_user_message_registry.cpp` and `game_dll_message_bridge.cpp` are now
implemented together by `src/engine/server/game_dll/game_dll_user_messages.cpp`.

The public headers remain separate:

- `game_dll_user_message_registry.hpp`
- `game_dll_message_bridge.hpp`

Reasoning:

- the bridge only turns a registration plan into begin/broadcast payload data;
- the registry plan and broadcast write path are the same user-message
  feature from the modern module perspective;
- `game_dll_message_session.cpp` stays separate because it is a state machine
  with byte writes, payload-size patching, rewrite handling, and lifecycle
  invariants;
- entity lifecycle and entity parse remain separate because they split runtime
  entity ownership from map/entity-key parsing.

### Shared Rules And Limits

`server_limits.cpp` now also implements the lifecycle-capacity helpers from
`server_lifecycle_limits.cpp`.

`server_group_filter.cpp`, `server_map_validation.cpp`, and
`server_visibility_constraints.cpp` are now implemented together by
`src/engine/server/shared/server_shared_rules.cpp`.

The public headers remain separate:

- `server_limits.hpp`
- `server_lifecycle_limits.hpp`
- `server_group_filter.hpp`
- `server_map_validation.hpp`
- `server_visibility_constraints.hpp`

Reasoning:

- lifecycle limits are derived directly from server limit constants;
- group filters, map validation, and visibility constraints are small shared
  compatibility predicates used by several server owners;
- preserving separate headers keeps call sites explicit about which rule they
  are consuming.

### World Policies

`server_world_link_policy.cpp`, `server_world_trace_policy.cpp`,
`server_movement_constraints.cpp`, `server_physics_routing_policy.cpp`, and
`server_pmove_bridge_policy.cpp` are now implemented together by
`src/engine/server/world/server_world_policies.cpp`.

The public headers remain separate:

- `server_world_link_policy.hpp`
- `server_world_trace_policy.hpp`
- `server_movement_constraints.hpp`
- `server_physics_routing_policy.hpp`
- `server_pmove_bridge_policy.hpp`

Reasoning:

- the entire world module was under 300 implementation lines after Phase 162;
- all five files describe simulation-facing predicates and fixture plans;
- keeping the headers separate avoids blurring PMove, trace, physics, movement,
  and area-link vocabulary at call sites.

### Resource Flow

`resource_transfer_manifest.cpp`, `server_resource_catalog.cpp`,
`server_download_policy.cpp`, `server_upload_queue.cpp`,
`server_hot_resource.cpp`, and `server_reslist_policy.cpp` are now implemented
together by `src/engine/server/resources/server_resource_flow.cpp`.

The public headers remain separate:

- `resource_transfer_manifest.hpp`
- `server_resource_catalog.hpp`
- `server_download_policy.hpp`
- `server_upload_queue.hpp`
- `server_hot_resource.hpp`
- `server_reslist_policy.hpp`

Reasoning:

- these helpers all participate in the same resource transfer/catalog/download
  flow;
- the combined file removes repeated string/path helpers such as null checks,
  slash normalization, and model-name probes;
- `resource_identity.cpp` and `server_consistency.cpp` stay separate because
  identity parsing/formatting and consistency validation are substantial
  resource subfeatures.

### Runtime Commands

`server_command_lifecycle.cpp` and `server_operator_command_policy.cpp` are now
implemented together by `src/engine/server/runtime/server_runtime_commands.cpp`.

The public headers remain separate:

- `server_command_lifecycle.hpp`
- `server_operator_command_policy.hpp`

Reasoning:

- both files are command argument planners;
- the merged implementation removes repeated string helpers;
- event-log formatting and server filter policy stay separate because they are
  not command planners.

### Save Restore

`save_restore_format.cpp`, `save_restore_values.cpp`, and
`save_restore_runtime.cpp` are now implemented together by
`src/engine/server/save/save_restore.cpp`.

The public headers remain separate:

- `save_restore_format.hpp`
- `save_restore_values.hpp`
- `save_restore_runtime.hpp`

Reasoning:

- all three files are save/restore fixture and policy helpers;
- parsing helpers and value-policy helpers already shared constants and
  parse-status vocabulary;
- keeping headers separate still communicates format parsing, runtime archive
  fixtures, and save admission/comment policy as distinct concepts.

## Boundaries Preserved

This pass intentionally did not merge legacy adapters or modern headers. The C
runtime still reaches modern code through the same adapter names, and modern
test targets still include the same conceptual headers.

This is the desired compromise:

- fewer implementation files to scan;
- no larger public C++ surface;
- no ABI movement;
- no hiding of legacy-owned side effects.

## Validation

Focused validation after the code collapse:

```powershell
.\waf.bat build --targets=test_engine_server_text_messages,test_engine_server_service_messages,test_engine_server_message_envelope,test_engine_server_userinfo_message,test_engine_server_resource_message,test_engine_server_customization_message,test_engine_server_consistency_list,test_engine_server_consistency_policy,test_engine_resource_domain,test_engine_server_messaging_domain,test_engine_source_query,test_engine_netapi_info,test_engine_client_session_domain,test_engine_game_dll_message_bridge,test_engine_game_dll_user_message_registry,test_engine_game_dll_bridge_domain
```

Result: 16/16 focused tests passed.

Focused validation after the second shared/world collapse:

```powershell
.\waf.bat build --targets=test_engine_server_limits,test_engine_server_lifecycle_limits,test_engine_server_group_filter,test_engine_server_map_validation,test_engine_server_visibility_constraints,test_engine_server_movement_constraints,test_engine_server_physics_routing_policy,test_engine_server_pmove_bridge_policy,test_engine_server_world_link_policy,test_engine_server_world_trace_policy,test_engine_world_trace_fixtures,test_engine_pmove_usercmd_fixtures
```

Result: 12/12 focused tests passed.

Focused validation after the third resource/runtime/save collapse:

```powershell
.\waf.bat build --targets=test_engine_server_command_lifecycle,test_engine_server_operator_command_policy,test_engine_save_restore_format,test_engine_save_restore_values,test_engine_save_restore_runtime,test_engine_server_resource_catalog,test_engine_server_download_policy,test_engine_server_upload_queue,test_engine_server_hot_resource,test_engine_server_reslist_policy,test_engine_resource_transfer_manifest,test_engine_resource_domain
```

Result: 12/12 focused tests passed.

Full validation after the first iteration:

```powershell
.\scripts\run-phase-validation.ps1 -SkipFocused -StopRunningXash -AllowSmokeNonZeroExit
```

Result:

- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 141/141 tests.
- Runtime binaries were refreshed in `run-win32`.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached
  first frame in 0.495 seconds and stopped with reason `command`.

Full validation after the second iteration:

```powershell
.\scripts\run-phase-validation.ps1 -SkipFocused -StopRunningXash -AllowSmokeNonZeroExit
```

Result:

- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 141/141 tests.
- Runtime binaries were refreshed in `run-win32`.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached
  first frame in 0.513 seconds and stopped with reason `command`.

Full validation after the third iteration:

```powershell
.\scripts\run-phase-validation.ps1 -SkipFocused -StopRunningXash -AllowSmokeNonZeroExit
```

Result:

- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 141/141 tests.
- Runtime binaries were refreshed in `run-win32`.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached
  first frame in 0.512 seconds and stopped with reason `command`.
