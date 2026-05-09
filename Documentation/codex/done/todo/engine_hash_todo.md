# Engine Hash And Checksum TODO

## Purpose

Plan modernization of small hash/checksum helpers such as `COM_HashKey` in
`public/crclib.*`. This is a low-blast-radius candidate for replacing a small
implementation directly after tests prove compatibility.

## Scope

Candidate helpers:

- `COM_HashKey`
- CRC/checksum helpers in `public/crclib.c`
- any narrow helper used by registries, texture hashes, sound hashes, or lookup
  tables.

## Method

- Keep public C function signatures unchanged.
- Prefer direct implementation replacement when behavior is fully pinned.
- Avoid changing hash output unless a caller-specific migration plan exists.

## Phase 39 Tasks: Hash And Checksum Helpers

- [x] `ENG-HASH-001` Audit hash/checksum helpers, callers, hash-size
  assumptions, and compatibility-sensitive outputs.
  Evidence: `Documentation/codex/legacy/engine/hash-checksum-baseline.md`
  documents `public/crclib.*`, `COM_HashKey` callers, CRC32/MD5 behavior, and
  filesystem file-hashing boundaries.

- [x] `ENG-HASH-002` Add tests for `COM_HashKey` case folding, power-of-two
  sizing assumptions, empty strings, mixed punctuation, and known output
  vectors.
  Evidence: `public/tests/test_crclib.c` exercises the public C symbol;
  `tests/utilities/hash.cpp` exercises the modern C++ utility.

- [x] `ENG-HASH-003` Add tests for selected CRC/checksum helpers before
  touching implementation.
  Evidence: `public/tests/test_crclib.c` covers CRC32 buffer/byte paths,
  `CRC32_BlockSequence`, and MD5 known vectors; `tests/utilities/hash.cpp`
  covers the modern CRC32 mirror.

- [x] `ENG-HASH-004` Add modern implementation helpers under `src/engine` or
  `src/utilities` only if they remain private to existing C APIs.
  Evidence: `src/include/utilities/hash.hpp`, `src/utilities/hash.cpp`,
  `src/include/utilities/checksum.hpp`, and `src/utilities/checksum.cpp`.

- [x] `ENG-HASH-005` Replace the implementation behind the existing C symbols
  when focused tests and `.\waf.bat build --alltests` pass.
  Evidence: `COM_HashKey` is now exported from
  `src/utilities/compat/crclib_hash.cpp` and delegates to
  `xash::utilities::LegacyHashKey`; `public/crclib.c` keeps CRC32/MD5 while
  their behavior is pinned. Focused tests passed:
  `.\waf.bat build --targets=test_crclib`,
  `.\waf.bat build --targets=test_utilities_hash`, and
  `.\waf.bat build --targets=test_engine_base_command_registry`.
  `.\waf.bat build --alltests` passed 49/49. Runtime smoke copied rebuilt
  `xash3d.exe`, `xash.dll`, `filesystem_stdio.dll`, and `ref_gl.dll` to
  `run-win32`, reached `Time to first frame: 0.589 seconds`, and stopped with
  reason `command`.
