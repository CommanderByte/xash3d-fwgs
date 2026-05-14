# Engine CRT Micro-Seams TODO

## Purpose

Track Phase 47 work for narrow public CRT helper families that can move behind
modern utilities without changing `public/crtlib.h` as the C-compatible ABI.

## Candidate Audit

| Candidate | Existing tests | Risk | Phase 47 decision |
| --- | --- | --- | --- |
| `Q_atoi`, `Q_atof`, `Q_atov`, `Q_atoi_hex` | `public/tests/test_atoi.c` | Low-medium | Migrated. Quirks are explicit and focused. |
| `Q_strnlwr` | `public/tests/test_strings.c` | Low | Leave for a later string helper sweep. |
| `Q_memfgets` | `public/tests/test_strings.c` | Low-medium | Leave for a later memory/string reader sweep. |
| `Q_stricmpext`, `Q_strnicmpext`, wildcard matching | limited indirect coverage | Medium | Dedicated wildcard/pattern phase. |
| `COM_ParseFileSafe` | `public/tests/test_parsefile.c` | Medium-high | Dedicated parser phase. |
| `Q_vsnprintf`, `Q_snprintf` | no focused golden coverage here | Medium-high | Dedicated formatting/fatal-output phase. |

## Phase 47 Tasks: Public CRT Micro-Seams

- [x] `ENG-CRT-001` Audit `public/crtlib.c` and `public/crtlib.h` for narrow
  helper families with existing tests.
  Evidence: this file.

- [x] `ENG-CRT-002` Pick one parser or conversion helper family only after its
  golden behavior is explicit.
  Evidence: selected numeric conversion family:
  `Q_atoi_hex`, `Q_atoi`, `Q_atof`, and `Q_atov`.

- [x] `ENG-CRT-003` Keep public inline/header ABI stable while moving
  implementation behind modern utilities or compat bridges.
  Evidence: `src/include/utilities/conversion.hpp`,
  `src/utilities/conversion.cpp`,
  `src/utilities/compat/crtlib_conversion.cpp`, `public/crtlib.c`,
  `public/wscript`.

- [x] `ENG-CRT-004` Run focused public tests plus
  `.\waf.bat build --alltests`.
  Evidence: `.\waf.bat build --targets=test_atoi,test_utilities_conversion --alltests`
  passed 2/2 focused tests, `.\waf.bat build --alltests` passed 57/57 tests,
  and Windows runtime smoke copied `build\engine\xash.dll`,
  `build\filesystem\filesystem_stdio.dll`, and `build\ref\gl\ref_gl.dll` into
  `run-win32`, then ran `.\xash3d.exe -dev 2 -log +fs_path +quit` from
  `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 12:43:58 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Compatibility Notes

The migrated conversion helpers intentionally preserve legacy quirks:

- only literal space characters are stripped before parsing;
- a leading `+` is not accepted;
- hex prefixes support `0x` and `0X`;
- character literals return the second byte after `'`;
- decimal floats do not support exponent notation;
- repeated decimal points reset the decimal marker;
- `Q_atov` uses space as its only separator and keeps its existing behavior for
  repeated spaces.
