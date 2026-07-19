# Deep Dive: Legacy Origins of `xash3dpp/launcher` — the Bootstrap Executable and the Engine-Load Handshake

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy behaviours the `xash3dpp`
**launcher** subsystem preserves — the `game_launch/game.cpp` bootstrap
executable (OS entry, argv marshalling, engine `dlopen`/`LoadLibrary` +
`Host_Main` resolution, GPU-select exports) and the `android/` JNI wrapper that
plays the same bootstrap role on Android. Line numbers are against the working
tree on that date; behaviour references, not design constraints. Everything
about the legacy is **reference-only**.*

`launcher` is not a distillation like `core`/`host` or a vendoring layer like
`abi` — it is the **thinnest** subsystem in the rewrite: a single ~95-line TU
(`src/launcher/main.cpp`) that resolves `rootdir`, scans argv into a stack
`HostArgs`, and calls `Host::Main`. Its legacy counterpart does *more* than the
rewrite's, because the legacy launcher had to bridge a **DLL boundary** the
rewrite deletes. This brief documents that legacy bootstrap so the "what the
launcher stopped doing" story is on record.

Primary legacy sources:

- `game_launch/game.cpp` (~230 lines) — the desktop bootstrap: `WinMain` /
  `main`, `CommandLineToArgvW` marshalling (`~185`), `Sys_LoadEngine`
  (`~90`), `Sys_ChangeGame` no-op (`~155`), `Sys_Start` (`~165`),
  `Launch_Error` (`~59`), the `NvOptimusEnablement` / `AmdPowerXpressRequest\
HighPerformance` exports (`~35`)
- `game_launch/game.rc`, `game_launch/wscript` — Win32 version resource + WAF
  build (links `DL`/`USER32`/`SHELL32` on Win32, `-lm` on Linux; produces
  `xash3d`)
- `android/app/.../XashActivity.java` — `SDLActivity` subclass; native-lib load
  order `["SDL2", "xash"]`; `System.exit(0)` on `onDestroy`
- Wide survey: `legacy-survey/launcher-and-android.md` (the 1-page summary this
  deep dive narrows)

**Global assumptions (legacy):** the launcher is a *separate* executable from
the engine; the engine ships as a dynamic library (`xash.dll` / `libxash.so`)
loaded at runtime; the launcher owns the OS entry point, marshals argv to a
`char**`, resolves `Host_Main` / `Host_Shutdown` by name, and forwards a default
gamedir string plus a `pfnChangeGame` callback whose *presence* is a feature
flag. All of it runs on the single process thread.

______________________________________________________________________

## 1. The desktop bootstrap — `game_launch/game.cpp`

### 1.1 OS entry and argv marshalling

Two entry points, one on each platform family:

- **POSIX** (`main(int argc, char **argv)`, `~178`): stores `argc`/`argv`
  verbatim, calls `Sys_Start`. argv is already `char**` — no conversion.
- **Win32** (`WinMain(HINSTANCE, HINSTANCE, LPSTR, int)`, `~190`): the command
  line arrives as UTF-16. The launcher calls `CommandLineToArgvW(GetCommandLine\
W(), &szArgc)`, then `malloc`s a `char**` and `wcstombs`-converts each argument
  into a fresh `malloc`ed narrow buffer (a NUL-terminator slot is appended).
  After `Sys_Start` returns it frees each buffer and the array. This is the
  legacy's **argv unicode conversion** step.

### 1.2 Engine load — `Sys_LoadEngine`

`Sys_LoadEngine` (`~90`) is the DLL handshake the rewrite deletes:

```c
// Win32
HMODULE hSDL = LoadLibraryExW( L"SDL2.dll", NULL, LOAD_LIBRARY_AS_DATAFILE ); // availability probe
if( !hSDL ) Launch_Error("Unable to load SDL2.dll: …");
FreeLibrary( hSDL );                                   // probe only — unloaded immediately

hEngine       = LoadLibraryW( L"xash.dll" );           // the real engine
Host_Main     = GetProcAddress( hEngine, "Host_Main" );      // required
Host_Shutdown = GetProcAddress( hEngine, "Host_Shutdown" );  // optional

// POSIX
hEngine       = dlopen( "libxash." OS_LIB_EXT, RTLD_NOW );
Host_Main     = dlsym( hEngine, "Host_Main" );
Host_Shutdown = dlsym( hEngine, "Host_Shutdown" );
```

Three legacy facts fall out, and the rewrite **removes** all three:

1. **A DLL boundary exists.** The engine is a separately-loaded module; the
   launcher couples to it by the exact symbol names `Host_Main` /
   `Host_Shutdown` (and the hardcoded library names `xash.dll` / `libxash.so`).
