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

- [ ] `ENG-LOG-005` Run focused tests, full tests, and Windows runtime smoke if
  any output path changes.
  Evidence: no output path changed during the audit pass; pending code changes.

- [x] `ENG-LOG-006` Document background console backend ownership, command
  input hierarchy, and per-platform capability expectations.
  Evidence: `Documentation/codex/modern/engine/platform-console-backends.md`,
  `Documentation/codex/legacy/engine/console-logging-baseline.md`.

- [ ] `ENG-LOG-007` Add internal platform-console capability/config types,
  null backend, and focused unit tests.
  Evidence: pending implementation pass.

- [ ] `ENG-LOG-008` Wrap POSIX/Linux background console output/input behind a
  platform console backend while preserving `Platform_Input()` semantics.
  Evidence: pending implementation pass.

- [ ] `ENG-LOG-009` Wrap Win32 external console output/input/lifecycle behind
  a platform console backend while preserving the existing `Wcon_*` C surface.
  Evidence: pending implementation pass.

- [ ] `ENG-LOG-010` Decide whether Android/iOS/Switch/Vita should use explicit
  output-only backends or remain direct `Sys_PrintStdout()` platform branches
  until the router phase.
  Evidence: pending implementation pass.
