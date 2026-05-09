# Engine String And Path Utility TODO

## Purpose

Plan modernization of shared string and path helpers. This area is useful but
quirk-heavy, so it follows info strings and hashes.

## Scope

Candidate areas:

- case-sensitive and case-insensitive compare helpers;
- token-safe copy/concat helpers;
- file basename, extension, and path normalization helpers;
- slash normalization and platform path quirks;
- helpers currently used by filesystem, renderer, model loading, and config
  parsing.

## Method

- Inventory behavior before moving anything.
- Preserve existing compatibility quirks unless a task explicitly changes them.
- Prefer small helper-by-helper migrations rather than one large utility
  rewrite.

## Phase 40 Tasks: String And Path Utilities

- [x] `ENG-STRPATH-001` Audit shared string/path helpers and rank them by
  caller breadth, compatibility risk, and testability.
  Evidence: `Documentation/codex/legacy/engine/string-path-baseline.md`
  documents `public/crtlib.*`, path helper callers, legacy quirks, and the
  deferred parser/formatting/string-copy surfaces.

- [x] `ENG-STRPATH-002` Add focused tests for path extension, basename,
  slash-normalization, case comparison, and bounded copy/concat behavior.
  Evidence: existing public tests `test_filebase`, `test_fileext`, `test_efp`,
  and `test_strings` were retained; new `public/tests/test_path.c` covers the
  routed C path API; `tests/utilities/path.cpp` covers the modern helper API.

- [x] `ENG-STRPATH-003` Decide which helpers belong in `src/engine`, which
  belong in `src/utilities`, and which must remain public C utilities.
  Evidence: `Documentation/codex/modern/engine/string-path-migration-guide.md`
  records `src/utilities` as the implementation home, `public/crtlib.h` as the
  C ABI, and parser/formatting/string-copy helpers as deferred narrower passes.

- [x] `ENG-STRPATH-004` Add modern helper implementations only after tests pin
  legacy edge cases.
  Evidence: `src/include/utilities/path.hpp` and `src/utilities/path.cpp`.

- [x] `ENG-STRPATH-005` Replace one narrow helper group and verify with focused
  tests, full tests, and a runtime smoke if filesystem or config parsing is
  touched.
  Evidence: `src/utilities/compat/crtlib_path.cpp` now exports the public C path
  symbols and delegates to `src/utilities/path.cpp`; the old non-inline path
  implementations were removed from `public/crtlib.c`. Focused tests passed:
  `.\waf.bat build --targets=test_path`,
  `.\waf.bat build --targets=test_utilities_path`, and
  `.\waf.bat build --targets=test_filebase,test_fileext,test_efp,test_strings`.
  `.\waf.bat build --alltests` passed 51/51. Runtime smoke copied rebuilt
  `xash3d.exe`, `xash.dll`, `filesystem_stdio.dll`, and `ref_gl.dll` to
  `run-win32`, reached `Time to first frame: 0.576 seconds`, and stopped with
  reason `command`.
