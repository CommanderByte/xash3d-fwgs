# Networking Threading Analysis

> Boundary spec: `docs/boundaries/networking-boundary.md`

## Ownership model

The networking subsystem is designed for **single-thread I/O** on the
`T_NetIO` role. All packet send/receive paths — `NetworkContext::get_packet`,
`NetworkContext::send_packet`, `NetworkContext::config`, `Netchan::transmit`,
`Netchan::process` — are confined to that role per
[`context-lifecycle.md`](../architecture/networking/context-lifecycle.md).

Enforcement is **documentation-only** today: there is no `assert_main_thread`
or thread-role debug check at any public entry point. The `@thread-safety:
T_NetIO-ready` markers on `get_packet` / `send_packet`
([networking.hpp:100, 108](../../include/xash3dpp/networking/networking.hpp))
are the only inline annotations.

A separate observability surface — `NetworkContext::stats()` — is **safe to
read concurrently from any thread** because every Tier-1 counter is
`std::atomic<std::uint64_t>` with `memory_order_relaxed`.

## Safe items

- **`default_protocol_driver_registry()`** — function-local static in
  [protocol_driver_goldsrc.cpp:138](../../src/networking/protocol_driver_goldsrc.cpp).
  C++11 magic-static initialisation; the registry and its embedded driver
  instances (`GoldSrcProtocolDriver`, `XashProtocolDriver`) are immutable
  after construction, so concurrent calls are safe. **Safe-RO**.
- **`NetworkingStats` (Tier-1 fields)** — every counter is
  `std::atomic<std::uint64_t>`; readers may scrape values from any thread.
  Tier-2/3 fields (under `#if XASH_STATS`) are plain integers updated only
  from `T_NetIO`. **Safe-RO for Tier-1; Safe-TLS-via-thread-confinement for
  Tier-2/3.**
- **`NetworkContext::Impl::bound_ports[2]`** — written once by `config()`,
  read-only thereafter until the next `config()` call. **Safe-RO between
  lifecycle transitions.**
- **`IProtocolDriver` instances** held by the registry — stateless after
  construction. **Safe-RO.**

## Hazards

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `NetworkContext::Impl::loopback` | `src/networking/context_impl.hpp` | Race-shared | Mutable dual-ring buffer; no internal lock. Documented as caller-synchronised; relies on `T_NetIO` confinement. |
| `NetworkContext::Impl::packet_pool` | `src/networking/context_impl.hpp` | Race-shared | Free-stack allocator state; no internal lock. Caller-synchronised. |
| `NetworkContext::Impl::lag_queues[2]` | `src/networking/context_impl.hpp` | Race-shared | Mutable deque per side; no lock. Caller-synchronised. |
| `NetworkContext::Impl::reassemblers[2]` | `src/networking/context_impl.hpp` | Race-shared | Per-side split-reassembly tables; no lock. Caller-synchronised. |
| `Netchan::Impl::*` | `src/networking/netchan.cpp` | Race-shared | Outgoing sequence, reliable buffer, `outgoing_fragments[2]`, `incoming_streams[2]`, `frag_offset[2]`. No lock — every method requires single-thread access. |
| `NetworkContext::Impl::os_sockets[2]` | `src/networking/context_impl.hpp` | Lifecycle-race | Opened/closed by `config()` / `shutdown()`; data-race with any concurrent `get_packet`/`send_packet` not explicitly prevented (caller-synchronised by convention). |
| `MasterListClient::{ctx_, cfg_}` | `src/networking/master_list.cpp` | Race-shared | Reference holders only; `heartbeat()` / `send_shutdown()` route through `NetworkContext::send_packet`, inheriting its T_NetIO confinement requirement. |

No `Race-static-buf`, `Race-lazy-init` (unsafe), or `Signal-unsafe` items
were found. There are no internal mutexes, condition variables, or
hand-rolled double-checked-locking patterns anywhere in
`xash3dpp/src/networking/`.

## Required caller contracts

1. **Single-thread I/O**: all `NetworkContext::{config, init, shutdown,
   get_packet, send_packet}` calls must come from the same logical thread
   (`T_NetIO`). The same applies to every method of a `Netchan` instance
   and to `MasterListClient::heartbeat` / `send_shutdown`.
2. **`NetworkContext::stats()` is the only thread-safe observation point**.
   Callers running on other threads must not poke `Impl` state directly;
   they may only read Tier-1 atomic counters.
3. **Lifecycle is caller-sequenced**: `init` → `config(true)` →
   (loop: `get_packet`/`send_packet`) → `config(false)` → `shutdown`. No
   two of these calls may overlap on different threads. There is no
   internal re-entrancy guard beyond the `initialised` boolean flag in
   `NetworkContext::init`.
4. **`Netchan` setup / teardown** must precede any concurrent observation
   of the channel. The channel's pool handle (`PoolHandle("networking")`)
   is owned by the parent `NetworkContext`; the channel borrows it.
5. **No signal-handler callers**: nothing in this subsystem is
   async-signal-safe. Signal handlers must defer to a `T_NetIO` task.

## Recommendations

1. **Add a debug-only `assert_thread_role(T_NetIO)` at the top of every
   public `NetworkContext` and `Netchan` method.** Today the single-thread
   contract is documentation-only — a violation produces silent corruption
   in `LoopbackTransport`, `PacketPool`, and the fragment queues. The
   project already has a `core::thread_role` facility (see
   `tests/core/test_thread_role.cpp`); wiring it in catches contract
   violations without any runtime overhead in release builds.
2. **Promote `os_sockets[2]` access through a small accessor that asserts
   the lifecycle state** (`initialised && configured`). Today a stray
   `send_packet` racing with `shutdown` could read a half-closed handle.
3. **Document the thread-safety contract on every public type**, not just
   `get_packet` / `send_packet`. Add `// @thread-safety: T_NetIO-only`
   comments to the `Netchan` class header and to `IMasterListClient`
   methods so it is locally visible at the call site.
4. **Consider folding Tier-2 counters into atomics** if any of them
   becomes interesting to read from a debug overlay running on the
   render thread. They are currently plain integers because they are
   only mutated on `T_NetIO`, but that constraint hides future hazards.
5. **Long-term**: when a worker-thread codec pass is introduced (compress
   in parallel with frame build), move the compression scratch buffers
   off the `Netchan::Impl` and into per-task locals so the per-channel
   state stays single-thread-owned.

## Keeping this document current

When any recommendation above is implemented in source code, flip the
relevant hazard row's `Class` column to **Fixed** in the same commit and
strike through the corresponding recommendation with `~~text~~`. A stale
hazard list is worse than no list.
