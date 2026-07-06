# host — architecture

`host` is the engine's lifecycle coordinator and main-thread frame orchestrator.
It owns the process-level bring-up/teardown sequence and the per-frame call
order; it does not parse maps, ship packets, own entities, or own the frame
clock. Full contract + legacy audit: [`../../boundaries/host-boundary.md`](../../boundaries/host-boundary.md).

## Layout

| File | Contents |
|---|---|
| `include/xash3dpp/host/host.hpp` | `Host` (pimpl), `HostArgs`, `HostInitParams`, `HostStatus`, `HostStats` |
| `include/xash3dpp/host/engine_context.hpp` | `EngineContext` flat owner + `EngineContextInitParams` |
| `src/host/host.cpp` | `Host` implementation (init / RunFrame / shutdown / signal_frame_abort) |
| `src/host/engine_context.cpp` | `EngineContext::init`/`shutdown` — subsystem bring-up in dependency order |
| `src/host/engine_context_accessor.cpp` | state behind the `xash::abi` singleton accessor (Q-2 exception, D-1) |

## Concepts

- **`Host`** — pimpl class; standalone lifecycle via `Host::Main(HostArgs)` or
  granular `init`/`RunFrame`/`shutdown` for embedding. Frame-abort propagation
  (Q-3/OQ-1) is a flag checked at the top of the next `RunFrame` — no
  setjmp/longjmp.
- **`EngineContext`** — flat struct owning every stateful subsystem as a direct
  member in dependency order; C++ construction/destruction order *is* the
  init/shutdown order. `init()` cascades subsystem `init()`s (filesystem →
  cmd_cvar → clock → networking → map_loader → host → server) and, only once all
  are up, publishes the ABI accessor.
- **`HostStatus`** — coarse process lifecycle enum (`Init`/`Running`/`Sleep`/
  `Shutdown`); PascalCase per QF (renamed from the former `k`-prefixed values —
  it is an internal enum, not ABI-visible).

## Threading

`host` is **main-thread only** (`ThreadRole::Main`). Per the QN thread-assert
policy ("documents-but-never-asserts is non-compliant"), every public mutating
entry opens with `::xash::core::assert_thread_role(::xash::core::ThreadRole::Main)`:

- `Host::init`, `Host::shutdown`, and the internal `Impl::shutdown`;
- `EngineContext::init`, `EngineContext::shutdown`;
- `set_current_engine_context` (the accessor's write path).

There is no internal synchronisation and no off-main surface. The one
cross-thread edge is the ABI accessor: `set_current_engine_context` (main-thread
release store) vs `current_engine_context` (lock-free acquire load, callable
from any C-ABI caller thread). `Host_Error` (the C-ABI shim) may enter from any
game-DLL thread and deliberately does not assert a role; its main-thread /
recursion policy is enforced downstream in `Host::signal_frame_abort` (Q-4/OQ-1).

**Registration.** `ThreadRole::Main` must be registered on the calling thread
before any asserted entry runs. This is wired on the real production entry —
`src/launcher/main.cpp` calls `register_thread_role(ThreadRole::Main)` before
constructing `Host` — and in the host test mains (`tests/host/test_host.cpp`,
`tests/host/test_engine_context_networking.cpp`). The launcher path uses
`Host::Main` directly; any future production entry that constructs an
`EngineContext` must register `Main` at its thread entry the same way.

## Observability

`HostStats` (status snapshot) is returned by reference from
`Host::stats() const noexcept` and kept in sync at every `Impl::status` write.
`make_unique` appears only as the `Host` pimpl allocation (`Impl`) — the
sanctioned pimpl idiom, not a pool-owned type.
