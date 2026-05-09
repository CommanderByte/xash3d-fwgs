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

- [ ] `ENG-HASH-001` Audit hash/checksum helpers, callers, hash-size
  assumptions, and compatibility-sensitive outputs.
  Evidence:

- [ ] `ENG-HASH-002` Add tests for `COM_HashKey` case folding, power-of-two
  sizing assumptions, empty strings, mixed punctuation, and known output
  vectors.
  Evidence:

- [ ] `ENG-HASH-003` Add tests for selected CRC/checksum helpers before
  touching implementation.
  Evidence:

- [ ] `ENG-HASH-004` Add modern implementation helpers under `src/engine` or
  `src/utilities` only if they remain private to existing C APIs.
  Evidence:

- [ ] `ENG-HASH-005` Replace the implementation behind the existing C symbols
  when focused tests and `.\waf.bat build --alltests` pass.
  Evidence:
