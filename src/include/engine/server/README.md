# Modern Server Headers

Private C++ contracts for target-neutral server helpers live here.

Headers in this folder should expose plain value types and small services used
by tests and legacy adapters. They should not expose game DLL, renderer,
filesystem module, or platform-specific types.

`resources/` contains grouped resource-domain contracts. The flat resource
headers in this directory currently forward to that subdirectory for adapter
compatibility.

`messaging/` contains grouped message payload, recipient, frame send-gate, and
packet-entity cursor contracts. The flat messaging headers in this directory
currently forward to that subdirectory for adapter compatibility.

`game_dll/` contains grouped game DLL bridge contracts. The flat game DLL
headers in this directory currently forward to that subdirectory so legacy
adapters can keep their includes while the modern module has a clearer home.

Current helpers:

- `game_dll/game_dll_changelevel_policy.hpp`: game DLL changelevel request,
  queued transition, and entity-patch write planning contracts.
- `game_dll/game_dll_client_info_policy.hpp`: client/userinfo, player stats,
  cvar-query, and game-directory routing contracts.
- `game_dll/game_dll_enginefuncs.hpp`: metadata for the stable
  `enginefuncs_t` game DLL callback table, including slot order, domains,
  adapter owners, and migration readiness.
- `game_dll/game_dll_entity_lifecycle.hpp`: game DLL entity lookup, index, and
  private-data allocation/free planning contracts.
- `game_dll/game_dll_entity_parse.hpp`: map entity key-value, `angle` rewrite,
  custom entity, parse, load, and spawn planning contracts.
- `game_dll/game_dll_load_policy.hpp`: fake-symbol game DLL load/unload decision
  contracts for required exports, API fallback, optional interfaces, and
  cleanup planning.
- `game_dll/game_dll_message_bridge.hpp`: aggregate game DLL message bridge contracts
  for building user-message begin requests and active registration resend
  payloads without owning live `sv.multicast` or callback publication.
- `game_dll/game_dll_message_session.hpp`: target-neutral model for game DLL
  message begin/write/end state, payload accounting, rewrite admission, and
  destination bounds.
- `game_dll/game_dll_movement_policy.hpp`: target-neutral movement and fake-client
  callback plans for angle stepping, walkmove routing, maxspeed clamping, and
  fake-client command snapshots.
- `game_dll/game_dll_output_policy.hpp`: target-neutral policy for game DLL command,
  client print, server print, alert, and end-section callback routing.
- `game_dll/game_dll_payload_policy.hpp`: sound, ambient, particle,
  lightstyle, decal, and make-static payload planning contracts.
- `game_dll/game_dll_resource_policy.hpp`: resource name normalization, slot,
  and model-load admission contracts.
- `game_dll/game_dll_string_pool_compat.hpp`: string processing, string-pool
  compatibility, override, and `MAKE_STRING` fallback contracts.
- `game_dll/game_dll_user_message_registry.hpp`: target-neutral policy for game DLL
  user-message registration, duplicate lookup, size validation, and resend
  planning.
- `game_dll/game_dll_visibility_trace_policy.hpp`: trace, visibility, PVS, and
  skip-player admission/result planning contracts.
- `save_restore_format.hpp`: read-only save/restore binary fixture parser
  contracts for headers, sections, entity patches, packed short fields, and
  bundled file entries.
- `client_policy.hpp`: userinfo penalty, rate/update interval, private
  client-flag snapshot/predicate, and prediction/lag/local-weapon flag decision
  contracts.
- `client_command_dispatch.hpp`: server client-command lookup and routing
  decisions.
- `netapi_info.hpp`: short `A2A_INFO` and long `A2A_NETINFO` info-string
  response builders.
- `connectionless_classifier.hpp`: server connectionless command
  classification.
- `connection_response.hpp`: challenge and rejection response string
  formatting.
- `resources/resource_identity.hpp`: custom resource `!MD5` identity, safe download-name
  checks, resource matching, and size summaries.
- `resources/resource_transfer_manifest.hpp`: target-neutral aggregate manifest over
  modern resource descriptors for catalog-to-download and resource-message
  flows.
- `remote_admin_command.hpp`: target-neutral rcon enable/password action and
  quoted command reconstruction helpers while redirects and command execution
  stay legacy-owned.
- `resources/server_resource_catalog.hpp`: server startup resource catalog planning for
  generic, sound, model, decal, and event precaches.
- `resources/server_download_policy.hpp`: `SV_DownloadFile_f()` allow/reject/send/logo
  decisions built from resource snapshots and adapter-supplied sidecar probes.
