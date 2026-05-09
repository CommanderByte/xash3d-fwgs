# Hash And Checksum Migration Guide

## Ownership

Hash/checksum helpers straddle two layers:

- `public/crclib.h` is the legacy C ABI used by engine, renderers, filesystem,
  and shared SDK-style code.
- `src/utilities/` is the modern implementation home for subsystem-neutral C++
  helpers.

Do not move these helpers into `src/engine/`; they are not engine-only policy.

## Current Shape

```text
public/crclib.h
        |
        +-- COM_HashKey C symbol
        |       |
        |       +-- src/utilities/compat/crclib_hash.cpp
        |               |
        |               +-- src/utilities/hash.cpp
        |
        +-- CRC32 and MD5 C symbols
                |
                +-- public/crclib.c
```

`src/utilities/checksum.cpp` mirrors CRC32 behavior for modern callers and tests,
but the public CRC32 functions remain in `public/crclib.c` for now.

## Rules

- Keep `public/crclib.h` C-compatible.
- Keep C++ compatibility exports in `src/utilities/compat/` when a public C
  symbol delegates to modern C++.
- Do not change hash output, CRC output, MD5 output, or static-buffer behavior
  without a caller-specific migration plan.
- Prefer public tests for C ABI behavior and `tests/utilities` for modern helper
  behavior.
- Treat file-level hashing (`CRC32_File`, `MD5_HashFile`) as filesystem work,
  because it depends on VFS path resolution and file handles.

## Next Safe Steps

Good follow-up work:

- Replace public CRC32 symbols with compatibility exports only after confirming
  performance and static-link behavior across C-only public tests.
- Add file-hashing tests around `CRC32_File` and `MD5_HashFile` before any
  filesystem-level migration.
- Consider a table-backed modern CRC32 implementation if the C symbols are
  routed through `src/utilities/checksum.cpp`.
