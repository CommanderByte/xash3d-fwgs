# Non-Windows System Runtime TODO

## Purpose

Track validation work for `system.c` runtime helper branches that Phase 49 did
not move because they cannot be verified from the current Windows environment.

## Scope

- POSIX `Sys_GetCurrentUser` using `getpwuid(geteuid())`.
- Vita `Sys_GetCurrentUser` using `sceAppUtilSystemParamGetString`.
- Switch and Android current-user fallback behavior.
- Android `Sys_GetNativeObject` fallback provider after `FS_GetNativeObject`.
- Restart helpers such as `Sys_CanRestart` and `Sys_NewInstance` when a target
  validation environment exists.

## Phase 802 Tasks: Non-Windows System Runtime Validation

- [ ] `ENG-NONWIN-SYS-001` Build a POSIX/Linux target and verify
  `Sys_GetCurrentUser` still returns the effective user name when available.
  Evidence:

- [ ] `ENG-NONWIN-SYS-002` Validate the POSIX fallback to `Player` when the
  password database lookup fails or returns an empty name, using a safe test
  harness if practical.
  Evidence:

- [ ] `ENG-NONWIN-SYS-003` Validate Vita username lookup manually before moving
  it behind the modern current-user adapter.
  Evidence:

- [ ] `ENG-NONWIN-SYS-004` Validate Android `Sys_GetNativeObject` provider
  order: filesystem provider first, Android provider second.
  Evidence:

- [ ] `ENG-NONWIN-SYS-005` Revisit `Sys_CanRestart` and `Sys_NewInstance` only
  after the target can run restart/change-game smoke tests.
  Evidence:
