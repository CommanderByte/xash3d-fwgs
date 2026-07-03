# networking — Architecture Overview

> **Source**: `xash3dpp/src/networking/`  
> **Public API**: `xash3dpp/include/xash3dpp/networking/`  
> **Legacy reference**: `engine/common/net_ws.c`, `engine/common/net_chan.c`,
> `engine/common/net_buffer.c/.h`, `engine/common/net_encode.c/.h`,
> `engine/common/masterlist.c`, `engine/common/netchan.h`

## Purpose

The networking module owns **every byte that crosses a process boundary over
the network**. It provides an address type, a bit/byte message codec,
transport-layer primitives (lag simulation, loopback, packet pools), wire
encode/decode helpers (split-packet framing, OOB, LZSS compression), and the
`NetworkContext` lifecycle hub that wires them together.

It does **not** execute game logic, parse game assets, manage DNS resolution
at the application layer, or implement the HTTP downloader. The netchan
reliable-channel layer is **fully implemented** in `netchan.hpp` /
`netchan.cpp` — reliable queue, fragment assembly and drain, bandwidth
choking, and file-transfer support are all complete. The delta encoder
(Layer 4, `delta/`) is implemented: `DeltaTables` lifecycle + delta.lst
parser, per-field codec, Xash mark-bit and GoldSrc group-mask wire formats
behind the private `IDeltaWireFormat` seam, table-descriptor sync, struct
codecs, and the game-DLL custom-encode hook. See
`docs/boundaries/networking-boundary.md` §"Delta encoder — implementation
notes" for the seam shifts vs. legacy.

## Design goals

- **Strict layer isolation**: each layer's types and helpers compile
  independently. Layer N may depend on Layer N−1 headers but not vice versa.
  Wire-format PODs are `#pragma pack(push,1)` frozen — no changes without a
  new protocol version.
- **No globals**: every component is a value-semantic class or a stateless
  free-function module. `NetworkContext` owns all mutable transport state;
  the legacy `net_t` global is fully encapsulated in its `Impl`.
- **Thread-safe observability, single-thread I/O**: `NetworkingStats` Tier-1
  fields use `std::atomic`; all I/O is confined to the `T_NetIO` thread.
  Callers are responsible for not racing on `NetworkContext` methods.
- **Injectable protocol policy**: `IProtocolDriver` / `IProtocolDriverRegistry`
  are public injectable interfaces so different protocol versions can coexist
  without `#ifdef`. The default GoldSrc driver is always linked.
- **Pool-backed allocations**: `NetworkContext::init()` creates a
  `PoolHandle("networking")` intended to back all fragment buffers. Netchan's
  fragment payloads and reliable buffer currently still use `std::vector`
  pending a pool-backed byte-vector adapter — tracked as `TODO(pool-migration)`
  in `netchan.cpp`. No `std::make_unique` after init except the single pimpl
  struct itself.
- **No exceptions, no RTTI**: compiled with `/EHs-c- /GR-`. Errors propagate
  through `Result<T>` (`std::expected<T, NetError>`), nullptr returns, or
  sticky overflow flags on `MessageBuf`.
- **C++23**: the only C++ standard requirement above C++20 is
  `std::expected<T,E>`, used for `Result<T>` in `errors.hpp`. The project
  standard was bumped when this header was introduced.

## Key invariants

- `NetworkContext::init()` must be called before any packet I/O; requires a
  non-null `IPlatformSockets`. Calling I/O methods before init returns
  `NetError::NotInitialised`.
- `NetworkContext::shutdown()` must be called before the injected
  `IPlatformSockets` pointer is invalidated.
- `PacketPool` and `LoopbackTransport` are caller-synchronised; they must not
  be accessed concurrently from multiple threads.
- `SplitReassembler` is single-slot: receiving a fragment with a different
  `sequence_number` silently discards the in-progress assembly.
- Wire-magic constants (`net_header_out_of_band_packet`, `net_header_split_packet`,
  `net_header_compressed_packet`) are frozen on the wire and must never be
  changed or moved to `limits.hpp`. They are identity discriminators, not
  tuneable parameters.
- All capacity limits (`net_max_datagram`, `net_max_fragment`,
  `net_splitpacket_max_fragments`, `net_max_loopback`, `net_packet_pool_slots`)
  live in `xash3dpp/include/xash3dpp/limits.hpp` with `XASH_LIMIT_<NAME>`
  override guards.

