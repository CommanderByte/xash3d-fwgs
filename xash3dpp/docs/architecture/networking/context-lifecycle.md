# Context Lifecycle

> **Defined in**: `networking/networking.hpp` (public); `private/networking/context_impl.hpp` (Impl)  
> **Source**: `src/networking/context.cpp`  
> **Namespace**: `xash::networking`

## Overview

`NetworkContext` is the **lifecycle hub** for the entire networking subsystem.
It owns the pool, holds all injected dependencies, and is the sole entry point
for packet I/O. A single `NetworkContext` per engine instance replaces the
scattered `net_t`, `netchan_t`, and fragment-pool globals in the legacy engine.

The class follows the pimpl pattern: the header exposes no internal types; all
mutable state lives in `NetworkContext::Impl` declared in
`private/networking/context_impl.hpp`.

## NetworkContext

### Members (via Impl)

| Field | Type | Role |
|-------|------|------|
| `sockets` | `IPlatformSockets *` | Non-owning; all real UDP I/O routes through this |
| `protocol_registry` | `IProtocolDriverRegistry *` | Optional; nullptr → GoldSrc driver only |
| `master_list_config` | `IMasterListConfig *` | Optional; nullptr → heartbeats disabled |
| `pool` | `xash::memory::PoolHandle` | Pool for fragment-buffer allocations; created in `init()` |
| `dedicated` | `bool` | Disables loopback ring and client-only paths |
| `initialised` | `bool` | True between successful `init()` and `shutdown()` |
| `configured` | `bool` | True after `config(multiplayer=true)` opens sockets |
| `stats` | `NetworkingStats` | Always-on observability counters |

### Key operations

**`init(params)`** — validates that `params.sockets` is non-null, stores all
injected dependencies, creates the networking pool, and sets `initialised = true`.
Returns `false` (without logging) on pool-creation failure. Re-entrancy-safe:
returns `true` immediately if already initialised. Must be called before any
other method.

**`shutdown()`** — destroys the pool, clears all injected pointers, resets
`initialised` and `configured`. Safe to call on a not-yet-initialised instance.
Must be called before the caller's `IPlatformSockets` is destroyed.

**`is_active()`** — `true` iff `impl_->initialised`.

**`config(multiplayer, change_port)`** — opens or closes real UDP sockets.
Currently a TODO stub (Chunk 4): returns `{}` unconditionally when active.

**`get_packet(sock, from, data)`** — pulls one packet: consults the loopback
ring first, then the real socket (Chunk 4+ TODO). Returns `WouldBlock` when no
packet is ready.

**`send_packet(sock, data, to)`** — sends one datagram to `to`. Short-circuits
to loopback for in-process addresses (Chunk 4+ TODO).

**`stats()`** — returns a const reference to `impl_->stats`; no lock needed
(Tier-1 fields are atomics).

### Lifecycle / ownership

```
[caller]
    │  NetworkContext ctx;          ← default constructor; allocates Impl
    │  ctx.init(params)             ← pool created, dependencies stored
    │  ctx.config(true, false)      ← sockets opened (Chunk 4)
    │  ctx.get_packet(...)          ← I/O loop
    │  ctx.send_packet(...)
    │  ctx.shutdown()               ← pool destroyed, pointers cleared
    │                               ← ctx destructor; Impl freed
```

Move is supported (`NetworkContext&&`); the moved-from instance has `impl_`
set to `nullptr`, which makes `is_active()` return `false` safely.

## NetworkInitParams

Dependency-injection bag passed to `NetworkContext::init()`.

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `sockets` | `IPlatformSockets *` | Yes | nullptr causes `init()` to fail |
| `protocol_registry` | `IProtocolDriverRegistry *` | No | nullptr → GoldSrc-only |
| `master_list_config` | `IMasterListConfig *` | No | nullptr → heartbeats off |
| `dedicated` | `bool` | — | Suppresses loopback and client paths |

All pointer fields are **non-owning**. The caller must guarantee their
lifetimes exceed the `NetworkContext`.

## SocketKind

```cpp
enum class SocketKind : std::uint8_t { Client, Server };
```

Selects which of the two logical UDP socket pairs an operation addresses. The
loopback ring uses `SocketKind ^ 1` semantics: a send from `Client` enqueues
on the `Server` ring, matching legacy `loopbacks[sock^1]`.

## Threading model

`NetworkContext` has no internal locks. All public methods are designed for
single-thread use on `T_NetIO`. `NetworkingStats::packets_sent` and sibling
Tier-1 counters are `std::atomic`, so they may be read from any thread without
synchronisation.

## Error handling

- `init()` returns `bool`; does not log (logging lands in a future audit pass).
- `config()`, `get_packet()`, `send_packet()` return `Result<T>`; callers check
  `.has_value()` or use `std::expected` monadic operations.
- Calling any I/O method before `init()` returns `NetError::NotInitialised`.

## Edge cases and invariants

- The `impl_` pointer is never `nullptr` after construction (the default
  constructor always allocates `Impl`). The only `nullptr` check inside
  methods is defensive re-entrancy guard `if (!impl_)`.
- `shutdown()` is idempotent; calling it twice is safe.
- `config(false, ...)` closes real sockets but does not destroy the pool;
  `init()` is not required again for a subsequent `config(true, ...)`.

## See also

- [address-errors.md](./address-errors.md) — `NetAddress`, `NetError`, `Result<T>`
- [satellites.md](./satellites.md) — `NetworkingStats`, `IMasterListConfig`
- [protocol-driver.md](./protocol-driver.md) — `IProtocolDriver`, `IProtocolDriverRegistry`
- Legacy: `engine/common/net_ws.c` — `NET_Init`, `NET_Shutdown`, `NET_Config`
