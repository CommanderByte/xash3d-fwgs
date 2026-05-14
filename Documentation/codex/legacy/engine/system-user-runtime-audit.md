# System User And Runtime Audit

## Scope

Phase 49 narrows the broader `system.c` platform audit to helpers that expose
user/runtime information through the legacy `Sys_*` facade:

- `Sys_GetCurrentUser`
- `Sys_GetNativeObject`
- nearby runtime helpers that still mix common engine policy with platform
  branches.

The public C surface remains `engine/common/system.h`.

## `Sys_GetCurrentUser`

Current behavior by platform:

| Platform | Current behavior | Phase 49 action |
| --- | --- | --- |
| Win32 | Calls `GetUserNameW`, converts to UTF-8, returns `Player` fallback. | Move live lookup behind `Xash_GetCurrentUserName`; test fallback policy in modern code. |
| Vita | Calls `sceAppUtilSystemParamGetString`, falls back to `Player`. | Keep legacy branch until Vita validation is available. |
| POSIX except Android/Switch | Uses `getpwuid(geteuid())`, copies `pw_name`, falls back to `Player`. | Keep legacy branch until POSIX validation is available. |
| Android/Switch/other | Falls through to `Player`. | Document as deferred non-Windows validation. |

The target-neutral policy is simple and now testable:

- non-empty candidate names are accepted as-is;
- null or empty candidates become `Player`.

Whitespace-only names remain accepted because the legacy code only rejected
empty strings.

## `Sys_GetNativeObject`

Current behavior:

1. Reject null/empty object names.
2. Ask `FS_GetNativeObject` first.
3. On Android, ask `Android_GetNativeObject` if the filesystem did not answer.
4. Return null otherwise.

This helper is already mostly a routing facade. The interesting second provider
is Android-only and cannot be validated in the current Windows pass, so Phase
49 leaves it in legacy code and records the validation need for the 800 series.

## Nearby Runtime Helpers

| Helper | Notes | Phase 49 decision |
| --- | --- | --- |
| `Sys_DoubleTime`, `Sys_FloatTime` | Already compatibility wrappers over `Platform_DoubleTime`. | No movement needed. |
| `Sys_GetClipboardData` | Already routes through `Platform_GetClipboardText`. | Leave until a broader clipboard/input phase. |
| `Sys_DebugBreak` | Mixes debugger detection, mouse grab restoration, and signal/debug-break behavior. | Defer; behavior is interactive and platform-sensitive. |
| `Sys_WaitForQuit`, `Sys_Error` | Fatal-path and shutdown-sensitive. | Defer. |
| `Sys_CanRestart`, `Sys_NewInstance` | Restart behavior depends on Switch, Vita, iOS, executable path probing, and `execv`. | Defer. |

## Phase 49 Conclusion

The safe implementation slice is the Win32-selected username lookup. It reduces
one live platform branch in `system.c`, keeps POSIX/Vita behavior unchanged, and
adds modern tests for the fallback rule that every platform should eventually
share.
