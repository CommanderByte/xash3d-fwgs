# Diagnostic Logging

> **Defined in**: `include/xash3dpp/core/log.hpp` / `src/core/log.cpp`  
> **Namespace**: `xash::core`

## Overview

The logging API is the single channel through which C++ subsystems emit
diagnostic messages. It replaces the legacy `Con_Printf` / `Sys_Error` mixture
with a structured, level-tagged interface that routes output to
`platform::console::write` by default and to an optional intercept callback
for test capture and future integration with an in-game console overlay.

Two distinct output channels exist by design:

| Channel | Use for |
|---------|---------|
| `core::log(Warning/Error/Fatal, …)` | Subsystem diagnostics — anything useful in a crash log or CI trace |
| `platform::console::write()` | Game console output — user-facing commands, runtime stats |
| `core::log(Info, …)` | Pre-console / early-init only; demoted — do not use for normal chatter |

## `LogLevel`

```cpp
enum class LogLevel { Verbose, Info, Warning, Error, Fatal };
```

| Value | When to use |
|-------|------------|
| `Verbose` | Detailed per-frame trace; compiled to a no-op unless `XASH_VERBOSE` is defined |
| `Info` | Early-init messages before the game console is available; demoted — prefer `Warning` or higher |
| `Warning` | Unexpected but recoverable condition (e.g. file not found, cvar clamp applied) |
| `Error` | Operation failed; the caller will propagate a non-fatal error return |
| `Fatal` | Unrecoverable invariant; the caller is expected to `XASH_FATAL` or abort after this call |

Note: `log(Fatal, …)` does **not** abort by itself. Use `XASH_FATAL` or
`platform::crash::abort()` after logging if you need to terminate.

## Core API

### `log()`

```cpp
void log( LogLevel level, std::string_view tag, std::string_view text ) noexcept;
```

Emit a pre-formatted message. The implementation prepends a `[tag][LEVEL]: `
prefix and appends a newline if missing, then writes the complete line to
`platform::console::write` and to the optional callback (if registered).

- **No heap allocation.** Uses a `char[2048]` stack buffer.
- **Thread-safe** for the default sink path. Callback re-entrancy is
  forbidden (the callback must not call `log()` back).
- **Truncation**: messages longer than ~2040 bytes are silently truncated with
  a ` [...]` suffix. The callback still fires with the truncated body.

### `logf()`

```cpp
[[gnu::format(printf, 3, 4)]]
void logf( LogLevel level, std::string_view tag, const char *fmt, ... ) noexcept;
```

`printf`-style overload. Formats into the same fixed stack buffer as `log()`.
The `[[gnu::format]]` attribute enables format-string checking on GCC/Clang.
MSVC SAL annotation is not applied — add it if/when needed.

### `log_va()`

```cpp
void log_va( LogLevel level, std::string_view tag,
             const char *fmt, va_list args ) noexcept;
```

For wrappers that already hold a `va_list`. Shares the same buffer and
truncation logic as `logf()`.

### Convenience helpers

All defined inline in `log.hpp`:

| Helper | Equivalent to |
|--------|--------------|
| `log_verbose(tag, text)` | `log(Verbose, …)` — compiled out without `XASH_VERBOSE` |
| `log_info(tag, text)` | `log(Info, …)` |
| `log_warning(tag, text)` | `log(Warning, …)` |
| `log_error(tag, text)` | `log(Error, …)` |
| `log_fatal(tag, text)` | `log(Fatal, …)` |

## Callback hook

### `LogCallback`

```cpp
using LogCallback = void (*)(LogLevel, std::string_view tag,
                             std::string_view text) noexcept;
```

The callback receives the **body only** — the `[tag][LEVEL]: ` prefix and the
trailing newline are stripped. This makes test assertions straightforward:

```cpp
capture.last_text == "path too long"  // not "[filesystem][WARN]: path too long\n"
```

### `log_set_callback()`

```cpp
void log_set_callback( LogCallback callback ) noexcept;
```

Installs a callback. Pass `nullptr` to clear. Only one callback is supported;
a second call replaces the first. The callback fires **in addition to** the
default sink — it cannot suppress console output.

## Buffer layout and output format

For a call `core::log(LogLevel::Warning, "filesystem", "path too long")`:

```
[filesystem][WARN]: path too long\n
```

Internally the line is built in a single `char[2048]` stack buffer:

```
┌──prefix──────────────────────┬──body────────────┬─\n─┐
│ [filesystem][WARN]:          │ path too long    │    │
└──────────────────────────────┴──────────────────┴────┘
  ^body_start offset stored separately for the callback
```

## Threading model

| Operation | Thread safety |
|-----------|--------------|
| `log()` / `logf()` / `log_va()` | Safe from any thread. The default sink (`platform::console::write`) is safe from any thread. The atomic callback load uses `relaxed` order — acceptable because the callback pointer is written once before workers start and never changes after that. |
| `log_set_callback()` | **Main thread only, before workers are spawned.** A data race exists if this is called concurrently with `log()`. |

## Error handling

The functions are noexcept and have no failure path — if `vsnprintf` returns a
negative value (encoding error), the body is left empty and the prefix-only
line is still emitted.

## Edge cases and invariants

- `LogLevel::Verbose` is compiled to a `return` at the top of `log()` unless
  `XASH_VERBOSE` is defined. The `log_verbose` helper is also a no-op inline.
  Both approaches are necessary: the direct `log(Verbose, …)` call is guarded
  too, so callers that construct a log message eagerly still pay zero I/O cost.
- The callback receives the body **without** a trailing newline, while the
  console always gets a line that ends in `\n`. Test assertions should not
  expect a newline in `last_text`.
- Truncation uses ` [...]` (6 bytes). The body slice passed to the callback is
  already the truncated version, including the marker.

## See also

- [assertions.md](./assertions.md) — `XASH_FATAL` uses `core::logf` before aborting
- [assert-main.md](./assert-main.md) — `assert_main_thread` uses `XASH_FATAL`
- `xash3dpp/docs/design/debug-stats-design.md` §1.2 — output-channel rule
