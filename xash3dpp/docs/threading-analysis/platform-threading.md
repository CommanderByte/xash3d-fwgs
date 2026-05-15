# Platform Threading Analysis

> Boundary spec: `docs/boundaries/platform-boundary.md`

## Ownership model

The platform subsystem has no single owner or lifecycle object.  Each function is
either a pure computation (`get_time`, `sleep`, `open_file`, …) or a thin wrapper
around a kernel API, except for three subsections that own non-trivial static state:

- **Console** (`console::read_line`) — accumulates input into static buffers;
  must only be called from one thread at a time.
- **Crash handler** (`crash::install_handler`, `crash::print_trace`) — registers
  a process-wide handler; must be installed before any threads are spawned.
- **Android JNI bridge** (`android_init_jni`, `get_asset_manager`) — populates
  module-level JNI globals; `android_init_jni` must complete before any thread
  calls `get_asset_manager`.

All other platform functions (`open_file`, `read`, `write`, `seek`, `tell`,
`flush`, `file_size`, `file_time`, `list_directory`, `make_directory`,
`rename_file`, `delete_file`, `is_case_insensitive`, `open_library`,
`get_symbol`, `close_library`, `get_executable_dir`, `get_working_directory`,
`message_box`, `shell_execute`, `is_debugger_present`) are either pure or operate
exclusively on caller-supplied arguments and are safe to call from any thread
concurrently.

## Safe items

- **`get_time()` clock epoch** (`sys.cpp`) — Initialised as
  `static const Clock::time_point k_start = Clock::now()`.  C++11 guarantees
  that function-local static initialisation is thread-safe (magic-static).  All
  subsequent reads are const.  **Safe-RO**.

- **`sleep()`** — calls `Sleep()` (Win32) / `nanosleep()` (POSIX).  No shared
  state.  **Safe-TLS**.

- **Library loading** (`open_library`, `get_symbol`, `close_library`) — wraps
  `LoadLibraryW` / `GetProcAddress` / `FreeLibrary` (Win32) or
  `dlopen` / `dlsym` / `dlclose` (POSIX), which are defined thread-safe by their
  respective OS specifications.  `LibHandle` is a plain struct returned by value;
  no internal shared state.  **Safe-TLS**.

- **Path queries** (`get_executable_dir`, `get_working_directory`) — read-only
  filesystem / process queries; result returned in a `std::string`.  No shared
  state between calls.  **Safe-TLS**.

- **`console::write()`** (Win32) — calls `WriteFile` on `STD_OUTPUT_HANDLE`.
  `WriteFile` on a console handle is documented thread-safe on Win32 for writes
  up to 65 535 bytes (single atomic kernel operation at the console buffer level).
  **Safe-concurrent** for log-sink usage.

- **`console::write()`** (POSIX) — calls `write()` on `STDOUT_FILENO`.  POSIX
  does not guarantee that concurrent `write()` calls to a terminal are
  interleave-free beyond `PIPE_BUF` bytes.  For the expected usage pattern
  (single log-sink thread), there is no concurrent caller in practice.
  **Safe-TLS** in expected usage; note for future multi-writer use.

- **`crash::print_trace()`** — uses a static `void *frames[64]` buffer and
  writes only to `STDERR_FILENO` / `STD_ERROR_HANDLE`.  Signal / SEH handlers
  are per-process on their respective platforms; it is structurally impossible
  for two unhandled-exception callbacks to execute simultaneously.  The async-
  signal-safety concern is addressed by using `backtrace_symbols_fd()` (POSIX,
  documented async-signal-safe) and `WriteFile` (Win32, no heap).  **Safe in
  crash context** (structurally single-caller); undefined if called concurrently
  from two normal threads (not an expected usage).

- **`get_asset_manager()` lazy init** (`android/os_io.cpp`) — uses
  `std::call_once(g_init_flags[idx], …)` for each of the two handle slots.
  `std::call_once` is specified thread-safe; concurrent callers for the same
  slot block until initialisation is complete.  **Race-shared, mitigated**.

## Hazards

All four hazards identified below have been resolved.  The original descriptions
are kept for historical context; see the **Status** column.

| Symbol | File | Class | Status | Notes |
|--------|------|-------|--------|-------|
| `installed` flag in `install_handler()` | `win32/crash.cpp`, `posix/crash.cpp` | **Race-init** | **Fixed** | Promoted to `static std::atomic<bool> installed{ false }`.  `installed.exchange(true)` is a single atomic RMW; concurrent callers are safe. |
| `accum`, `accum_len`, `result` in `console::read_line()` | `win32/console.cpp`, `posix/console.cpp` | **Race-shared** | **Mitigated** | `detail::assert_main_thread("console::read_line")` added at function entry.  Fires in debug builds if called from a worker thread.  The static buffers remain, so the constraint is enforced rather than removed. |
| `evbuf[64]` in `console::read_line()` (Win32 only) | `win32/console.cpp` | **Race-shared** | **Mitigated** | Same assert as `accum`/`result` above — the assert covers the entire function body. |
| `g_jni` read vs. write | `android/os_io.cpp` | **Race-init** (weak) | **Fixed** | `android_init_jni()` now calls `std::call_once(g_jni_flag, [&](){ … })`.  The `once_flag` establishes a C++ `happens-before` edge: any thread that reads `g_jni` after `call_once` completes is guaranteed to see the fully written state. |

## Required caller contracts

1. **`crash::install_handler()` is main-thread-only, before any other thread.**
   Call it at `main()` entry, before spawning any worker threads.  The function
   is idempotent but not thread-safe.

2. **`console::read_line()` is main-thread-only.**  It must not be called
   concurrently from two threads.  The returned `string_view` must be consumed
   (or copied) before the next call to `read_line()`.

3. **`android_init_jni()` is one-shot, pre-thread.**  It must be called exactly
   once, from the main thread (typically `JNI_OnLoad`), before any thread calls
   `get_asset_manager()`, `list_assets()`, `asset_exists()`, or `open_asset()`.

## Recommendations

All three actionable recommendations have been implemented.

1. ~~**Add `assert_main_thread()` to `console::read_line()` and
   `crash::install_handler()`.**~~ **Done.**
   `src/platform/detail/assert_main.hpp` provides `capture_main_thread()` and
   `assert_main_thread(loc)`.  `capture_main_thread()` is called from a
   magic-static in `get_time()` on first use.  Both `console::read_line()` and
   `crash::install_handler()` call `assert_main_thread()` at entry.

2. ~~**Promote `installed` to `std::atomic<bool>`**~~ **Done.**
   Both `win32/crash.cpp` and `posix/crash.cpp` now use:
   ```cpp
   static std::atomic<bool> installed{ false };
   if( installed.exchange( true ) ) return;
   ```

3. ~~**Protect `g_jni` with `std::once_flag`**~~ **Done.**
   `android_init_jni()` now wraps all `g_jni` writes in
   `std::call_once(g_jni_flag, [&](){ … })`.  The `once_flag` is declared
   alongside `g_jni` in the anonymous namespace of `android/os_io.cpp`.

4. **Consider a per-call lock for `console::read_line()`.** If the engine ever
   gains a background I/O thread that also needs to issue console prompts, a
   `std::mutex` guard around the static buffers is the minimal fix.  Alternatively,
   replace the static buffers with a per-call `std::string` return type (breaks
   the `string_view` contract but removes the lifetime hazard entirely).
