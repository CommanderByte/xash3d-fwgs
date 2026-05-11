# Resource Transfer Consolidation Audit

Phase 115 audits the resource-transfer domain after the C++ ownership
checkpoint. The goal is to decide how the existing resource helpers should
converge without prematurely moving live server state out of the legacy runtime.

## Current Answer

Resource transfer is a real modern domain, not just a set of unrelated wrappers.
The safe first consolidation step is **aggregate tests around a target-neutral
resource manifest/list-view seam**. Directory regrouping and adapter merging
should wait until those tests prove the grouped behavior is clearer than the
current focused helper set.

## Legacy Ownership Map

### `sv_init.c`

`sv_init.c` owns startup and hot-resource publication:

- `SV_ModelIndex()`, `SV_SoundIndex()`, `SV_EventIndex()`, and
  `SV_GenericIndex()` update the live precache arrays and send late resource
  announcements when the server is already active.
- `SV_ReadResourceList()` parses `.res` files and `reslist.txt`, then routes
  tokens into sound or generic precaches.
- `SV_CreateGenericResources()` adds map-specific resource lists and world WAD
  requirements.
- `SV_CreateResourceList()` builds `sv.resources[]` from the precache arrays,
  decals, event scripts, file-size probes, and catalog helper decisions.
- `SV_SendSingleResource()` handles late precache announcements through
  `sv.reliable_datagram`.
- `SV_ActivateServer()` sequences generic resources, resource-list creation,
  consistency setup, HPAK flushes, and resource cleanup.

Legacy-owned state and side effects:

- `sv.model_precache`, `sv.sound_precache`, `sv.files_precache`,
  `sv.event_precache`, `sv.model_precache_flags`;
- `sv.resources[]`, `sv.num_resources`, `sv.reliable_datagram`;
- `host.draw_decals`, `world.wadlist`, `sv.state`;
- filesystem probes through `FS_FileSize()` and resource-list file reads;
- console output and `Host_Error()` behavior.

### `sv_custom.c`

`sv_custom.c` is the current live transfer service:

- creates per-client customization lists from resources on hand;
- sets up and sends file consistency lists;
- parses file consistency responses and calls the game DLL inconsistent-file
  callback;
- owns the `resourcesneeded` and `resourcesonhand` linked-list mutation helpers;
- estimates missing custom resources and requests uploads;
- propagates customizations to other spawned clients;
- serializes resource rows, customization payloads, and consistency lists.

Legacy-owned state and side effects:

- `resource_t` linked-list storage and mutation;
- `cl->resourcesneeded`, `cl->resourcesonhand`, `cl->customdata`, `cl->upstate`;
- HPAK lookup and data access through `HPAK_*`;
- `COM_CreateCustomization()` and customization list lifetime;
- game DLL callbacks `pfnPlayerCustomization()` and `pfnInconsistentFile()`;
- `sv.consistency_list`, `sv.num_consistency`, `sv.resources[]`;
- client drops, client prints, netchan message mutation, and cvar reads.

### `sv_client.c`

`sv_client.c` connects resource transfer to client commands and packets:

- `SV_SendRes_f()` sends the startup resource list and fragments it through
  netchan;
- `SV_DownloadFile_f()` handles client download requests, including model
  texture sidecar probes and custom-logo HPAK lookup;
- `SV_ParseResourceList()` parses client resource lists, validates
  descriptors, enforces update timing, estimates upload needs, applies upload
  limits, and starts batch upload requests;
- `SV_ExecuteClientMessage()` dispatches `clc_resourcelist` and
  `clc_fileconsistency` into the resource/customization code.

Legacy-owned state and side effects:

- command argument reads through `Cmd_Argv()`/`Cmd_Argc()`;
- `sv_allow_download`, `sv_send_resources`, `sv_send_logos`,
  `sv_allow_upload`, `sv_uploadmax`, and `sv_upload_penalty_time`;
- `Netchan_CreateFileFragments()`, `Netchan_CreateFileFragmentsFromBuffer()`,
  `Netchan_CreateFragments()`, and `Netchan_FragSend()`;
- file probes through `FS_FileExists()` and `Mod_StudioTexName()`;
- resource list allocation through `Z_Calloc()` and cleanup through
  `SV_ClearResourceList()`.

### `sv_game.c`

`sv_game.c` owns the game DLL side of resource admission:

- `SV_SetModel()` uses the game DLL resource name policy, indexes models, and
  updates edict model state;
- `pfnPrecacheModel()`, `pfnModelIndex()`, `SV_SoundIndex()`,
  `SV_GenericIndex()`, `pfnPrecacheEvent()`, and `pfnDecalIndex()` expose
  resource lookup and precache behavior through the game DLL ABI;
- `pfnForceUnmodified()` records consistency requirements during loading and
  validates later requests against the precached consistency list.

Legacy-owned state and side effects:

- game DLL ABI callback table order and exported calling conventions;
- edict mutation, model handles, model bounds, and string-pool interaction;
- precache array mutation through `SV_*Index()` functions;
- consistency list storage through `sv.consistency_list`;
- console output and fatal limit behavior.

## Existing Modern Helpers

### Shared Identity

- `resource_identity`: modern `ResourceType`, `ResourceDescriptor`, safe
  download-name checks, `!MD5` parsing/formatting, resource-name matching, and
  size summaries.

This is the natural base for a future resource manifest. It is already
target-neutral.

### Startup And Catalog

- `server_resource_catalog`: catalog entries for generic, sound, model, decal,
  and event resources.
- `server_reslist_policy`: `.res` and `reslist.txt` token normalization and
  routing.
- `server_hot_resource`: late-precache file-size queries and announcement
  entries.
