# Wire Encoding (Layer 2)

> **Defined in**: `private/networking/split_packet.hpp`, `private/networking/split_reassembler.hpp`,
> `private/networking/oob_packet.hpp`, `private/networking/compressed_packet.hpp`,
> `private/networking/compress.hpp`  
> **Source**: `src/networking/compat_xash.cpp`, `src/networking/compat_goldsrc.cpp`,
> `src/networking/split_reassembler.cpp`, `src/networking/oob_packet.cpp`,
> `src/networking/compressed_packet.cpp`, `src/networking/compress_lzss.cpp`  
> **Namespace**: `xash::networking` (split), `xash::networking::oob`, `xash::networking::compressed_packet`, `xash::networking::lzss`

## Overview

Layer 2 provides **stateless wire-encode/decode helpers** for the three
non-netchan datagram shapes, plus the stateful `SplitReassembler` for
fragment accumulation. All helpers operate on caller-provided spans; none
allocate via the subsystem pool except `lzss::compress` (returns a
`std::vector`).

Four sub-systems:

1. **Split-packet encode/decode** — fragment production and reassembly
2. **OOB (out-of-band) encode/decode** — connectionless command framing
3. **Compressed-packet wrapper** — `net_header_compressed_packet` + LZSS
4. **LZSS codec** — raw compress/decompress algorithm

---

## Split-packet: encode (SplitProducer)

`SplitProducerXash` and `SplitProducerGoldSrc` are pull-style iterators that
fragment a logical payload into wire-ready split packets.

### Construction

```cpp
SplitProducerXash( payload, sequence_number, splitsize );
SplitProducerGoldSrc( payload, sequence_number, splitsize );
```

`splitsize` is the **on-wire packet size including the header**. The producer
computes the body size as `splitsize − sizeof(Header)`.

### Key operations

**`next(out)`** → `Result<std::size_t>` — writes one fragment (including the
packed split header) into `out`. Returns 0 when the producer is exhausted.
`out` must be at least `splitsize` bytes.

**`exhausted()`**, **`total_fragments()`** — state accessors.

### Capacity limits

| Protocol | Max fragments | Constraint |
|----------|---------------|-----------|
| Xash | `net_splitpacket_max_fragments` (256) | `uint8_t packet_id` field range |
| GoldSrc | `goldsrc_nibble_max` (15) | 4-bit nibble field cap; 5 used in practice |

### decode side

`decode_split_xash(packet)` / `decode_split_goldsrc(packet)` — parse one
received datagram starting with `net_header_split_packet` and return a
`SplitFragmentInfo` aliasing the input buffer. The caller must have confirmed
the magic word before calling.

`SplitFragmentInfo` carries `sequence_number`, `packet_number`, `packet_count`,
and `payload` (a `span<const byte>` aliasing the input — do not free it).

---

## Split-packet: reassembly (SplitReassembler)

`SplitReassembler` accumulates decoded fragments until the last expected one
arrives, then exposes the assembled payload.

### Fields

| Name | Type | Role |
|------|------|------|
| `sequence_` | `std::int32_t` | Current assembly sequence; −1 = idle |
| `expected_` | `std::uint8_t` | Fragment count declared in the first fragment |
| `received_` | `std::uint8_t` | Count of distinct fragments seen |
| `got_[max_fragments]` | `std::array<bool, 256>` | Per-slot arrival bitmask |
| `fragments_[max_fragments]` | `std::array<std::vector<byte>, 256>` | Per-slot payload copy |
| `assembled_` | `std::vector<byte>` | Contiguous output buffer (populated on completion) |

### Key operations

**`ingest(frag)`** → `Ingested` — the main intake function.

```
Ingested { outcome: Outcome, assembled: span<const byte> }
```

| `Outcome` | Meaning |
|-----------|---------|
| `Discarded` | Fragment rejected: bad count, mismatched body, or zero expected |
| `Pending` | Accepted; more fragments needed |
| `Complete` | Final fragment received; `assembled` holds the payload |
| `Duplicate` | Already had this slot; state unchanged |

When `sequence_number` changes, the in-progress assembly is **silently
dropped** and a new assembly begins. This matches legacy single-slot semantics.

**`reset()`** — clears all state; slots are re-used in place (no deallocation).

