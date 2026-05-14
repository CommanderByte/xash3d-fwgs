# game_launch/ + android/

## game_launch/

- **Purpose**: thin executable bootstrap that dynamically loads the engine library (`xash.dll` or `libxash.so`) and invokes `Host_Main`, bridging OS entry points to the engine.
- **game.cpp**: platform-specific entry (WinMain on Windows, main on POSIX); uses LoadLibraryW/dlopen to load the engine, resolves `Host_Main` and `Host_Shutdown`, passes argv and gamedir.
- **game.rc**: Windows resource metadata (version info, company name, icon) compiled into the .exe.
- **wscript**: WAF build config; platform compiler flags, links DL/USER32/SHELL32 on Windows, `-lm` on Linux, produces `xash3d` installed to BINDIR.

### Platform handling

- **Windows**: WinMain entry, pre-loads SDL2.dll (errors if missing), then `xash.dll`; includes GPU enablement exports (`NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance`) for hybrid graphics
- **POSIX**: main entry, loads `libxash.so` via dlopen, no SDL2 preload; Sailfish variant sets `XASH3D_BASEDIR` and `XASH3D_RODIR`

## android/

- **Purpose**: Material Design wrapper app that manages game installation, metadata UI, lifecycle, and invokes the native engine .so via JNI/SDL2.
- **Structure**:
  - **MainActivity.kt** — Androidx navigation host with toolbar; crash history and game browser UI
  - **XashActivity.java** — extends `SDLActivity`; landscape-only; loads `["SDL2", "xash"]` native libraries via JNI; handles lifecycle cleanup (calls `System.exit` on destroy)
  - **Game.kt** — model for game instance (basedir, gameInfoFile, icon/cover bitmaps, title); applies mobile-specific hack configs (aom, bdlands, hl_urbicide, …); manages game directory string ("valve" default)
  - **GameLibDownloader.kt** — maps ABI names (arm64-v8a → arm64, armeabi-v7a → armv7l, …), checks for predownloaded .so libs, handles zipfile extraction
- **Native bridge**: JNI entry via NDK in externalNativeBuild, configured through python wrapper around wscript for architecture filtering
- **Resources**:
  - **AndroidManifest.xml** — permissions (MANAGE_EXTERNAL_STORAGE, RECORD_AUDIO, INTERNET, FOREGROUND_SERVICE), optional hardware features (gamepad, touchscreen, microphone, bluetooth), `installLocation="preferExternal"`, API 21 minimum
  - **gradle** — NDK 29.0, compileSdk 35, ABIs: armeabi-v7a, arm64-v8a, x86, x86_64; versionCode generated from git hash

## Dependencies

- **game_launch/** requires filesystem/, the engine binary, Windows SDK (`shellapi.h`) and POSIX `dlfcn.h`
- **android/** requires NDK-built `libxash.so`, SDL2 prebuilt or side-loaded, Gradle with Kotlin plugin

## Coupling and Risks

- **Hardcoded library names** — `xash.dll` on Windows, platform-prefixed `libxash.so` on POSIX; renaming breaks both launchers
- **SDL2 pre-check on Windows** — `game_launch` forcibly loads SDL2.dll before `xash.dll`; failures are fatal and hard to debug if SDL2 versioning changes
- **JNI boundary in Android** — `getLibraries()` and ndk-build tie app lifecycle to SDL2's event loop; `System.exit(0)` in `onDestroy()` is a workaround for incomplete native shutdown
- **Platform-specific entry shims** — WinMain cmdline parsing and POSIX argv must stay in sync with engine expectations
- **Mobile hacks in Game.kt** — hardcoded game name list for special-case behavior creates maintenance debt

## Modernization Opportunities

- **Unify launcher logic** — extract platform abstraction for library loading (dlopen/LoadLibrary) into a shared module usable by `xash3dpp/`
- **SDL2 decoupling** — move SDL2 init into the engine itself; launcher becomes SDL2-agnostic
- **Game config model** — migrate `Game.kt` to a shared platform-agnostic format (`gameinfo.txt` parsing) consumed by desktop and mobile
- **Cleaner JNI lifecycle** — restructure `XashActivity` to avoid `System.exit()`; ensure engine cleans up globals on shutdown
- **Asset streaming** — extract `GameLibDownloader` logic into a library shared by app and CI