- `game_dll_resource_policy`: game-DLL-facing name normalization, optional
  model handling, slot bounds, and duplicate-name comparison.

### Transfer And Client Uploads

- `server_download_policy`: client download admission, model texture sidecar
  routing, regular resource lookup, and custom-logo hash parsing.
- `server_upload_queue`: client resource descriptor validation, update timing,
  missing-resource estimation, upload limit, and batch actions.

### Consistency And Customization

- `server_consistency_list`: consistency-list serialization and
  force-unmodified decision.
- `server_consistency_policy`: consistency setup, reserved model bounds,
  checksum/bounds response evaluation, and count matching.
- `server_customization_message`: `svc_customization` payload serialization.

### Wire Payloads

- `server_resource_message`: resource row serialization for `svc_resourcelist`
  and hot-resource announcements.

## Current Adapter Shape

The resource adapters are still small and understandable, but they duplicate a
few translations:

- multiple adapters map `resourcetype_t` to modern `ResourceType`;
- `server_download_policy_adapter.cpp` builds a vector of
  `ResourceDescriptor` snapshots from `resource_t`;
- `server_upload_queue_adapter.cpp`, `server_resource_message_adapter.cpp`,
  and `server_customization_message_adapter.cpp` each translate a
  `resource_t` into a modern value object;
- consistency adapters intentionally stay closer to live `resource_t` data
  because they deal with reserved bytes, hashes, and model bounds.

This duplication is not yet bad enough to justify a broad adapter merge, but it
does identify a later shrink target: a private resource adapter utility can own
legacy `resource_t` to modern descriptor conversion once aggregate resource
tests exist.

## Separation Faults In The Old Grouping

The old file grouping is historically sensible, but it mixes domain concepts:

- `sv_init.c` is both a server startup file and a resource manifest builder.
- `sv_custom.c` is named after custom resources, but it also owns consistency
  checks, resource row serialization, upload queues, customization propagation,
  and linked-list mutation.
- `sv_client.c` owns client command dispatch and session state, but it also
  performs resource transfer orchestration.
- `sv_game.c` is a game DLL bridge, but resource callbacks feed the same
  resource catalog and consistency systems.

The modern grouping should not mirror those files directly. A future
`server/resources` domain should be organized around the resource lifecycle:

1. **Admission:** game DLL precache names, `.res` tokens, client request names.
2. **Manifest:** catalog entries and resource descriptor lists.
3. **Publication:** resource rows, consistency lists, hot-resource messages.
4. **Transfers:** download and upload decisions.
5. **Customization:** custom resource propagation and payloads.
6. **Consistency:** force-unmodified setup and response validation.

## Missing Aggregate Tests

Existing tests cover individual helper leaves well. The missing coverage is
cross-helper behavior:

1. **Catalog to download lookup**
   Build generic, sound, model, decal, and event catalog entries, place them in
   a descriptor list, then verify download decisions find the same resources
   and keep GoldSrc sound path quirks.

2. **Catalog to resource message**
   Build catalog entries and serialize them as resource rows, including model
   flags, sound sentence marker behavior, custom hashes, reserved consistency
   bytes, and overflow handling.

3. **Reslist to manifest**
   Classify `.res` tokens, route supported sounds to sound precache paths,
   route unsupported sounds to generic paths, and verify the resulting manifest
   entries preserve normalized paths.

4. **Consistency setup to response**
   Mark resources as force-unmodified, build the consistency list, then verify
   checksum and bounds response evaluation against the same resource indexes.

5. **Upload queue to customization payload**
   Parse a custom decal descriptor into upload decisions, estimate missing data,
   choose batch upload action, and serialize a customization payload after the
   resource is considered on hand.

6. **Hot resource announcement**
   Build a late-precache file-size query, add the announced descriptor to a
   manifest, and serialize the hot resource row.

These tests should remain target-neutral. They should use modern descriptors
and byte buffers, not live `sv.resources[]`, HPAK, filesystem probes, cvars, or
netchan fragments.

## First Seam Recommendation

The first grouped seam should be **aggregate tests plus a target-neutral
resource manifest/list-view helper if the tests reveal repeated glue**.

Recommended Phase 116 starting point:

- create aggregate tests that build a small descriptor list from catalog
  entries;
- exercise download lookup and resource-row serialization against that same
  list;
- include at least one sound resource, one model with `RES_FATALIFMISSING`,
  one generic resource, and one event script;
- do not move directories yet;
- do not merge adapters yet;
- add a helper only if it avoids duplicating manifest/list traversal across the
  aggregate tests.

Good helper name candidates:

- `resource_transfer_manifest`;
- `server_resource_manifest`;
- `resource_transfer_view`.

The helper should accept and expose modern `ResourceDescriptor` values. It
should not include `custom.h`, `server.h`, `resource_t`, `sv_client_t`, cvars,
HPAK calls, filesystem calls, or netchan calls.

## Deferred Work

Keep these legacy-owned until later implementation phases:

- resource storage in `sv.resources[]` and `resource_t` linked lists;
- HPAK lookup, HPAK data reads, and HPAK flush/purge behavior;
- filesystem probes and `.res` file loading;
- server cvars that allow or reject resource transfer behavior;
- client drops, client prints, console output, and netchan fragments;
- game DLL callbacks for customization and inconsistent files;
- game DLL ABI callback table order;
- edict model mutation and model bounds.

## Decision

Proceed to Phase 116 with aggregate resource-transfer tests first. If the test
code needs a repeated descriptor-list abstraction, add a small target-neutral
manifest helper. Leave directory regrouping and adapter consolidation for Phase
117 or later, after the manifest boundary is proven.
