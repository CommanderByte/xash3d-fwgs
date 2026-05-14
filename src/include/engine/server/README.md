# Modern Server Headers

Private C++ contracts for target-neutral server helpers live here. These
headers are internal to the modernization layer, focused tests, and legacy
adapters; they are not public engine or game DLL ABI.

## Include Rules

- Use canonical grouped include paths, for example
  `engine/server/resources/server_resource_catalog.hpp`.
- Expose plain values, enums, small builders, and narrow policy functions.
- Do not expose game DLL, renderer, filesystem module, platform-specific, or
  legacy server storage types.
- Do not include `server.h` from modern headers.

## Domains

- `shared/`: server-wide limit, lifecycle, group-filter, map-validation, and
  visibility-constraint contracts.
- `runtime/`: server shell, command, filter, and log-formatting contracts.
- `save/`: savegame format, value, and runtime fixture contracts.
- `world/`: simulation-facing movement, physics, PMove, world-link, and trace
  setup contracts.
- `resources/`: resource identity, transfer, consistency, catalog, upload,
  download, hot-resource, and reslist contracts.
- `messaging/`: message envelope, payload writer, recipient, event, datagram,
  voice, static, sound, text, service, and packet-entity contracts.
- `game_dll/`: game DLL ABI metadata, lifecycle, entity, message, resource,
  payload, movement, visibility/trace, output, changelevel, and string-pool
  contracts.
- `client/`: admission, session, command dispatch, challenge, query, timeout,
  remote-admin, userinfo, and user-agent contracts.

## Compatibility Rule

The legacy C ABI remains in the legacy headers and adapters. Modern headers may
model compatibility behavior, but they should not publish C++ types across
game DLL, renderer, filesystem, or platform module boundaries.
