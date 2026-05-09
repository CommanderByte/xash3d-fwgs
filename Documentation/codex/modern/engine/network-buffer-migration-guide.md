# Network Buffer Migration Guide

## Shape

Modern network-buffer internals live in:

- `src/include/engine/network/network_buffer.hpp`
- `src/engine/network/network_buffer.cpp`

The legacy C surface remains in `engine/common/net_buffer.h` and
`engine/common/net_buffer.c`. The bridge for live routing is private to the
engine:

```text
engine/common/net_buffer.c
        |
engine/common/network_buffer_adapter.cpp
        |
src/engine/network/network_buffer.cpp
```

## Rules For Future Changes

- Treat every `MSG_*` byte as wire-format compatibility, not implementation
  detail.
- Keep the modern primitive private to the engine until protocol ABI decisions
  are explicit.
- Add golden-vector tests before routing any new read/write helper.
- Preserve overflow, cursor, padded-byte, and alternate-sign quirks exactly
  unless a protocol compatibility phase intentionally changes them.
- Keep diagnostics and `net_send_debug`/`net_recv_debug` output in the legacy
  command/console layer until logging ownership is migrated.
- Prefer routing one helper family at a time.

## Current Routing Slice

Phase 42 routes `MSG_ExciseBits` through the modern primitive. The rest of the
`MSG_*` read/write functions remain legacy-owned after this phase.

This slice is intentionally modest: `MSG_ExciseBits` is used by netchan fragment
processing, has existing legacy tests, and exercises bit copying without taking
over the entire serialization surface.

## Later Candidates

Good follow-up slices:

- unsigned bit reads/writes after wider golden-vector coverage;
- signed bit reads/writes, including alternate-sign GoldSrc mode;
- byte primitive wrappers after endian coverage is explicit;
- string helpers after format-string translation behavior has standalone tests;
- coordinate/angle helpers after `ENGINE_WRITE_LARGE_COORD` policy is isolated.
