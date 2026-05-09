# Hash And Checksum Baseline

## Purpose

Phase 39 audits the public hash/checksum helpers before migrating any logic out
of legacy C files. The main implementation lives in `public/crclib.*`, not only
under `engine/`, so this layer has a wider ownership boundary than BaseCmd or
info strings.

## Public Surface

`public/crclib.h` exposes:

- `CRC32_Init`
- `CRC32_Final`
- `CRC32_ProcessByte`
- `CRC32_ProcessBuffer`
- `CRC32_BlockSequence`
- `MD5Init`
- `MD5Update`
- `MD5Final`
- `MD5_Print`
- `COM_HashKey`

The filesystem also exposes file-level wrappers through `fs_api_t`:

- `CRC32_File`
- `MD5_HashFile`

Those wrappers are path-resolution and VFS-sensitive and should stay with the
filesystem compatibility layer until a separate file-hashing migration.

## Callers And Compatibility

`COM_HashKey` is used by:

- client sound lookup hash tables
- OpenGL and software renderer texture hash tables
- modern `BaseCommandRegistry` tests and bucket compatibility

Its compatibility rules are quirky:

- The seed is `5381`.
- ASCII uppercase is first folded to lowercase, then the byte is masked with
  `0xDF`, so ASCII names are case-insensitive.
- The return value is `hash & (hashSize - 1)`, not modulo. Callers therefore
  assume power-of-two table sizes. Non-power-of-two sizes intentionally keep the
  mask behavior.
- A `hashSize` of zero returns the raw 32-bit accumulated hash because
  `hashSize - 1` wraps to `UINT_MAX`.

CRC32 behavior:

- Initial value is `0xFFFFFFFF`.
- Final value XORs with `0xFFFFFFFF`.
- Buffer processing is equivalent to byte-wise CRC32 over the input bytes, with
  little-endian chunk handling to preserve behavior on big-endian platforms.
- `CRC32_BlockSequence` clamps payload length to 60 bytes, uses the absolute
  value of negative sequence numbers, appends four bytes selected from the CRC
  table, and returns the low byte of the finalized CRC.

MD5 behavior:

- The implementation is the legacy MD5 transform in `public/crclib.c`.
- `MD5_Print` returns uppercase hex in a static buffer.

## Phase 39 Migration

The first safe migration target is `COM_HashKey`:

- The public C symbol is now emitted by
  `src/utilities/compat/crclib_hash.cpp`.
- The implementation delegates to `src/utilities/hash.cpp`.
- `public/crclib.c` still owns CRC32 and MD5 while tests pin their behavior.

CRC32 has a modern mirror in `src/utilities/checksum.cpp`, but the public CRC32
C symbols are not routed yet. They are broader, performance-sensitive, and
shared with engine/game exports, so their migration should be a separate small
step after the focused tests stay green.
