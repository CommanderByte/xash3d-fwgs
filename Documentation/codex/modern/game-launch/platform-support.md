# Launcher Platform Support

## Purpose

Track the platform seams that the modular launcher must preserve while moving
implementation into `src/launcher/`.

## Support Matrix

| Platform | Entry Source | Current Status | Notes |
| --- | --- | --- | --- |
| Windows | `src/launcher/platform/entry.cpp` | Build and smoke tested locally. | Uses `WinMain`, `CommandLineToArgvW`, `LoadLibraryW`, `GetProcAddress`, `MessageBoxA`, and Windows GPU preference exports. |
| POSIX | `src/launcher/platform/entry.cpp` | Compile path preserved, not locally tested in this Windows pass. | Uses `main`, `dlopen`, `dlsym`, `dlclose`, and `stderr` fatal errors. |
| Linux | `src/launcher/platform/entry.cpp` plus `game_launch/wscript` flags | Not locally tested in this Windows pass. | Keeps the existing `-Wl,--no-as-needed -lm` workaround for Half-Life 25th anniversary server libraries. |
| Sailfish | `src/launcher/application.cpp` | Not locally tested in this Windows pass. | Keeps existing launcher-provided `XASH3D_BASEDIR` and `XASH3D_RODIR` defaults behind `XASH_SAILFISH`. |
| Windows resources | `resources/launcher/windows/` | Build tested locally. | `game_launch/wscript` wires `game.rc`; source PNG remains in `resources/launcher/source/`. |

## Rules

- Platform entry signatures stay in `src/launcher/platform/`.
- Target-neutral launch behavior stays in `src/launcher/`.
- Public executable wiring stays in `game_launch/wscript` until the build
  system itself is reorganized.
- Platform assets stay in `resources/launcher/<platform>/`.
- Untested platform branches must remain small and explicit, with behavior
  covered by helper unit tests where possible.

## Next Audit Points

- Confirm POSIX launcher compilation on Linux after the Windows phase lands.
- Confirm Sailfish defaults still match packaging expectations.
- Decide whether future Android/iOS launchers use this executable wrapper or a
  platform-native app entry.
- Add a fake loader boundary only if dynamic library failure modes become
  complicated enough to require direct unit tests.
