# Engine Info String TODO

## Purpose

Plan a low-level rewrite of `engine/common/infostring.c` while keeping the
legacy `Info_*` C API stable. This is the preferred first "rewrite low, then
backpropagate upward" candidate because it is string-heavy, widely used, and
testable without full engine startup.

## Scope

Legacy surface:

- `Info_ValueForKey`
- `Info_RemovePrefixedKeys`
- `Info_RemoveKey`
- `Info_SetValueForKey`
- `Info_SetValueForKeyf`
- `Info_SetValueForStarKey`
- `Info_IsValid`
- `Info_WriteVars`
- `Info_Print`

Primary callers include cvar userinfo/serverinfo propagation, master server
queries, and console/config helpers.

## Method

- Capture behavior first.
- Add standalone tests under `tests/engine`.
- Build a modern implementation under `src/engine`.
- Run legacy and modern behavior in parallel where useful.
- Replace implementation behind the same C API only when tests are green.

## Phase 38 Tasks: Info String Rewrite

- [x] `ENG-INFO-001` Audit `infostring.c` format rules, invalid characters,
  max-size behavior, important-key behavior, star-key handling, and largest-key
  removal policy.
  Evidence: `Documentation/codex/legacy/engine/info-string-baseline.md`
  documents format rules, invalid inputs, overflow behavior, important-key
  eviction, and removal quirks.

- [x] `ENG-INFO-002` Add standalone tests for lookup, set, remove, prefix
  removal, invalid strings, invalid keys/values, max-size overflow, and
  important-key preservation.
  Evidence: `tests/engine/info_string.cpp` and
  `.\waf.bat build --targets=test_engine_info_string`.

- [x] `ENG-INFO-003` Add a modern info-string implementation under
  `src/engine/` with no public C++ types exposed through `common.h`.
  Evidence: `src/include/engine/info_string.hpp` and
  `src/engine/info_string.cpp`; `common.h` still exposes only the legacy
  `Info_*` C declarations.

- [x] `ENG-INFO-004` Add parallel/shadow tests comparing legacy and modern
  behavior across deterministic mutation sequences.
  Evidence: `tests/engine/info_string.cpp` includes deterministic modern
  mutation coverage, and `Test_InfoStrings` in `engine/common/common.c`
  exercises the routed legacy C API with matching cases.

- [x] `ENG-INFO-005` Route the legacy `Info_*` functions through the modern
  implementation while preserving signatures and return semantics.
  Evidence: `engine/common/infostring.c` was replaced by
  `engine/common/infostring.cpp`, which exports the same C symbols and delegates
  to `src/engine/info_string.cpp`.

- [x] `ENG-INFO-006` Run focused tests, `.\waf.bat build --alltests`, and a
  Windows runtime smoke because userinfo/serverinfo is launch-sensitive.
  Evidence: `.\waf.bat build --targets=test_engine_info_string` passed;
  `.\waf.bat build --targets=xash_tests` passed; `.\waf.bat build --alltests`
  passed 47/47; copied `build\src\xash3d.exe`, `build\engine\xash.dll`, and
  `build\filesystem\filesystem_stdio.dll` to `run-win32` and ran
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` with the Half-Life rodir. The
  runtime smoke exited cleanly, reached `Time to first frame: 0.565 seconds`,
  and stopped with reason `command`.
