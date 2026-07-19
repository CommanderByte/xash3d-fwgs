# core — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `core/log.hpp` | `xash::core` | `LogLevel`, `log()`, `logf()`, `log_va()`, `log_set_callback()`, `log_verbose()`, `log_info()`, `log_warning()`, `log_error()`, `log_fatal()`, `LogCallback` |
| `core/assert.hpp` | (macros) | `XASH_ASSERT`, `XASH_FATAL`, `XASH_DEBUG_BREAK` |
| `core/thread_role.hpp` | `xash::core` | `ThreadRole`, `register_thread_role()`, `current_thread_role()`, `assert_thread_role()`, `thread_role_name()` |
| `core/clock.hpp` | `xash::core` | `Clock`, `ClockStats` — frame timing (realtime/frametime), FPS gating |
| `core/error.hpp` | `xash::core` | `ErrorCode`, `error_code_name()` |

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/core/assert_main.hpp` | Inline `capture_main_thread()` and `assert_main_thread()` — legacy main-thread enforcement used by the platform layer |

## Source files

| File | Responsibility | Build target |
|------|---------------|--------------|
| `src/core/error.cpp` | `error_code_name()` string table | `xash3dpp_core` |
| `src/core/clock.cpp` | Frame-timing service (`Clock`) over `platform::get_time` | `xash3dpp_core` |
| `src/core/log.cpp` | Format prefix, truncate body, emit to `platform::console::write` and optional callback | `xash3dpp_platform` (D-1 hosted tier) |
| `src/core/thread_role.cpp` | `thread_local` role storage; `assert_thread_role` mismatch logging | `xash3dpp_platform` (D-1 hosted tier) |

## Tests

| File | What it covers |
|------|---------------|
| `tests/core/test_log.cpp` | All log levels, callback capture, truncation, callback reset, `log_verbose` compile-out |
| `tests/core/test_thread_role.cpp` | Default Unknown, register/update, `thread_local` isolation, all `thread_role_name` values, `assert_thread_role` on matching role |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `LogLevel` | `enum class` | `core/log.hpp` | Severity tier for diagnostic messages |
| `LogCallback` | function-pointer typedef | `core/log.hpp` | Optional intercept hook for test/diagnostics layer |
| `ThreadRole` | `enum class` | `core/thread_role.hpp` | Identity of a thread for cross-thread invariant checks |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_core` | STATIC | `xash3dpp` include dir (C++23) | `xash3dpp_platform` (one-way; D-1 — no reverse link) |

## Key constants

| Symbol | Defined in | Value | Meaning |
|--------|-----------|-------|---------|
| `xash::limits::platform_log_buffer_size` | `limits.hpp` | 2048 bytes | Stack buffer for `logf()` / `log_va()`; controls max message length |
