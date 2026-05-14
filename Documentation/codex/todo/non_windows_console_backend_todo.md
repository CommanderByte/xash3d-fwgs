# Non-Windows Console Backend TODO

## Purpose

Track console and log-output backend work for platforms that we cannot validate
in the current Windows development pass. Phase 43 should stay focused on the
live Win32 routing path; POSIX/Linux validation lives in Phase 800, and this
document covers the remaining output-only or platform-specific targets.

## Scope

Candidate platform paths:

- Android log output in `Sys_PrintStdout()`.
- iOS `IOS_Log()` output.
- Switch and Vita debug/stderr output paths.
- Any platform where a background command console is absent or intentionally
  output-only.

## Migration Position

Treat these platforms as capability sets:

- `Output` may mean platform debug log, stderr, or no-op depending on target.
- `Input`, `Visibility`, `StatusLine`, and `CommandRegistration` are normally
  absent.
- The first implementation should avoid routing changes until the target can be
  built or manually validated.
- If validation is unavailable, document the expected behavior and leave the
  current `Sys_PrintStdout()` branch intact.

## Phase 801 Tasks

- [ ] `ENG-NONWIN-CON-001` Audit Android, iOS, Switch, Vita, and other
  non-Windows console/log output branches after the Windows backend route is
  stable.
  Evidence:

- [ ] `ENG-NONWIN-CON-002` Decide whether each platform should gain an explicit
  output-only backend or remain a direct `Sys_PrintStdout()` platform branch
  until the router phase.
  Evidence:

- [ ] `ENG-NONWIN-CON-003` Build or cross-compile at least one non-Windows
  target where practical before moving any live platform branch.
  Evidence:

- [ ] `ENG-NONWIN-CON-004` Record manual validation expectations for platforms
  that cannot be built in the current Windows environment.
  Evidence:
