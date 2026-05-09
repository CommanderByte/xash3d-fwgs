# Engine Command-Line Facade TODO

## Purpose

Consolidate the remaining target-neutral command-line helper behavior behind
the modern `src/engine/platform/command_line.*` seam created during Phase 44.
The public `Sys_*` functions stay in `engine/common/system.c`, but they should
delegate lookup and value discovery to the modern helper layer.

## Scope

- `Sys_CheckParm`
- `Sys_GetParmFromCmdLine`
- `_Sys_GetParmFromCmdLine`
- `Sys_GetIntFromCmdLine`
- `Sys_ParseCommandLine` change-game censoring from Phase 44
- modern C++ command-line helpers and their C adapter

Out of scope:

- launcher command-line parsing;
- full process restart argument rebuilding;
- platform shell/console behavior;
- changing `Q_atoi` numeric parsing compatibility.

## Legacy Behavior Baseline

`Sys_CheckParm`:

- scans from argument index 1;
- ignores null `argv` entries;
- returns the first case-insensitive match;
- returns 0 when missing.

`Sys_GetParmFromCmdLine`:

- finds the parameter with `Sys_CheckParm`;
- returns the following raw argument string;
- returns false when the parameter is missing or has no following value;
- copies with `Q_strncpy` in the C facade.

`Sys_GetIntFromCmdLine`:

- finds the following raw argument string;
- parses the value with `Q_atoi`, preserving legacy decimal, hex, and character
  literal behavior;
- writes 0 and returns false when no value is present.

## Phase 45 Tasks: Command-Line Facade Consolidation

- [x] `ENG-CMDLINE-001` Audit `Sys_CheckParm`, `_Sys_GetParmFromCmdLine`, and
  `Sys_GetIntFromCmdLine` for target-neutral behavior.
  Evidence: this file.

- [x] `ENG-CMDLINE-002` Add modern command-line view, argument lookup, and value
  lookup helpers.
  Evidence: `src/include/engine/platform/command_line.hpp`,
  `src/engine/platform/command_line.cpp`.

- [x] `ENG-CMDLINE-003` Add C adapter functions for legacy C callers.
  Evidence: `src/include/engine/platform/command_line_adapter.h`,
  `src/engine/platform/command_line.cpp`.

- [x] `ENG-CMDLINE-004` Route legacy command-line facades through the adapter
  while preserving `Q_strncpy` and `Q_atoi` behavior in C.
  Evidence: `engine/common/system.c`.

- [x] `ENG-CMDLINE-005` Add focused tests for lookup order, case-insensitive
  matching, null entry handling, missing values, and C adapter parity.
  Evidence: `tests/engine/platform_command_line.cpp`.

- [x] `ENG-CMDLINE-006` Run focused tests, full tests, and Windows runtime
  smoke, recording first-frame timing when available.
  Evidence: `.\waf.bat build --targets=test_engine_platform_command_line`
  passed 1/1 tests, `.\waf.bat build` passed 30/30 tests, and Windows runtime
  smoke copied `build\engine\xash.dll` plus
  `build\filesystem\filesystem_stdio.dll` into `run-win32`, then ran
  `.\xash3d.exe -dev 2 -log +fs_path +quit` from `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The fresh smoke log `run-win32\engine.log` reached renderer initialization
  and stopped with reason `command` at May 10 2026 00:39:34 local time. The
  quick `+quit` smoke did not emit a first-frame timing marker.

## Notes

The value lookup helper adds an explicit `index + 1 < argc` check before
returning a value. This is a compatibility-preserving safety improvement for
the legacy `_Sys_GetParmFromCmdLine` path, which previously relied on the
caller-provided argv storage being readable past the found parameter.
