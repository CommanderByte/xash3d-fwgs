# Modern Server Internals

This folder contains target-neutral server helpers extracted from
`engine/server/`.

Files here should avoid including `server.h`. Legacy server globals, command
parsing, file I/O, networking sends, and game DLL callbacks belong in adapter
code until a later compatibility phase changes those boundaries.

Current helpers:

- `game_dll_enginefuncs.cpp`: target-neutral metadata lookup for the stable
  `enginefuncs_t` game DLL callback table.
- `game_dll_entity_lifecycle.cpp`: target-neutral entity index, bugcompat
  player-slot, and private-data allocation/free plans for the game DLL bridge.
- `game_dll_string_pool_compat.cpp`: fixture-safe game DLL string-pool
  compatibility model for escape processing, duplicate handling, static and
  dynamic arena accounting, overflow rewind behavior, and make-string
  fallback decisions.
- `game_dll_message_session.cpp`: mockable game DLL message-session state
  machine for payload accounting and low-risk compatibility decisions.
- `game_dll_output_policy.cpp`: mockable game DLL command and output routing
  policy for callback decisions that do not own live sinks.
- `game_dll_user_message_registry.cpp`: mockable game DLL user-message
  registration policy for validation, duplicate handling, and active-server
  resend planning.
- `save_restore_format.cpp`: read-only save/restore file-format fixture
  parsing for headers, token tables, field sections, `.HL3` entity patches,
  packed short fields, and bundled save files.
- `client_policy.cpp`: target-neutral userinfo penalty, rate/update interval,
  and prediction/lag/local-weapon flag decisions.
- `client_command_dispatch.cpp`: target-neutral client command lookup and
  routing decisions.
- `netapi_info.cpp`: target-neutral NetAPI info-string construction.
- `connectionless_classifier.cpp`: target-neutral server connectionless command
  classification.
- `connection_response.cpp`: target-neutral challenge and rejection response
  string formatting.
- `resource_identity.cpp`: target-neutral custom resource identity, download
  name checks, matching, and size-summary helpers.
- `server_resource_catalog.cpp`: target-neutral startup resource catalog
  planning for generic, sound, model, decal, and event precaches.
- `server_download_policy.cpp`: target-neutral server download allow/reject,
  precache, model sidecar, and custom logo lookup decisions.
- `server_consistency_list.cpp`: target-neutral consistency-list enable and
  resource-index serialization.
- `server_consistency_policy.cpp`: target-neutral consistency setup,
  reserved bounds payload, and response validation policy.
- `server_customization_message.cpp`: target-neutral propagated customization
  payload serialization for `svc_customization`.
- `server_command_lifecycle.cpp`: target-neutral server lifecycle command
  argument, alias, and validation routing decisions.
- `server_spawn_handshake.cpp`: target-neutral fixed `svc_serverdata`
  payloads and new/spawn/begin handshake decisions.
- `server_frame_datagram.cpp`: target-neutral frame datagram fanout,
  overflow, resend, and send-loop gate decisions.
- `server_resource_message.cpp`: target-neutral resource-list row
  serialization using modern bit-buffer primitives.
- `server_upload_queue.cpp`: target-neutral client resource upload queue
  admission, missing decal estimation, limit checks, and batch actions.
- `server_filter.cpp`: target-neutral ban filter policy.
- `server_event_log.cpp`: target-neutral server event log line and stock
  message formatting.
- `server_hot_resource.cpp`: target-neutral hot-resource announcement
  planning for resources added after server startup.
- `server_reslist_policy.cpp`: target-neutral `.res` and `reslist.txt` token
  classification for safe-download filtering and resource indexing.
- `server_userinfo_message.cpp`: target-neutral `svc_updateuserinfo` payload
  serialization for client slot, user ID, active bit, sanitized userinfo, and
  hashed CD key digest bytes.
- `server_service_messages.cpp`: target-neutral payload serialization for
  compact service messages such as file-transfer failure, reconnect, set-view,
  set-pause, and voice-init.
- `server_sound_message.cpp`: target-neutral `svc_sound` / `svc_restoresound`
  command planning and bit-packed payload serialization.
- `server_static_messages.cpp`: target-neutral `svc_bspdecal` payload
  serialization and safe static-entity admission checks.
- `server_multicast_policy.cpp`: target-neutral `SV_Multicast()`
  destination and recipient routing decisions while leaving masks and writes
  legacy-owned.
- `server_text_messages.cpp`: target-neutral payload serialization for
  `svc_print` and `svc_stufftext` NUL-terminated text messages.
- `server_voice_relay.cpp`: target-neutral voice relay gates, recipient
  decisions, and `svc_voicedata` payload serialization.
- `source_query.cpp`: target-neutral GoldSrc query payload construction.
- `user_agent_policy.cpp`: target-neutral connection user-agent validation.
