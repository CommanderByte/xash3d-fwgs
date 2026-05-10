# Modern Server Headers

Private C++ contracts for target-neutral server helpers live here.

Headers in this folder should expose plain value types and small services used
by tests and legacy adapters. They should not expose game DLL, renderer,
filesystem module, or platform-specific types.

Current helpers:

- `server_filter.hpp`: ID/IP filter policy and formatting.
- `source_query.hpp`: GoldSrc source-query response byte builders.
- `user_agent_policy.hpp`: connection UUID and input-device validation policy.
