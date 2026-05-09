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

## Method

- Audit ownership before routing any output path.
- Separate message construction from platform sink behavior where possible.
- Keep varargs C APIs stable.
- Preserve existing console/log file text until tests and compatibility notes
  say otherwise.
- Decide how `xash::debugging` sinks should feed into engine output without
  making debugging utilities depend on legacy console globals.

## Phase 43 Tasks: Console And Logging Ownership

- [ ] `ENG-LOG-001` Audit definitions and callers for console, report, debug,
  log, and system print functions.
  Evidence:

- [ ] `ENG-LOG-002` Document output ownership, filtering, color/control prefix
  behavior, log file lifecycle, shutdown footer behavior, and fatal-path
  constraints.
  Evidence:

- [ ] `ENG-LOG-003` Add focused tests or a test seam for target-neutral message
  formatting/filtering behavior where practical.
  Evidence:

- [ ] `ENG-LOG-004` Decide how modern debugging utilities and deferred
  filesystem logging cleanup should feed engine output.
  Evidence:

- [ ] `ENG-LOG-005` Run focused tests, full tests, and Windows runtime smoke if
  any output path changes.
  Evidence:
