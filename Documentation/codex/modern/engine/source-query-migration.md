# Source Query Migration

Phase 54 extracts deterministic GoldSrc query response encoding from
`engine/server/sv_query.c`.

## Implemented Shape

Modern code lives in:

- `src/include/engine/server/source_query.hpp`;
- `src/engine/server/source_query.cpp`;
- `tests/engine/source_query.cpp`.

Legacy glue lives in:

- `engine/server/source_query_adapter.h`;
- `engine/server/source_query_adapter.cpp`;
- `engine/server/sv_query.c`.

The modern builder owns byte layout for details, rules, and players. It accepts
plain value rows (`SourceQueryDetails`, `SourceQueryRule`,
`SourceQueryPlayer`) and writes little-endian protocol bytes into a
caller-provided buffer. It does not include `server.h`, read globals, or send
packets.

The adapter keeps the live server boundary in legacy code. `sv_query.c` still
walks cvars and clients, still decides whether player lists are allowed, and
still calls `NET_SendPacket`.

## Compatibility Rules

- `0xffffffff` connectionless headers are emitted byte-for-byte.
- Details responses still include bots in the player count.
- Rules responses still hide protected values as `"1"` or `"0"`.
- Empty rules and empty player lists still suppress sending.
- Player rows still use sequential exported row indexes.
- Fake-client duration remains `-1.0`.
- Platform response bytes remain compile-target dependent.

## Tests

`tests/engine/source_query.cpp` covers:

- info-response golden bytes;
- rules-response golden bytes;
- protected cvar masking, including case-insensitive `none`;
- player-response golden bytes, including signed frags and float durations;
- streaming rule construction used by the adapter.

## Later Work

A later server networking phase can decide whether the source-query helper
should use the modern bit-buffer primitive directly or remain a simpler
byte-stream builder. The current payloads are byte-aligned and string-heavy, so
the private byte writer is intentionally narrow.

The next useful server candidates after this phase are:

- connectionless duplicate handling in `sv_client.c`, especially the older
  server-info response that notes it should match `SV_SourceQuery_Details`;
- user-agent/input-device policy from `sv_main.c`;
- server event log formatting once the log/router phase is selected.
