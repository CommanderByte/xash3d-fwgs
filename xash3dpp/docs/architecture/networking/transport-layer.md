# Transport Layer (Layer 1)

> **Defined in**: `networking/lag_queue.hpp`, `private/networking/loopback_transport.hpp`,
> `private/networking/packet_pool.hpp`, `private/networking/wire_format.hpp`  
> **Source**: `src/networking/lag_queue.cpp`, `src/networking/loopback_transport.cpp`,
> `src/networking/packet_pool.cpp`  
> **Namespace**: `xash::networking`

## Overview

Layer 1 provides three value-type components that sit directly above the
platform socket layer and below the encoding helpers:

- **`LagQueue`** — delayed-delivery datagram queue for fake-lag simulation
- **`LoopbackTransport`** — in-process dual-ring loopback (listen-server path)
- **`PacketPool`** — fixed-slab pre-reserved datagram buffer allocator

This page also covers the **wire-format POD types** (`wire_format.hpp`) — the
frozen on-wire header structs and magic constants that the Layer 2 encoding
helpers depend on.

---

## LagQueue

Holds datagrams for a configurable delay before releasing them. Models the
legacy `fakelag` and `fakeloss` paths in `net_ws.c`, but with drop policy
moved to the caller and time supply moved to a caller-provided monotonic clock.

### Fields

| Name | Type | Role |
|------|------|------|
| `queue_` | `std::deque<DelayedPacket>` | FIFO arrival order; head = earliest scheduled release |

`DelayedPacket` holds `release_time_ms`, `peer: NetAddress`, and `data: std::vector<std::byte>`.

### Key operations

**`enqueue(now_ms, delay_ms, peer, data)`** — stores `data` for release at
`now_ms + delay_ms`. Returns `false` if `data` is empty (defensive; empty
datagrams are nearly always a bug upstream).

**`try_dequeue(now_ms)`** → `std::optional<DelayedPacket>` — pops and returns
the front element if its `release_time_ms ≤ now_ms`; otherwise returns
`std::nullopt`. Only the head is checked — out-of-order release (from
variable-delay scenarios) simply causes later packets to wait, matching legacy
behaviour.

**`size()`, `empty()`, `clear()`** — diagnostic and lifecycle helpers.

### Threading model

Caller-synchronised. Designed for the `T_NetIO` single-thread tick loop.

### Edge cases

- The queue is a FIFO. Variable delay across enqueue calls can create
  head-of-line blocking when a later packet has a shorter delay than an earlier
  one. This matches legacy `fakelag` semantics.
- Packet loss (fakeloss) is **not** implemented in `LagQueue`. Drop before
  calling `enqueue` based on the caller's RNG and rate.

---

## LoopbackTransport

In-process dual-ring loopback for the listen-server path. Implements the
legacy `net_loopback_t[2]` global arrays and the `NET_GetLoopPacket` /
`NET_SendLoopPacket` pair.

Sending from `Client` enqueues on the `Server` ring; sending from `Server`
enqueues on the `Client` ring (`sock ^ 1` semantics). Each ring is a fixed-
capacity circular buffer of `net_max_loopback` slots.

### Fields

| Name | Type | Role |
|------|------|------|
| `rings_[2]` | `std::array<Ring, 2>` | Index 0 = Client, index 1 = Server |

`Ring` has `slots[net_max_loopback]` (each `net_max_datagram` bytes), plus
`get` and `put` head/tail indices.

### Key operations

**`send(sender, data)`** → `Result<void>` — enqueues `data` on the opposite
ring. Returns `NetError::Overflow` when `data` exceeds `net_max_datagram`.
When the ring is full, the **oldest** slot is recycled (matching the legacy
`send - get > MAX_LOOPBACK` clamp).

**`receive(target, out)`** → `Result<std::size_t>` — pops one slot from
`target`'s ring into `out`. Returns `NetError::BufferTooSmall` if `out` is
too small, `NetError::WouldBlock` if the ring is empty.

**`clear()`** — drops all queued packets on both rings.

