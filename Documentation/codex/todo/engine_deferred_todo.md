# Engine Deferred TODO

## Purpose

Track engine/common follow-up candidates that should stay visible after the
BaseCmd command/cvar pilot moved to the completed TODO folder.

These are not the immediate next phase unless selected in `tasks.md`.

## Deferred Candidates

- [ ] `ENG-LOG-001` Audit `Con_Printf`, `Con_DPrintf`, `Con_Reportf`,
  `Log_Printf`, `Sys_Print`, and `Sys_PrintLog` ownership after command/cvar
  tests are stable.
  Evidence:

- [ ] `ENG-SYS-001` Audit `system.c` platform branches and decide what can move
  under `engine/platform/` without changing `Sys_*` callers.
  Evidence:

- [ ] `ENG-FSBRIDGE-001` Revisit `filesystem_engine.c` adapter cleanup after
  console/logging policy exists.
  Evidence:
