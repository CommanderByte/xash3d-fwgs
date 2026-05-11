# Game DLL Bridge Domain Pilot

Phase 139 physically groups the modern game DLL bridge helpers under
`src/engine/server/game_dll/` and `src/include/engine/server/game_dll/`.
A later cleanup pass removed the temporary flat forwarding headers under
`src/include/engine/server/`, so internal code now includes the canonical
`engine/server/game_dll/...` paths directly.

## Domain Shape

The grouped module is still an internal helper surface, not a new public ABI.
It keeps the game DLL bridge split into small behavior owners:

- ABI metadata: stable `enginefuncs_t` slot names, order, domains, adapter
  owners, and readiness.
- DLL load policy: required symbol checks, entity API fallback, optional API
  admission, and unload cleanup intent.
- Message bridge: user-message registration, message-session begin/write/end,
  active registration resend payloads, and rewrite admission.
- Entity lifecycle and parse policy: entity index compatibility, private data
  allocation/free intent, key-value filtering, `angle` rewrite, and spawn
  rejection decisions.
- Resource, payload, output, client-info, movement, visibility, string-pool,
  and changelevel policies.

## Compatibility Boundaries

The pilot deliberately leaves these legacy-owned:

- `enginefuncs_t` publication order and the actual callback table stored in
  `sv_game.c`.
- DLL loading/unloading, `svgame`, `server_static_t`, `server_t`, and
  `svgame.dllFuncs` lifetime.
- Live `edict_t`, `entvars_t`, private data pointers, and destructor calls.
- `MSG_*`, `sizebuf_t`, `sv.multicast`, and final network sends.
- Game DLL callback ordering such as `pfnKeyValue`, `pfnSpawn`, physics
  overrides, and save/changelevel side effects.

## Test Coverage

`tests/engine/game_dll_bridge_domain.cpp` is the Phase 139 aggregate test. It
uses the new canonical `engine/server/game_dll/...` includes and proves that
representative helpers compose without a live DLL:

- ABI metadata classifies message callbacks as message-session owned.
- User-message registration produces a session begin request and active-server
  resend payload.
- Resource normalization, sound routing, output routing, client-info routing,
  entity allocation, key-value angle rewrite, string-pool handles, fake-client
  movement, trace-model admission, changelevel queuing, and load/unload intent
  can all be evaluated as plain values.

This test does not replace the focused tests for each helper; it exists to
catch accidental domain drift after the physical folder move.

## Grouping Decision

Grouped folders reduce visual noise now that the game DLL bridge has many
small helpers. The implementation files remain separate because each maps to a
different legacy adapter or runtime responsibility. Further merging should
only happen if it removes repeated adapter code without hiding ownership of
the live game DLL ABI.
