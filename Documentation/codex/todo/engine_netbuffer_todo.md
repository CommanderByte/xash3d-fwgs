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

## Phase 43 Tasks: Network Buffer Primitive

- [ ] `ENG-NETBUF-001` Audit read/write helpers, overflow behavior, bit order,
  endian behavior, string handling, coordinate/angle encoding, and caller
  expectations.
  Evidence:

- [ ] `ENG-NETBUF-002` Add golden-vector tests for representative primitive
  reads/writes and overflow paths.
  Evidence:

- [ ] `ENG-NETBUF-003` Add round-trip tests comparing legacy and modern
  behavior on deterministic streams.
  Evidence:

- [ ] `ENG-NETBUF-004` Add a modern buffer primitive behind private engine
  headers, keeping C APIs unchanged.
  Evidence:

- [ ] `ENG-NETBUF-005` Route one narrow helper group through the modern
  primitive only after golden-vector and round-trip tests pass.
  Evidence:

- [ ] `ENG-NETBUF-006` Run focused tests, full tests, and multiplayer/protocol
  smoke where practical.
  Evidence:
