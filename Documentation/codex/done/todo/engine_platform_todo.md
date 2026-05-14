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

- [x] `ENG-SYS-001` Audit `system.c`, `system.h`, and platform source
  responsibilities.
  Evidence: `Documentation/codex/legacy/engine/system-platform-facade-audit.md`.

- [x] `ENG-SYS-002` Document which branches can move to `engine/platform/`
  without changing `Sys_*` callers.
  Evidence: `Documentation/codex/legacy/engine/system-platform-facade-audit.md`,
  `Documentation/codex/modern/engine/system-platform-facade-plan.md`.

- [x] `ENG-SYS-003` Identify target-neutral helpers that can gain focused tests
  before any code movement.
  Evidence: change-game command-line censor helper selected in
  `Documentation/codex/legacy/engine/system-platform-facade-audit.md`.

- [x] `ENG-SYS-004` Move one narrow platform-neutral or platform-selected helper
  only if the audit finds a low-risk candidate.
  Evidence: `src/include/engine/platform/command_line.hpp`,
  `src/include/engine/platform/command_line_adapter.h`,
  `src/engine/platform/command_line.cpp`, `engine/common/system.c`,
  `tests/engine/platform_command_line.cpp`.

- [x] `ENG-SYS-005` Run focused tests, full tests, and Windows runtime smoke if
  any platform path changes.
  Evidence: `.\waf.bat build --targets=test_engine_platform_command_line`
  passed 1/1 tests, `.\waf.bat build` passed 30/30 tests, and Windows runtime
  smoke copied `build\engine\xash.dll` plus
  `build\filesystem\filesystem_stdio.dll` into `run-win32`, then ran
  `run-win32\xash3d.exe -dev 2 -log +fs_path +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 00:14:26 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.