2. **Win32 SDL2 availability probe.** The launcher `LoadLibraryEx`es `SDL2.dll`
   as a data file purely to fail early with a friendly message if SDL2 is
   missing, then unloads it. This is a desktop-Win32 launcher concern.
3. **`Launch_Error`** (`~59`): a `MessageBoxA` (Win32) / `stderr` (POSIX)
   fatal-error reporter with a 16 KB static buffer, used only for load failures
   — the launcher's own tiny error path, distinct from `Host_Error`.

### 1.3 The `Host_Main` call and `Sys_ChangeGame`

`Sys_Start` (`~165`) is the whole run:

```c
Sys_LoadEngine();
ret = Host_Main( szArgc, szArgv, XASH_GAMEDIR /* "valve" */, 0 /* bChangeGame */,
                 XASH_DISABLE_MENU_CHANGEGAME ? NULL : Sys_ChangeGame );
Sys_UnloadEngine();      // Host_Shutdown() if resolved, then FreeLibrary
return ret;
```

- **`XASH_GAMEDIR` default `"valve"`** is passed as the base gamedir string.
- **`pfnChangeGame` presence is the flag.** `Sys_ChangeGame` (`~155`) is a
  **no-op** whose only purpose is to *exist*: passing a non-null pointer tells
  the engine it is allowed to enable the `game` console command (the engine
  restarts via `Sys_NewInstance`, never by calling this back). Passing `NULL`
  disables change-game. This is the legacy launcher⇄engine "change-game
  permission" ABI.
- **Sailfish env** (`~168`, under `Sys_Start` on that variant): `setenv(
  "XASH3D_BASEDIR", "$HOME/xash", …)` and `XASH3D_RODIR` before load — the
  library-path / content-root setup a couple of POSIX variants need.

### 1.4 GPU-select exports

At file scope (`game.cpp:35`), inside `extern "C"`:

```c
__declspec(dllexport) DWORD NvOptimusEnablement                     = 0x00000001;
__declspec(dllexport) int   AmdPowerXpressRequestHighPerformance    = 1;
```

These **must** be exported symbols of the *executable* (not the engine DLL) for
the GPU drivers to see them and select the discrete GPU on hybrid-graphics
laptops. They are the one piece of the legacy launcher the rewrite must **keep**
in the `.exe` (frozen symbol *names*; see As-built mapping — not yet ported).

______________________________________________________________________

## 2. The Android bootstrap — `android/`

On Android the "launcher" role is played by an SDL-based Java/Kotlin wrapper,
not `game_launch`. The relevant bootstrap slice (survey detail in
`launcher-and-android.md`):

- **`XashActivity.java`** extends `SDLActivity`; `getLibraries()` returns
  `["SDL2", "xash"]` — the JNI load order that mirrors the desktop launcher's
  "SDL2 first, then engine" sequence. Landscape-locked. `onDestroy` calls
  `System.exit(0)` as a workaround for incomplete native-global teardown.
- **`Game.kt`** carries the gamedir string (`"valve"` default) and applies
  per-mod hack configs — the mobile analog of the launcher's gamedir default.
- **`GameLibDownloader.kt`** maps Android ABI names (`arm64-v8a` → `arm64`, …)
  and locates/extracts the predownloaded engine `.so` — the mobile analog of
  the desktop `LoadLibrary("xash.dll")` step.

The Android bootstrap is **out of scope for the desktop `xash3dpp` launcher
executable**; it is documented here because it is the platform's peer bootstrap
and any future Android target of the rewrite will re-implement the same
"load SDL2, load engine, pass gamedir, run" contract without a `Host_Main` DLL
export (the engine is statically linkable, but the JNI/SDL activity boundary
persists).

______________________________________________________________________

## 3. What the rewrite keeps vs replaces

