# assert_main_thread — Legacy Main-Thread Helper

> **Defined in**: `include/xash3dpp/private/core/assert_main.hpp`  
> **Namespace**: `xash::core::detail`  
> **Visibility**: private — not part of the public `xash3dpp_core` API

## Overview

`assert_main.hpp` is a compatibility shim that predates the full
`ThreadRole` API. It provides a lighter, header-only mechanism for
"this function must be called from the main thread" checks used by the
platform layer (`sys.cpp`, `crash.cpp`, `console::read_line`).

New code must use `xash::core::assert_thread_role(ThreadRole::Main)` from
[thread-role.md](./thread-role.md) instead. This header is retained because
migrating the three platform call sites has no urgency — the behaviour is
identical and the two mechanisms do not conflict.

## Functions

### `capture_main_thread()`

```cpp
inline void capture_main_thread() noexcept;
```

Records `std::this_thread::get_id()` as the main thread ID. Called exactly
once, automatically, from inside a magic-static in `platform::get_time()` —
this guarantees it runs before any worker threads start (the platform clock is
queried during engine startup before the thread pool is created).

Calling it a second time is harmless but discouraged — it would silently
change the recorded main thread.

### `assert_main_thread()`

```cpp
inline void assert_main_thread( const char *location ) noexcept;
```

Asserts that the calling thread is the recorded main thread. On mismatch,
fires `XASH_FATAL` with a message that includes `location` (a string literal
naming the function being protected, e.g. `"crash::install_handler"`).

**Pre-condition**: `capture_main_thread()` must have been called first.
If it has not been called yet, the stored ID is `std::thread::id{}` (default-
constructed, not equal to any real thread), and `assert_main_thread` silently
passes — allowing pre-capture calls during very early startup without
triggering false positives.

### `main_thread_id_ref()`

```cpp
inline std::thread::id &main_thread_id_ref() noexcept;
```

Returns a reference to the statically-stored main-thread ID. Not intended for
direct use; exposed for the inline implementations of `capture_main_thread()`
and `assert_main_thread()`.

## Threading model

| Operation | Safety |
|-----------|--------|
| `capture_main_thread()` | Called from the main thread inside a magic-static; the C++11 guarantee makes this safe even if multiple threads enter `get_time()` concurrently — only one will execute the lambda body. |
| `assert_main_thread()` | Reads the stored ID (no lock needed — written before workers start, never written again). Calls `XASH_FATAL` if mismatched. |

## Relationship to `assert_thread_role`

| Aspect | `assert_main_thread()` | `assert_thread_role(Main)` |
|--------|------------------------|---------------------------|
| Storage | `static std::thread::id` in this header | `thread_local ThreadRole` in `thread_role.cpp` |
| Capture | Implicit, via `get_time()` magic-static | Explicit: `register_thread_role(Main)` |
| Failure message | "platform main-thread-only function called from a worker thread" | "thread role mismatch: expected Main, got Worker" |
| Planned fate | Retained for platform/ legacy code | Preferred for all new code |

## Edge cases and invariants

- The silent-pass when `id == std::thread::id{}` is intentional. It allows
  early startup code to call `assert_main_thread` before `get_time()` has run
  without crashing. Once workers are spawned, `get_time()` will always have
  been called and the guard is fully active.
- This header includes `<thread>` — translation units that include it will pull
  in the platform threading headers. This is acceptable because all callers are
  already in the platform layer.

## See also

- [thread-role.md](./thread-role.md) — the preferred replacement for new code
- [assertions.md](./assertions.md) — `XASH_FATAL` used internally
- `xash3dpp/docs/design/threading-model.md` — authoritative threading model
