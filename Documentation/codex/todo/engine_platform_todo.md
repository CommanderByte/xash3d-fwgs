# Engine Platform Facade TODO

## Purpose

Plan a platform-boundary cleanup pass for engine common code after the
console/logging ownership pass. This follows the launcher lesson: keep common
facades stable, but move platform-specific implementation branches into
platform-owned files when it is safe.

## Scope

Candidate areas:

- `engine/common/system.c`
- `engine/common/system.h`
- `engine/platform/*`
- `Sys_*` facade functions
- `Platform_*` helpers
- command-line parsing, time, environment, console/window, and crash/platform
  branches that still live in common code.

## Method

- Audit all `#if XASH_*` branches in `system.c` before moving code.
- Keep `Sys_*` declarations stable for engine/client/server callers.
- Prefer Waf-selected platform implementation files over mixed-platform common
  files.
- Add tests only for target-neutral helpers; do not fake OS behavior unless the
  seam is already explicit.

## Phase 44 Tasks: System Platform Facade Audit

- [ ] `ENG-SYS-001` Audit `system.c`, `system.h`, and platform source
  responsibilities.
  Evidence:

- [ ] `ENG-SYS-002` Document which branches can move to `engine/platform/`
  without changing `Sys_*` callers.
  Evidence:

- [ ] `ENG-SYS-003` Identify target-neutral helpers that can gain focused tests
  before any code movement.
  Evidence:

- [ ] `ENG-SYS-004` Move one narrow platform-neutral or platform-selected helper
  only if the audit finds a low-risk candidate.
  Evidence:

- [ ] `ENG-SYS-005` Run focused tests, full tests, and Windows runtime smoke if
  any platform path changes.
  Evidence:
