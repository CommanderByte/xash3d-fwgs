# Logging

> **Moved**: logging now lives in `xash3dpp_core`. This page is retained for cross-reference and will be relocated to `docs/architecture/core/logging.md` in a future doc reshuffle.

> **Defined in**: `include/xash3dpp/core/log.hpp`\
> **Source**: `src/core/log.cpp` (shared across all platforms)\
> **Namespace**: `xash::core`\
> **Library**: `xash3dpp_core`

## Overview

The logging API provides structured, level-tagged message output. Every xash3dpp
subsystem uses it in place of bare `printf` or the legacy `Con_Printf` /
`Con_DPrintf`. It is designed to be callable before any subsystem initialisation —
even from global constructors — because the default output sink
(`platform::console::write`) requires no setup.

All formatting uses a fixed-size stack buffer; no heap allocation occurs on the
hot path.

## `LogLevel`

| Value | Use |
|-------|-----|
| `Verbose` | Detailed trace output; compiled out at object-code level unless `XASH_VERBOSE` is defined |
| `Info` | Normal informational messages |
| `Warning` | Unexpected but recoverable condition |
| `Error` | Operation failed; caller propagates an error return |
| `Fatal` | Unrecoverable invariant violated; caller must abort |

`Fatal` does **not** abort by itself — it is a severity hint. Callers that need to
abort after logging use `XASH_FATAL` (see [assertions.md](./assertions.md)), which
calls `logf` at `Fatal` level, then `XASH_DEBUG_BREAK()`, then `std::abort()`.

## Core functions

### `log(level, tag, text) → void`

Emits a pre-formatted message. The implementation prepends a `[tag][LEVEL]: `
prefix and appends a trailing newline if one is absent. Both the default sink
(`platform::console::write`) and the optional callback (if set) are called. The
callback receives only the body text — no prefix, no trailing newline.

### `logf(level, tag, fmt, ...) → void`

`printf`-style variant. Uses a fixed `limits::platform_log_buffer_size`-byte stack
buffer (default: 2048). Messages that exceed the buffer are silently truncated and
end with ` [...]`. The `[[gnu::format(printf, 3, 4)]]` attribute enables
compiler format-string checking on GCC and Clang.

### `log_va(level, tag, fmt, args) → void`

`va_list` variant for wrappers that already hold a vararg list. Shares the same
fixed stack buffer and truncation behaviour as `logf`.

## Callback

### `LogCallback`

```cpp
using LogCallback = void(*)(LogLevel, std::string_view tag, std::string_view text) noexcept;
```

The callback receives the body text **without** the `[tag][LEVEL]:` prefix and
**without** a trailing newline.

### `log_set_callback(callback) → void`

Installs or removes the optional callback. Pass `nullptr` to revert to
default-sink-only behaviour.

Stored as `std::atomic<LogCallback>` with relaxed load/store ordering. The pointer
is always written before worker threads are started, so release/acquire visibility
is not needed.

The callback is invoked **in addition to** the default sink — `log_set_callback`
does not suppress `platform::console::write`. There is no mechanism to replace the
default sink via this API.

## Output path

```text
log() / logf() / log_va()
  │
  ├─ format prefix ("[tag][LEVEL]: ") prepended
  │
  ├─ body text formatted into fixed stack buffer
  │  └─ truncated with " [...]" if over limit
  │
  ├─► platform::console::write(full_line_with_prefix)   ← always called
  │
  └─► callback(level, tag, body_only)                    ← called if non-null
```

## Level helpers

Inline free functions that call `log` with a fixed level:

| Helper | Level | Compile condition |
|--------|-------|-------------------|
| `log_verbose(tag, text)` | `Verbose` | Elided unless `XASH_VERBOSE` defined |
| `log_info(tag, text)` | `Info` | Always compiled |
| `log_warning(tag, text)` | `Warning` | Always compiled |
| `log_error(tag, text)` | `Error` | Always compiled |
| `log_fatal(tag, text)` | `Fatal` | Always compiled |

## Level prefix tags

The implementation maps `LogLevel` to short string tags used in the output prefix:

| Level | Tag |
|-------|-----|
| `Verbose` | `VERB` |
| `Info` | `INFO` |
| `Warning` | `WARN` |
| `Error` | `ERR ` |
| `Fatal` | `FATAL` |

## Threading model

| Operation | Thread safety |
|-----------|--------------|
| `log` / `logf` / `log_va` | Safe from any thread — `platform::console::write` is thread-safe by contract |
| `log_set_callback` | Must be called from the main thread before worker threads are spawned |
| Callback invocation | Called from whichever thread calls `log`/`logf`; the callback must be thread-safe if multiple threads log concurrently |

## Error handling

No failure modes are surfaced. Over-length messages are silently truncated. Write
errors in the console sink are silently dropped.

## Edge cases and invariants

- `Verbose` messages are completely elided at the object-code level when
  `XASH_VERBOSE` is not defined — `log_verbose` becomes an empty inline.
- `logf` with a message that exactly fills the stack buffer is NUL-terminated at
  the last byte; the truncation marker ` [...]` is appended only when the body
  text is actually cut short, not when the buffer is exactly full.
- `log_set_callback(nullptr)` is safe to call even if no callback was installed.

## See also

- [assertions.md](./assertions.md) — `XASH_FATAL` delegates to `logf` at `LogLevel::Fatal` before aborting
- [console.md](./console.md) — `platform::console::write` is the default output sink called by every `log()`
