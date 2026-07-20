# Core Subsystem Boundary Spec

**Status**: Chunk 3+ (S3 hardening, 6B wave)  
**Decision ref**: `docs/design/decisions-architecture.md` Q-1..Q-4, Q-7, Q-22, QN

> Refreshed 2026-07-06 (as-built pass). Reconciled against `src/core/**`
> (`clock.cpp`, `error.cpp`, `log.cpp`, `thread_role.cpp`) and the public
> headers. `compliance_scan.py core` is **clean** (0 blocker/warning/note),
> `stub_scan.py core` is **clean** (0 TODO markers), and `status_table.py`
> reports core **Complete**. The main drift since the last revision is the
> **diagnostics-tier target split** (D-1): `log.cpp` + `thread_role.cpp` now
> compile into `xash3dpp_platform`, not `xash3dpp_core` — see the new
> *As-built reconciliation* section below. Threading refreshed with an
> atomics/thread-role class table; new *Extension axes (Q-21)* section records
> core as the P-4 introspection substrate and the enforcement primitive
> (`assert_thread_role`) behind every off-main door.

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

## As-built reconciliation (2026-07-06)

Additive notes reconciling this spec with the compiled code. Prior prose is
retained; superseded statements are flagged inline.

### Diagnostics-tier target split (D-1)

`src/core/CMakeLists.txt` builds `xash3dpp_core` from **only** `error.cpp` +
`clock.cpp`. The **diagnostics tier** — `log.cpp` and `thread_role.cpp`, i.e.
the implementations behind `log.hpp` / `assert.hpp` / `thread_role.hpp` /
`private/core/assert_main.hpp` — is **compiled into `xash3dpp_platform`**
(D-1 dependency hardening, 2026-07-06). The headers keep their
`xash3dpp/core/` paths and the `xash::core` namespace; only **target
membership** moved. This breaks the historical core ⇄ platform link cycle:
platform's console/crash/socket TUs use the assert / thread-role helpers and
the default log sink is `platform::console::write`, so hosting those two TUs
in platform makes the base layer self-contained.

> **Superseded 2026-07-06:** the *ABI* section's "Core is internal to
> `xash3dpp_core.a`" is now imprecise. `clock`/`error` live in
> `xash3dpp_core.a`; `log`/`thread_role` live in `xash3dpp_platform.a`.
> Neither exports an ABI-stable symbol — the split is purely a build-graph
> detail — but the vocabulary (`xash::core`) and header paths are unchanged.
> Layering remains one-way: **core → platform** (`xash3dpp_core PRIVATE`
> links `xash3dpp_platform`).

### Core ↔ platform thread-primitive split (carry-in)

