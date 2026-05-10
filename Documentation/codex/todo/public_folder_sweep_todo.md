# Public Folder Relocation Sweep TODO

## Purpose

Plan an aggressive but controlled pass over `public/`, focusing on helper
implementations that can move into `src/utilities` while keeping public headers
and C ABI symbols stable.

This phase follows the patterns proven by:

- `COM_HashKey` through `src/utilities/hash.*`;
- CRC32 table access through `src/utilities/checksum.*`;
- `Q_atoi`, `Q_atof`, and `Q_atov` through `src/utilities/conversion.*`;
- path helpers through `src/utilities/path.*`.

## Rules

- Keep public headers in `public/` unless there is a deliberate ABI decision.
- Move implementation, not caller contracts.
- Prefer `src/utilities/compat/*.cpp` for public C symbols backed by modern C++.
- Add modern tests beside public C tests before changing behavior-heavy helpers.
- Do not move third-party/drop-in or platform fallback code just because it is
  small.

## Candidate Audit

| Candidate | Current home | Existing tests | Risk | Recommendation |
| --- | --- | --- | --- | --- |
| MD5 family: `MD5Update`, `MD5Final`, `MD5Transform`, `MD5_Print` | `public/crclib.c` | `public/tests/test_crclib.c` | Low-medium | First implementation slice. Move to `src/utilities/md5.*`; keep public symbols through compat export. |
| CRT text helpers: `Q_strnlwr`, `Q_memfgets` | `public/crtlib.c` | `public/tests/test_strings.c` | Low-medium | Good second slice; add modern tests for truncation/offset/null behavior. |
| Atlas packer | `public/atlas.c`, `public/atlas.h` | `public/tests/test_atlas.c` | Low | Good standalone slice; keep `atlas_t` and `Atlas_*` ABI, move allocation logic to `src/utilities/atlas.*`. |
| Build number calculation | `public/build.c` | `public/tests/test_build.c` | Low-medium | Move pure `Q_buildnum_iso` math first; keep `Q_buildnum` tied to generated `build_vcs.c`. |
| UTF helpers | `public/utflib.c`, `public/utflib.h` | none focused | Medium | Add tests first, then move to `src/utilities/unicode.*`. |
| DLL export helpers | `public/dllhelpers.c` | none focused | Low-medium | Could move after adding tiny tests, but it is less urgent. |
| `getopt.c` | `public/getopt.c` | none focused | Medium | Leave in public for now; it is a Win32 platform fallback and BSD-derived drop-in. |
| `miniz.*` | `public/miniz.*` | indirect archive tests | High | Do not move in this sweep; third-party-style vendored code. |
| `matrixlib.c`, `xash3d_mathlib.c`, `swaplib.h` | public math/swap area | partial/indirect | Medium-high | Dedicated math/platform-data phase if we touch them. |

## Phase 48 Tasks: Public Folder Relocation Sweep

- [x] `PUB-SWEEP-001` Audit public folder files and rank relocation
  candidates by ABI risk, test coverage, and dependency shape.
  Evidence: this file,
  `Documentation/codex/modern/public/public-folder-sweep-roadmap.md`.

- [x] `PUB-SWEEP-002` Move the remaining `crclib` MD5 family behind
  `src/utilities/md5.*` and a public C compatibility export.
  Evidence: `src/include/utilities/md5.hpp`, `src/utilities/md5.cpp`,
  `src/utilities/compat/crclib_md5.cpp`, `public/crclib.c`,
  `tests/utilities/md5.cpp`.

- [x] `PUB-SWEEP-003` Move low-risk `crtlib` text helpers such as
  `Q_strnlwr` and `Q_memfgets` after adding modern coverage.
  Evidence: `src/include/utilities/text.hpp`, `src/utilities/text.cpp`,
  `src/utilities/compat/crtlib_text.cpp`, `public/crtlib.c`,
  `public/tests/test_strings.c`, `tests/utilities/text.cpp`.

- [ ] `PUB-SWEEP-004` Move the atlas packer implementation behind
  `src/utilities/atlas.*` while preserving `atlas_t` and `Atlas_*`.
  Evidence:

- [ ] `PUB-SWEEP-005` Split pure build-number calculation from generated VCS
  build data, moving only the pure calculation first.
  Evidence:

- [ ] `PUB-SWEEP-006` Add focused UTF helper tests before any `utflib`
  migration.
  Evidence:

- [x] `PUB-SWEEP-007` Explicitly defer `getopt`, `miniz`, math, matrix, and
  swap helpers unless a later dedicated phase selects them.
  Evidence: this file and
  `Documentation/codex/modern/public/public-folder-sweep-roadmap.md`.

- [x] `PUB-SWEEP-008` Run focused public/modern tests plus
  `.\waf.bat build --alltests`, and smoke test when `public` relinks runtime
  binaries.
  Evidence: `.\waf.bat build --targets=test_crclib,test_utilities_md5 --alltests`,
  `.\waf.bat build --targets=test_strings,test_utilities_text --alltests`,
  `.\waf.bat build --alltests`, and a Windows `+fs_path +quit` smoke passed on
  2026-05-10.
