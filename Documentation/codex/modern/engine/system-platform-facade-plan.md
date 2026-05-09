# System Platform Facade Plan

## Direction

The public `Sys_*` functions should remain the compatibility surface for engine
callers. Modern code can move behind that surface in two forms:

- platform-selected implementation files under `engine/platform/` or a later
  modern platform service;
- target-neutral helpers under `src/engine/platform/` with C adapters where
  legacy C files need to call them.

This mirrors the filesystem and launcher strategy: modernize inward while
leaving the legacy call surface stable.

## First Implementation Slice

The first Phase 44 code slice is the change-game command-line censor helper.
The legacy behavior remains:

- only `Sys_ParseCommandLine` mutates `host.argv`;
- mutation only happens when `host.change_game` is true;
- `-game`, `+game`, `+map`, `+load`, and `+changelevel` are replaced with
  `censored`;
- matching is case-insensitive.

The modern helper owns the blocked-argument list and tests. The C facade still
owns the host-global mutation.

Implemented files:

- `src/include/engine/platform/command_line.hpp`
- `src/include/engine/platform/command_line_adapter.h`
- `src/engine/platform/command_line.cpp`
- `tests/engine/platform_command_line.cpp`
- `engine/common/system.c`

## Later Candidates

- Phase 45 extends the command-line seam to `Sys_CheckParm`,
  `Sys_GetParmFromCmdLine`, and `Sys_GetIntFromCmdLine`.
- Username lookup can move behind platform-selected implementations after
  platform validation.
- Win32 console print normalization can move behind a tested system-console
  formatter when we are ready to route more of `Sys_Print`.
- Restart and fatal-error paths should stay in legacy common code until their
  shutdown constraints have dedicated tests and smoke coverage.
