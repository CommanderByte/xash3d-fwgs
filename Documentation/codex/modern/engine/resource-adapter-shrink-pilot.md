# Resource Adapter Shrink Pilot

Phase 149 uses the Phase 148 aggregate resource tests as a guardrail, then
shrinks only mechanical adapter glue. It does not merge the resource call sites
or move live resource ownership out of legacy server files.

## Adapter Audit

The resource adapters fall into two groups:

- plain-value conversion adapters, where repeated glue can safely move into
  `resource_adapter_shared`;
- side-effect boundary adapters, where separate C entry points still document
  which legacy subsystem owns filesystem, HPAK, netchan, or game DLL effects.

The safe shrink target was the first group. `resource_adapter_shared` now owns:

- `resource_t[]` to checked consistency-resource index snapshots;
- modern `ResourceDescriptor` to legacy descriptor fields;
- bounded string copy for legacy fixed-size adapter buffers.

These are pure translations. They do not inspect cvars, read files, send
packets, mutate linked resource lists, call HPAK, or invoke game DLL callbacks.

## Routed Adapters

The following adapters were reduced to use the shared plain-value helpers:

- `server_consistency_list_adapter.cpp`;
- `server_resource_catalog_adapter.cpp`;
- `server_hot_resource_adapter.cpp`;
- `server_reslist_policy_adapter.cpp`;
- `game_dll_resource_policy_adapter.cpp`.

The existing adapters for download, upload, resource messages, customization
messages, and consistency policy already used the shared conversion helper or
owned genuinely separate enum/result mapping. They stay split for now.

## Boundaries Kept Separate

Phase 149 deliberately keeps these live effects in the legacy call sites:

- `sv_custom.c`: resource-list mutation, HPAK custom data probes, customization
  propagation, consistency response drops, and reliable datagram ownership.
- `sv_client.c`: filesystem/model-texture probes, HPAK custom-logo lookup,
  download fragments, resource-list parsing, and upload throttling.
- `sv_game.c`: game DLL precache/lookups, callback order, and model/decal table
  ownership.

This is still adapter shrink, not a resource-domain facade. Phase 150 can now
move to the messaging aggregate tests without inheriting extra resource-side
coupling.
