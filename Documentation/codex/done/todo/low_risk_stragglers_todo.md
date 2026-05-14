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
| CRC32 table constants | `public/crclib.c` | `src/utilities/checksum.cpp` | Low | Done in Phase 46. Public CRC code now reads the modern shared table through a private C adapter. |
| CRC32 public wrappers | `public/crclib.c` | `xash::utilities::Crc32*` | Low-medium | Still possible later, but less urgent now that the table source is centralized. Keep ABI names and public tests. |
| MD5 file/hash helpers | `public/crclib.c` | `src/utilities/hash.cpp` only covers other hashes today | Medium | Defer until MD5 has dedicated modern tests and ownership doc. |
| `MD5_Print` hex formatting | `public/crclib.c` | none yet | Low-medium | Small, but should wait for an MD5 utility namespace decision so it does not create a one-function orphan. |
| `Q_atoi` quirks | `public/crtlib.c` | none yet | Medium | Good later CRT micro-phase because tests exist, but parsing quirks are compatibility-sensitive. |
| `COM_ParseFile` parser | `public/crtlib.c` | none yet | Medium-high | Defer to dedicated parser phase; broad behavior surface. |
| `Sys_GetCurrentUser` | `engine/common/system.c` | future platform helper | Medium | Good platform phase, but not this omnibus pass. |

## Phase 46 Tasks: Standalone Straggler Sweep

- [x] `ENG-STRAG-001` Audit current low-risk public/engine utility stragglers
  against existing modern helpers and tests.
  Evidence: this file,
  `Documentation/codex/modern/engine/standalone-stragglers-roadmap.md`.

- [x] `ENG-STRAG-002` Start with CRC32 table/constants by removing duplication
  or routing public CRC lookup through the existing modern checksum helper.
  Evidence: `src/include/utilities/checksum.hpp`,
  `src/utilities/checksum.cpp`,
  `src/include/utilities/compat/checksum_adapter.h`,
  `src/utilities/compat/checksum_adapter.cpp`, `public/crclib.c`,
  `public/wscript`.

- [x] `ENG-STRAG-003` Preserve public `CRC32_Init`, `CRC32_Final`,
  `CRC32_ProcessByte`, `CRC32_ProcessBuffer`, and `CRC32_BlockSequence`
  behavior.
  Evidence: public ABI declarations remain in `public/crclib.h`; public CRC
  implementations remain in `public/crclib.c` and now use
  `Xash_Crc32Table()`.

- [x] `ENG-STRAG-004` Add or confirm tests for CRC known vectors,
  byte-vs-buffer parity, empty buffer finalization, block sequence negative
  sequence handling, payload clamp, and sequence wraparound.
  Evidence: `public/tests/test_crclib.c`, `tests/utilities/hash.cpp`.

- [x] `ENG-STRAG-005` Run focused CRC tests, modern checksum/hash tests,
  `.\waf.bat build --alltests`, and a smoke test if public linkage changes
  the engine/runtime binaries.
  Evidence: `.\waf.bat build --targets=test_crclib,test_utilities_hash --alltests`
  passed 2/2 focused tests, `.\waf.bat build --alltests` passed 56/56 tests,
  and Windows runtime smoke copied `build\engine\xash.dll`,
  `build\filesystem\filesystem_stdio.dll`, and `build\ref\gl\ref_gl.dll` into
  `run-win32`, then ran `.\xash3d.exe -dev 2 -log +fs_path +quit` from
  `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 12:32:29 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Notes

The CRC32 table now lives behind `xash::utilities::Crc32Table()` and is shared
with public C code through `Xash_Crc32Table()`. The public `CRC32_*` functions
stay in `public/crclib.c`, so the public ABI remains unchanged while the table
source is no longer duplicated there.
