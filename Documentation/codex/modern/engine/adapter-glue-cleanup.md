# Adapter Glue Cleanup

Phase 165 trims adapter implementation noise without changing the narrow
legacy-facing headers or call sites.

## Collapsed Adapter Implementations

| New adapter implementation | Replaces | Reason |
| --- | --- | --- |
| `engine/server/server_query_responses_adapter.cpp` | `source_query_adapter.cpp`, `netapi_info_adapter.cpp` | Source-query and NetAPI response builders now share one modern query-response implementation unit. The split legacy headers remain so `sv_query.c` and `sv_client.c` still include the boundary they use. |
| `engine/server/server_consistency_adapter.cpp` | `server_consistency_list_adapter.cpp`, `server_consistency_policy_adapter.cpp` | Consistency list writes and reservation/check policy now share one modern consistency implementation unit. |
| `engine/server/server_resource_flow_adapter.cpp` | `server_resource_catalog_adapter.cpp`, `server_download_policy_adapter.cpp`, `server_upload_queue_adapter.cpp`, `server_hot_resource_adapter.cpp`, `server_reslist_policy_adapter.cpp` | Catalog, download, upload, hot-resource, and reslist adapters are all plain resource-flow conversions around the same modern resource-flow implementation unit. |

The adapter source count drops from 52 to 46. Headers remain intentionally
split so each legacy file still exposes its local dependency instead of a
broad `server_adapter` facade.

## Reviewed And Kept Split

- Session/admission adapters remain split except query responses, because they
  preserve distinct call-site ownership in `sv_client.c`, `sv_main.c`, and
  `sv_query.c`.
- Replication/message adapters remain split because they protect packet byte
  layouts and make write-buffer side effects easy to inspect.
- Game API adapters remain split where they expose game DLL callback ABI,
  edict ownership, private data ownership, or game-DLL-facing payload policy.
  The message bridge adapter was already grouped around message session and
  user-message registry behavior.
- `resource_adapter_shared.cpp` and `server_message_adapter_shared.hpp` remain
  the only shared glue helpers; no giant adapter facade was introduced.

## Validation

Initial compile check:

```powershell
.\waf.bat build --targets=xash
```

Result: passed.

Standard validation:

```powershell
.\scripts\run-phase-validation.ps1 -FocusedTarget "test_engine_server_query_responses,test_engine_server_consistency,test_engine_server_resource_flow,test_engine_resource_adapter_shared,test_engine_resource_domain,test_engine_resource_transfer_manifest" -StopRunningXash -CopyLauncher
```

Result:

- focused adapter/module targets passed;
- `.\waf.bat build --targets=xash` passed;
- `.\waf.bat build --alltests` passed 127/127;
- runtime smoke reached first frame in 0.492 seconds and stopped with reason
  `command`.
