# Resource Transfer Domain Pilot

Phase 137 is the first physical consolidation pass after the server domain
layout plan. It uses the resource-transfer helpers because they are already
well covered by focused tests and have a clear domain boundary.

## Consolidated Domain

The resource-transfer domain now owns the modern value helpers for:

- resource identity and safe download-name matching;
- aggregate transfer manifests;
- startup resource catalog entries;
- download and upload policy decisions;
- consistency-list and consistency-response policy;
- hot-resource announcement planning;
- `.res` and `reslist.txt` token classification.

These files moved from the flat `src/engine/server/` layer into
`src/engine/server/resources/`:

- `resource_identity.cpp`
- `resource_transfer_manifest.cpp`
- `server_resource_catalog.cpp`
- `server_download_policy.cpp`
- `server_upload_queue.cpp`
- `server_consistency_list.cpp`
- `server_consistency_policy.cpp`
- `server_hot_resource.cpp`
- `server_reslist_policy.cpp`

The matching C++ contracts moved into
`src/include/engine/server/resources/`. The previous flat header paths under
`src/include/engine/server/` remain as forwarding headers, so existing tests
and legacy adapters can keep their includes until a later cleanup pass.

## Messaging Boundary

`server_resource_message` and `server_customization_message` are still in the
flat server messaging layer. They serialize resource-shaped data, but their
primary ownership is packet payload format rather than resource catalog or
transfer state. Phase 137 added aggregate manifest coverage that feeds both
writers without moving them into the resource directory.

## Legacy-Owned Boundaries

The following surfaces remain legacy-owned:

- HPAK lookup and persistence;
- filesystem probes and file-size reads;
- live `resource_t` linked-list mutation;
- `MSG_*`, `sizebuf_t`, netchan fragments, and packet delivery;
- `svc_resource*` and `svc_customization` command routing;
- game DLL callbacks and resource callback ordering.

The modern resource helpers should continue to receive snapshots or plain
descriptors from adapters. They should not take dependencies on `server.h`,
`client_t`, `resource_t`, filesystem APIs, or netchan state.

## Validation

- Focused resource-domain build:
  `.\waf.bat build --targets=test_engine_resource_transfer_manifest,test_engine_resource_identity,test_engine_server_resource_catalog,test_engine_server_download_policy,test_engine_server_upload_queue,test_engine_server_consistency_list,test_engine_server_consistency_policy,test_engine_server_hot_resource,test_engine_server_reslist_policy`
  passed 9/9.
- `.\waf.bat build --alltests` passed 129/129.
- Runtime smoke via `scripts/run-phase-validation.ps1 -SkipFocused -SkipFullTests`
  built `xash`, refreshed `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit`, reached first frame
  in 0.501 seconds, and stopped with reason `command` at May 11 2026
  13:49:11 local time.
