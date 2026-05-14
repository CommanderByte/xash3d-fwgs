# Server C++ Ownership Consolidation Checkpoint

Phase 114 checks whether the modern server helpers are moving toward real C++
ownership, or whether they are becoming a larger set of prettier C wrappers.
The answer is mixed in a useful way: the helper layer has created strong test
coverage and a better vocabulary, but the file layout still reflects the order
in which legacy seams were extracted.

This checkpoint follows `Documentation/codex/modern/cpp-ownership-target.md`.

## Current Inventory

Current scan:

- 52 modern server `.cpp` files under `src/engine/server`;
- 52 matching modern server `.hpp` files under `src/include/engine/server`;
- 43 server adapter `.cpp` files under `engine/server`;
- 52 focused engine tests covering the server, game DLL bridge, and adjacent
  policy helpers.

That is a good amount of tested surface, but it is not the final module shape.
One source/header/test triplet per extracted seam was the right migration
method. It should not automatically become the long-term architecture.

## Ownership Classes

### Reusable Domain Concepts

These helpers are already more than temporary wrappers. They describe concepts
that can be reused across old `sv_*.c` boundaries or across future modern
modules.

- Client and connection:
  `client_policy`, `client_command_dispatch`, `connection_response`,
  `connectionless_classifier`, `server_challenge_policy`,
  `server_spawn_handshake`, `source_query`, `netapi_info`, and
  `user_agent_policy`.
- Server resources and transfers:
  `resource_identity`, `server_resource_catalog`, `server_resource_message`,
  `server_reslist_policy`, `server_download_policy`, `server_upload_queue`,
  `server_consistency_list`, `server_consistency_policy`,
  `server_customization_message`, `server_hot_resource`, and
  `game_dll_resource_policy`.
- Server messaging and recipient decisions:
  `server_text_messages`, `server_service_messages`, `server_sound_message`,
  `server_static_messages`, `server_voice_relay`, `server_multicast_policy`,
  `server_event_log`, `server_event_playback_policy`, and
  `server_frame_datagram`.
- Game DLL bridge:
  `game_dll_enginefuncs`, `game_dll_load_policy`,
  `game_dll_entity_lifecycle`, `game_dll_entity_parse`,
  `game_dll_string_pool_compat`, `game_dll_message_session`,
  `game_dll_user_message_registry`, `game_dll_visibility_trace_policy`,
  `game_dll_movement_policy`, `game_dll_output_policy`,
  `game_dll_payload_policy`, `game_dll_client_info_policy`, and
  `game_dll_changelevel_policy`.
- World, rules, and constraints:
  `server_group_filter`, `server_map_validation`,
  `server_movement_constraints`, `server_visibility_constraints`,
  `server_limits`, and `server_lifecycle_limits`.
- Engine-wide helpers that affect the server:
  read-only `cvar_snapshot` helpers belong above the server layer because the
  server is only one consumer of cvar reads.

### Behavior Owners

These helpers own a narrow behavior slice and are valuable as-is, but they have
not yet proven that they should be a public concept or directory.

- `server_command_lifecycle`: currently a server command normalization and
  lifecycle planning helper. It belongs near server runtime or operator
  command work if that area grows.
- `server_filter`: a strong behavior owner for ban filter decisions, but live
  command/file ownership remains in `sv_filter.c`.
- `save_restore_format`: a fixture parser and compatibility model. It is
  valuable, but runtime save/load ownership is still legacy-owned.
- `server_frame_datagram`: real frame send-loop policy, but it depends on
  frame, snapshot, visibility, and client buffer ownership that still live in
  `sv_frame.c`.

### Temporary Facades

These are still scaffolding. They are useful and should remain boring, but they
should not be treated as architecture.

- C adapter files under `engine/server/*_adapter.*`.
- The `server_cvar_snapshot_adapter` bridge, because the modern concept is
  engine-wide and the server call site only asks for one snapshot.
- Any helper whose only job is to translate live legacy structs into plain
  values for a modern policy call.

The adapter count is not a failure. It is the compatibility cost of keeping
game DLL ABI, server globals, packet buffers, commands, cvars, filesystem
calls, allocation, and console output legacy-owned during migration.

## Legacy Grouping Review

The old `sv_*.c` grouping is useful as a compatibility map, but several files
mix concerns that should eventually separate.

### `sv_game.c`

Current grouping: game DLL bridge, edict lifecycle, string pool, user-message
callbacks, message writing, resources, trace/visibility, movement callbacks,
event playback, changelevel/save intent, and DLL load/unload.

Better separation:

- `GameDllBridge`: exported callback table and ABI publication;
- `GameDllLifecycle`: load/unload, fake symbol handling, extension tables;
- `GameDllEntities`: edict lifecycle, private data, spawn/parse decisions;
- `GameDllMessaging`: message sessions, user messages, text/sound/static
  payloads, reliable datagram access;
- `GameDllResources`: precache, model/sound/generic/event indexes, resource
  policy;
- `GameDllWorldQueries`: trace, visibility, entity lookup, PVS/PAS route
  choices;
- `GameDllStringPool`: string compatibility, static/dynamic arenas, overflow
  behavior.

This is the biggest adapter-reduction opportunity, but only after grouped
ownership is stable. A single giant `game_dll_adapter.cpp` would be worse than
today's smaller adapters unless it is organized around those internal concepts.

### `sv_client.c`

Current grouping: connection handshake, challenge validation, rejection
responses, fake clients, downloads, userinfo/rate policy, client command
dispatch, rcon/redirects, move parsing, voice data, resource lists, cvar query
responses, and entity debug commands.

Better separation:

- `ClientAdmission`: challenge, bandwidth test, IP restrictions, empty slot,
  rejection strings;
