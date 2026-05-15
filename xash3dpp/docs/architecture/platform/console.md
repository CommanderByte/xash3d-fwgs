# Console I/O

> **Defined in**: `include/xash3dpp/platform/console.hpp`  
> **Source**: `src/platform/win32/console.cpp`, `src/platform/posix/console.cpp`, `src/platform/android/console.cpp`  
> **Namespace**: `xash::platform::console`

## Overview

The console API is the engine's developer terminal — a single read channel and a
single write channel, implemented per platform:

| Platform | Write channel | Read channel |
|----------|--------------|--------------|
| Win32 | conhost stdout via `WriteFile` | conhost stdin via `PeekConsoleInput` |
| POSIX | `stdout` via `::write(STDOUT_FILENO, …)` | `stdin` via non-blocking `select` |
| Android | logcat via `__android_log_write` | no-op (`{}` always returned) |

The console is the **default sink for `core::log`** — every `log()` / `logf()`
call eventually calls `console::write`. `console` is a platform-level OS
abstraction; `core::log` is a higher-level diagnostic primitive that happens
to use `console::write` as its default sink. High-level code may redirect
output by installing a callback with `log_set_callback`, but the console write
still fires unless explicitly suppressed at the application level.

## `write(text: string_view) → void`

Writes a UTF-8 string to the system console.

- The implementation may append a newline if `text` does not end with one.
- Never blocks — if the output buffer is full the write is dropped rather than
  stalling the caller.
- An empty `text` is a valid no-op call.

POSIX implementation loops `::write(STDOUT_FILENO, ptr, left)` and retries on
`EINTR` until all bytes are sent or an unrecoverable error occurs.

Android implementation calls `__android_log_write(ANDROID_LOG_INFO, tag, …)`.

## `read_line() → string_view`

Non-blocking poll for one complete line of user input.

Returns:
- A view of the line (without the trailing newline) when a complete line is ready.
- An empty `string_view` (`{}`) when no input is available.

Implementation per platform:

| Platform | Mechanism |
|----------|-----------|
| Win32 | `PeekConsoleInput` checks the input queue without blocking |
| POSIX | `select(0, &fds, …, &tv0)` checks stdin readability; if ready, `read(STDIN_FILENO, …)` fills the buffer |
| Android | Returns `{}` unconditionally — logcat is write-only from the host side |

**The returned view is invalidated by the next call to `read_line()`.**
The view points into a static buffer inside the per-platform implementation.
Callers must copy the contents into their own storage if the value is needed
beyond the current frame or call boundary.

## Threading model

`write` and `read_line` are **main-thread only** by contract. The header states
this explicitly for `read_line` and for `write` in most contexts.

One deliberate exception: `console::write` is called by `core::log()`, which
must be safe from any thread (see [logging.md](./logging.md)). In practice:

- POSIX: `::write(STDOUT_FILENO, …)` is async-signal-safe and atomically
  complete for small messages (less than `PIPE_BUF` bytes) — calling from multiple
  threads is race-safe for typical log lines.
- Win32: `WriteFile` on a console handle is safe from multiple threads.
- Android: `__android_log_write` is documented as thread-safe.

For `read_line` specifically, the static accumulator buffer is **not** protected by
a lock. This function must only be called from the main thread.

## Error handling

All failures are silent. A failed `write` is dropped; `read_line` returns `{}` on
read errors as well as on "no input ready".

## Edge cases and invariants

- `write("")` (empty view) is a safe no-op in all three implementations.
- Android's `console::write` sends output at `ANDROID_LOG_INFO` priority; the
  logcat tag is implementation-defined.
- The static input buffer for `read_line` is overwritten on each call — do not
  cache the returned `string_view` across calls.

## See also

- [logging.md](./logging.md) — `console::write` is the default output sink for `log`/`logf` (which lives in `xash3dpp_core`)