| Legacy responsibility | Rewrite |
|---|---|
| Separate `.exe` + `LoadLibrary`/`dlopen` engine, resolve `Host_Main`/`Host_Shutdown` by name | **Removed** — `host` is **statically linked** into the launcher executable; no DLL boundary, no `GetProcAddress` |
| `Host_Main(argc, argv, gamedir, bChangeGame, pfnChangeGame)` C entry | **Replaced** — `Host host; host.Main(HostArgs&)` (typed struct in, exit code out) |
| `Sys_ChangeGame` no-op whose presence is the change-game flag | **Removed** — replaced by a `HostArgs::changegame_enabled`-style field (host-boundary.md) |
| Win32 `SDL2.dll` availability probe | **Moved** to `platform` (or a build-time DLL search path) — not a launcher concern |
| Sailfish `XASH3D_BASEDIR`/`RODIR` `setenv` | **Moved** to launcher/`platform` envvar resolution |
| GPU-select exports (`NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance`) | **Kept** — must remain `dllexport` symbols of the `.exe` (**not yet defined** in `main.cpp` — port gap, see mapping) |
| argv unicode conversion (`CommandLineToArgvW` + `wcstombs` + `malloc`) | **Simplified** — Win32 entry reads the CRT-provided `__argc`/`__argv` directly; no manual UTF-16→narrow marshalling in the launcher |
| `Launch_Error` (`MessageBoxA`/`stderr` + `exit`) | **Subsumed** — load failures no longer exist (static link); real fatals go through `core::log`/`Host` |

______________________________________________________________________

## 4. As-built mapping (legacy → `xash3dpp/launcher`)

| Legacy construct | Where it lives now | Notes |
|------------------|--------------------|-------|
| `WinMain` / `main` entry | `src/launcher/main.cpp` (`WinMain` under `_WIN32`, else `main`) | Win32 target built `WIN32_EXECUTABLE`; entry reads `__argc`/`__argv` |
| `CommandLineToArgvW` + `wcstombs` argv marshal | **dropped** — CRT `__argc`/`__argv` | No manual UTF-16 conversion; the CRT supplies argv |
| `Sys_LoadEngine` (`LoadLibrary`/`dlopen` + `GetProcAddress`) | **dropped** — static link to `xash3dpp_host` | No DLL boundary in the rewrite (host-boundary.md § External ABI) |
| `Host_Main(argc, argv, gamedir, bChangeGame, func)` | `xash::Host::Main(HostArgs&)` | Typed struct; `Host` constructed on the stack, exit code returned |
| `XASH_GAMEDIR` default `"valve"` | `get_arg(argc, argv, "-game", "valve")` → `HostArgs::gamedir`; `basedir` defaults to it | Same default, now an argv default |
| `-basedir` / `-rodir` / `-dedicated` / `-dev` handling | `get_arg`/`has_flag` in `main.cpp` → `HostArgs` fields | Allocation-free argv scan; `-dev` via `std::atoi` (see launcher-modernization L-1) |
| `rootdir` (was implicit in engine) | `platform::get_executable_dir()` → `HostArgs::rootdir` | The one platform call |
| `Sys_ChangeGame` no-op / change-game flag | `HostArgs` change-game field (host) | Presence-as-flag hack removed |
| Win32 `SDL2.dll` availability probe | `platform` / build-time search path | Not in the launcher (host-boundary.md) |
| Sailfish `XASH3D_BASEDIR`/`RODIR` `setenv` | launcher / `platform` envvar resolution | Not in `main.cpp` as-built |
| `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` exports | launcher `.exe` (**not yet defined**) | Must stay `dllexport` symbols of the executable — port gap (boundary As-built reconciliation, launcher-modernization C-1) |
| `Launch_Error` (`MessageBoxA`/`stderr` + `exit`) | **dropped** — no load failures; fatals via `core::log`/`Host` | The static 16 KB buffer is gone |
| (none — legacy had no thread-role notion) | `core::register_thread_role(ThreadRole::Main)` — first statement | **New in rewrite** — establishes Main before any engine call (see §5) |
| Android `XashActivity`/`getLibraries` `["SDL2","xash"]` | out of scope (future Android target) | JNI/SDL bootstrap peer; same "load SDL2 → engine → run" contract |

______________________________________________________________________

## 5. New-in-rewrite (no legacy analog)

- **`register_thread_role(ThreadRole::Main)` as the first statement.** The
  legacy launcher had no thread-role concept; the rewrite makes the launcher the
  **provider** of `ThreadRole::Main` so every downstream `assert_thread_role\
(Main)` (host, engine_context, map_loader, server) is meaningful. This is the
  file's single hard invariant (boundary spec § Threading / Quirks).
- **Typed `HostArgs` handoff.** The legacy passed five positional C arguments to
  `Host_Main`; the rewrite fills one stack `HostArgs` value and hands it by
  reference — the narrowest-state, no-global (P-3/P-5) shape, and the natural
  home for a future load-time flavor (G-2) or service-mode (G-1) switch
  (boundary § Extension axes).
- **No DLL boundary at all.** The entire `LoadLibrary`/`GetProcAddress`/
  `FreeLibrary` lifecycle — the legacy launcher's whole reason to exist beyond
  argv — dissolves into a static link. The launcher shrinks to argv + rootdir +
  role + run.
