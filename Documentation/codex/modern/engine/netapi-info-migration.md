# NetAPI Info Migration

Phase 56 extracts deterministic info-string construction from the legacy
connectionless server-info paths.

## Implemented Shape

Modern code lives in:

- `src/include/engine/server/netapi_info.hpp`;
- `src/engine/server/netapi_info.cpp`;
- `tests/engine/netapi_info.cpp`.

Legacy glue lives in:

- `engine/server/netapi_info_adapter.h`;
- `engine/server/netapi_info_adapter.cpp`;
- `engine/server/sv_client.c`.

The modern builder owns the info-string key order and formatting. It consumes
plain value rows for short server info, NetAPI rules, players, and details.

The adapter keeps these live concerns in legacy code:

- `Cmd_Argv()` request parsing;
- cvar iteration;
- client iteration;
- `SV_GetPlayerCount()`;
- `SV_HavePassword()`;
- `Netchan_OutOfBandPrint()`.

## Compatibility Rules

- Short `A2A_INFO` wrong-version responses remain plain text, not info strings.
- Short server-info hostnames are still written last and truncated with the old
  `Q_strncpy` size-minus-one behavior.
- Protected cvar masking matches Phase 54 source-query behavior.
- NetAPI player rows keep sequential exported indexes.
- Player time uses `%f` formatting.
- Forbidden/protocol/undefined errors keep the legacy `neterror` values.

## Tests

`tests/engine/netapi_info.cpp` covers:

- short server-info key order and wrong-version text;
- protocol, undefined, forbidden, and ping responses;
- protected and unprotected rule responses;
- player rows with signed frags and float time formatting;
- details response key order.

## Later Work

The long NetAPI details path still does not literally match the Phase 54 source
query response; it only overlaps on hostname, game folder, current/max players,
and map. A later connectionless-query compatibility phase can decide whether to
share a richer snapshot between both paths.
