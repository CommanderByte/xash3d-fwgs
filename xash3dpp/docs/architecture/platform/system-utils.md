# System utilities

> **Defined in**: `include/xash3dpp/platform/platform.hpp`\
> **Source**: `src/platform/win32/sys.cpp`, `src/platform/posix/sys.cpp`\
> **Namespace**: `xash::platform`

## Overview

`platform.hpp` groups the process-level OS services that cannot be attributed to a
more specific feature area (I/O, logging, crash): monotonic time, thread sleep,
shared-library loading and symbol resolution, executable and working-directory
queries, debugger detection, a modal error dialog, and an OS shell-open call.

The API is a collection of free functions — there is no class to construct and no
`init()` / `shutdown()` to call. Functions are usable immediately, even from global
constructors.

## Time

### `get_time() → double`

Returns elapsed seconds since the first call to `get_time()` in this process.

| Platform | Clock |
|----------|-------|
| Win32 | `QueryPerformanceCounter` / `QueryPerformanceFrequency` |
| POSIX | `clock_gettime(CLOCK_MONOTONIC, …)` |

The epoch is set by a magic-static initialiser on the first call, making the
initialisation thread-safe (C++11 guarantee). Subsequent calls from any thread
are safe.

**Side effect of the first call**: `get_time()` also invokes
`detail::capture_main_thread()`, recording the calling thread's ID as the main
thread. All platform functions that require single-thread access check against
this captured ID in debug builds. Callers that need proper enforcement of
main-thread preconditions must therefore ensure `get_time()` is called from the
main thread first — in practice this happens at engine startup before any worker
threads are spawned.

### `sleep(ms: unsigned) → void`

Yields the current thread for at least `ms` milliseconds.

- `sleep(0)` is a legal hint to the OS scheduler; it may return immediately.
- Win32: `Sleep(ms)`.
- POSIX: `nanosleep(&ts, nullptr)` with `EINTR` retry.

## Dynamic library loading

### `LibHandle`

An opaque `void* native` wrapped in a struct. Default-constructed value
(`native == nullptr`) represents "no library loaded". `operator bool` checks
`native != nullptr`.

### `open_library(path: string_view) → LibHandle`

Loads the shared library at the UTF-8 `path`. Returns `{}` on any failure.

| Platform | Call |
|----------|------|
| Win32 | Converts `path` to UTF-16, then calls `LoadLibraryW` |
| POSIX | `dlopen(path, RTLD_NOW \| RTLD_LOCAL)` |

Passing an empty `path` returns `{}` on both platforms. This intentionally blocks
the `dlopen(NULL, …)` behaviour that would otherwise return a handle to the main
executable.

### `get_symbol(lib: LibHandle, name: const char*) → void*`

Resolves an exported symbol by name. Returns `nullptr` on failure or on a null
handle.

| Platform | Call |
|----------|------|
| Win32 | `GetProcAddress(HMODULE, name)` |
| POSIX | `dlsym(handle, name)` |

### `close_library(lib: LibHandle&) → void`

Unloads the library and zeroes `lib.native`. Calling with a null handle is a
no-op.

| Platform | Call |
|----------|------|
| Win32 | `FreeLibrary(HMODULE)` |
| POSIX | `dlclose(handle)` |

`lib.native` is set to `nullptr` even if the OS unload call fails. A failed close
is treated as a usage error; the caller must not use the handle afterwards.

## System path queries

### `get_executable_dir() → std::string`

Returns the directory containing the engine executable, with forward slashes and
a trailing `/`. The returned `std::string` is caller-owned.

| Platform | Source |
|----------|--------|
| Win32 | `GetModuleFileNameW(nullptr, …)` → UTF-8 via `WideCharToMultiByte` |
| Linux | `readlink("/proc/self/exe", …)` |
| macOS | `_NSGetExecutablePath` |
| FreeBSD / NetBSD / OpenBSD | `sysctl CTL_KERN` |

After obtaining the raw path, both implementations call
`utilities::extract_dir` + `utilities::fix_slashes` to normalise the result.

### `get_working_directory() → std::string`

Returns the current working directory (forward slashes, trailing `/`).

| Platform | Call |
|----------|------|
| Win32 | `GetCurrentDirectoryW` → UTF-8 |
| POSIX | `getcwd` |

## Diagnostics and UI

### `is_debugger_present() → bool`

| Platform | Implementation |
|----------|---------------|
| Win32 | `IsDebuggerPresent()` |
| Linux | Reads `TracerPid` from `/proc/self/status`; returns `true` if > 0 |
| macOS / BSD / Android | Returns `false` (not yet implemented) |

### `message_box(title, message) → void`

Displays a modal error dialog and blocks until dismissed.

| Platform | Behaviour |
|----------|-----------|
| Win32 | `MessageBoxW` (UTF-8 → UTF-16) |
| POSIX without display | Writes to `stderr` |

Falls back to stderr on headless or embedded targets — never blocks indefinitely.

### `shell_execute(path, params) → void`

Asks the OS to open `path` using its default handler (browser, file manager, etc.).
Fire-and-forget: errors are silently swallowed.

| Platform | Mechanism |
|----------|-----------|
| Win32 | `ShellExecuteW` |
| POSIX | `fork()` + `execvp("xdg-open", …)`; if `fork` fails the call is dropped |

## Threading model

| Operation | Thread safety |
|-----------|--------------|
| `get_time()` | Safe from any thread; epoch and main-thread capture happen in a C++11 magic-static (exactly once) |
| `sleep(ms)` | Safe from any thread |
| `open_library` / `get_symbol` / `close_library` | No internal lock; the caller must serialise access to a given `LibHandle` |
| `get_executable_dir` / `get_working_directory` | Safe from any thread (heap allocates a `std::string` per call) |
| `is_debugger_present` | Safe from any thread |
| `message_box` / `shell_execute` | Main-thread only (Win32 UI calls; POSIX `fork`) |

## Error handling

All functions that can fail return `{}` (null `LibHandle`), an empty `std::string`,
or `false` — no exceptions, no OOM handler.

## Edge cases and invariants

- `get_time()` returns `0.0` on the very first call (the epoch equals the first
  sample).
- `open_library("")` returns `{}` on all platforms, blocking the
  `dlopen(NULL, …)` main-program-open behaviour.
- `close_library` zeroes `lib.native` regardless of whether the OS call succeeded.
- Paths from `get_executable_dir()` and `get_working_directory()` always end with
  `/` and never contain backslashes; both call `utilities::fix_slashes` internally.

## See also

- [assertions.md](./assertions.md) — `assert_main_thread` and `capture_main_thread`, triggered by `get_time()`
- [os-io.md](./os-io.md) — file-level OS I/O in the same `xash::platform` namespace