**`pending(target)`** → `std::size_t` — number of packets waiting for
`target`.

### Threading model

Caller-synchronised. Must not be used concurrently from multiple threads.

---

## PacketPool

Fixed-capacity slab allocator for datagram buffers. Avoids per-packet heap
traffic in the `T_NetIO` tick loop.

### Fields

| Name | Type | Role |
|------|------|------|
| `slots_` | `std::vector<Slot>` | Pre-reserved vector; `@pre-reserved: net_packet_pool_slots` |
| `free_list_` | stack/bitset | Tracks available slot indices |

`static constexpr std::size_t slot_capacity = xash::limits::net_max_datagram` — one
slot holds a full-sized datagram.

### Key operations

**`acquire()`** → `std::optional<PacketSlot>` — returns an RAII slot handle.
Returns `nullopt` when all slots are in use. The slot's `bytes()` is a full
`net_max_datagram`-byte span; callers call `set_length(n)` to trim to the
actual payload size.

**`PacketSlot`** — RAII move-only handle. `~PacketSlot()` calls `release()`,
returning the slot to the pool. `set_length(n)` shrinks `bytes()` to `n`
bytes without re-allocating.

### Threading model

Caller-synchronised; intended for a single `T_NetIO` thread.

---

## Wire-format constants and PODs

All types and constants in `private/networking/wire_format.hpp`. These are
**frozen on the wire** and must never change.

### Magic constants (discriminators)

| Constant | Value | Meaning |
|----------|-------|---------|
| `net_header_out_of_band_packet` | `0xFFFFFFFFu` | Connectionless packet |
| `net_header_split_packet` | `0xFFFFFFFEu` | Fragmented oversized packet |
| `net_header_compressed_packet` | `0xFFFFFFFDu` | LZSS-compressed datagram |

These are read before the netchan sequence parser. Third-party tools and game
DLLs depend on them. They are identity discriminators, not tuneable sizes, so
they live in `wire_format.hpp` rather than `limits.hpp`.

### Split-packet headers

| Struct | Size | Field layout |
|--------|------|-------------|
| `SplitHeaderXash` | 10 bytes | `uint32 net_id`, `int32 sequence_number`, `int16 packet_id` (high byte = fragment number, low byte = count) |
| `SplitHeaderGoldSrc` | 9 bytes | `uint32 net_id`, `int32 sequence_number`, `uint8 packet_id` (high nibble = number, low nibble = count) |

Both structs are `#pragma pack(push,1)`. Static asserts enforce byte sizes.

**`goldsrc_nibble_max = 15u`** — the maximum fragment number or count
encodable in a 4-bit nibble field; limits GoldSrc packets to ≤ 15 fragments.

### LongPacket

Single in-flight reassembly scratch buffer. Not a wire type — internal state
for the legacy single-slot reassembly path. Holds:
- `current_sequence` (−1 = idle)
- `split_count`, `total_size`
- `buffer[xash::limits::net_max_fragment]`

The rewrite replaces this with `SplitReassembler` (see
[wire-encoding.md](./wire-encoding.md)), but `LongPacket` remains in the
header as the base struct for the simple path.

## Threading model

All Layer 1 components are **caller-synchronised**. They are designed for
single-thread `T_NetIO` use. `wire_format.hpp` constants are `constexpr` and
safe to use from any thread.

## Error handling

- `LagQueue::enqueue` returns `bool`; empty-data case is the only failure.
- `LoopbackTransport` returns `Result<T>`; callers check `NetError`.
- `PacketPool::acquire` returns `std::optional`; `nullopt` = pool exhausted.

## See also

- [wire-encoding.md](./wire-encoding.md) — Layer 2 encoding helpers that consume wire-format types
- [context-lifecycle.md](./context-lifecycle.md) — `NetworkContext::Impl` will hold these components after Chunk 4
- Legacy: `engine/common/net_ws.c` — `net_loopback_t`, `NET_GetLoopPacket`, `NET_SendLoopPacket`, fakelag/fakeloss
