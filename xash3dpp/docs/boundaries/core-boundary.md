# Core Subsystem Boundary Spec

**Status**: Chunk 3+ (S3 hardening, 6B wave)  
**Decision ref**: `docs/design/decisions-architecture.md` Q-1..Q-4, Q-7, Q-22, QN

## Responsibility

The core subsystem provides cross-cutting, engine-wide primitives that are usable from frame zero and require no context initialization:

1. **Logging** (`log.hpp`): free-function diagnostic I/O (log/logf/log_va, callable from any thread)
2. **Assertions** (`assert.hpp`): XASH_ASSERT (debug-only) and XASH_FATAL (always-on) macros
3. **Thread role registry** (`thread_role.hpp`): per-thread role tracking and assertions (main-thread-only subsystem entry guards)
4. **Error codes** (`error.hpp`): Chunk-2 vocabulary enum (Q-5)
5. **Frame timing** (`clock.hpp`): monotonic clock service over legacy host.c (realtime/frametime state, FPS gating, cvar integration)

Core is deliberately minimal and does NOT depend on:
- Any subsystem initialization (EngineContext)
- Memory allocation (utilities/memory)
- Networking, threading models, or concurrency synchronization beyond std::atomic

## ABI

Core is **internal to xash3dpp_core.a** — no ABI-stable exports (Chunk 3+). Logging and assertions are used by every subsystem; thread-role is used by subsystem entry points; clock is embedded in Host.

## Interface

**Public headers**: `include/xash3dpp/core/{log,assert,error,thread_role,clock}.hpp`

**Private detail**: `include/xash3dpp/private/core/assert_main.hpp` (legacy wrapper for platform code)

**No boundary-crossing pointers**: all APIs are free functions or inline value types.

## Dependencies

**Inbound** (higher-layer subsystems that use core):
- Every subsystem: log/assert/error  
- Server, map_loader, networking, platform, filesystem, host, cmd_cvar, utilities: thread_role asserts
- Host, server: Clock

**Outbound** (core depends on):
- **platform**: `platform::get_time()`, `platform::console::write()`, `platform::sleep()` (via platform.hpp in clock.cpp; log.cpp)
- **limits**: frame-time/FPS bounds (via limits.hpp in clock.cpp, log.cpp)
- **cmd_cvar** (Clock only): `CmdCvarContext` (forward-declared in clock.hpp; full include in clock.cpp — see Quirks)

**Cyclic dependency broken by forward-declaration**: Clock::ClockInitParams holds a non-owning `cmd_cvar::CmdCvarContext*` pointer. Since it is a pointer member (not by-value), a forward declaration in clock.hpp suffices; the full `#include <xash3dpp/cmd_cvar/context.hpp>` lives in clock.cpp only. This respects the layer model (core < cmd_cvar) without inversion. ✓

## Owned state

| State | Owner | Lifetime | Thread-safety |
|-------|-------|----------|---|
| `g_log_callback` (log.cpp) | core | process | std::atomic relaxed; single write from main before workers spawn |
| Clock timing atomics | Clock::Impl | Clock instance | std::atomic; reads from Chunk 10 renderer, writes from main |
| Thread-local `tls_role` (thread_role.cpp) | thread-local per thread | thread | no synchronization needed (thread-local) |

## Quirks

1. **g_log_callback is a mutable global** (compliance-allow at definition, propagates to uses via tooling 8c8d888a): diagnostics seam, G-1 hook. No context exists at log time; std::atomic + relaxed ordering ensure safe writer-once, reader-many.

2. **Clock::init/shutdown/set_frame_rate_gate assert ThreadRole::Main**: main-thread-only engine machinery. The scheduler (Chunk 10) may tick from other threads in future, but frame acceptance currently runs on main. See frame-budget discussion in clock.hpp.

3. **cmd_cvar forward-declared in clock.hpp but included in clock.cpp**: respects layer boundary (core < cmd_cvar). The pointer-member pattern avoids circular include. Clock stores a non-owning reference handed in by EngineContext at init time — lifetime is engine-context-owned (outlives Clock).

## Constant classification (Q-O)

No magic number literals requiring limits.hpp entries within core source files — all frame-timing bounds (min_fps, max_fps_hard, platform_log_buffer_size) are already in limits.hpp and used as absolute references (::xash::limits::*). ✓

## Q-11 Satellite Verdict

Core has **no satellite subsystem attachments** (Q-11 inapplicable). Core is a utility library, not a domain service.

## Threading

| Component | Constraint |
|-----------|-----------|
| log() / logf() / log_va() | Any thread; calls platform::console::write (platform-safe) |
| log_set_callback() | Main thread only (happens-before guarantee for worker thread spawning) |
| assert_thread_role() / register_thread_role() | Thread-local; safe from any thread |
| XASH_ASSERT / XASH_FATAL macros | Any thread; calls core::logf (any-thread-safe) |
| Clock::init / shutdown / set_frame_rate_gate | Main thread only (asserted at entry via assert_thread_role) |
| Clock::tick | Main thread only (can be verified by caller; tick() does not self-assert to allow test stubs) |
| Clock observers (realtime/frametime/etc.) | Any thread; all reads are via std::atomic load |

## Architecture docs

See `docs/architecture/core/` for detailed breakdowns of:
- `README.md` — subsystem overview
- `logging.md` — log output pipeline and callback design
- `thread-role.md` — thread-local role registry  
- `assert-*.md` — assertion policies

Architecture docs include threading analysis and Q-22 lifecycle model decisions.
