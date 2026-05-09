# Launcher Platform Support

## Purpose

Track the platform seams that the modular launcher must preserve while moving
implementation into `src/launcher/`.

## Support Matrix

| Platform | Entry Source | Current Status | Notes |
| --- | --- | --- | --- |
| Windows | `src/launcher/platform/entry.cpp`, `src/launcher/platform/library_win32.cpp`, `src/launcher/platform/win32_argv.cpp` | Build and smoke tested locally. | Uses `WinMain`, `CommandLineToArgvW`, `LoadLibraryW`, `GetProcAddress`, `MessageBoxA`, and Windows GPU preference exports. |
| POSIX | `src/launcher/platform/entry.cpp`, `src/launcher/platform/library_posix.cpp` | Compile path preserved, not locally tested in this Windows pass. | Uses `main`, `dlopen`, `dlsym`, `dlclose`, and `stderr` fatal errors. |
| Linux | `src/launcher/platform/entry.cpp` plus `src/wscript` flags | Not locally tested in this Windows pass. | Keeps the existing `-Wl,--no-as-needed -lm` workaround for Half-Life 25th anniversary server libraries. |
| Sailfish | `src/launcher/platform/environment_sailfish.cpp` | Not locally tested in this Windows pass. | Keeps existing launcher-provided `XASH3D_BASEDIR` and `XASH3D_RODIR` defaults behind Waf source selection. |
| Windows resources | `resources/launcher/windows/` | Build tested locally. | `src/wscript` wires `game.rc`; source PNG remains in `resources/launcher/source/`. |

## Rules

- Platform entry signatures stay in `src/launcher/platform/`.
- Target-neutral launch behavior stays in `src/launcher/`.
- Waf should select whole platform implementation files where possible instead
  of compiling mixed-platform source files full of conditionals.
- Public executable wiring stays in `src/wscript` with the modern launcher
  library and platform source selection.
- Platform assets stay in `resources/launcher/<platform>/`.
- Untested platform branches must remain small and explicit, with behavior
  covered by helper unit tests where possible.
- Optional runtime config uses `launcher.json`, but compiled defaults remain
  authoritative when the file is missing, invalid, or unavailable.

## Next Audit Points

- Confirm POSIX launcher compilation on Linux after the Windows phase lands.
- Confirm Sailfish defaults still match packaging expectations.
- Decide whether future Android/iOS launchers use this executable wrapper or a
  platform-native app entry.
- Add a fake loader boundary only if dynamic library failure modes become
  complicated enough to require direct unit tests.
