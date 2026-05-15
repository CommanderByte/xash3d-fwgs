# Assertion Macros

> **Defined in**: `include/xash3dpp/core/assert.hpp`\
> **Namespace**: (macros — no namespace)

## Overview

`assert.hpp` defines the two assertion tiers used throughout xash3dpp.
Neither macro throws. Both abort the process if the condition is false. They
replace `<cassert>` entirely — `assert()` from the standard library is
explicitly prohibited because it calls `_invoke_watson` on MSVC under
`/EHs-c-` and provides no diagnostic log entry.

## Two-tier policy

| Macro | Build | When to use |
|-------|-------|------------|
| `XASH_ASSERT(expr)` | Debug only (`#ifndef NDEBUG`) | Cheap invariants that are true by construction in correct code (null-pointer guards, index bounds, subsystem-initialised checks) |
| `XASH_FATAL(expr, msg)` | Always (debug and release) | Invariants whose violation indicates data corruption or a porting bug; produces a human-readable crash message visible in stderr/logfiles even in production |

### `XASH_ASSERT`

```cpp
XASH_ASSERT( expr )
```

In debug builds: evaluates `expr`; on failure, calls `XASH_DEBUG_BREAK()`.
In release builds (`NDEBUG`): expands to `(void)sizeof(expr)` — zero runtime
cost, but `expr` is still syntactically valid so the compiler checks types.

Does **not** call `core::log` — fires directly via the debugger break path.
Use `XASH_ASSERT` for checks that add no diagnostic value in production.

### `XASH_FATAL`

```cpp
XASH_FATAL( expr, "human readable message" )
```

Evaluates `expr` in every build. On failure:

1. Calls `core::logf(LogLevel::Fatal, "assert", "FATAL: <expr>  [file:line]  <msg>")`.
1. Calls `XASH_DEBUG_BREAK()` (attaches a debugger if one is running).
1. Calls `std::abort()`.

The log step ensures the reason appears in the platform console output and in
any registered `LogCallback` (e.g. a crash reporter) even in release builds.

Example failure message:

```text
[assert][FATAL]: FATAL: pool != nullptr  [src/memory/memory.cpp:87]  pool must be valid here
```

### `XASH_DEBUG_BREAK`

Platform-specific inline breakpoint:

| Platform | Expansion |
|----------|-----------|
| MSVC | `__debugbreak()` |
| Clang / GCC with `__builtin_debugtrap` | `__builtin_debugtrap()` |
| GCC / Clang fallback | `__builtin_trap()` |
| Other | `std::abort()` |

This macro is not part of the public API and should not be called directly;
it is an implementation detail of `XASH_ASSERT` and `XASH_FATAL`.

## Choosing the right tier

```text
Is the check cheap?
  YES → Is it only useful to developers, not operators?
    YES → XASH_ASSERT
    NO  → XASH_FATAL
  NO  → Consider a bool/optional return instead (Q-5 error handling)
```

Concrete examples:

| Scenario | Correct macro |
|----------|--------------|
| `ptr != nullptr` inside a constructor (guaranteed by construction) | `XASH_ASSERT` |
| Pool handle valid before allocation | `XASH_ASSERT` |
| Thread role matches required role | `XASH_FATAL` (via `assert_thread_role`) |
| Platform ABI version matches expected | `XASH_FATAL` |
| File-not-found | Neither — return `false` / `std::nullopt` |

## Threading model

Both macros are noexcept and have no shared mutable state of their own.
`XASH_FATAL` calls `core::logf` which is thread-safe (see
[logging.md](./logging.md)). The macros may be used from any thread.

## Error handling

These macros **are** the error handling for programmer bugs. There is no
recovery path — they always abort on failure.

## Edge cases and invariants

- `XASH_ASSERT` in release builds still evaluates `sizeof(expr)`, keeping the
  expression type-checked. This prevents the common pitfall of debug-only code
  with side effects that accidentally compiles out in release.
- `XASH_FATAL` logs **before** calling `XASH_DEBUG_BREAK`. This ordering
  ensures the message reaches the log sink even when a debugger is not attached
  and the break instruction terminates the process immediately.
- The `"assert"` tag used in the `XASH_FATAL` log call is intentionally fixed
  — it lets crash triage filter all assertion failures with a single tag query.

## See also

- [logging.md](./logging.md) — `LogLevel::Fatal` and `core::logf`
- [assert-main.md](./assert-main.md) — uses `XASH_FATAL` for main-thread checks
- [thread-role.md](./thread-role.md) — `assert_thread_role` uses `XASH_FATAL`
- `xash3dpp/docs/design/design-paradigms-round2.md` §QH — assertion policy rationale
