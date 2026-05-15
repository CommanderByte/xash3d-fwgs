# platform — Index

> **Note**: `log.hpp`, `assert.hpp`, `thread_role.hpp`, and `private/.../assert_main.hpp` have moved to `xash3dpp_core` (`xash::core` namespace). They are documented here for cross-reference until the doc tree is reshuffled. The `xash3dpp_platform` library now contains only OS abstraction (console, crash, os_io, os_fd, sys, platform.hpp).

## Public API headers

| Header | Library | Namespace | Key symbols |
|--------|---------|-----------|-------------|
| `platform/platform.hpp` | `xash3dpp_platform` | `xash::platform` | `get_time`, `sleep`, `LibHandle`, `open_library`, `get_symbol`, `close_library`, `get_executable_dir`, `get_working_directory`, `is_debugger_present`, `message_box`, `shell_execute` |
| `platform/os_fd.hpp` | `xash3dpp_platform` | `xash::platform` | `OsFd` |
| `platform/os_io.hpp` | `xash3dpp_platform` | `xash::platform` | `OpenMode`, `open_file`, `open_memfd`, `read`, `write`, `seek`, `tell`, `flush`, `close_fd`, `file_size`, `file_time`, `list_directory`, `is_case_insensitive`, `make_directory`, `rename_file`, `delete_file` — plus Android bridge on `XASH_ANDROID` builds |
| `core/log.hpp` | `xash3dpp_core` | `xash::core` | `LogLevel`, `LogCallback`, `log`, `logf`, `log_va`, `log_set_callback`, `log_verbose`, `log_info`, `log_warning`, `log_error`, `log_fatal` |
| `core/assert.hpp` | `xash3dpp_core` | macros | `XASH_ASSERT`, `XASH_FATAL`, `XASH_DEBUG_BREAK` |
| `core/thread_role.hpp` | `xash3dpp_core` | `xash::core` | `ThreadRole`, `register_thread_role`, `current_thread_role`, `assert_thread_role`, `thread_role_name` |
| `platform/console.hpp` | `xash3dpp_platform` | `xash::platform::console` | `write`, `read_line` |
| `platform/crash.hpp` | `xash3dpp_platform` | `xash::platform::crash` | `install_handler`, `print_trace` |

## Private / internal headers

| Header | Library | Purpose |
|--------|---------|---------|
| `private/core/assert_main.hpp` | `xash3dpp_core` | `detail::capture_main_thread()`, `detail::assert_main_thread(loc)` — main-thread enforcement used by `sys.cpp` and `crash.cpp` |

## Source files

| File | Responsibility |
|------|---------------|
| `src/core/log.cpp` | `log`, `logf`, `log_va`, `log_set_callback` — shared across all platforms (library: `xash3dpp_core`) |
| `src/platform/win32/sys.cpp` | Win32: `get_time` (QPC), `sleep`, dynlib, paths, debugger, `message_box`, `shell_execute` |
| `src/platform/win32/os_io.cpp` | Win32: `OsFd::close`, `open_file`, `open_memfd`, read/write/seek/stat, directory ops |
| `src/platform/win32/console.cpp` | Win32: `console::write`, `console::read_line` via conhost |
| `src/platform/win32/crash.cpp` | Win32: SEH crash handler, `CaptureStackBackTrace` |
| `src/platform/posix/sys.cpp` | POSIX: `get_time` (`CLOCK_MONOTONIC`), `sleep`, dlopen/dlsym/dlclose, paths, debugger, `fork`+`execvp` |
| `src/platform/posix/os_io.cpp` | POSIX: `OsFd::close`, `open_file`, read/write/seek/stat, directory ops |
| `src/platform/posix/console.cpp` | POSIX: `console::write`/`read_line` via stdin/stdout with `select` |
| `src/platform/posix/crash.cpp` | POSIX: `sigaction` crash handler, `backtrace_symbols_fd` |
| `src/platform/android/os_io.cpp` | Android: POSIX I/O extended with AAsset bridge (`android_init_jni`, `open_asset`, etc.) |
| `src/platform/android/console.cpp` | Android: `console::write` → logcat; `read_line` → `{}` (no terminal) |
| `src/platform/android/crash.cpp` | Android: crash handler adapted for the NDK signal stack |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `OsFd` | class | `platform/os_fd.hpp` | RAII non-copyable movable wrapper around a native file descriptor (`int`); destructor calls `close_fd` |
| `OpenMode` | enum class | `platform/os_io.hpp` | Bit-flag set for `open_file`; `operator\|`/`operator&`/`any()` provided as `constexpr` free functions |
| `LibHandle` | struct | `platform/platform.hpp` | Opaque shared-library handle; `native` is `void*`; default-constructed = null |
| `LogLevel` | enum class | `core/log.hpp` | `Verbose`, `Info`, `Warning`, `Error`, `Fatal` |
| `LogCallback` | using | `core/log.hpp` | `void(*)(LogLevel, std::string_view, std::string_view) noexcept` |
| `AssetManagerHandle` | struct | `platform/os_io.hpp` (Android only) | Opaque AAsset manager wrapper; full definition in `android/os_io.cpp` |

## Free functions (platform.hpp)

| Function | Return | Notes |
|----------|--------|-------|
| `get_time()` | `double` | Monotonic elapsed seconds; epoch on first call; also captures main-thread ID |
| `sleep(ms)` | `void` | Yields thread; `0` is a legal scheduler hint |
| `open_library(path)` | `LibHandle` | UTF-8 path; returns `{}` on failure; Win32 converts to UTF-16 |
| `get_symbol(lib, name)` | `void*` | Returns `nullptr` on null handle or missing symbol |
| `close_library(lib&)` | `void` | Zeroes `lib.native`; no-op if null |
| `get_executable_dir()` | `std::string` | Forward-slash path, trailing `/` |
| `get_working_directory()` | `std::string` | Forward-slash path, trailing `/` |
| `is_debugger_present()` | `bool` | `false` on platforms where detection is unimplemented |
| `message_box(title, msg)` | `void` | Degrades to stderr on headless builds |
| `shell_execute(path, params)` | `void` | Fire-and-forget; errors silently swallowed |

## Free functions (os_io.hpp)

| Function | Return | Notes |
|----------|--------|-------|
| `open_file(path, mode)` | `OsFd` | Invalid on error; Win32 converts to UTF-16 |
| `open_memfd(name)` | `OsFd` | `memfd_create` on Linux; temp-file fallback elsewhere |
| `read(fd, buf, size)` | `int64_t` | Bytes read, or `-1` on error |
| `write(fd, buf, size)` | `int64_t` | Bytes written, or `-1` on error |
| `seek(fd, offset, whence)` | `int64_t` | POSIX SEEK_SET/CUR/END; returns new offset or `-1` |
| `tell(fd)` | `int64_t` | Current offset, or `-1` on error |
| `flush(fd)` | `void` | `fsync` (POSIX) / `_commit` (Win32) |
| `close_fd(raw_fd)` | `void` | Called by `OsFd::close()`; do not call directly unless raw fd is from `OsFd::release()` |
| `file_size(path)` | `optional<int64_t>` | `nullopt` if not found or error |
| `file_time(path)` | `optional<file_time_type>` | `nullopt` on failure |
| `list_directory(path)` | `vector<string>` | Names only, excluding `.` and `..`; empty on error |
| `is_case_insensitive(path)` | `bool` | Win32/macOS: always `true`; Linux: per-inode flag |
| `make_directory(path)` | `bool` | Creates one level; `true` if created or already exists |
| `rename_file(from, to)` | `bool` | Returns success flag |
| `delete_file(path)` | `bool` | Returns success flag |

## Global state (file-scope, implementation detail)

| Variable | Type | Where | Purpose |
|----------|------|-------|---------|
| Clock epoch / QPC init | `static const` | `sys.cpp` (per platform) | Set once by `get_time()` magic-static |
| Main-thread ID | `static std::thread::id` | `assert_main.hpp` (inline) | Set once by `capture_main_thread()` in `get_time()` |
| `g_log_callback` | `std::atomic<LogCallback>` | `log.cpp` | Optional callback; relaxed load/store |
| Crash `installed` flag | `static std::atomic<bool>` | `crash.cpp` (per platform) | Prevents double-registration |
| Console input buffer | `static char[]` | `console.cpp` (per platform) | Static accumulator for `read_line()`; overwritten each call |
| JNI state | statics | `android/os_io.cpp` | Written once at `JNI_OnLoad`; read-only thereafter |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_platform` | STATIC | *(none)* | `xash3dpp_utilities`, `dl` (POSIX Linux only), `android` + `log` (Android NDK) |

The target requires C++20 (`target_compile_features … PUBLIC cxx_std_20`).
Android builds receive `-DXASH_ANDROID` as a private compile definition, enabling
the AAsset bridge declarations in `os_io.hpp`.

To port to a new platform, create a subdirectory under `src/platform/` and
implement four files (`sys.cpp`, `os_io.cpp`, `console.cpp`, `crash.cpp`), then
add an `elseif` branch in `src/platform/CMakeLists.txt`.