Core **owns the threading vocabulary**: the `ThreadRole` enum, the
`register_thread_role` / `current_thread_role` / `assert_thread_role` API, and
the `thread_local tls_role` storage (`thread_role.cpp`). `assert_thread_role`
is THE enforcement primitive — the door-keeper every subsystem's Main-only
contract asserts through. What core does **not** own is the OS **thread-spawn +
role-registration wiring**: there is no primitive today that spawns an OS
thread and calls `register_thread_role` on it. That primitive belongs to
**platform** (which already hosts `thread_role.cpp`'s TU) and is platform's
Q-21 headline **door** — see `docs/boundaries/platform-boundary.md`
(Extension axes) and `docs/modernization-opportunities/platform-modernization.md`
(cross-cutting flags). Split summary: **core defines & enforces roles;
platform will create threads and stamp them.**

### Error vocabulary expanded beyond Chunk 2

`error.hpp`'s `ErrorCode` enum has grown past the original Chunk-2 vocabulary:
generic (`Ok`, `InvalidArgument`, `OutOfMemory`, `NotInitialised`,
`AlreadyInitialised`), **Host** (`HostFatal` = 100, `FrameAborted` = 101), and
**Map-load / BSP** (`MapNotFound` = 200, `MapLoadFailed` = 201,
`BspUnsupportedVersion` = 202, `BspCorruptLump` = 203, `BspBadWorld` = 204).
Numeric values are committed (diagnostic logs include the integer). New codes
are appended at the end per subsystem. `error_code_name()` maps every code to a
stable non-null string. (The *Responsibility* line "Chunk-2 vocabulary enum"
names the origin, not the current span.)

### Clock — as-built surface

`Clock` (pimpl) registers **seven** timing cvars at `init()` (skipped when
`cmd_cvar` is null, e.g. tests): `host_maxfps`, `fps_override`,
`host_framerate`, `host_sleeptime`, `host_sleeptime_debug`, `sys_timescale`,
`sys_ticrate` (legacy `host.c` roster). It exposes six atomic observers
(`realtime` / `frametime` / `realframetime` / `pureframetime` / `starttime` /
`framecount`) plus an **always-on `ClockStats` snapshot** (`stats()`) synced at
every accepted `tick()` and at `shutdown()` — this is a P-4 introspection
surface (see *Extension axes*). `set_frame_rate_gate()` injects the OQ-11
singleplayer-no-demo gate (`bool (*)() noexcept`, nulled until
`Server::init()`). `init` / `shutdown` / `set_frame_rate_gate` assert
`ThreadRole::Main`; `tick()` deliberately does **not** self-assert so tests can
drive it from a role-less thread.

### Minor drift (source-comment recommendation only)

`thread_role.hpp`'s header comment cites the legacy wrapper as
`<xash3dpp/private/platform/assert_main.hpp>`, but the file actually lives at
`include/xash3dpp/private/core/assert_main.hpp` (matching this spec's
*Interface* section). Recommend correcting the header comment — **no source
edit made in this doc pass.**

## ABI

Core is **internal to xash3dpp_core.a** — no ABI-stable exports (Chunk 3+). Logging and assertions are used by every subsystem; thread-role is used by subsystem entry points; clock is embedded in Host.

## Interface

**Public headers**: `include/xash3dpp/core/{log,assert,error,thread_role,clock}.hpp`

**Private detail**: `include/xash3dpp/private/core/assert_main.hpp` (legacy main-thread check for platform code — an independent mechanism, NOT a wrapper over `assert_thread_role`; corrected 2026-07-20)

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

## Queue family (P-1 primitives, added 2026-07-20)

`core` owns the two generic lock-free primitives of the P-1 queue family
(Q-24 `QUEUE_FAMILY_HOME`; contracts in
`design/thread-spawn-and-inbox-brief.md` §3.1/§3.2):

| Header | Type | Contract |
|---|---|---|
| `core/mpsc_queue.hpp` | `MpscQueue<T, Capacity, ReserveCapacity>` | Bounded Vyukov-ticket MPSC, single consumer. Trivially-copyable `T` only; no allocation after construction and none on any push/pop. `try_push` (normal region) + `push_reserved_class` (may use the reserved headroom) let a consumer subsystem's full-queue POLICY compose without forking the queue — the primitive itself never blocks and never drops, it returns `false`. |
| `core/spsc_ring.hpp` | `SpscRing<T, Capacity>` | Fixed-size SPSC ring, exactly one release-store/acquire-load pair per side. `try_write` short-writes rather than overwriting unread data; `read` returns 0 on empty and the caller owns the silence path. Occupancy is a first-class accessor — this retires the legacy `s_rawend` volatile-peek idiom. |

Both carry explicitly 64-bit position/generation counters (not `size_t`) so
the numeric-wrap window is unreachable on 32-bit targets, with a
`std::atomic<pos_t>::is_always_lock_free` static_assert — an adversarial
concurrency review found the original `size_t` counters admitted past the
advertised ceiling across the wrap. **Genericity is pinned by construction**:
core instantiates only non-audio types and no audio vocabulary appears in
either header. Sound is the first consumer (S9.7b); the Main-inbox drain
contract they were designed for remains DESIGNED-NOT-BUILT — sound's queue
runs Main→worker, the opposite direction from the G-1/G-3 inbox, and does
not discharge it.

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

> Refreshed 2026-07-06 (as-built pass) — analyse-threading class table.
> Core owns no locks and spawns no threads; its concurrency surface is exactly
> three primitives: **atomics** (Clock timing + the log-callback slot), a
> **thread-local** (the role registry), and **three `assert_thread_role(Main)`
> sites** (all in `clock.cpp`). This is the whole synchronization footprint.

| Component | Kind | Storage | Access rule |
|-----------|------|---------|-------------|
| `Clock::Impl` timing fields (`realtime_` / `frametime_` / `realframetime_` / `pureframetime_` / `starttime_` / `framecount_`) | `std::atomic<double>` ×5 + `std::atomic<uint64_t>` ×1 | Clock instance | Written on Main in `tick()`; read from any thread (Chunk 10/13 renderer) via relaxed load — lock-free, no torn reads |
| `g_log_callback` (`log.cpp`) | `std::atomic<LogCallback>` | process global (`compliance-allow`) | Written once on Main via `log_set_callback` before workers spawn; read relaxed on every `log()` — the G-1 diagnostics seam |
| `tls_role` (`thread_role.cpp`) | `thread_local ThreadRole` | per-thread | Mutated only by that thread's `register_thread_role`; no cross-thread sync needed — this IS the role primitive |
| `assert_thread_role(Main)` | enforcement call | — | 3 sites: `Clock::init` / `shutdown` / `set_frame_rate_gate`. `tick()` omits the assert by design (test-stub allowance) |
| `Clock::ClockStats stats_` | plain struct snapshot | Clock instance | Written on Main at each accepted `tick()`; read via `stats()` — no atomic, so off-main readers observe a possibly-stale-but-consistent-enough snapshot (P-4 note) |

## Role & parity

- **Role:** role-neutral substrate — logging, clock, thread-role registry and
  queue primitives; used by every role. No cross-role parity obligation.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`. Core is **pivotal**: it is
both the **P-4 diagnostics / introspection substrate** every frontend consumes
and the home of `assert_thread_role` — the **enforcement primitive behind every
off-main door** (G-1 / G-2 / G-3). Core keeps these doors open not by adding
seams but by *being* the vocabulary the other subsystems assert and publish
through. Verdicts:

| Goal / primitive | Applies? | Verdict / door |
|------------------|----------|----------------|
| **P-4** typed introspection substrate | **Yes — headline (provider)** | Core *provides* three of the four introspection channels named in P-4: **logs** (`log` / `log_set_callback` sinks), **perf** (`Clock::stats()` → the always-on `ClockStats` snapshot that feeds the three-tier stats model), and the **error vocabulary** (`error_code_name`). Debug/MCP/overlay frontends read these; they must **extend** them (add typed fields), never `extern`-poke Clock's `Impl`. |
| **Enforcement primitive** (cross-cutting) | **Yes — pivotal** | `assert_thread_role(ThreadRole::Main)` is the door-keeper every subsystem uses to pin Main-only machinery. Every off-main goal (G-1 listener, G-3 debug thread, G-2 v2-ABI worker) is validated by *this* check. The `ThreadRole` enum is **additive** — G-3's `Service` / `Debug` roles land here with their first consumer, changing no existing numeric value. |
| **G-1** in-engine MCP | **Door-keep** | `log_set_callback` is the explicit **REPL / log-capture hook** an MCP or debug frontend installs (extension-goals G-5 lists it by name); `ClockStats` is a ready perf surface. Door rule: the callback stays a single atomic slot, main-write-once — a service that needs fan-out builds a multiplexer *on top*, it does not re-plumb the seam. |
| **G-3** dedicated debug thread | **Door-keep** | The debug thread reads `Clock` observers (already atomic) and `stats()` without stalling Main, and registers a new `ThreadRole` via core's registry. No new primitive owed by core; the door is already open. |
| **P-3** context-first, no new file-scope state | **Documented exception** | Core is deliberately **context-less** (free functions, usable at frame zero before `EngineContext`). `g_log_callback` is the **documented P-3 exception** — a diagnostics seam that cannot take a context because no context exists at log time (`compliance-allow(mutable-global, di-global-ref)`). It is the *only* mutable global in core; `Clock` itself is a context-carrying instance. |
| **P-7** pool-owned RAII lifecycle | **Partial / N/A** | `Clock` is an RAII class with a `unique_ptr<Impl>` pimpl (the sanctioned `make_unique` exception), but it is a **host-embedded service**, not a pool allocation — it is not `create_*`/`pool_new`-owned. Core allocates nothing on the memory pools by design (it sits below `memory`). |
| **P-1** main-thread inbox | Consumer-side | Core has no inbox, but `assert_thread_role(Main)` is what the inbox drain point asserts, and `log_set_callback` gives an off-main producer a safe fire-and-forget diagnostic path. |
| **P-6** services are satellites | **Yes (base lib)** | Core is a *base* library that satellites (MCP, debug, script) consume; the engine never links toward a service. Nothing to change. |
| **P-8** annotation discipline | **Yes — met** | Core headers carry `@thread-safety:` on every public surface and the `compliance-allow` rationale is inline on `g_log_callback`. `assert_thread_role` is a documents-**and**-asserts primitive (the anti-pattern P-8 forbids is absent). |
| **P-2** published-snapshot reads | **Yes — already shaped** | `Clock::stats()` returns the by-value `ClockStats` snapshot; the log callback delivers values, not references into live state. Core holds no other sim state; keep any new introspection value-returning. |
| **P-5** narrowest-state signatures | **Already minimal** | The free functions (`log`, `assert_thread_role`, `error_code_name`) take exactly the values they touch; `Clock` methods operate on their own instance. No god-aggregate exists at this layer. |
| **G-2** game ABI v2 | Consumer/enabler | Core owns no game-facing surface; a v2 ABI binds `core::log`/`ErrorCode` through context-carrying descriptors at the abi shim. No core change forced — the enforcement-primitive row above is core's actual G-2 contribution. |
| **G-4** expanded in-game debugging | Provider via P-4 | The overlay/console frontends consume the same three channels the P-4 row names (log sinks, `ClockStats`, error vocabulary). Door rule identical to P-4: extend typed, never poke `Impl`. |
| **G-5** scripting runtime | Door-keep | `log_set_callback` is named by extension-goals §G-5 as the REPL/log-capture hook, and `error_code_name` is script-bindable vocabulary. Cold-path, `noexcept`, exception-free — already suited to the isolated-island constraint. |

**Net door-keep verdict:** core requires **no new seam** to keep the extension
doors open — its existing atomics, the `ThreadRole` enum (additive), the log
callback, and `ClockStats` already are the substrate. The one binding rule is
negative: **do not add a second mutable global** and **do not let a frontend
reach around `Clock::stats()` / the log sinks into `Impl`** (P-4 door).

## Architecture docs

See `docs/architecture/core/` for detailed breakdowns of:

- `README.md` — subsystem overview
- `logging.md` — log output pipeline and callback design
- `thread-role.md` — thread-local role registry  
- `assert-*.md` — assertion policies

Architecture docs include threading analysis and Q-22 lifecycle model decisions.
