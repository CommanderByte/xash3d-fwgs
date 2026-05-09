# Engine Console And Logging TODO

## Purpose

Plan the next engine/common ownership pass around console and logging output.
This is the blocker for resuming deferred filesystem logging cleanup because the
filesystem currently receives engine print/log callbacks through the legacy
filesystem bridge.

## Scope

Candidate areas:

- `engine/common/sys_con.c`
- `engine/common/con_utils.c`
- `Con_Printf`, `Con_DPrintf`, `Con_Reportf`
- `Log_Printf`
- `Sys_Print`, `Sys_PrintLog`
- log file lifecycle and shutdown footer behavior
- developer/report filtering and color/control prefixes
- filesystem callback ownership in `engine/common/filesystem_engine.c`

Current ownership summary:

- `engine/common/sys_con.c` owns `Con_Printf`, `Con_DPrintf`, `Con_Reportf`,
  `Sys_PrintLog`, log file open/close, timestamp prefixing, and color stripping
  or ANSI translation for low-level writes.
- `engine/common/system.c` owns `Sys_Print`, which fans formatted text out to
  the rendered console, Win32 console, engine log, and rcon.
- `engine/client/console.c` owns the rendered Half-Life style in-game console,
  scrollback, notify lines, console input UI, and console drawing.
- `engine/platform/win32/con_win.c` owns the external Win32 console window,
  command-line input editing, command history, and dedicated console status
  line.
- `engine/platform/posix/con_posix.c` owns dedicated stdin command input;
  POSIX stdout is handled by `sys_con.c`.
- `engine/server/sv_log.c` owns multiplayer server event logging and only
  echoes through `Con_Printf` when cvars ask it to.
- `engine/common/filesystem_engine.c` passes print callbacks into the
  filesystem module; filesystem logging cleanup should feed the engine output
  layer rather than own output routing.

## Method

- Audit ownership before routing any output path.
- Separate message construction from platform sink behavior where possible.
- Keep varargs C APIs stable.
- Preserve existing console/log file text until tests and compatibility notes
  say otherwise.
- Decide how `xash::debugging` sinks should feed into engine output without
  making debugging utilities depend on legacy console globals.

Design decision from the audit:

- Keep `xash::debugging` engine-neutral.
- Add engine-side adapters later if debugging records should be mirrored to the
  console/logging layer.
- Keep the rendered console and platform console as sinks, not as the core
  logging abstraction.
- Keep `Log_Printf` as a server event log service, separate from `engine.log`.
- Start implementation with platform console backend boundaries and
  target-neutral filtering/formatting helpers, not a full sink router.

Platform backend decision from the second audit:

- Treat Win32, POSIX, mobile log-only, and missing consoles as capability sets.
- Keep background platform console input separate from the rendered in-game
  console input path.
- Preserve `Platform_Input()` and `Wcon_*` style C functions as adapters while
  adding internal C++ backend types.
- Defer the broad output hub until platform backends are small, testable, and
  selected through a stable internal interface.

## Phase 43 Tasks: Console And Logging Ownership

- [x] `ENG-LOG-001` Audit definitions and callers for console, report, debug,
  log, and system print functions.
  Evidence: `Documentation/codex/legacy/engine/console-logging-baseline.md`.

- [x] `ENG-LOG-002` Document output ownership, filtering, color/control prefix
  behavior, log file lifecycle, shutdown footer behavior, and fatal-path
  constraints.
  Evidence: `Documentation/codex/legacy/engine/console-logging-baseline.md`,
  `Documentation/codex/modern/engine/console-logging-migration-guide.md`.

- [ ] `ENG-LOG-003` Add focused tests or a test seam for target-neutral message
  formatting/filtering behavior where practical.
  Evidence: pending implementation pass.

- [x] `ENG-LOG-004` Decide how modern debugging utilities and deferred
  filesystem logging cleanup should feed engine output.
  Evidence: `Documentation/codex/modern/engine/console-logging-migration-guide.md`.