## Relationship to legacy code

| Area | Legacy | Rewrite |
|------|--------|---------|
| Address type | `netadr_t` (`common/netadr.h`) — SDK struct, packed, mixed IPv4/IPv6 | `NetAddress` — clean internal; ABI shim in `xash3dpp_abi` converts at DLL boundary |
| Message buffer | `sizebuf_t` + `MSG_*` free functions, global state | `MessageBuf` value type, no globals, caller-owned storage |
| Wire framing | Implicit `int` literals, signed/unsigned punning | Explicit `uint32_t` constants, `#pragma pack` PODs, `static_assert` on byte sizes |
| Loopback | Global `net_loopback_t` arrays | `LoopbackTransport` value class, dedicated `SocketKind` enum |
| Lag simulation | `net_fakelag` cvar side effects in `NET_GetPacket` | `LagQueue` pure value type, caller controls time |
| Compression | Codec calls embedded in `net_chan.c` | `xash::networking::lzss` namespace, `compressed_packet` wrapper |
| Protocol compat | `#ifdef XASH_GOLDSRC` scattered guards | `IProtocolDriver` per-channel injectable |
| Master-server | Free functions + global state | `IMasterListConfig`/`IMasterListClient` interfaces; built-in `MasterListClient` sends GoldSrc OOB heartbeat/shutdown |
| Netchan | `netchan_t` POD + free functions, fragment globals, `pfnBlockSize` callback | `Netchan` value class with pimpl, `NetchanConfig` setup, `IBlockSizeProvider` interface, `IProtocolDriver` chooses framing (no `gs_netchan` bool) |

## Architecture at a glance

```
┌──────────────────────────────────────────────────────────┐
│                      PUBLIC API                          │
│  NetworkContext   NetworkInitParams   SocketKind         │
│  NetAddress       MessageBuf          Result<T>          │
│  IProtocolDriver  IMasterListConfig                       │
└────────────────────┬─────────────────────────────────────┘
                     │ uses
┌────────────────────▼─────────────────────────────────────┐
│              LAYER 2 — encoding helpers                  │
│  SplitProducerXash/GoldSrc  SplitReassembler             │
│  oob::encode/decode          compressed_packet::encode/decode │
│  lzss::compress/decompress                               │
└────────────────────┬─────────────────────────────────────┘
                     │ uses
┌────────────────────▼─────────────────────────────────────┐
│              LAYER 1 — transport primitives              │
│  PacketPool    LoopbackTransport    LagQueue             │
│  wire_format.hpp  (magic constants, SplitHeader PODs)    │
└────────────────────┬─────────────────────────────────────┘
                     │ uses
┌────────────────────▼─────────────────────────────────────┐
│              LAYER 0 — foundation                        │
│  NetAddress / IpFamily     NetError / Result<T>          │
│  MessageBuf                limits.hpp capacities          │
└──────────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────┐
  │                   SATELLITES                         │
  │  NetworkingStats (atomics)                           │
  │  IMasterListConfig / IMasterListClient (interfaces)  │
  │  IProtocolDriver / IProtocolDriverRegistry           │
  └──────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────┐
  │               LAYER 3 — netchan                      │
  │  Netchan reliable queue, stream frags, flow ctrl     │
  │  Fragment batching (Fragbuf/IncomingStream)           │
  │  Bandwidth choking, path-traversal-safe file xfer    │
  └──────────────────────────────────────────────────────┘
```

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [context-lifecycle.md](./context-lifecycle.md) — NetworkContext, Impl, NetworkInitParams, lifecycle hub
- [address-errors.md](./address-errors.md) — NetAddress, IpFamily, NetError, Result<T>
- [message-buf.md](./message-buf.md) — MessageBuf bit/byte codec
- [transport-layer.md](./transport-layer.md) — LagQueue, LoopbackTransport, PacketPool (Layer 1)
- [wire-encoding.md](./wire-encoding.md) — wire framing: split packets, OOB, compressed packet, LZSS (Layer 2)
- [protocol-driver.md](./protocol-driver.md) — IProtocolDriver, IProtocolDriverRegistry, GoldSrcProtocolDriver, compat
- [satellites.md](./satellites.md) — NetworkingStats, IMasterListConfig/Client
