# Engine Common TODO

## Purpose

Track the engine/common modernization work after the initial audit. The current
recommendation is to avoid starting with `host.c` or console/logging directly.
Instead, use the command/cvar registry area as the next pilot because it is
central, testable, and already has a registry-like shape.

## Phase 34 Tasks: Engine/Common Audit

- [x] `ENG-AUDIT-001` Audit `engine/` scale, build ownership, and major
  directory responsibilities.
  Evidence: `Documentation/codex/legacy/engine/common-audit.md`.

- [x] `ENG-AUDIT-002` Audit `engine/common/` clusters, global state, and
  coupling hotspots.
  Evidence: `Documentation/codex/legacy/engine/common-audit.md`.

- [x] `ENG-AUDIT-003` Recommend the next practical modernization pilot.
  Evidence: `Documentation/codex/legacy/engine/common-audit.md`.
  Decision: start with command/cvar ownership and tests before changing
  console/logging or host lifecycle.

## Phase 35 Tasks: Modern Engine Skeleton

- [x] `ENG-STRUCT-001` Create the initial `src/engine/` modernization
  structure without wiring it into the build.
  Evidence: `src/engine/README.md`, `src/include/engine/README.md`.

- [x] `ENG-STRUCT-002` Reserve engine implementation lanes for command/cvar,
  console/logging, filesystem bridge, host lifecycle, memory, models, network,
  and platform facades.
  Evidence: `src/engine/*/README.md`, `src/include/engine/*/README.md`.

- [x] `ENG-STRUCT-003` Mark command/cvar as the first intended implementation
  lane while keeping the other folders as future homes only.
  Evidence: `src/engine/README.md`, `Documentation/codex/done/todo/engine_common_todo.md`.

## Phase 36 Tasks: Command/Cvar Baseline

- [x] `ENG-CMD-001` Document `cmd.c`, `cvar.c`, and `base_cmd.c` ownership,
  lifecycle, and legacy quirks.
  Evidence: `Documentation/codex/legacy/engine/command-cvar-baseline.md`.

- [x] `ENG-CMD-002` Inventory existing command/cvar tests under
  `XASH_ENGINE_TESTS` and decide which behavior should become standalone unit
  tests.
  Evidence: `Documentation/codex/legacy/engine/command-cvar-baseline.md`.

- [x] `ENG-CMD-003` Add BaseCmd-level tests for duplicate typed entries and
  command/cvar/alias name collisions.
  Evidence: `tests/engine/base_command_registry.cpp`; command
  `.\waf.bat build --targets=test_engine_base_command_registry`.
  Note: higher-level `Cmd_AddCommandEx`, alias creation, and
  `Cvar_RegisterVariable` duplicate policy is covered under `ENG-CMD-003B`.

- [x] `ENG-CMD-003B` Add higher-level tests for duplicate command/cvar
  registration policy.
  Evidence: `engine/common/cmd.c`, `engine/common/cvar.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.

- [x] `ENG-CMD-004` Add tests for privileged and filterable command/cvar
  behavior.
  Evidence: existing and expanded `Test_RunCmd` / `Test_RunCvar` coverage in
  `engine/common/cmd.c` and `engine/common/cvar.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.

- [x] `ENG-CMD-005` Add tests for command buffer insertion, execution order,
  `wait`, and overflow behavior.
  Evidence: `Test_RunCommandBufferPolicy` in `engine/common/cmd.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.

- [x] `ENG-CMD-006` Decide whether the first implementation helper should be a
  C adapter around `src/engine/commands/` or a private C++ helper used directly
  by `cmd.c` and `cvar.c`.
  Decision: start with a private C++ `BaseCommandRegistry` helper and standalone
  tests; add the C adapter only after the helper semantics are pinned.
  Evidence: `Documentation/codex/modern/engine/basecmd-migration-guide.md`,
  `src/include/engine/commands/base_command_registry.hpp`.

## Phase 37 Candidate: Command/Cvar Migration Pilot

- [x] `ENG-CMD-007` Create the first command/cvar implementation adapter only
  after Phase 36 has enough behavior coverage to compare against.
  Evidence: `engine/common/base_cmd_adapter.h`,
  `engine/common/base_cmd_adapter.cpp`, `engine/common/base_cmd.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.

- [x] `ENG-CMD-008` Introduce a private helper under `src/engine/commands/`
  without changing the public `Cmd_*`, `Cbuf_*`, `Cvar_*`, or `BaseCmd_*`
  surface.
  Evidence: `src/include/engine/commands/base_command_registry.hpp`,
  `src/engine/commands/base_command_registry.cpp`,
  `tests/engine/base_command_registry.cpp`; command
  `.\waf.bat build --targets=test_engine_base_command_registry`.

- [x] `ENG-CMD-009` Build and run the focused command/cvar tests plus full
  `.\waf.bat build --alltests` after the first helper extraction.
  Evidence: command
  `.\waf.bat build --targets=test_engine_base_command_registry` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.

- [x] `ENG-CMD-009A` Add a parallel/shadow verification step before replacing
  the legacy BaseCmd table.
  Evidence: `tests/engine/base_command_registry.cpp`; command
  `.\waf.bat build --targets=test_engine_base_command_registry` passed;
  command `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
  Note: this started as a pre-routing shadow comparison. `BaseCommandRegistry`
  is now authoritative through `engine/common/base_cmd_adapter.cpp`, while the
  focused shadow test remains as the lower-level behavior mirror.

- [x] `ENG-CMD-010` Run a Windows runtime smoke after command/cvar behavior is
  touched, because startup scripts and cvar registration are launch-sensitive.
  Evidence: copied `build\src\xash3d.exe`, `build\engine\xash.dll`, and
  `build\filesystem\filesystem_stdio.dll` to `run-win32`; ran
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  Exit code was 0; `engine.log` recorded `Time to first frame: 0.568 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"` on 2026-05-09.

## Follow-Up

Deferred engine/common candidates have moved to
`Documentation/codex/todo/engine_deferred_todo.md` so this completed pilot can
live under `Documentation/codex/done/todo/`.
