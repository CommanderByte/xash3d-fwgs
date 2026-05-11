# Server Domain Layout Plan

Phase: 136

## Purpose

This plan defines where modern server code should eventually live once the
flat `src/engine/server` helper layer starts consolidating by domain. It does
not move files yet. The intent is to make future moves boring: every helper has
an intended domain, every risky include/build effect is named, and the legacy
ABI boundaries remain explicit.

## Target Layout

Use this as the long-term target under `src/engine/server` and
`src/include/engine/server`.

```text
src/engine/server/
  shared/
  resources/
  messaging/
  game_dll/
  client/
  runtime/
  world/
  save/
```

The mirrored private headers should use the same subdirectories:

```text
src/include/engine/server/
  shared/
  resources/
  messaging/
  game_dll/
  client/
  runtime/
  world/
  save/
```

### `shared`

Server-wide constraints and pure policies that are consumed by more than one
domain.

Initial candidates:

- `server_limits`
- `server_lifecycle_limits`
- `server_group_filter`
- `server_map_validation`
- `server_visibility_constraints`

This directory must stay small. If a helper mainly belongs to one domain, put
it in that domain and let other domains depend on it.

### `resources`

Resource identity, catalog, transfer, consistency, customization, and reslist
logic.

Initial candidates:

- `resource_identity`
- `resource_transfer_manifest`
- `server_resource_catalog`
- `server_download_policy`
- `server_upload_queue`
- `server_consistency_list`
- `server_consistency_policy`
- `server_hot_resource`
- `server_reslist_policy`

Cross-domain note: `server_resource_message` writes resource protocol rows, so
it can start in `messaging` or `resources`. Prefer `messaging` if the file stays
primarily a payload writer; prefer `resources` if Phase 137 turns it into part
of a resource transfer facade. `server_customization_message` has the same
split: customization is a resource-transfer concern, but the current helper is
primarily a `svc_customization` payload writer.

### `messaging`

Network-message payload builders, recipient/envelope policy, frame send gates,
and packet-entity cursor planning.

Initial candidates:

- `server_message_envelope`
- `server_text_messages`
- `server_service_messages`
- `server_sound_message`
- `server_static_messages`
- `server_userinfo_message`
- `server_voice_relay`
- `server_resource_message`
- `server_customization_message`
- `server_spawn_handshake`
- `server_multicast_policy`
- `server_event_playback_policy`
- `server_frame_datagram`
- `server_packet_entities_delta`

Cross-domain note: frame/datagram and packet-entity helpers are messaging
adjacent today, but they may eventually move to a `snapshot` subdomain once
real frame snapshot ownership moves.

### `game_dll`

Game DLL ABI metadata, DLL load policy, callback-domain helpers, entity
lifecycle policy, user-message registry, game-DLL-facing string pool behavior,
and callback-side movement/visibility/resource/message helpers.

Initial candidates:

- `game_dll_enginefuncs`
- `game_dll_load_policy`
- `game_dll_entity_lifecycle`
- `game_dll_entity_parse`
- `game_dll_string_pool_compat`
- `game_dll_message_bridge`
- `game_dll_message_session`
- `game_dll_user_message_registry`
- `game_dll_output_policy`
- `game_dll_payload_policy`
- `game_dll_resource_policy`
- `game_dll_client_info_policy`
- `game_dll_changelevel_policy`
- `game_dll_movement_policy`
- `game_dll_visibility_trace_policy`

Possible future internal grouping:

```text
game_dll/
  abi/
  lifecycle/
  entities/
  messaging/
  resources/
  movement/
  world_query/
  output/
  string_pool/
  changelevel/
```

Do not introduce these subdirectories until Phase 139 proves they reduce
confusion rather than adding directory theater.

### `client`

Client admission, session slots, command dispatch, userinfo/capability policy,
connectionless/query responses, remote admin, and client-facing policy that is
not resource or message specific.

