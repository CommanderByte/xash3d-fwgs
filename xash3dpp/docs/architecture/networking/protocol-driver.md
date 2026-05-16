# Protocol Driver

> **Defined in**: `networking/protocol_driver.hpp` (public), `private/networking/protocol_driver.hpp` (redirect)  
> **Source**: `src/networking/protocol_driver_goldsrc.cpp`, `src/networking/compat_goldsrc.cpp`, `src/networking/compat_xash.cpp`  
> **Namespace**: `xash::networking`

## Overview

The protocol driver system provides a **per-netchan compat seam** so that
different wire-protocol versions (GoldSrc, Xash, future variants) can coexist
without `#ifdef` guards scattered throughout the engine. The design follows
the Q-12 per-subsystem policy: compat decisions are made per channel at
connection time, not engine-wide.

Two interfaces form the seam:
- `IProtocolDriver` — the per-channel policy (split format, delta table set)
- `IProtocolDriverRegistry` — a factory registry for looking up drivers by
  wire protocol number

---

## SplitFormat

```cpp
enum class SplitFormat : std::uint8_t { Xash, GoldSrc };
```

Selects the on-wire fragment header shape for a given channel:
- `Xash` — 10-byte `SplitHeaderXash`, 16-bit packed `packet_id`
- `GoldSrc` — 9-byte `SplitHeaderGoldSrc`, 8-bit nibble-packed `packet_id`

---

## DeltaTableSet

```cpp
enum class DeltaTableSet : std::uint8_t { GoldSrc, Xash };
```

Identifies which set of entity-state delta field tables the driver expects.
Used by the delta encoder (Layer 4, not yet implemented) to select the
appropriate table layout.

---

## FrameMeta

```cpp
struct FrameMeta {
    std::uint32_t sequence      { 0 };
    std::uint32_t sequence_ack  { 0 };
    bool          is_reliable   { false };
    bool          is_split      { false };
    bool          is_oob        { false };
};
```

Decoded packet-header metadata — the result of parsing the netchan sequence
header before payload dispatch. Populated by `IProtocolDriver::read_packet_header`
(planned for Chunk 4 when the netchan layer lands).

---

## IProtocolDriver

Pure-virtual interface representing one protocol version's on-wire policy.

| Method | Returns | Notes |
|--------|---------|-------|
| `name()` | `const char *` | Static string for diagnostics |
| `split_format()` | `SplitFormat` | Xash or GoldSrc framing |
| `delta_tables()` | `DeltaTableSet` | GoldSrc or Xash delta tables |

**Planned (Chunk 4)**:
- `write_packet_header(msg)` — write sequence/ack header into a `MessageBuf`
- `read_packet_header(msg)` → `FrameMeta` — parse and return header fields

Instances are **not owned** by `NetworkContext`; the registry or the caller
manages their lifetime.

---

## IProtocolDriverRegistry

Pure-virtual factory registry. Callers that need to support non-default
protocol versions implement this and pass it via `NetworkInitParams`.

| Method | Returns | Notes |
|--------|---------|-------|
| `find_driver(protocol_number)` | `IProtocolDriver *` | Non-owning; returns nullptr for unknown numbers |

When `NetworkInitParams::protocol_registry` is `nullptr`, only the built-in
`GoldSrcProtocolDriver` is available.

---

## GoldSrcProtocolDriver (built-in)

The default driver, always linked into `xash3dpp_networking`. Implements
`IProtocolDriver` for the GoldSrc wire protocol.

```
name()          → "GoldSrc"
split_format()  → SplitFormat::GoldSrc
delta_tables()  → DeltaTableSet::GoldSrc
```

Currently lives as a **file-scope singleton** in `protocol_driver_goldsrc.cpp`:

```cpp
// detail-audit: accepted — temporary file-scope singleton; registry accessor
// will replace this in Chunk 4 per the TODO below.
[[maybe_unused]] GoldSrcProtocolDriver g_goldsrc_driver;
```

This is intentionally temporary (see `TODO(Chunk 4)` comment). The singleton
will be replaced by a proper `IProtocolDriverRegistry::find_driver()` lookup
once the registry accessor lands in Chunk 4.

---

## Compat helpers (compat_goldsrc.cpp / compat_xash.cpp)

Low-level free functions that implement the per-protocol fragment framing
differences. These are not part of the public API; they are called by the
split-packet producers and decoders in Layer 2.

**`compat_goldsrc.cpp`**:
- Nibble-pack/unpack helpers for `SplitHeaderGoldSrc::packet_id`
- Overflow guard using `goldsrc_nibble_max` (= 15)

**`compat_xash.cpp`**:
- Byte-pack/unpack helpers for `SplitHeaderXash::packet_id`
  (`high_byte = number`, `low_byte = count`)

---

## Lifecycle / ownership

- `IProtocolDriver` instances are owned by either the built-in TU
  (GoldSrc singleton) or by the user-provided `IProtocolDriverRegistry`.
- `NetworkInitParams` stores a non-owning pointer to the registry.
- `NetworkContext` does not free any driver; their lifetime must exceed the
  context's `shutdown()` call.

## Threading model

`IProtocolDriver` and `IProtocolDriverRegistry` are pure-virtual interfaces
with no internal mutable state in the base. Implementations are responsible
for their own thread safety. The built-in GoldSrc driver has no mutable state.

## Error handling

`IProtocolDriverRegistry::find_driver()` returns `nullptr` for unknown protocol
numbers — callers must check before dereferencing.

## See also

- [wire-encoding.md](./wire-encoding.md) — `SplitFormat` drives which producer/decoder is used
- [context-lifecycle.md](./context-lifecycle.md) — registry injected via `NetworkInitParams`
- Legacy: `engine/common/net_ws.c` — `SPLITPACKET` / `SPLITPACKETGS` selection logic
