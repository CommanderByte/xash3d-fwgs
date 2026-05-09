# Game Launch TODO

## Purpose

Track the next modularization pilot after the filesystem milestone. The goal is
to understand the native launcher, document its current startup behavior, and
move source code into `src/launcher/` without changing executable behavior.

The launcher is a good pilot because it is small, platform-sensitive, and
boundary-heavy: it parses process arguments, checks dynamic-library
dependencies, loads the engine library, finds exports, calls `Host_Main`, and
unloads cleanly.

## Phase 30 Tasks

- [x] `LAUNCH-001` Audit current launcher responsibilities and platform
  branches.
  Evidence: `Documentation/codex/modern/game-launch/architecture.md`.
  Notes: map Windows `WinMain`, POSIX `main`, Sailfish environment setup,
  SDL2 dependency checking, engine library loading, export lookup, and shutdown.

- [x] `LAUNCH-002` Capture current launch behavior in modern documentation.
  Evidence: `Documentation/codex/modern/game-launch/architecture.md`.
  Notes: include DLL/SO load sequence, error reporting, and change-game
  callback behavior.

- [x] `LAUNCH-003` Add a minimal launcher TODO/test strategy before extraction.
  Evidence: `Documentation/codex/modern/game-launch/architecture.md`,
  `tests/launcher/README.md`.
  Notes: prefer target-neutral unit tests for path/library-name planning and
  argument conversion helpers; avoid tests that open GUI message boxes.

- [x] `LAUNCH-004` Create `src/launcher/` and `src/include/launcher/` only when
  a helper is ready to move.
  Evidence: `src/include/launcher/launch_settings.hpp`,
  `src/launcher/launch_settings.cpp`.
  Notes: this was the conservative first step. `LAUNCH-011` later moved the
  thin entry shell into `src/launcher/platform/` once the source/resource
  layout policy was documented.

- [x] `LAUNCH-005` Extract the first target-neutral helper with tests.
  Evidence: `src/include/launcher/launch_settings.hpp`,
  `src/launcher/launch_settings.cpp`, `tests/launcher/launch_settings.cpp`;
  command `.\waf.bat build --targets=test_launcher_launch_settings,xash3d`
  passed on 2026-05-09.
  Notes: the first helper owns engine library naming, export names, SDL2
  dependency labels, build-default game directory fallback, and menu
  change-game flag calculation without OS calls.

- [x] `LAUNCH-006` Rebuild and smoke test the launcher on Windows defaults.
  Evidence: command
  `.\waf.bat build --targets=test_launcher_launch_settings,xash3d` passed;
  full `.\waf.bat build` passed 41/41 tests;
  copied `build\game_launch\xash3d.exe` to `run-win32`; command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0 and logged
  `FS_LoadProgs`, `FS_InitStdio`, `Time to first frame: 0.440 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"` on 2026-05-09.
  Notes: use the existing `run-win32` recipe and verify engine load, first
  frame, and clean shutdown.

- [x] `LAUNCH-007` Move engine DLL/SO loading and export lookup behind a
  launcher helper while keeping entry points in `game_launch/`.
  Evidence: `src/include/launcher/engine_library.hpp`,
  `src/launcher/engine_library.cpp`, `tests/launcher/engine_library.cpp`;
  command
  `.\waf.bat build --targets=test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0
  from `run-win32` and logged `Time to first frame: 0.406 seconds` on
  2026-05-09.
  Notes: keep user-facing launch errors in `game.cpp`; the helper reports
  formatted failures without opening message boxes or exiting.

- [x] `LAUNCH-008` Move shared launch sequencing and Windows argv ownership
  into launcher helpers.
  Evidence: `src/include/launcher/application.hpp`,
  `src/launcher/application.cpp`, `src/include/launcher/win32_argv.hpp`,
  `src/launcher/win32_argv.cpp`, `tests/launcher/application.cpp`;
  command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0
  from `run-win32` and logged `Time to first frame: 0.414 seconds` on
  2026-05-09.
  Notes: this first kept entry points in the launcher target folder while
  extracting reusable behavior. `LAUNCH-011` later moved the thin entry shell
  into `src/launcher/platform/`.

- [x] `LAUNCH-009` Define launcher source/resource layout policy before moving
  platform assets.
  Evidence: `Documentation/codex/modern/game-launch/layout-policy.md`.
  Notes: reusable code belongs under `src/launcher`; executable entry shells
  and platform resources stay under the launcher target area.

- [x] `LAUNCH-010` Move Windows launcher resources into a platform resource
  subfolder and update `game_launch/wscript`.
  Evidence: `resources/launcher/windows/game.rc`,
  `resources/launcher/windows/icon-xash-material.ico`,
  `resources/launcher/source/icon-xash-material.png`,
  `game_launch/wscript`; command `.\waf.bat build --targets=xash3d`
  passed; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0
  from `run-win32` on 2026-05-09.
  Notes: launcher resources live under top-level `resources/launcher/` because
  they are product/build assets, not launcher implementation code.

- [x] `LAUNCH-011` Move the thin executable entry source into the `src`
  launcher tree.
  Evidence: `src/launcher/platform/entry.cpp`,
  `src/launcher/platform/README.md`, `game_launch/wscript`; command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; direct `build\src\test_launcher_application.exe`,
  `build\src\test_launcher_engine_library.exe`, and
  `build\src\test_launcher_launch_settings.exe` passed; copied
  `build\game_launch\xash3d.exe` to `run-win32`; command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`
  exited 0 and logged `Time to first frame: 0.430 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"`; command
  `.\waf.bat build --alltests` passed 45/45 tests on 2026-05-09.
  Notes: `game_launch/` now remains only the executable target wrapper; source
  code lives under `src/launcher/`.

- [x] `LAUNCH-012` Document platform-support expectations for the modular
  launcher layout.
  Evidence: `Documentation/codex/modern/game-launch/platform-support.md`.
  Notes: Windows was build/smoke tested locally; POSIX, Linux-specific linker
  flags, and Sailfish environment defaults remain preserved but untested in
  this Windows pass.

## Boundaries

- Preserve `WinMain` and POSIX `main` signatures.
- Preserve exported GPU-selection globals on Windows.
- Do not throw C++ exceptions across platform entry points.
- Keep GUI error presentation behavior stable until an explicit UX/error
  policy phase changes it.
- Do not move `Host_Main` or `Host_Shutdown` ownership out of the engine.