Initial candidates:

- `client_command_dispatch`
- `client_policy`
- `client_session_slots`
- `connection_response`
- `connectionless_classifier`
- `server_challenge_policy`
- `user_agent_policy`
- `source_query`
- `netapi_info`
- `remote_admin_command`

Cross-domain note: downloads/uploads and voice stay in `resources` and
`messaging` respectively unless Phase 140 proves the client/session domain
needs a facade over them.

### `runtime`

Server shell behavior: lifecycle command policy, operator command admission,
timeout policy, event/log formatting, admin filters, and other runtime
decisions that are neither game DLL nor client transfer ownership.

Initial candidates:

- `server_command_lifecycle`
- `server_operator_command_policy`
- `server_timeout_policy`
- `server_event_log`
- `server_filter`

Cross-domain note: `server_map_validation` and `server_lifecycle_limits` are
runtime-adjacent but should start in `shared` because save/load, game DLL
changelevel, and spawn/lifecycle helpers also consume them.

### `world`

World, physics, movement constraints, visibility capacity, area-link policy,
PMove bridge policy, and future trace fixtures.

Initial candidates:

- `server_world_link_policy`
- `server_physics_routing_policy`
- `server_pmove_bridge_policy`
- `server_movement_constraints`

Cross-domain note: `server_group_filter` and `server_visibility_constraints`
are heavily world-adjacent but currently cross resource/message/game-DLL/frame
boundaries. Keep them under `shared` until a later visibility/world domain owns
the real data.

### `save`

Save/restore format fixtures and later pure save-admission, save-comment,
version, manifest, and entity-patch value objects.

Initial candidates:

- `save_restore_format`

Future candidates from Phase 141:

- save admission snapshots;
- save version/comment classification;
- save archive manifests;
- entity patch plans;
- landmark transition value objects.

## Current Helper Domain Map

| Domain | Current helpers |
| --- | --- |
| `shared` | `server_limits`, `server_lifecycle_limits`, `server_group_filter`, `server_map_validation`, `server_visibility_constraints` |
| `resources` | `resource_identity`, `resource_transfer_manifest`, `server_resource_catalog`, `server_download_policy`, `server_upload_queue`, `server_consistency_list`, `server_consistency_policy`, `server_hot_resource`, `server_reslist_policy` |
| `messaging` | `server_message_envelope`, `server_text_messages`, `server_service_messages`, `server_sound_message`, `server_static_messages`, `server_userinfo_message`, `server_voice_relay`, `server_resource_message`, `server_customization_message`, `server_spawn_handshake`, `server_multicast_policy`, `server_event_playback_policy`, `server_frame_datagram`, `server_packet_entities_delta` |
| `game_dll` | `game_dll_enginefuncs`, `game_dll_load_policy`, `game_dll_entity_lifecycle`, `game_dll_entity_parse`, `game_dll_string_pool_compat`, `game_dll_message_bridge`, `game_dll_message_session`, `game_dll_user_message_registry`, `game_dll_output_policy`, `game_dll_payload_policy`, `game_dll_resource_policy`, `game_dll_client_info_policy`, `game_dll_changelevel_policy`, `game_dll_movement_policy`, `game_dll_visibility_trace_policy` |
| `client` | `client_command_dispatch`, `client_policy`, `client_session_slots`, `connection_response`, `connectionless_classifier`, `server_challenge_policy`, `user_agent_policy`, `source_query`, `netapi_info`, `remote_admin_command` |
| `runtime` | `server_command_lifecycle`, `server_operator_command_policy`, `server_timeout_policy`, `server_event_log`, `server_filter` |
| `world` | `server_world_link_policy`, `server_physics_routing_policy`, `server_pmove_bridge_policy`, `server_movement_constraints` |
| `save` | `save_restore_format` |

## Temporary Facades

The following helpers should be treated as scaffolding until a domain phase
decides otherwise:

- `resource_transfer_manifest`: likely becomes the resource domain's first
  facade if Phase 137 strengthens aggregate tests.
- `server_message_envelope`: useful shared messaging vocabulary, but not a
  full messaging owner.
- `game_dll_message_bridge`: useful game DLL messaging aggregate, but it should
  not become a broad `sv_game.c` replacement.
- `server_frame_datagram` and `server_packet_entities_delta`: currently
  messaging-adjacent; eventual home may be snapshot/frame ownership.
- `server_filter`: runtime/admin behavior owner, but live filter lists and
  files still live in `sv_filter.c`.
- `save_restore_format`: fixture parser, not a runtime save system.

## Build And Include Impact

### Modern implementation sources

`src/wscript` builds `modern_engine` with:

```python
bld.path.ant_glob('engine/**/*.cpp')
```

Because this glob is recursive, moving modern `.cpp` files below
`src/engine/server/<domain>/` should not require source-list edits.

### Modern private headers

The project currently includes headers as flat paths, for example:

```cpp
#include "engine/server/server_resource_catalog.hpp"
```

Moving headers to subdirectories changes include paths. To avoid touching every
adapter/test in the same commit, use one of these strategies:

1. keep flat forwarding headers temporarily:

```cpp
// src/include/engine/server/server_resource_catalog.hpp
#include "engine/server/resources/server_resource_catalog.hpp"
```

2. or update the full domain in one commit and run full tests immediately.

Forwarding headers are preferred for the first physical move because they
preserve adapter call sites while making the new layout visible.

### Legacy adapters

`engine/wscript` lists every C++ adapter source explicitly. Moving adapter
implementations under subdirectories such as `engine/server/resources/` would
require source-list edits and possibly include-dir updates.

Do not move legacy adapters during the first domain layout pass. Keep adapters
flat under `engine/server` until a grouped domain adapter exists and the Waf
source-list change is intentional.

### Tests

`src/wscript` lists test source paths explicitly in the `tests` dictionary.
Moving tests into domain subdirectories such as `tests/engine/server/resources`
would require explicit path updates.

Recommendation: keep tests flat for the first resource and messaging pilots.
Once the domain layout stabilizes, tests can be moved in a separate mechanical
commit.

### Waf target names

Keep existing test target names stable during file moves. A test target such as
`test_engine_server_resource_catalog` should not be renamed just because the
source file moves.

## No-Move / No-Rename Boundaries

Do not move or rename these during domain consolidation:

- public SDK and ABI headers such as `engine/eiface.h`;
- `engine/server/server.h` before Phase 143 completes;
- legacy `sv_*.c` files;
- game DLL callback table fields and `enginefuncs_t` ordering;
- exported `SV_*` names consumed outside the legacy server module;
- C adapter function names that are already called by legacy C files;
- protocol command names, byte layouts, bit widths, save formats, demo
  formats, and file names;
- `SAVERESTOREDATA`, `ENTITYTABLE`, `edict_t`, `sv_client_t`, `server_t`, and
  `server_static_t` layout;
- build target names used by automation and phase validation scripts.

If a future phase must touch one of these boundaries, it should be its own
compatibility task with focused tests and runtime smoke evidence.

## Recommended First Physical Move

After Phase 136, the first safe physical move should be limited to modern
resource helpers:

1. create `src/engine/server/resources/` and
   `src/include/engine/server/resources/`;
2. move only resource-domain `.cpp` and `.hpp` files;
3. leave flat forwarding headers in `src/include/engine/server/`;
4. keep adapters flat in `engine/server`;
5. keep test files and target names unchanged;
6. run the resource-focused tests, full tests, and a smoke build.

This lets Phase 137 prove the layout policy on a domain with good existing
coverage and relatively low runtime risk.

## Validation

Phase 136 is documentation-only. Validation should be:

- `scripts/phase-status.ps1 -PhaseNumber 136`;
- `git diff --check`.
