#pragma once
// xash3dpp — platform crash-handler contract
// Legacy reference: engine/platform/win32/crash_win.c
//                   engine/platform/posix/crash_posix.c
//                   engine/platform/posix/crash_libbacktrace.c
//
// Porting contract (mandatory, see docs/boundaries/platform-boundary.md):
//   Every platform must provide all functions below.
//   Platforms without crash-handler support provide no-op implementations.
//
// Design:
//   • install_handler() is called once at engine startup, before any worker
//     threads are spawned.  It registers signal handlers / SEH / etc.
//   • print_trace() may be called from a signal/exception context; it must
//     not call malloc.  Writing to a pre-allocated static buffer is fine.
//   • Both functions are async-signal-safe on POSIX implementations.

namespace xash::platform::crash {

// Install the platform crash handler.
// Idempotent — calling more than once is a no-op.
// Must be called from the main thread before any other threads are created.
void install_handler() noexcept;

// Write a best-effort stack trace to stderr (or logcat on Android).
// Safe to call from a signal handler or structured exception handler.
// No heap allocation is performed.
void print_trace() noexcept;

} // namespace xash::platform::crash
