# Client Policy Migration

Phase 85 extracts the small, target-neutral userinfo and rate decisions from
`sv_client.c`.

## Modern Helper

The modern helper lives in:

- `src/include/engine/server/client/client_policy.hpp`
- `src/engine/server/client/client_policy.cpp`

It intentionally accepts plain values instead of `sv_client_t`, cvars,
`server.h`, or info-string buffers.

Current responsibilities:

- plan userinfo penalty state changes from a snapshot;
- resolve missing/malformed requested rates to defaults;
- hard-clamp client data rates to legacy `MIN_RATE` / `MAX_RATE`;
- resolve missing/malformed update rates to a default 20 Hz interval;
- apply `sv_minupdaterate` / `sv_maxupdaterate` interval limits;
- preserve the current `SV_CheckRate()` no-op behavior for `sv_minrate` and
  `sv_maxrate`;
- compute prediction, lag compensation, and local-weapons flag decisions.

## Adapter Boundary

The C adapter lives in:

- `engine/server/client_policy_adapter.h`
- `engine/server/client_policy_adapter.cpp`

The adapter is called from `SV_ShouldUpdateUserinfo()`,
`SV_CheckUpdateRate()`, `SV_CheckRate()`, and the low-risk rate/flag part of
`SV_UserinfoChanged()`.

Legacy code still owns:

- command and packet entry points;
- cvar reads;
- `Info_*` parsing and mutation;
- duplicate-name resolution;
- logging;
- game DLL callbacks;
- mutation of `sv_client_t` and `edict_t`.

## Follow-Up Options

Good future slices:

- move duplicate-name planning into a helper that consumes a snapshot of active
  client names;
- convert `SV_CheckRate()` only if a compatibility decision explicitly changes
  the current no-op cvar behavior;
- add a broader client-connection policy object once the game DLL bridge and
  connection lifecycle are better isolated.

Avoid moving `pfnClientUserInfoChanged()` or name mutation until the game DLL
bridge phase has concrete adapter fixtures.
