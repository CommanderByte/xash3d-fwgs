# Thread Role Registry

> **Defined in**: `include/xash3dpp/core/thread_role.hpp` / `src/core/thread_role.cpp`\
> **Namespace**: `xash::core`

## Overview

`ThreadRole` is a lightweight, zero-overhead mechanism for declaring each
thread's identity and enforcing thread-safety contracts at runtime. Every
thread that calls into a xash3dpp subsystem is expected to register a role
on startup. Subsystem entry points that have thread-safety requirements call
`assert_thread_role(expected)` to verify the calling thread's identity.

This exists because the rewrite introduces parallelism incrementally —
concurrency bugs discovered late are expensive. `ThreadRole` provides an
early-warning system that catches cross-thread violations as hard failures
(via `XASH_FATAL`) rather than silent data races.

## `ThreadRole` enum

```cpp
enum class ThreadRole {
    Unknown,        // not yet registered
    Main,           // host/frame-loop thread
    AudioCallback,  // OS audio buffer-fill (real-time)
    AudioDecoder,   // background audio stream decoder
    Worker,         // generic worker-pool thread
    Render,         // dedicated render thread (planned — Chunk 10)
    NetIO,          // networking I/O thread (planned — Chunk 2)
};
```

The numeric values of existing entries must not change — they may appear in
diagnostic messages and logs. New roles are appended at the end.

## API

### `register_thread_role()`

```cpp
void register_thread_role( ThreadRole role ) noexcept;
```

Records the calling thread's role. Must be called from the thread itself.
Not idempotent if called with a different role — the previous role is
silently overwritten. Use this once at thread startup:

```cpp
void worker_thread_main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Worker );
    // ...
}
```

### `current_thread_role()`

```cpp
[[nodiscard]] ThreadRole current_thread_role() noexcept;
```

Returns the role recorded for the calling thread. Returns `ThreadRole::Unknown`
if `register_thread_role()` has never been called on this thread. Zero cost —
reads a `thread_local` variable.

### `assert_thread_role()`

```cpp
void assert_thread_role( ThreadRole expected ) noexcept;
```

On mismatch, logs the actual vs expected role names via `core::logf` then
calls `XASH_FATAL`. Fires in both debug and release builds — thread role
mismatches are always programmer bugs.

Example failure message:

```text
[thread_role][FATAL]: FATAL: actual == expected  [src/core/thread_role.cpp:52]
    thread role mismatch: expected Main, got Worker
```

### `thread_role_name()`

```cpp
[[nodiscard]] const char *thread_role_name( ThreadRole role ) noexcept;
```

Returns a statically allocated, NUL-terminated string. Safe to call from any
thread, including in signal handlers (no heap allocation, no locks).

| Role | Returned string |
|------|----------------|
| `Unknown` | `"Unknown"` |
| `Main` | `"Main"` |
| `AudioCallback` | `"AudioCallback"` |
| `AudioDecoder` | `"AudioDecoder"` |
| `Worker` | `"Worker"` |
| `Render` | `"Render"` |
| `NetIO` | `"NetIO"` |
| (unrecognised) | `"?"` |

## Lifecycle / ownership

- No init or shutdown — the `thread_local` variable is zero-initialised by
  the runtime when each thread starts, which maps to `ThreadRole::Unknown`.
- Thread roles are never explicitly freed. When a thread exits, its
  `thread_local` storage is reclaimed by the runtime automatically.
- The main thread's role is not set by the platform layer automatically —
  the first thing `EngineContext::init()` (or equivalent) must do is call
  `register_thread_role(ThreadRole::Main)`.

## Threading model

| Operation | Safety |
|-----------|--------|
| `register_thread_role()` | **Call from the thread being registered only.** Writing a `thread_local` from another thread is undefined behaviour. |
| `current_thread_role()` | Safe from the calling thread. Never race-free for reading another thread's role (use `std::atomic` wrappers if cross-thread polling is needed, which is not supported here). |
| `assert_thread_role()` | Calls `core::logf` (thread-safe) then `XASH_FATAL` if mismatched. Safe from any thread. |
| `thread_role_name()` | Lock-free; reads a `switch` over a pure-enum argument. Safe from any thread including signal handlers. |

## Error handling

`assert_thread_role()` never returns on mismatch — it always aborts.
There is no "soft" check; if you need a conditional, call `current_thread_role()`
and compare manually.

## Edge cases and invariants

- A thread that has never called `register_thread_role()` has role `Unknown`.
  Calling `assert_thread_role(Unknown)` on such a thread will **pass** — this
  is intentional for bootstrapping code that runs before roles are assigned.
- The `Render` and `NetIO` roles are defined in the enum but not yet enforced
  anywhere — they are placeholder reservations for planned Chunks 2 and 10.
  Do not add `assert_thread_role(Render)` calls until the render thread exists.
- `register_thread_role()` is idempotent only when called with the same value.
  Calling it twice with different values (e.g. promoting a `Worker` to
  `AudioDecoder` mid-execution) silently changes the role. This should not be
  needed in practice; raise a design question if it is.

## See also

- [assertions.md](./assertions.md) — `XASH_FATAL` used by `assert_thread_role`
- [assert-main.md](./assert-main.md) — legacy precursor to `assert_thread_role`
- `xash3dpp/docs/design/threading-model.md` — full threading model and role assignment rules
