# Network Buffer Baseline

## Scope

This document captures the compatibility surface for the engine message and bit
buffer helpers before deeper modernization.

Primary files:

- `engine/common/net_buffer.h`
- `engine/common/net_buffer.c`
- callers in `engine/common/net_chan.c`, `engine/common/net_encode.c`,
  `engine/client/parse/`, `engine/server/`, and client/server event paths.

The user-facing GoldSrc protocol note in
`Documentation/goldsrc-protocol-support.md` confirms that GoldSrc network
compatibility is a supported use case, but it does not describe bit-level wire
format details. `Documentation/protocol/03-netchan.md` is currently a `TBD`
placeholder. Golden byte-vector tests are therefore the primary migration
safety net for this phase.

## Legacy Buffer Shape

`sizebuf_t` stores:

- `pData`: caller-owned byte storage;
- `bOverflow`: sticky read/write overflow flag;
- `iCurBit`: current bit cursor;
- `nDataBits`: maximum readable/writable bit count;
- `pDebugName`: diagnostic label;
- `iAlternateSign`: GoldSrc-compatible alternate signed-integer mode depth.

The public C surface remains `MSG_*`.

## Behavior To Preserve

- Bit order is little-endian within each byte: bit position `n` maps to
  `BIT(n & 7)` in byte `n >> 3`.
- `BitByte(bits)` rounds up to the number of bytes needed to store a bit count.
- `MSG_GetNumBytesWritten` returns padded byte count; `MSG_GetRealBytesWritten`
  returns unpadded whole bytes.
- `MSG_SeekToBit` supports `SEEK_SET`, `SEEK_CUR`, and `SEEK_END` and rejects
  positions outside `[0, nDataBits]`.
- `MSG_WriteOneBit` sets `bOverflow` without advancing if it cannot write one
  bit.
- `MSG_WriteUBitLong` writes least-significant bits first and moves `iCurBit` to
  `nDataBits` on overflow.
- `MSG_ReadUBitLong(8)` returns `0` without setting overflow when fewer than
  eight bits remain.
- Other overflowing unsigned reads set overflow, move `iCurBit` to
  `nDataBits`, and return `0`.
- Signed integer encoding has two modes:
  - default mode writes magnitude bits first and sign bit last;
  - alternate mode writes sign bit first, then magnitude bits.
- `MSG_StartBitWriting` increments `iAlternateSign`; `MSG_EndBitWriting`
  decrements it and pads the cursor to the next byte boundary.
- Strings are null-terminated on write; string reads translate `%` to `.` to
  avoid format-string accidents.
- Coordinates use legacy `short * 1/8` precision unless
  `ENGINE_WRITE_LARGE_COORD` is enabled.
- `MSG_ExciseBits` removes a bit range in-place and leaves trailing storage
  bytes unspecified.

## Phase 42 Routing Boundary

The first live migration slice routes only `MSG_ExciseBits` through a modern
private helper. This keeps wire-format read/write behavior under existing C
functions while proving the modern bit-order model on a fragmentation helper
used by `Netchan_Process`.

Do not route broad `MSG_Read*` or `MSG_Write*` families until byte-vector and
round-trip coverage is stronger.
