# Resource Adapter Shrink Review

Phase 117 reviews the resource-related legacy adapters after the Phase 116
manifest pilot. The goal is to remove obvious duplicate conversion glue without
merging adapters that still represent different legacy side-effect families.

## Decision

Do a narrow adapter shrink, not a broad adapter merge.

The repeated conversion from `resourcetype_t` / `resource_t` to modern resource
values is pure adapter glue and belongs in one private C++ helper. The adapter
entry points themselves should remain separate for now because they still sit at
different legacy ownership boundaries.

## Shared Conversion Helper

`engine/server/resource_adapter_shared.*` now owns:

- `resourcetype_t` to `ResourceType`;
- `ResourceType` to `resourcetype_t`, with caller-selected fallback behavior;
- `resource_t` to `ResourceDescriptor`;
- `resource_t[]` to a descriptor snapshot vector;
- `resource_t` to resource-message row;
- `resource_t` to customization-message payload value.

This keeps the conversion rules in one place while avoiding `resource_t` or
`custom.h` exposure in the modern `src/engine/server` helper layer.

## Adapters Routed Through The Helper

The following adapters now share the conversion helper:

- `server_download_policy_adapter.cpp`;
- `server_resource_message_adapter.cpp`;
- `server_upload_queue_adapter.cpp`;
- `server_customization_message_adapter.cpp`;
- `server_consistency_policy_adapter.cpp`;
- `server_resource_catalog_adapter.cpp`;
- `server_hot_resource_adapter.cpp`;
- `server_reslist_policy_adapter.cpp`.

The shrink removes the repeated type switch and several hand-built resource
descriptor translations. It does not change exported adapter functions.

## Adapters Kept Separate

The adapters should not merge yet because each still maps to a different live
legacy owner:

- download policy reads cvars and drives file/model/custom-logo transfer
  choices through `sv_client.c`;
- upload queue decisions sit on client resource-list mutation and HPAK probes;
- resource messages write `svc_resourcelist` rows for `sv_custom.c`;
- customization payloads propagate client-specific custom resources;
- consistency policy/list helpers own force-unmodified setup and response
  validation;
- catalog, reslist, and hot-resource helpers still hang off startup and late
  precache paths in `sv_init.c`.

Keeping those adapters separate makes side effects auditable while still
sharing the value conversion they have in common.

## Compatibility Checks

`tests/engine/resource_adapter_shared.cpp` checks the shared conversion helper
directly. It covers resource type round trips, unknown fallback behavior,
descriptor snapshots, null-resource behavior, resource-message rows, and
customization-message values.

The focused engine build also verifies that the routed legacy adapters still
compile into `xash.dll`. Full validation and the runtime smoke test remain the
guard against accidental ABI or resource-transfer regressions.

## Next Step

Phase 118 should treat this as a stable private adapter utility. Further shrink
should only happen when aggregate tests prove a larger grouped adapter makes
side-effect ownership clearer, not merely because filenames look related.
