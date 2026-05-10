# Modern Server Headers

Private C++ contracts for target-neutral server helpers live here.

Headers in this folder should expose plain value types and small services used
by tests and legacy adapters. They should not expose game DLL, renderer,
filesystem module, or platform-specific types.

Current helpers:

- `client_command_dispatch.hpp`: server client-command lookup and routing
  decisions.
- `netapi_info.hpp`: short `A2A_INFO` and long `A2A_NETINFO` info-string
  response builders.
- `connectionless_classifier.hpp`: server connectionless command
  classification.
- `connection_response.hpp`: challenge and rejection response string
  formatting.
- `resource_identity.hpp`: custom resource `!MD5` identity, safe download-name
  checks, resource matching, and size summaries.
- `server_resource_catalog.hpp`: server startup resource catalog planning for
  generic, sound, model, decal, and event precaches.
- `server_download_policy.hpp`: `SV_DownloadFile_f()` allow/reject/send/logo
  decisions built from resource snapshots and adapter-supplied sidecar probes.
- `server_consistency_list.hpp`: consistency-list enable and resource-index
  serialization for server resource checks.
- `server_consistency_policy.hpp`: consistency setup, reserved bounds payload,
  and response validation policy.
- `server_customization_message.hpp`: propagated customization payload
  serialization for `svc_customization`.
- `server_resource_message.hpp`: resource-list row serialization using modern
  bit-buffer primitives.
- `server_upload_queue.hpp`: client resource-list admission, missing custom
  decal estimation, upload-limit, and upload batch action decisions.
- `server_filter.hpp`: ID/IP filter policy and formatting.
- `server_event_log.hpp`: server event log line and stock message formatting.
- `server_hot_resource.hpp`: hot-resource announcement planning for resources
  added after server startup.
- `server_reslist_policy.hpp`: `.res` and `reslist.txt` token classification
  for safe-download filtering and resource indexing.
- `server_userinfo_message.hpp`: `svc_updateuserinfo` payload serialization
  for client slot, user ID, active bit, sanitized userinfo, and hashed CD key
  digest bytes.
- `server_service_messages.hpp`: compact service-message payload writers for
  file-transfer failure, reconnect, set-view, set-pause, and voice-init.
- `server_sound_message.hpp`: `svc_sound` / `svc_restoresound` constants,
  network flag planning, stream-channel handling, and bit-packed payload
  writers.
- `server_text_messages.hpp`: `svc_print` and `svc_stufftext` command constants
  and NUL-terminated text payload writers.
- `server_voice_relay.hpp`: voice relay gates, per-recipient decisions, and
  `svc_voicedata` payload serialization.
- `source_query.hpp`: GoldSrc source-query response byte builders.
- `user_agent_policy.hpp`: connection UUID and input-device validation policy.
