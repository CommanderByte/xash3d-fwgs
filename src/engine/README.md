# Modern Engine Area

This folder is reserved for modern engine internals extracted from the legacy
`engine/` tree.

Folders here are not build-wired just because they exist. Each implementation
slice should be added only when it has:

- a legacy ownership note;
- behavior tests or a smoke-test plan;
- a C-compatible adapter plan when legacy callers still own the public surface.

## Initial Layout

- `commands/`: command buffer, command registry, aliases, cvar registry helpers.
- `console/`: future console/logging policy and sink routing.
- `filesystem/`: engine-side filesystem bridge helpers, not filesystem module
  internals.
- `host/`: future host lifecycle helpers called by legacy `host.c`.
- `memory/`: future memory pool helpers around `zone.c` behavior.
- `models/`: future model and world-loading helpers after binary-format tests.
- `network/`: future net buffer/channel/protocol helpers after protocol tests.
- `platform/`: engine-side platform facade helpers behind existing `Sys_*`
  contracts.

The first build-wired implementation lane is `commands/`, starting with a
private `BaseCommandRegistry` helper for the legacy `BaseCmd_*` table.
