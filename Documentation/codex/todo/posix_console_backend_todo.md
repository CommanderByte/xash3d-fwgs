# POSIX Console Backend Validation TODO

## Purpose

Defer live POSIX/Linux console routing until a POSIX validation environment is
available. Phase 43 added a target-neutral POSIX-style backend wrapper with
injectable I/O and Windows-build tests, but it did not route the live
`engine/platform/posix` stdin/stdout paths.

## Scope

- `engine/platform/posix/con_posix.c`
- `engine/platform/posix/sys_posix.c`
- `engine/platform/platform.h` `Platform_Input()` selection
- `engine/common/sys_con.c` POSIX stdout branch
- `src/include/engine/console/platform_console_backend.hpp`
- `src/engine/console/platform_console_backend.cpp`

## Validation Notes

- Preserve dedicated-only stdin command polling.
- Preserve daemonize behavior where stdin/stdout/stderr can be redirected to
  `/dev/null`.
- Preserve mobile and low-memory exclusions.
- Validate Linux desktop/dedicated behavior first; BSD/macOS should be checked
  when available.

## Phase 800 Tasks: POSIX Console Backend Validation

- [ ] `ENG-POSIX-CON-001` Build on a POSIX/Linux target with the current
  backend wrapper present.
  Evidence:

- [ ] `ENG-POSIX-CON-002` Route `Platform_Input()` through the POSIX backend
  without changing `Host_GetCommands()` behavior.
  Evidence:

- [ ] `ENG-POSIX-CON-003` Route POSIX stdout output through the backend or
  document why stdout remains in `Sys_PrintStdout()` until the router phase.
  Evidence:

- [ ] `ENG-POSIX-CON-004` Validate dedicated stdin command input manually.
  Evidence:

- [ ] `ENG-POSIX-CON-005` Validate daemonize/no-stdin behavior manually.
  Evidence:

- [ ] `ENG-POSIX-CON-006` Run full tests and a POSIX runtime smoke test.
  Evidence:
