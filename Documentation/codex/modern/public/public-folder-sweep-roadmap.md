# Public Folder Sweep Roadmap

## Direction

`public/` currently mixes several categories:

- public C ABI headers and compatibility symbols;
- small project-owned utility implementations;
- generated build metadata;
- platform fallback code;
- vendored/drop-in code.

The modernization target is not an empty `public/` folder. The target is a
cleaner boundary:

```text
public/*.h                  stable C ABI and shared SDK-style declarations
public/*.c                  temporary compatibility exports or retained C-only code
src/utilities/*.cpp         reusable implementation
src/utilities/compat/*.cpp  public C symbol adapters
```

## First Slice: Remaining `crclib`

The remaining easy `crclib` island is MD5:

- `MD5Update`
- `MD5Final`
- `MD5Transform`
- `MD5_Print`

It has public tests for empty and `abc` hashes. The next implementation pass
should add modern MD5 tests first, then move implementation into
`src/utilities/md5.*` while keeping `MD5Context_t` and public C symbols stable.

Status: completed on 2026-05-10. `public/crclib.c` now owns only CRC bodies;
MD5 lives in `src/utilities/md5.cpp` with public symbols exported through
`src/utilities/compat/crclib_md5.cpp`.

## Second Slice: Remaining Low-Risk `crtlib`

The next easy `crtlib` helpers are:

- `Q_strnlwr`
- `Q_memfgets`

Both have public tests. They should gain modern tests for truncation, null
inputs, and offset behavior before moving behind a compat export.

Status: completed on 2026-05-10. `Q_strnlwr` and `Q_memfgets` now route through
`src/utilities/text.cpp`; `Q_memfgets` copies by bounded memory length instead
of relying on a null-terminated source line.

Avoid broad `COM_ParseFileSafe`, wildcard matching, and formatting in this
sweep. They are important, but each is behavior-heavy enough to deserve its own
phase.

## Small Public Files

Good candidates:

- `atlas.c`: tiny, project-owned, tested, and utility-shaped.
- `build.c`: split only pure date/build-number math first.

Needs tests before movement:

- `utflib.c`
- `dllhelpers.c`

Keep in public for now:

- `getopt.c`: platform fallback/drop-in code.
- `miniz.*`: vendored archive dependency.
- `matrixlib.c`, `xash3d_mathlib.c`, `swaplib.h`: broad math/data helpers
  with wider call surfaces.

## Validation Rule

Any `public` implementation move should run:

- focused public C tests for the moved ABI;
- new or existing modern utility tests;
- `.\waf.bat build --alltests`;
- Windows smoke if runtime binaries relink.
