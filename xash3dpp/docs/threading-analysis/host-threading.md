# host — Threading Analysis (Chunk 3)

> Boundary spec: `docs/boundaries/host-boundary.md`

*2026-07-06 (as-built pass). Decision refs: host-boundary.md Resolved-decision
OQ-1 (hybrid abort, no `setjmp`/`longjmp`) and OQ-10 (single sanctioned global
accessor); `decisions-architecture.md` Q-2 (`EngineContext`), Q-4 (DI-params),
Q-21 (extension posture). Enforcement primitive: `core::assert_thread_role` /
`ThreadRole::Main` — owned by core, **established** here. Companion docs:
`server-threading.md` (OQ-9 main-thread-only precedent),
`memory-threading.md` (below-platform posture), `platform-threading` section in
`platform-boundary.md` (the missing thread-spawn+ThreadRole primitive).*

Sources scanned: `src/host/host.cpp`, `src/host/engine_context.cpp`,
`src/host/engine_context_accessor.cpp`, `include/xash3dpp/host/host.hpp`,
`include/xash3dpp/host/engine_context.hpp`, `tests/host/*`.

______________________________________________________________________

## Ownership model

**Single-threaded, main-thread-only — the layer that *establishes* Main.**
Host is not merely a main-thread consumer of the `ThreadRole` machinery: it is
where `ThreadRole::Main` is expected to already be registered (the launcher /
test harness calls `register_thread_role(ThreadRole::Main)` before the first
host entry — see `tests/host/test_host.cpp` `main()`), and it is the top of the
static dependency graph that drives every other subsystem's lifecycle from that
one thread.

Enforcement is **partial and asymmetric** — this is the headline finding of the
as-built pass:

- **Asserted `ThreadRole::Main`** (fail-fast in debug):
  `Host::init`, `Host::shutdown`, `Host::Impl::shutdown`,
  `EngineContext::init`, `EngineContext::shutdown`, and
  `abi::set_current_engine_context` (the write side of the global accessor).
- **NOT asserted** (declared main-thread-only in the header contract, but no
  runtime check): `Host::RunFrame`, `Host::RequestShutdown`,
  `Host::signal_frame_abort`, and every observer
  (`status`/`dedicated`/`realtime`/`frame_abort_pending`/`frame_abort_code`/
  `stats`). The two mutating gaps (`RunFrame`, `RequestShutdown`) are the ones
  that matter: they mutate `Impl` state with no guard.

The header (`host.hpp`) documents the contract correctly —
"@thread-safety: main-thread only … every public mutating entry runs on
ThreadRole::Main and asserts it" — but the implementation does **not** assert on
`RunFrame`/`RequestShutdown`/`signal_frame_abort`. Per the extension-goals P-8
door rule ("documents-but-never-asserts is non-compliant") this is a
conformance gap, not a race today (there is only one thread), but it is exactly
the assertion that would catch a future off-main frame pump.

There is no init-phase-then-read-only transition: `Impl` state is mutable for
the whole active lifetime, but only ever from the one thread.

______________________________________________________________________

## Thread-role posture

Host sits **above** platform (where `ThreadRole` lives), so it is free to call
`assert_thread_role` and does — 6 sites (listed above). This is the correct
posture for the orchestration layer: it should be the *strictest* asserter in
the tree, because every off-main door (`G-1` MCP inbox, `G-3` debug thread,
NetIO) must ultimately marshal back into the frame loop host owns.

The one cross-thread surface in the whole subsystem is the ABI accessor
(`abi::current_engine_context()`), covered under *Safe items* below.

______________________________________________________________________

