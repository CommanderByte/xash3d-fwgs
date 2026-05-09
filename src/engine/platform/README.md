# Engine Platform Facades

Reserved for future helpers that reduce platform branching in common engine
code while preserving existing `Sys_*` contracts.

The legacy implementation still lives mainly under `engine/common/system.c` and
`engine/platform/`.

- `command_line.cpp` owns the target-neutral change-game command-line censor
  rule used by `Sys_ParseCommandLine`.
