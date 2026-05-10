# Modern Server Internals

This folder contains target-neutral server helpers extracted from
`engine/server/`.

Files here should avoid including `server.h`. Legacy server globals, command
parsing, file I/O, networking sends, and game DLL callbacks belong in adapter
code until a later compatibility phase changes those boundaries.

Current helpers:

- `netapi_info.cpp`: target-neutral NetAPI info-string construction.
- `connectionless_classifier.cpp`: target-neutral server connectionless command
  classification.
- `server_filter.cpp`: target-neutral ban filter policy.
- `server_event_log.cpp`: target-neutral server event log line and stock
  message formatting.
- `source_query.cpp`: target-neutral GoldSrc query payload construction.
- `user_agent_policy.cpp`: target-neutral connection user-agent validation.
