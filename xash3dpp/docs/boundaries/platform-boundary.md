# Platform Boundary Spec

## Responsibility

Provides thin OS-abstraction wrappers for the handful of system capabilities
that every other xash3dpp subsystem needs: monotonic time, thread sleep,
dynamic library loading and symbol resolution, system path queries, error
dialogs, and shell execution. Platform is the lowest non-trivial subsystem —
only `xash3dpp_utilities` sits below it in the dependency graph.

## External ABI contracts

None — fully internal. No Game DLL or client DLL header exposes
`xash::platform` symbols directly. The engine host will eventually fill legacy
`enginefuncs_t` function pointers (e.g., `pfnLoadLibrary`) from this module via
a thin shim.

## Interface (what the rest of the engine calls)

| Symbol | Kind | Description |
|--------|------|-------------|
| `get_time()` | function | Monotonic elapsed seconds (double); epoch on first call |
| `sleep(ms)` | function | Yield current thread for ≥ ms milliseconds |
| `LibHandle` | struct | Opaque shared-library handle; default = null |
| `open_library(path)` | function | Load a shared library (UTF-8 path); returns `{}` on failure |
| `get_symbol(lib, name)` | function | Resolve exported symbol by name; returns nullptr on failure |
| `close_library(lib)` | function | Unload library and zero the handle; no-op if null |
| `get_executable_dir()` | function | Directory of the engine executable (forward slashes, trailing `/`) |
| `get_working_directory()` | function | Current working directory (forward slashes, trailing `/`) |
| `is_debugger_present()` | function | True if a debugger is attached; false on unsupported platforms |
| `message_box(title, msg)` | function | Modal error dialog; degrades to stderr on headless builds |
| `shell_execute(path, params)` | function | Open path with OS default handler; fire-and-forget |

## Dependencies (what this module calls)

| Subsystem | Why |
|-----------|-----|
| `xash3dpp_utilities` (PRIVATE) | `path::extract_dir`, `path::fix_slashes` for normalising paths returned by `get_executable_dir` |
| OS libraries | `kernel32` (Win32, implicit); `dl` (`-ldl`, POSIX) for dlopen/dlsym/dlclose |

No dependency on `xash3dpp_memory` — all public functions return by value
(`std::string`, `LibHandle`) or operate on primitive types. No pool-backed
long-lived state is needed.

## Owned state

| State | Location | Notes |
|-------|----------|-------|
| Clock epoch | `static const double s_epoch` in `get_time()` | Initialised once on first call via C++11 magic-static; thread-safe |
| (Win32) QPC frequency and start | `static const auto` in `get_time()` | Same magic-static pattern |

No global objects. No `Init` / `Shutdown` functions. The subsystem is ready to
use immediately without any explicit initialisation.

## Quirks and invariants

- `get_time()` returns **elapsed seconds from first call**, not wall-clock time.
  The epoch resets if the process image is replaced (irrelevant in practice).
- `open_library` on Win32 converts the UTF-8 path to UTF-16 before calling
  `LoadLibraryW`. Raw `LoadLibraryA` is never used.
- `get_executable_dir()` returns the **directory**, not the full path to the
  executable. The result always ends with a forward slash and never contains
  backslashes.
- `close_library` zeroes the `LibHandle::native` field on success. Calling it
  on an already-null handle is a no-op.
- `message_box` on POSIX writes to `stderr`. Builds that include SDL2 can
  override this behaviour at the renderer/host layer.
- `shell_execute` uses `fork` + `execvp` on POSIX (fire-and-forget); if
  `fork` fails the call is silently dropped. No guarantee of success on any
  platform.
- `is_debugger_present` reads `/proc/self/status` on Linux; returns `false`
  on macOS, BSD, and all embedded targets (TODO: implement per-platform).

## Open questions

- **Clipboard** (`Sys_GetClipboardData`) — needed by the in-game console for
  paste. Include here or own it in a future `xash3dpp_window` subsystem?
- **System console / stdin** (`Wcon_*`, `Posix_Input`) — interactive console
  I/O; might belong in a separate `xash3dpp_console` module.
- **Signal handling** (`Posix_SetupSigtermHandling`) — should SIGTERM be caught
  here and turned into a `host::request_quit()` call, or owned by the host?
- **Android extras** (`Android_GetNativeObject`, `Android_GetKeyboardHeight`,
  `Android_GetAndroidID`) — defer until the Android build target is added.
- **High-resolution sleep** (`Win32_NanoSleep`, `SDLash_NanoSleep`) — needed by
  the frame-timing loop; add to the platform API when the host subsystem is
  started.