- `resources/server_consistency_list.hpp`: consistency-list enable and resource-index
  serialization for server resource checks.
- `resources/server_consistency_policy.hpp`: consistency setup, reserved bounds payload,
  and response validation policy.
- `messaging/server_customization_message.hpp`: propagated customization payload
  serialization for `svc_customization`.
- `server_command_lifecycle.hpp`: server lifecycle command request
  normalization and action planning.
- `messaging/server_spawn_handshake.hpp`: fixed `svc_serverdata` payload constants,
  signon-number payload writer, and new/spawn/begin handshake decisions.
- `messaging/server_frame_datagram.hpp`: frame datagram transfer, overflow, resend, and
  send-loop gate plans.
- `messaging/server_resource_message.hpp`: resource-list row serialization using modern
  bit-buffer primitives.
- `resources/server_upload_queue.hpp`: client resource-list admission, missing custom
  decal estimation, upload-limit, and upload batch action decisions.
- `server_filter.hpp`: ID/IP filter policy and formatting.
- `server_group_filter.hpp`: entity group-filter operation, pair-filter, and
  active-mask decision contracts.
- `server_event_log.hpp`: server event log line and stock message formatting.
- `resources/server_hot_resource.hpp`: hot-resource announcement planning for resources
  added after server startup.
- `resources/server_reslist_policy.hpp`: `.res` and `reslist.txt` token classification
  for safe-download filtering and resource indexing.
- `server_limits.hpp`: server-only limits, flags, and private constants
  mirrored as typed modern values with compatibility-role metadata.
- `server_challenge_policy.hpp`: challenge-window calculation and accepted
  current/previous window pair contracts.
- `server_lifecycle_limits.hpp`: maxclient, update-backup, packet-entity
  capacity, game-entity count, and spawn settling policy contracts.
- `server_map_validation.hpp`: map validation flag decoding, load
  classification, changelevel landmark decision, and game DLL existence
  compatibility contracts.
- `messaging/server_message_envelope.hpp`: shared server message command/string envelope
  writers and plain recipient-facts adapters for aggregate messaging tests.
- `server_movement_constraints.hpp`: server monster movement mode and
  fly-move clip-plane constraint contracts.
- `server_physics_routing_policy.hpp`: server `MOVETYPE_*` handler routing,
  pusher-candidate, and precise-blocking predicates.
- `server_pmove_bridge_policy.hpp`: PMove unlag admission, interpolation
  timing, teleport threshold, and player-index predicates.
- `server_operator_command_policy.hpp`: `kick`, `serverinfo`, and `localinfo`
  argument classification while command registration, console output, lookup,
  cvar mutation, and info-string mutation stay legacy-owned.
- `messaging/server_event_playback_policy.hpp`: event playback admission, flag
  normalization, recipient decisions, queue-slot planning, and queued emit-count
  clamping.
- `messaging/server_packet_entities_delta.hpp`: packet-entity header and sorted cursor
  planning for `SV_EmitPacketEntities()` while `entity_state_t` storage,
  baselines, delta writes, and `client_frame_t` mutation stay legacy-owned.
- `server_visibility_constraints.hpp`: entity leaf capacity, overflow marker,
  cached leaf index, and portal viewentity capacity contracts.
- `server_world_link_policy.hpp`: area-node split axis, split distance, link
  child selection, and recursive child traversal mask contracts for
  `sv_world.c`.
- `messaging/server_userinfo_message.hpp`: `svc_updateuserinfo` payload serialization
  for client slot, user ID, active bit, sanitized userinfo, and hashed CD key
  digest bytes.
- `messaging/server_service_messages.hpp`: compact service-message payload writers for
  file-transfer failure, reconnect, set-view, set-pause, and voice-init.
- `messaging/server_sound_message.hpp`: `svc_sound` / `svc_restoresound` constants,
  network flag planning, stream-channel handling, and bit-packed payload
  writers.
- `messaging/server_static_messages.hpp`: `svc_bspdecal` payload writer constants and
  static-entity admission decisions.
- `messaging/server_text_messages.hpp`: `svc_print` and `svc_stufftext` command constants
  and NUL-terminated text payload writers.
- `server_timeout_policy.hpp`: `SV_CheckTimeouts()` client timeout and pause
  release decisions built from plain runtime snapshots while cvar reads, local
  address checks, client drops, and pause toggles stay legacy-owned.
- `messaging/server_voice_relay.hpp`: voice relay gates, per-recipient decisions, and
  `svc_voicedata` payload serialization.
- `source_query.hpp`: GoldSrc source-query response byte builders.
- `user_agent_policy.hpp`: connection UUID and input-device validation policy.
