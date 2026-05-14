# Engine Network Buffer TODO

## Purpose

Plan modernization of network/message buffer primitives after lower-risk
low-level helpers have been migrated.

This is powerful but risky: serialization compatibility is unforgiving, so it
must be test-heavy before any implementation replacement.

## Scope

Candidate files:

- `engine/common/net_buffer.c`
- `engine/common/net_buffer.h`
- message and bit-buffer read/write helpers used by net channels, delta
  encoding, server/client parsing, and protocol code.

## Method

- Treat wire format as compatibility-critical.
- Add binary-vector tests before implementation changes.
- Compare legacy and modern readers/writers on deterministic byte streams.
- Keep any modern types private until public ABI decisions are explicit.

## Phase 42 Tasks: Network Buffer Primitive

- [x] `ENG-NETBUF-001` Audit read/write helpers, overflow behavior, bit order,
  endian behavior, string handling, coordinate/angle encoding, and caller
  expectations.
  Evidence: `Documentation/codex/legacy/engine/network-buffer-baseline.md`.

- [x] `ENG-NETBUF-002` Add golden-vector tests for representative primitive
  reads/writes and overflow paths.
  Evidence: `tests/engine/network_buffer.cpp` pins `BitByte`, byte/bit golden
  writes, reads, signed round-trips, overflow quirks, and excise behavior.

- [x] `ENG-NETBUF-003` Add round-trip tests comparing legacy and modern
  behavior on deterministic streams.
  Evidence: `engine/common/net_buffer.c` keeps the legacy `xash_tests` vectors
  and adds `Test_Buffer_ModernExciseShadow` for deterministic modern/legacy
  excise comparison.

- [x] `ENG-NETBUF-004` Add a modern buffer primitive behind private engine
  headers, keeping C APIs unchanged.
  Evidence: `src/include/engine/network/network_buffer.hpp` and
  `src/engine/network/network_buffer.cpp`.

- [x] `ENG-NETBUF-005` Route one narrow helper group through the modern
  primitive only after golden-vector and round-trip tests pass.
  Evidence: `MSG_ExciseBits` now routes through
  `engine/common/network_buffer_adapter.cpp`; all other `MSG_Read*` and
  `MSG_Write*` wire-format helpers remain legacy-owned.

- [x] `ENG-NETBUF-006` Run focused tests, full tests, and multiplayer/protocol
  smoke where practical.
  Evidence: `.\waf.bat build --targets=test_engine_network_buffer`,
  `.\waf.bat build --targets=xash_tests`, and `.\waf.bat build --alltests`
  passed 53/53. Windows runtime smoke command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0, reached
  `Time to first frame: 0.519 seconds`, and stopped with reason `command`.
