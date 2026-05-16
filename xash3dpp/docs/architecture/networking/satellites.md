# Satellites: Stats and Master List

> **Defined in**: `networking/stats.hpp`, `networking/master_list.hpp`  
> **Source**: `src/networking/master_list.cpp`  
> **Namespace**: `xash::networking`

## Overview

Two small satellite systems attach to `NetworkContext` without owning any of
its core transport machinery:

- **`NetworkingStats`** — always-on observability counters (three tiers)
- **`IMasterListConfig` / `IMasterListClient`** — master-server heartbeat interfaces

---

## NetworkingStats

Tiered instrumentation struct. The same instance lives in
`NetworkContext::Impl` and is returned by reference from `NetworkContext::stats()`.

### Tier 1 — always-on (unconditional)

| Field | Type | Incremented when |
|-------|------|-----------------|
| `packets_sent` | `std::atomic<std::uint64_t>` | Each datagram sent |
| `packets_received` | `std::atomic<std::uint64_t>` | Each datagram received |
| `bytes_sent` | `std::atomic<std::uint64_t>` | Per send, by datagram size |
| `bytes_received` | `std::atomic<std::uint64_t>` | Per receive, by datagram size |

### Tier 2 — `#if XASH_STATS` (profiling builds)

| Field | Type | Meaning |
|-------|------|---------|
| `fragments_sent` | `std::atomic<std::uint64_t>` | Individual split-packet fragments sent |
| `fragments_received` | `std::atomic<std::uint64_t>` | Individual split-packet fragments received |
| `packets_dropped_overflow` | `std::atomic<std::uint64_t>` | Datagrams discarded due to `MessageBuf` overflow |
| `packets_dropped_invalid` | `std::atomic<std::uint64_t>` | Datagrams rejected as malformed |
| `peak_loopback_depth` | `std::uint32_t` | High-water mark for `LoopbackTransport` queue depth |
| `peak_inflight_fragments` | `std::uint32_t` | High-water mark for simultaneous in-flight fragments |

Note: Tier-2 `peak_*` fields are plain `uint32_t` (not atomic). They are only
meaningful in profiling builds where updates are serialised by the `T_NetIO`
thread contract.

### Tier 3 — `#if XASH_DEBUG_NETWORKING` (dev-only, not yet defined)

Reserved for per-packet trace logs and fragment histograms. No fields exist yet.

### Design principle

**Never gate counter increments on a runtime boolean** — always measure.
This keeps hot-path measurements accurate even in release builds. Builds
that do not want the overhead simply omit the `XASH_STATS` define; Tier-1
atomics are always present.

### Threading model

Tier-1 fields use `std::atomic` with relaxed ordering. They may be read from
any thread at any time. The `stats()` reference returned by `NetworkContext`
is valid for the lifetime of the context.

Tier-2 plain integer fields are updated only from the `T_NetIO` thread; no
synchronisation is needed for them.

---

## IMasterListConfig

Pure-virtual interface implemented by the server layer. Passed via
`NetworkInitParams::master_list_config`. When `nullptr`, master-server
heartbeats are disabled.

| Method | Returns | Notes |
|--------|---------|-------|
| `lan_only()` | `bool` | If true, skip master-server registration |
| `nat_bypass()` | `bool` | Enable NAT traversal mode |
| `heartbeat_interval_seconds()` | `double` | How often to send a heartbeat |

---

## IMasterListClient

Pure-virtual interface for the master-list satellite's outward-facing actions.
Not yet used by `NetworkContext`; the implementation stub in `master_list.cpp`
is a Chunk 6 TODO.

| Method | Notes |
|--------|-------|
| `heartbeat()` | Send a heartbeat to all configured master servers (called from server frame tick) |
| `send_shutdown()` | Inform masters that this server is shutting down |

---

## master_list.cpp (current state)

`src/networking/master_list.cpp` currently contains only a stub comment:

```cpp
// TODO(Chunk 6): implement heartbeat scheduling against IMasterListConfig
```

The actual heartbeat scheduling, UDP I/O, and server-browser query handling
will be added in Chunk 6 once the transport layer (`IPlatformSockets`) is
wired in (Chunk 4).

---

## Lifecycle / ownership

- `IMasterListConfig` is a **non-owning pointer** stored in `NetworkContext::Impl`.
  Its lifetime must exceed `NetworkContext::shutdown()`.
- `IMasterListClient` is implemented by the server subsystem; it is not
  stored in `NetworkContext` today (Chunk 6 pending).
- `NetworkingStats` is **owned** by `NetworkContext::Impl`; it is destroyed
  when `shutdown()` runs.

## Threading model

`IMasterListConfig` implementations must be safe to read from the `T_NetIO`
thread (where heartbeats will be triggered).

## See also

- [context-lifecycle.md](./context-lifecycle.md) — `NetworkInitParams` carries the config pointer
- Legacy: `engine/common/masterlist.c` — heartbeat scheduling, master-server UDP queries