**`assembled`** span is valid only until the next `ingest()` or `reset()` call.

### Threading model

Caller-synchronised. One `SplitReassembler` per direction.

---

## OOB packet (xash::networking::oob)

Connectionless datagram framing: 4-byte LE magic `0xFFFFFFFF` followed by
an ASCII command string.

| Function | Returns | Notes |
|----------|---------|-------|
| `is_oob(packet)` | `bool` | True iff starts with OOB magic |
| `encode(payload, dst)` | `Result<std::size_t>` | Writes magic + payload; returns total bytes written |
| `encode(string_view, dst)` | `Result<std::size_t>` | Text overload; no NUL appended |
| `decode(packet)` | `Result<span<const byte>>` | Strips 4-byte header; returns inner payload |

`header_size = 4` (the magic word). `decode` returns `BadAddress` if the
packet is too short or lacks the magic.

---

## Compressed-packet wrapper (xash::networking::compressed_packet)

Datagram with magic `0xFFFFFFFD` followed by an LZSS-compressed body.

| Function | Returns | Notes |
|----------|---------|-------|
| `is_compressed_packet(packet)` | `bool` | Magic check |
| `encode(payload)` | `Result<std::vector<byte>>` | LZSS-compress + prepend magic; `BadAddress` if LZSS doesn't save space |
| `decode(packet, out)` | `Result<std::size_t>` | Strip magic + decompress into `out` |
| `inflated_size(packet)` | `std::uint32_t` | Read uncompressed size from inner LZSS header; 0 on malformed input |

`encode` returns `BadAddress` when LZSS compression does not reduce the
payload — callers should fall back to sending the uncompressed wire path.

---

## LZSS codec (xash::networking::lzss)

Wire-frozen algorithm parameters:

| Constant | Value | Meaning |
|----------|-------|---------|
| `magic_id` | `0x53535A4Cu` | `'LZSS'` LE — header magic |
| `header_size` | 8 | Bytes: 4 magic + 4 uncompressed-size |
| `window_size` | 4096 | Sliding window in bytes |
| `lookshift` | 4 | Exponent: lookahead = 1 << lookshift |
| `lookahead` | 16 | Maximum match length |

These are part of the protocol contract; changing any of them breaks on-wire
compatibility. They live in `compress.hpp`, not `limits.hpp`.

| Function | Returns | Notes |
|----------|---------|-------|
| `is_compressed(src)` | `bool` | Checks `magic_id` at offset 0 |
| `actual_size(src)` | `std::uint32_t` | Reads uncompressed size from header |
| `compress(src)` | `Result<std::vector<byte>>` | Returns compressed buffer; `BadAddress` if compression doesn't save space |
| `decompress(src, dst)` | `Result<std::size_t>` | Writes into caller `dst`; `BufferTooSmall` or `BadAddress` on failure |

---

## Threading model

All Layer 2 helpers are **stateless free functions** or **value-type objects**
with no shared mutable state. They may be used independently from any thread as
long as the input spans are valid and exclusively accessed by the caller.
`SplitReassembler` is stateful but caller-synchronised.

## Error handling

All encode/decode functions return `Result<T>`. `BadAddress` indicates a
malformed input; `BufferTooSmall` indicates a destination that is too small.
Neither condition logs anything — callers propagate or handle the error code.

## Edge cases and invariants

- LZSS `compress` returns `BadAddress` when the compressed output would not
  be smaller than the input (legacy behaviour: caller sends uncompressed).
- GoldSrc split-packet producers with a payload that needs more than 15
  fragments return `BadAddress` — the upper layer must refuse to send or split
  differently.
- `SplitReassembler::ingest` with a zero `packet_count` in the fragment info
  returns `Discarded` immediately.
- The `assembled` span from `SplitReassembler::ingest` aliases the internal
  `assembled_` vector; it is invalidated by any subsequent `ingest` or `reset`.

## See also

- [transport-layer.md](./transport-layer.md) — wire-format magic constants and POD header types
- [protocol-driver.md](./protocol-driver.md) — `SplitFormat` selects Xash vs. GoldSrc framing per channel
- Legacy: `engine/common/net_ws.c` — `NET_SendLong`, `NET_GetLong`, `NET_GetPacket` (compression path)
