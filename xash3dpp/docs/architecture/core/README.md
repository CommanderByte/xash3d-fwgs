# core — Architecture Overview

> **Source**: `xash3dpp/src/core/`\
> **Public API**: `xash3dpp/include/xash3dpp/core/`\
> **Private utilities**: `xash3dpp/include/xash3dpp/private/core/`\
> **Legacy reference**: `engine/common/con_utils.c`, `engine/common/host.c`,
> `engine/common/system.h`

## Purpose

`xash3dpp_core` supplies the cross-cutting primitives that every other
xash3dpp subsystem may call, including from before `EngineContext` is
constructed. Concretely, it provides:

- **Diagnostic logging** — a tag-and-level log API with an optional
  intercept callback and a fixed-size-buffer format path that does zero heap
  allocation.
- **Assertion macros** — a two-tier policy (`XASH_ASSERT` for debug-only,
  `XASH_FATAL` for always-on), both noexcept, that log before aborting.
- **Thread role registry** — a `thread_local` store that lets each thread
  declare its role at startup and lets subsystem entry points verify at runtime
  that they are being called from the correct thread.
- **Legacy main-thread helper** — the inline `assert_main_thread()` utility
  used by the platform layer while it still predates the full thread-role API.

`xash3dpp_core` is deliberately narrow. It does **not** own: OS I/O, file
operations, crash signal handling, memory pools, cvar/command state, or any
game logic. Those live in the layers above it.

## Design goals

- **Zero-init, no lifecycle** — all APIs are free functions. There is no
  `init()` or `shutdown()` — `log()` and `thread_role` are safe to call from
  `main()` line one.
- **No heap allocation on the hot path** — `logf()` formats into a fixed stack
  buffer; the callback receives a `string_view` slice of that same buffer.
- **Noexcept everywhere** — no exceptions; no RTTI. Builds with `/EHs-c-` and
  `/GR-`.
- **Always-on assertion policy** — `XASH_FATAL` fires in release builds.
  Thread role mismatches are programmer bugs; the engine must not continue.
- **Single global callback slot** — one optional `LogCallback` avoids lock
  contention on the hot path. Registered once from the main thread before
  workers start.

## Key invariants

- `log_set_callback()` must be called from the main thread before any worker
  threads are spawned. After that point the callback pointer is only read.
- `register_thread_role()` must be called from the thread it describes, before
  that thread calls any subsystem entry point guarded by `assert_thread_role()`.
- `capture_main_thread()` must run before any `assert_main_thread()` call has
  a meaningful effect. It is triggered automatically by `platform::get_time()`
  via a magic-static on first call.
- `LogLevel::Verbose` is a compile-time no-op unless `XASH_VERBOSE` is defined.
  Do not rely on verbose output in CI unless that flag is set.

## Relationship to legacy code

The legacy engine mixed diagnostic output with game-console output through
`Con_Printf` / `Con_DPrintf` and aborted via `Sys_Error` with no structured
logging. The rewrite separates concerns:

| Legacy | Rewrite equivalent |
|--------|-------------------|
| `Con_Printf` | `platform::console::write` (game console) |
| `Con_DPrintf` | `core::log(LogLevel::Verbose, ...)` |
| `Con_Reportf` | `core::log(LogLevel::Warning, ...)` |
| `Sys_Error` | `XASH_FATAL(false, msg)` then `platform::crash::abort()` |
| `ASSERT()` | `XASH_ASSERT(expr)` |

Thread role tracking has no direct legacy equivalent — the old engine relied
on single-threaded assumptions. `ThreadRole` exists to allow the rewrite to
introduce parallelism incrementally without losing the ability to catch
cross-thread invariant violations early.

## Architecture at a glance

`xash3dpp_core` is a leaf library from any subsystem's perspective but
internally depends on `xash3dpp_platform` for its default log sink:

```text
┌──────────────────────────────────────────────────────────┐
│  xash3dpp_cmd_cvar / xash3dpp_filesystem / …             │
│  (subsystems) — link core PRIVATE                        │
└───────────────────────┬──────────────────────────────────┘
                        │ #include <xash3dpp/core/log.hpp>
                        │         <xash3dpp/core/assert.hpp>
                        │         <xash3dpp/core/thread_role.hpp>
          ┌─────────────▼────────────────────────────────┐
          │  xash3dpp_core                               │
          │  • log.cpp — format + emit to console        │
          │  • thread_role.cpp — thread_local registry   │
          │  • assert.hpp (header-only macros)           │
          │  • private/core/assert_main.hpp (inline)     │
          └─────────────┬────────────────────────────────┘
                        │ PRIVATE: platform::console::write
          ┌─────────────▼────────────────────────────────┐
          │  xash3dpp_platform                           │
          │  • console::write — OS output                │
          └──────────────────────────────────────────────┘
```

The mutual dependency (`platform` also PRIVATE-links `core` for
`assert_main.hpp`) resolves at final link time across the two static archives.

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [logging.md](./logging.md) — `LogLevel`, `log()`, `logf()`, callback hook
- [assertions.md](./assertions.md) — `XASH_ASSERT`, `XASH_FATAL`, `XASH_DEBUG_BREAK`
- [thread-role.md](./thread-role.md) — `ThreadRole` registry and `assert_thread_role`
- [assert-main.md](./assert-main.md) — legacy `assert_main_thread` helper (private)