## Classification table (analyse-threading)

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `g_engine_ctx` (`std::atomic<EngineContext*>`) | `engine_context_accessor.cpp` | **Atomic (acquire/release) — sanctioned global** | The ONE documented file-scope global (Q-2/OQ-10, `compliance-allow(di-global-ref)`). Written on Main (release-store, asserted) by `set_current_engine_context`; read lock-free / role-agnostic by C-ABI callers via `current_engine_context()`. Set only after full init, cleared FIRST in shutdown |
| `Host::Impl::frame_abort_pending` / `_code` / `_detail[]` | `host.cpp` | **Main-only, unsynchronised (safe-by-contract)** | Cross-*call* channel within one thread: set by `signal_frame_abort`, read+cleared at the top of the next `RunFrame`. `frame_abort_detail` is a fixed `std::array<char, host_frame_abort_detail_buf>` member — no heap, no static. Safe only while the OQ-9-style single-thread contract holds |
| `Host::Impl::status` / `stats_.status` | `host.cpp` | **Main-only, unsynchronised** | Written by init/shutdown/RequestShutdown; read by observers + the `Main()` loop. No atomic; correct under the single-thread contract, but `RunFrame`/`RequestShutdown` do not assert it |
| `Host::Impl::pool` + `own_fs` | `host.cpp` | **Main-only, owned** | Standalone-mode subsystems; created in `init`, destroyed in `Impl::shutdown` (both asserted). First created / last destroyed |
| injected dep pointers (`cmd_cvar`/`clock`/`map_loader`/`ext_fs`) | `host.cpp` | **Immutable-after-init** | Written once in `init`, never again; nulled in `shutdown`. Non-owning; caller guarantees they outlive Host |
| `EngineContext` members | `engine_context.hpp` | **Main-only, owned (declaration-order)** | Constructed in declaration order, destroyed reverse; `init`/`shutdown` both assert Main. Pinned (non-copyable/non-movable) because the accessor may hold its address for process life |
| `platform::crash::print_trace()` (called from recursive-abort path) | `host.cpp` | **Async-signal-safe (platform-owned)** | The only signal-context-safe call host makes; owned & contracted by platform (no malloc). Reached only from the fatal recursive-abort escalation, itself already a process-abort path |

______________________________________________________________________

## Safe items

- **`g_engine_ctx`** — `std::atomic<EngineContext*>`, release-store on Main,
  acquire-load anywhere. The read path is deliberately role-agnostic so an ABI
  symbol on any thread can reach the live context. The construction/teardown
  bookend (set-last-in-init, clear-first-in-shutdown) means a C-ABI caller never
  observes a partially-initialised or half-torn-down context. This is the single
  documented exception to the "no global accessor" rule and is correct as
  written.
- **The frame-abort trio** (`frame_abort_pending`/`_code`/`_detail`) — a
  deferred-work channel between `signal_frame_abort` (producer) and the next
  `RunFrame` (consumer), both on Main. No `setjmp`/`longjmp`, no exceptions:
  the legacy stack-jump is replaced by a flag polled at frame top, so every
  destructor from the aborted frame has already run before recovery executes.
- **The `std::array` abort-detail buffer** — bounded `memcpy`
  (`n = min(detail.size(), buf-1)`) with explicit null-termination; no
  static, no heap, no over-read (see the modernization note).

______________________________________________________________________

## Hazards

None **under the single-thread contract.** The table records the items that
*would* race if a future integration pumped `RunFrame` or signalled an abort
off-Main, and the items the module relies on but does not itself synchronise:

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `Impl::status` / `stats_.status` | `host.cpp` | Race-shared (contained; **unenforced**) | `RunFrame` and `RequestShutdown` mutate/read status with no `assert_thread_role`. Off-main entry would tear the lifecycle state. The missing assertion is the real defect, not the missing atomic |
| `frame_abort_pending`/`_code`/`_detail` | `host.cpp` | Race-shared (contained; **unenforced**) | `signal_frame_abort` is reachable via the `Host_Error` ABI shim (game DLL, synchronous, on Main today) and is `noexcept` with no `assert_thread_role`. A game DLL that ever called it off-Main would race the `RunFrame` consumer |
| recursive-abort escalation | `host.cpp` | Fatal-by-design | Second `signal_frame_abort` in one frame calls `platform::crash::print_trace()` then `XASH_FATAL` (process abort). Intentional (legacy Q-4 `Sys_Error` semantics). `print_trace` is async-signal-safe; the surrounding code is not, but it is a terminal path |
| `g_engine_ctx` | `engine_context_accessor.cpp` | Safe (atomic) | Listed for completeness: correct as-is. The only residual assumption is one live `EngineContext` per process (the accessor is a single global) |

