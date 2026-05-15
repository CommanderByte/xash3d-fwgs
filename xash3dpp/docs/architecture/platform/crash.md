# Crash handler

> **Defined in**: `include/xash3dpp/platform/crash.hpp`\
> **Source**: `src/platform/win32/crash.cpp`, `src/platform/posix/crash.cpp`, `src/platform/android/crash.cpp`\
> **Namespace**: `xash::platform::crash`

## Overview

The crash handler registers a platform signal/exception callback that catches
unhandled faults, prints a stack trace to stderr, and then re-raises the signal so
the OS can record the correct exit status and generate a core dump. The design
avoids async-signal-unsafe operations (no heap allocation) in `print_trace` so
that it can be called reliably from inside a signal context.

## `install_handler() → void`

Registers the crash handler with the OS.

| Platform | Mechanism |
|----------|-----------|
| Win32 | `SetUnhandledExceptionFilter(seh_filter)` — an unstructured exception filter that fires when no other handler claims the exception |
| POSIX | `sigaction` for `SIGSEGV`, `SIGFPE`, `SIGILL`, `SIGBUS`, `SIGABRT`, all with `SA_RESETHAND` set |
| Android | Same sigaction mechanism, adapted for the NDK signal stack |

**Idempotency**: a `static std::atomic<bool> installed` flag ensures the
registration runs at most once. Calling `install_handler()` a second time is a
safe no-op.

**Main-thread requirement**: must be called from the main thread **before** any
other threads are created. On POSIX, signal disposition is process-wide; a
`sigaction` called after threads are spawned may not correctly set up the signal
mask for those threads. The `detail::assert_main_thread` check fires in debug
builds if this invariant is violated — see [assertions.md](./assertions.md).

## `print_trace() → void`

Writes a best-effort stack trace to stderr.

| Platform | Implementation | Output |
|----------|---------------|--------|
| Win32 | `CaptureStackBackTrace(0, 64, frames, nullptr)` | Hexadecimal frame addresses written to `stderr` via `WriteFile` (no symbol lookup — post-mortem debugger required) |
| POSIX | `backtrace(frames, 64)` + `backtrace_symbols_fd(frames, n, STDERR_FILENO)` | Symbol names if debug symbols are present; raw addresses otherwise |
| Android | Same as POSIX; `STDERR_FILENO` is captured by the logcat daemon |

**Async-signal safety**: `print_trace` makes no heap allocations and calls only
async-signal-safe OS functions on POSIX. It is safe to call from inside a signal
handler.

## Per-platform handler behaviour

### Win32 SEH filter

The `seh_filter` function:

1. Calls `print_trace()`.
1. Returns `EXCEPTION_CONTINUE_SEARCH`, allowing downstream handlers such as
   the Visual Studio JIT debugger to run after the trace is printed.

### POSIX signal handler

The signal handler function:

1. Calls `print_trace()`.
1. Restores the default signal disposition (done automatically by `SA_RESETHAND`).
1. Calls `raise(signum)` to re-deliver the signal under the default handler,
   producing the correct signal-death exit status and triggering core dump
   generation.

## Threading model

| Operation | Thread safety |
|-----------|--------------|
| `install_handler` | Main-thread only; safe to call multiple times (idempotent) |
| `print_trace` | Async-signal-safe; callable from any thread or signal context |

## Error handling

Both functions are `noexcept` with no return value. Internal write failures in
`print_trace` are silently dropped — the goal is best-effort crash reporting, not
guaranteed delivery.

## Edge cases and invariants

- `SA_RESETHAND` on POSIX restores the default signal disposition after the first
  invocation. If a second fault occurs during crash handling (e.g. a fault in
  `print_trace` itself), the process terminates immediately with the correct signal
  number rather than looping.
- The SEH filter returning `EXCEPTION_CONTINUE_SEARCH` means that if no JIT
  debugger is registered, the OS default handler terminates the process as expected.
- `install_handler()` called after worker threads start is undefined behaviour on
  POSIX per the POSIX spec on `sigaction` and per-thread signal masks. The
  `assert_main_thread` enforcement is the only guard; production builds do not
  check this.

## See also

- [assertions.md](./assertions.md) — `XASH_FATAL` and `XASH_DEBUG_BREAK` for assertion-triggered aborts
- [system-utils.md](./system-utils.md) — `is_debugger_present` for detecting debugger attachment before handler installation
