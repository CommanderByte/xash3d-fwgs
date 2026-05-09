# System Platform Facade Audit

## Purpose

Map what `engine/common/system.c` still owns before moving more code toward
`engine/platform/` or modern `src/engine/platform/` helpers. The intent is to
keep the public `Sys_*` facade stable while shrinking platform conditionals in
common engine code.

## Current Facade Shape

`engine/common/system.h` exposes the stable C surface used by engine, client,
server, and module code:

- time wrappers: `Sys_DoubleTime`, `Sys_FloatTime`;
- diagnostics and shutdown: `Sys_DebugBreak`, `Sys_Warn`, `Sys_Error`,
  `Sys_Quit`;
- command-line helpers: `Sys_ParseCommandLine`, `Sys_CheckParm`,
  `Sys_GetParmFromCmdLine`, `Sys_GetIntFromCmdLine`;
- dynamic library wrappers: `Sys_LoadLibrary`, `Sys_FreeLibrary`;
- output fanout: `Sys_Print`;
- restart/native object helpers: `Sys_CanRestart`, `Sys_NewInstance`,
  `Sys_GetNativeObject`;
- logging/console functions implemented in `sys_con.c`.

The Waf target already selects many platform files from `engine/platform/*`.
`system.c` is therefore a compatibility facade plus several older platform
branches that have not yet moved.

## Existing Platform Homes

| Area | Current home | Notes |
| --- | --- | --- |
| Time/sleep | `Platform_DoubleTime`, `Platform_Sleep` in Win32, POSIX, SDL, DOS platform files | `Sys_DoubleTime` and `Sys_FloatTime` are binary-compatibility wrappers. |
| Shell execute/message box | `Platform_ShellExecute`, `Platform_MessageBox` in platform files | `Sys_Warn` and `Sys_Error` still decide when to show boxes. |
| Platform init/shutdown | `engine/platform/platform.h` inline selection plus platform files | Waf selects source folders and the header selects initialization order. |
| Console input/output | Win32 and POSIX platform console files plus `sys_con.c` | Phase 43 isolated the system/background console path. |
| SDL input/window/audio | `engine/platform/sdl*` | Mostly already platform-owned. |
| Crash handlers | `engine/platform/win32`, `engine/platform/posix` | Already separate, but still called through `Sys_*` setup declarations. |
| Dynamic libraries | `engine/platform/win32`, `engine/platform/posix`, Android helpers | `Sys_LoadLibrary` remains a common validation wrapper around `COM_LoadLibrary`. |

## Platform Branches Still In `system.c`

| Function | Branches | Migration risk |
| --- | --- | --- |
| `Sys_DebugBreak` | Win32 `__debugbreak`, POSIX signal raise, mouse-grab release/restore | Medium. Debugger behavior and mouse capture restoration must stay exact. |
| `Sys_GetCurrentUser` | Win32 username, Vita username, POSIX password DB, fallback `Player` | Medium. Good candidate for platform implementation files after tests or manual platform validation. |
| `Sys_WaitForQuit` | Win32 message pump only | Low-to-medium. Could move behind a platform wait-for-error-ack helper. |
| `Sys_Error` | SDL window hide, Win32 console visibility/input, dedicated wait | High. Fatal path is shutdown-sensitive and should be moved only after a narrow helper is proven. |
| `Sys_Print` | Win32 console normalization before `Wcon_WinPrint` | Medium. Good later candidate for a tested formatting/normalization helper, but it affects live output. |
| `Sys_CanRestart` | Switch/Vita true, iOS false, executable path probe elsewhere | Medium. Needs platform validation because restart availability differs by target. |
| `Sys_NewInstance` | Switch restart, Vita exec path, POSIX `execv`, process argument allocation | High. Restart path explicitly avoids engine allocation and runs during shutdown. |
| `Sys_GetNativeObject` | filesystem native object, Android fallback | Low. Could become a platform native-object query after Android validation. |

## Target-Neutral Candidate

`Sys_ParseCommandLine` has a small target-neutral rule used during change-game
re-entry: replace unsafe arguments with the literal `censored` so the engine
does not re-apply `-game`, `+game`, `+map`, `+load`, or `+changelevel` while
the new game is initializing.

This is the safest Phase 44 implementation candidate because:

- it has no OS APIs;
- it is called through one stable `Sys_*` facade;
- it can be unit-tested without a host runtime;
- preserving the exact blocked argument list is straightforward.

## Suggested Migration Order

1. Move the change-game command-line censor rule behind a modern helper and C
   adapter.
2. Add tests for blocked arguments, case-insensitivity, null arguments, and the
   disabled-change-game path.
3. Keep `Sys_ParseCommandLine` as the public C facade that mutates `host.argv`.
4. Revisit `Sys_GetCurrentUser` next, but only after deciding how to validate
   Win32, POSIX, and Vita username behavior.
5. Leave `Sys_Error`, `Sys_NewInstance`, and restart behavior for later because
   they are shutdown/fatal-path sensitive.
