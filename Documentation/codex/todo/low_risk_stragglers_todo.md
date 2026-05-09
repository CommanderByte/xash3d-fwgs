# Low-Risk Standalone Stragglers TODO

## Purpose

Keep a small, deliberate cleanup phase at the top of the roadmap for isolated
utility seams that already have tests and modern equivalents. This is meant to
catch easy wins without turning the next phase into a dependency fight.

## Selection Rules

A candidate belongs here only if it is:

- standalone or nearly standalone;
- already covered by public or modern unit tests, or easy to cover with golden
  vectors;
- not tied to shutdown, allocation ownership, renderer state, networking state,
  or platform validation;
- compatible with the existing public C ABI.

If a candidate needs broad engine ownership decisions, move it to a dedicated
phase instead.

## Candidate Scan

| Candidate | Current home | Modern hook | Risk | Recommendation |
| --- | --- | --- | --- | --- |
| CRC32 table constants | `public/crclib.c` | `src/utilities/checksum.cpp` | Low | Next phase. Remove table duplication or route table lookup through a compat helper while preserving public `CRC32_*`. |
| CRC32 public wrappers | `public/crclib.c` | `xash::utilities::Crc32*` | Low-medium | Consider after constants; keep ABI names and public tests. |
| MD5 file/hash helpers | `public/crclib.c` | `src/utilities/hash.cpp` only covers other hashes today | Medium | Defer until MD5 has dedicated modern tests and ownership doc. |
| `Q_atoi` quirks | `public/crtlib.c` | none yet | Medium | Good later CRT micro-phase because tests exist, but parsing quirks are compatibility-sensitive. |
| `COM_ParseFile` parser | `public/crtlib.c` | none yet | Medium-high | Defer to dedicated parser phase; broad behavior surface. |
| `Sys_GetCurrentUser` | `engine/common/system.c` | future platform helper | Medium | Good platform phase, but not this omnibus pass. |

## Phase 46 Tasks: Standalone Straggler Sweep

- [ ] `ENG-STRAG-001` Audit current low-risk public/engine utility stragglers
  against existing modern helpers and tests.
  Evidence: this file,
  `Documentation/codex/modern/engine/standalone-stragglers-roadmap.md`.

- [ ] `ENG-STRAG-002` Start with CRC32 table/constants by removing duplication
  or routing public CRC lookup through the existing modern checksum helper.
  Evidence:

- [ ] `ENG-STRAG-003` Preserve public `CRC32_Init`, `CRC32_Final`,
  `CRC32_ProcessByte`, `CRC32_ProcessBuffer`, and `CRC32_BlockSequence`
  behavior.
  Evidence:

- [ ] `ENG-STRAG-004` Add or confirm tests for CRC known vectors,
  byte-vs-buffer parity, empty buffer finalization, block sequence negative
  sequence handling, payload clamp, and sequence wraparound.
  Evidence:

- [ ] `ENG-STRAG-005` Run focused CRC tests, modern checksum/hash tests,
  `.\waf.bat build --alltests`, and a smoke test if public linkage changes
  the engine/runtime binaries.
  Evidence:

## Notes

The CRC32 table currently lives as a static 256-entry table in
`public/crclib.c`, while modern code already derives entries with
`xash::utilities::Crc32TableEntry`. The safest first implementation is to
preserve the public C ABI and only change the internal source of the table
values.