- [x] `ENG-LOG-005` Run focused tests, full tests, and Windows runtime smoke if
  any output path changes.
  Evidence: backend-wrapper changes did not reroute any legacy output/input
  path, so Windows runtime smoke is not required yet. Focused/full validation
  evidence is recorded under `ENG-LOG-007`, `ENG-LOG-008`, and `ENG-LOG-009`;
  the first live `Wcon_*` routing smoke test is tracked by `ENG-LOG-013`.

- [x] `ENG-LOG-006` Document background console backend ownership, command
  input hierarchy, and per-platform capability expectations.
  Evidence: `Documentation/codex/modern/engine/platform-console-backends.md`,
  `Documentation/codex/legacy/engine/console-logging-baseline.md`.

- [x] `ENG-LOG-007` Add internal platform-console capability/config types,
  null backend, and focused unit tests.
  Evidence: `src/include/engine/console/platform_console_backend.hpp`,
  `src/engine/console/platform_console_backend.cpp`,
  `tests/engine/platform_console_backend.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend`,
  `.\waf.bat build --targets=test_engine_base_command_registry,test_engine_command_buffer,test_engine_info_string,test_engine_network_buffer,test_engine_platform_console_backend`,
  direct execution of `build\src\test_engine_platform_console_backend.exe`,
  and `.\waf.bat build` passed 24/24 executed tests.

- [x] `ENG-LOG-008` Add a POSIX/Linux-style background console backend wrapper
  with injectable output/input and tests that preserve current `Platform_Input`
  semantics: output is available after initialization, command reads require a
  dedicated host configuration, disabled input returns no command, and missing
  input/output capabilities are honored.
  Evidence: `src/include/engine/console/platform_console_backend.hpp`,
  `src/engine/console/platform_console_backend.cpp`,
  `tests/engine/platform_console_backend.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend`,
  direct execution of `build\src\test_engine_platform_console_backend.exe`,
  `.\waf.bat build --targets=test_engine_base_command_registry,test_engine_command_buffer,test_engine_info_string,test_engine_network_buffer,test_engine_platform_console_backend`,
  and `.\waf.bat build` passed 24/24 executed tests.

- [x] `ENG-LOG-009` Add a Win32 external-console backend wrapper with
  injectable output/input/lifecycle operations and tests for current `Wcon_*`
  behavior boundaries: print/show/status/read are normal capabilities,
  input-disable and command registration are dedicated-only, missing
  capabilities are honored, and shutdown is idempotent.
  Evidence: `src/include/engine/console/platform_console_backend.hpp`,
  `src/engine/console/platform_console_backend.cpp`,
  `tests/engine/platform_console_backend.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend`,
  direct execution of `build\src\test_engine_platform_console_backend.exe`,
  `.\waf.bat build --targets=test_engine_base_command_registry,test_engine_command_buffer,test_engine_info_string,test_engine_network_buffer,test_engine_platform_console_backend`,
  and `.\waf.bat build` passed 24/24 executed tests.

- [ ] `ENG-LOG-010` Decide whether Android/iOS/Switch/Vita should use explicit
  output-only backends or remain direct `Sys_PrintStdout()` platform branches
  until the router phase.
  Evidence: pending implementation pass.

- [x] `ENG-LOG-011` Move live POSIX/Linux console routing and validation out of
  Phase 43.
  Evidence: `Documentation/codex/todo/posix_console_backend_todo.md`, Phase 800
  in `Documentation/codex/tasks.md`.

- [x] `ENG-LOG-012` Document the rendered in-game console sink boundary and
  defer code extraction until a later router/client-rendering phase.
  Evidence: `Documentation/codex/modern/engine/rendered-console-sink.md`.

- [ ] `ENG-LOG-013` Route the existing Win32 `Wcon_*` C functions through the
  Win32 backend wrapper and run a Windows runtime smoke test.
  Evidence: pending implementation pass.
