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
- `server_download_policy.hpp`: `SV_DownloadFile_f()` allow/reject/send/logo
  decisions built from resource snapshots and adapter-supplied sidecar probes.
- `server_customization_message.hpp`: propagated customization payload
  serialization for `svc_customization`.
- `server_resource_message.hpp`: resource-list row serialization using modern
  bit-buffer primitives.
- `server_upload_queue.hpp`: client resource-list admission, missing custom
  decal estimation, upload-limit, and upload batch action decisions.
- `server_filter.hpp`: ID/IP filter policy and formatting.
- `server_event_log.hpp`: server event log line and stock message formatting.
- `source_query.hpp`: GoldSrc source-query response byte builders.
- `user_agent_policy.hpp`: connection UUID and input-device validation policy.