- `ClientSession`: connect, drop, fake client, begin/spawn transitions;
- `ClientUserinfo`: rates, flags, identity strings, player counts;
- `ClientCommand`: command dispatch and admin/debug entity commands;
- `ClientTransfer`: downloads, client resource list parsing, upload batches;
- `ClientVoice`: voice relay parsing and recipient decisions;
- `RemoteAdmin`: rcon, redirects, and status/info responses.

### `sv_custom.c`

Current grouping: custom resource lists, consistency setup/response, upload
requests, HPAK/resource side effects, and customization propagation.

Better separation:

- `ResourceTransfer`: upload/download admission and batch planning;
- `ResourceConsistency`: consistency list setup and responses;
- `CustomizationPropagation`: client-to-client custom resource announcements.

This area should converge with the resource helpers already in the modern tree.

### `sv_frame.c`

Current grouping: entity packet selection, baseline deltas, event emission,
ping emission, clientdata, reliable update fanout, datagram send loops, and
inactive-client handling.

Better separation:

- `SnapshotBuilder`: packet entity selection and baselines;
- `VisibilityFrame`: PVS/PAS and group-filter decisions;
- `EventEmission`: queued event visibility and payload routing;
- `DatagramWriter`: reliable/unreliable send-loop decisions;
- `ClientFrameState`: update skipping and inactive-client bookkeeping.

### `sv_world.c`, `sv_phys.c`, `sv_move.c`, and `sv_pmove.c`

Current grouping is closer to domain ownership, but still mixes storage,
collision, physics, and exported game DLL callbacks.

Better separation:

- `WorldLinks`: area tree, link/unlink, trigger touch admission;
- `WorldTrace`: hull selection, clip-to-entity, move traces, texture/surface
  trace;
- `WorldVisibility`: leaf collection, PVS/PAS, portal view constraints;
- `PhysicsStep`: entity think/run, gravity, toss, pusher, blocker, impact;
- `MonsterMove`: check-bottom, move-step, chase/fly route decisions;
- `PlayerMoveBridge`: PMove setup, unlag state, physent population.

This is high-risk runtime code. Keep it audit-first until fixture coverage is
stronger.

### `sv_init.c`, `sv_main.c`, and `sv_cmds.c`

These files still own much of the runtime shell:

- `sv_init.c`: resource precache, baseline creation, activation/deactivation,
  test packets, spawn server;
- `sv_main.c`: cvar registration, server frame, timeouts, packet read loop,
  master heartbeat, shutdown;
- `sv_cmds.c`: operator commands, lifecycle commands, map/save/load
  commands, status/info commands.

Better separation:

- `ServerRuntime`: init, active state, shutdown, frame gate;
- `ServerConfiguration`: cvar registration, read-only snapshots, movevars;
- `ServerLifecycle`: spawn, activate, deactivate, changelevel requests;
- `ServerOperatorCommands`: command registration and command handlers;
- `MasterHeartbeat`: master server announcements and status reporting.

## Proposed Modern Layout

Do not move all files immediately. When a domain is ready, group it by concept:

```text
src/engine/server/
  client/
  connection/
  game_dll/
  messaging/
  resources/
  runtime/
  world/
  save/
```

Headers would mirror that layout under `src/include/engine/server`.

This shape follows the useful parts of the old `sv_*.c` grouping while fixing
the parts that became mixed because C files grew around call sites rather than
owned concepts.

## Adapter Reduction Strategy

Do not merge adapters simply to reduce file count. A grouped adapter should
exist only when it maps to a real modern domain.

Reasonable future grouped adapters:

- `game_dll_bridge_adapter.cpp`: game DLL ABI publication and callback routing,
  internally split by lifecycle, entities, messages, resources, world queries,
  and string pool.
- `server_resource_adapter.cpp`: resource catalog, consistency, download,
  upload, customization, hot-resource, and reslist bridges.
- `server_message_adapter.cpp`: text, service, sound, static, voice,
  multicast, event playback, and frame datagram bridges.
- `server_client_adapter.cpp`: client admission, userinfo, command dispatch,
  challenge/rejection, and query response bridges.
- `server_world_adapter.cpp`: only after visibility, hull, and trace fixtures
  exist.

Adapters that should stay separate for now:

- world/physics/PMove adapters, because runtime state and fixture risk remain
  high;
- cvar snapshot adapters, because the modern helper is engine-wide;
- save/restore runtime adapters, until stream ownership and game DLL field
  serialization are audited.

## Test Policy

Keep the existing focused tests. They are the compatibility net that makes
consolidation possible.

When files are regrouped, add concept-level tests for the grouped domain rather
than deleting the narrow tests. For example:

- resource transfer tests should cover the combined catalog/download/upload
  flow in addition to individual policy tests;
- messaging tests should cover recipient choice plus payload construction when
  those concepts move together;
- game DLL bridge tests should cover callback-domain routing without requiring
  a live DLL;
- world/trace tests should wait for synthetic or golden fixtures.

## Recommendation

Phase 114 should not move files yet. The correct next step is to use this
classification as a map for the next consolidation phases.

Best near-term targets:

1. resource transfer grouping, because several helpers already share resource
   identity, catalogs, consistency, uploads, downloads, and customizations;
2. server messaging grouping, because message payload writers and multicast or
   event routing are related but currently split into many small files;
3. game DLL bridge grouping, because the broad old `sv_game.c` file has the
   clearest need for conceptual submodules;
4. client/session grouping, because `sv_client.c` currently mixes admission,
   commands, transfers, voice, and admin surfaces;
5. world/physics grouping only after stronger visibility, hull, and trace
   fixtures exist.

This keeps the iterative method, but prevents migration scaffolding from
quietly becoming the permanent architecture.