**Step 5 — signal-handler reachability:** host registers **no** signal handler
(platform owns crash/SEH/`sigaction`). No host code runs *in* a signal context
except the `platform::crash::print_trace()` call inside the already-fatal
recursive-abort branch, which is platform-contracted async-signal-safe. The
`Host_Error` path is reached from **game-DLL synchronous calls on Main**, not
from an OS signal — so the abort machinery is not signal-handler-reachable in
the async sense. Step-5 verdict: **N/A** for host proper; the one signal-safe
requirement is discharged by delegating to platform.

______________________________________________________________________

## Required caller contracts

The module relies on these being upheld; the first two are only *partially*
asserted (see the enforcement-gap finding):

1. **Every host entry point is called from the Main thread.** `init`/`shutdown`
   assert this; `RunFrame`/`RequestShutdown`/`signal_frame_abort` currently do
   **not**. A future off-main frame driver (a threaded dedicated-server pump, a
   listen-server client at Chunk 12) must marshal onto Main — it must not call
   `RunFrame` directly. This is the OQ-9 posture inherited from server.
2. **`signal_frame_abort` runs on Main.** The `Host_Error` ABI shim reaches the
   live host via `current_engine_context()` and calls `signal_frame_abort` on
   the calling (Main) thread. Game DLLs must not marshal it off-Main; there is
   no synchronisation on the frame-abort trio.
3. **One live `EngineContext` per process.** `g_engine_ctx` is a single global;
   a second context would clobber the accessor. Matches the legacy single-`host`
   assumption.
4. **The read side of `current_engine_context()` may run on any thread** and is
   the contract that lets a future off-main service reach the context — but it
   returns a pointer into live main-owned state, so callers must not *mutate*
   through it off-Main (they marshal via the future P-1 inbox instead).

______________________________________________________________________

## Recommendations

Ordered; none change behaviour today, all are door-keepers for the extension
goals:

1. **Close the assertion gap (P-8).** ~~Add `assert_thread_role(ThreadRole::Main)`
   to `RunFrame`, `RequestShutdown`, and `signal_frame_abort`.~~
   **DONE 2026-07-19 (consolidation audit, HB-3):** all three entry points now
   open with the assert (`host.cpp` RunFrame / RequestShutdown /
   signal_frame_abort), matching the header contract. The enforcement gap this
   doc headlined is closed; the census is now 9 sites across 3 files
   (was 6/3).
2. **When the P-1 main-thread inbox lands (Chunk 7 queue family),** drain it in
   `RunFrame` at a defined frame point — `RunFrame` is *the* consumer end of the
   inbox that G-1 (MCP mutations) and G-3 (debug-thread actions) marshal into.
   The existing `if (s.cmd_cvar) s.cmd_cvar->cbuf_execute();` call is the legacy
   text-path sibling and marks the natural drain slot (before
   `map_loader.run_frame_step`). See the boundary spec's Extension axes.
3. **Keep the global count at one.** `g_engine_ctx` is the sole sanctioned
   file-scope mutable in the subsystem (P-3 door). Do not add a second; any new
   cross-thread reach goes through the accessor or the future inbox, not a new
   global.
4. **If a real off-Main frame driver ever appears,** the `Impl::status` and
   frame-abort rows graduate from "contained" to real hazards: the fix is a
   Main-thread marshal at the boundary (the P-1 inbox), never per-field atomics
   on the frame-abort trio (which would mask the contract violation and cannot
   make the deferred-recovery semantics correct off-thread).
5. **When `ThreadRole::Service`/`Debug` are added** (additive enum, with their
   first consumer), host's frame boundary becomes the publish point for P-2
   snapshots; `HostStats` is the seed of that surface (today one `status`
   field). No work until a consumer schedules it.
