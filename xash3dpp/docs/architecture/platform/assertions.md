# Assertion macros

> **Moved**: `assert.hpp` and `assert_main.hpp` now live in `xash3dpp_core`. This page is retained for cross-reference and will be relocated to `docs/architecture/core/assertions.md` in a future doc reshuffle.

> **Defined in**: `include/xash3dpp/core/assert.hpp`  
> **Private utility**: `include/xash3dpp/private/core/assert_main.hpp`  
> **Namespace**: macros (project-wide); `xash::core::detail` (private functions)

## Overview

The assertion layer provides two macros and one debugger-break primitive for
enforcing invariants in xash3dpp code:

- `XASH_ASSERT(expr)` — debug-only; fires during development, silent in release.
- `XASH_FATAL(expr, msg)` — always-on; logs and aborts in all builds.
- `XASH_DEBUG_BREAK()` — platform-appropriate debugger break.

In addition, `assert_main.hpp` provides `detail::assert_main_thread(loc)`, used
internally by platform source files to enforce main-thread-only preconditions
at debug time.

## `XASH_DEBUG_BREAK()`

Triggers a debugger break using the best available mechanism for the compiler:

| Compiler | Instruction |
|----------|-------------|
| MSVC (`_MSC_VER` defined) | `__debugbreak()` |
| Clang with `__builtin_debugtrap` | `__builtin_debugtrap()` |
| GCC / Clang (fallback) | `__builtin_trap()` |
| Unknown | `std::abort()` |

Defined as a macro so its expansion appears at the exact call site in debugger
stack frames.

## `XASH_ASSERT(expr)`

Debug-only invariant check:

- **Debug builds** (`NDEBUG` not defined): `if(!expr) XASH_DEBUG_BREAK()`.
- **Release builds** (`NDEBUG` defined): `(void)sizeof(expr)` — the expression is
  type-checked by the compiler but never evaluated at runtime.

Use for cheap, high-confidence invariants that are true by construction in correct
code: null pointer checks, index bounds verified by the caller, subsystem
initialisation state. Do **not** use for conditions that can fail due to external
input or runtime error — those require `if`-checks with proper error returns.

`XASH_ASSERT` produces no log message. If a message before the break is needed,
use `XASH_FATAL` instead.

## `XASH_FATAL(expr, msg)`

Always-on invariant check that logs and aborts:

1. Evaluates `expr` in all builds (including release with NDEBUG).
2. If `!expr`:
   a. Calls `core::logf(LogLevel::Fatal, "assert", "FATAL: %s [file:line] %s", #expr, msg)`.
   b. Calls `XASH_DEBUG_BREAK()`.
   c. Calls `std::abort()`.

`msg` must be a string literal — the macro cannot allocate heap storage for a
`std::string`.

Use for invariants whose violation indicates data corruption or a porting bug that
would cause silent, hard-to-diagnose misbehaviour downstream. The logged message
appears in the default console sink and any installed callback before the process
terminates.

## `detail::capture_main_thread()` and `detail::assert_main_thread(loc)`

Private utilities in `assert_main.hpp`. Only `platform/` source files include
this header — nothing outside `src/platform/` should use it directly.

### `capture_main_thread() → void`

Records `std::this_thread::get_id()` as the main-thread ID. Called exactly once
from a C++11 magic-static initialiser inside `get_time()`. Because the
magic-static runs exactly once and `get_time()` is the first platform call, the
main-thread ID is captured before any worker threads are spawned.

### `assert_main_thread(loc: const char*) → void`

Compares the current thread ID against the captured main-thread ID:

- If they differ: calls `XASH_FATAL(false, "…called from a worker thread")`.
- If the main-thread ID is the default-constructed `std::thread::id{}` (i.e.
  `capture_main_thread()` has not been called yet): the check is **skipped**.
  This prevents false positives during early startup when `get_time()` has not
  yet been called.

`loc` is a string literal naming the calling function, used in the fatal message.

Used in: `sys.cpp` (`get_time` magic-static triggers `capture_main_thread`),
`crash.cpp` (`install_handler`), `console.cpp` (`read_line`).

## Threading model

| Macro / function | Thread safety |
|-----------------|--------------|
| `XASH_ASSERT` | No shared mutable state; safe from any thread |
| `XASH_FATAL` | Calls `core::logf` — safe from any thread (same thread safety as `logf`) |
| `XASH_DEBUG_BREAK` | No shared state; safe from any thread |
| `capture_main_thread` | Called inside a C++11 magic-static; initialised exactly once in a thread-safe manner |
| `assert_main_thread` | Reads the captured thread ID; safe from any thread after capture |

## Error handling

Neither macro nor function throws. `XASH_FATAL` calls `std::abort()` on invariant
violation.

## Edge cases and invariants

- `XASH_ASSERT` is elided at the object-code level in NDEBUG builds; `sizeof(expr)`
  still type-checks the expression.
- `XASH_FATAL(true, msg)` — where the condition is always true — is completely
  optimised away by any modern compiler.
- `assert_main_thread` skips the check if `capture_main_thread()` has not run yet
  (the stored ID is default-constructed). This means a platform function that
  asserts main-thread but is called before the first `get_time()` will not fire
  even from a worker thread. This is an accepted edge case limited to the brief
  startup window.

## See also

- [logging.md](./logging.md) — `XASH_FATAL` delegates to `core::logf` for the message
- [crash.md](./crash.md) — `crash::install_handler` is protected by `assert_main_thread`
- [system-utils.md](./system-utils.md) — `get_time()` triggers `capture_main_thread()`
