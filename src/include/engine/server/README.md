# Modern Server Headers

Private C++ contracts for target-neutral server helpers live here.

Headers in this folder should expose plain value types and small services used
by tests and legacy adapters. They should not expose game DLL, renderer,
filesystem module, or platform-specific types.

Current helpers:

- `netapi_info.hpp`: short `A2A_INFO` and long `A2A_NETINFO` info-string
  response builders.
- `connectionless_classifier.hpp`: server connectionless command
  classification.
- `connection_response.hpp`: challenge and rejection response string
  formatting.
- `server_filter.hpp`: ID/IP filter policy and formatting.
- `server_event_log.hpp`: server event log line and stock message formatting.
- `source_query.hpp`: GoldSrc source-query response byte builders.
- `user_agent_policy.hpp`: connection UUID and input-device validation policy.
