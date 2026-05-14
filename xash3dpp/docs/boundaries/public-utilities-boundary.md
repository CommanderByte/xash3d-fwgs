# Public Utilities Boundary Spec

## Responsibility

This module provides a portable, self-contained utility library used throughout
the engine and its plugins. It covers: string manipulation and file-path helpers
(`crtlib`), CRC32/MD5 hash routines (`crclib`), 3×4 and 4×4 matrix operations
(`matrixlib`), float vector math as macros and inline functions
(`xash3d_mathlib`), zlib-compatible compression (`miniz`), UTF-8/UTF-16
conversion and codepoint mapping (`utflib`), a strip-based 2-D texture atlas
packer (`atlas`), POSIX-compatible command-line argument parsing on Win32
(`getopt`), byte-order swapping for on-disk structs (`swaplib`), build number
and VCS metadata (`build`/`build_vcs`), and DLL-export validation helpers
(`dllhelpers`).

The module does **not** own memory pools, I/O, platform syscalls, or any engine
state.

---

## External ABI contracts

**Adapter required, not a design constraint.** The `enginefuncs_t` game DLL ABI
(`engine/eiface.h` lines 192–195) exposes four CRC32 function pointers:

```c
void    (*pfnCRC32_Init)( CRC32_t *pulCRC );
void    (*pfnCRC32_ProcessBuffer)( CRC32_t *pulCRC, const void *p, int len );
void    (*pfnCRC32_ProcessByte)( CRC32_t *pulCRC, unsigned char ch );
CRC32_t (*pfnCRC32_Final)( CRC32_t pulCRC );  // applies XOR 0xFFFFFFFF
```

The rewrite is free to implement CRC32 however it likes (SIMD, hardware
intrinsic, different source file). The only obligation is that when the engine
fills `enginefuncs_t`, those four slots are bound to callables that produce
IEEE 802.3 CRC32 results with the standard init/XOR masks. A one-line forwarding
shim is sufficient if the new implementation uses a different calling style.

Everything else in `public/` has **no direct game/client DLL ABI exposure**.

---

## Interface (what the rest of the engine calls)

| Component | Key symbols | Notes |
|---|---|---|
| `crtlib` | `Q_strncpy`, `Q_strncat`, `Q_strcmp/stricmp`, `Q_snprintf`, `Q_vsnprintf`, `COM_ParseFileSafe`, `COM_FileBase`, `COM_FileExtension`, `COM_DefaultExtension`, `COM_ReplaceExtension`, `COM_ExtractFilePath`, `COM_FileWithoutPath`, `COM_StripExtension`, `COM_PathSlashFix`, `Q_atoi`, `Q_atof`, `Q_atov`, `Q_timestamp`, `matchpattern_with_separator` | Custom CRT replacing libc string ops; `restrict` keyword stripped in C++ via macro |
| `crclib` | `CRC32_Init`, `CRC32_ProcessBuffer`, `CRC32_ProcessByte`, `CRC32_Final`, `CRC32_BlockSequence`, `MD5Init`, `MD5Update`, `MD5Final` | Four CRC32 functions bound into `enginefuncs_t`; a shim or direct export handles the ABI binding. MD5 is engine-internal. |
| `matrixlib` | `Matrix3x4_*`, `Matrix4x4_*` | Operates on `matrix3x4`/`matrix4x4` C arrays from `xash3d_types.h`; depends on `com_model.h` for bone data types |
| `xash3d_mathlib` | `DotProduct`, `CrossProduct`, `VectorNormalize`, `AngleVectors`, `VectorAngles`, `AngleQuaternion`, `QuaternionSlerp`, `R_ConcatTransforms`, plus ~60 macros | Entirely macros/inlines; no `.c` linkage |
| `miniz` | `mz_compress`, `mz_uncompress`, `tinfl_decompress`, `tdefl_compress`, ZIP read/write | Vendored single-file library; used by filesystem and network |
| `utflib` | `Q_DecodeUTF8`, `Q_DecodeUTF16`, `Q_EncodeUTF8`, `Q_UTF8Length`, `Q_UTF16ToUTF8`, `Q_UnicodeToCP1251`, `Q_UnicodeToCP1252` | Stateful decoder via `utfstate_t`; zero-init required |
| `atlas` | `Atlas_Init`, `Atlas_AllocBlock` | Strip-based packer; max atlas 1024×1024 (`ATLAS_MAX_SIZE`); output `x, y` offsets |
| `getopt` | `getopt` + `optarg/optind/opterr/optopt/optreset` | Win32-only shim (no-op guard on non-Windows) |
| `swaplib` | `SwapStruct`, `swap_struct_def_t` | Reflection-driven big/little endian swap; tested via `test_swapstruct.c` |
| `build` | `Q_buildnum`, `Q_buildnum_iso`, `Q_buildnum_compat`, `g_buildcommit`, `g_buildbranch`, `g_buildcommit_date` | `build_vcs.c` supplies VCS strings at link time; `build.c` computes day-offset from `g_buildcommit_date` |
| `dllhelpers` | `ClearExports`, `ValidateExports`, `dllfunc_t` | Used by all plugin loaders to resolve export tables |

---

## Dependencies (what this module calls)

- **`common/xash3d_types.h`** — `byte`, `vec_t`, `vec2_t`…`matrix4x4`, `qboolean`, `poolhandle_t`; required by every component.
- **`common/build.h`** — compiler feature macros (`RETURNS_NONNULL`, `FORMAT_CHECK`, `MAYBE_ALIGNED`, `XASH_WIN32`, etc.).
- **`common/port.h`** — used by `matrixlib.c` for platform alignment.
- **`common/com_model.h`** — used by `matrixlib.c` for bone/attachment data types (`mstudio*`).
- **C standard library** — `<string.h>`, `<stdarg.h>`, `<math.h>`, `<ctype.h>`, `<stdio.h>` (for `sscanf` in `build.c`).
- **No engine subsystems** — no filesystem, no console, no memory pools.

---

## Owned state

| Symbol | Location | Notes |
|---|---|---|
| `g_buildcommit`, `g_buildbranch`, `g_buildcommit_date` | `build_vcs.c` (generated) | Static strings; populated at link time from VCS |
| `static int b` | `build.c :: Q_buildnum()` | One-time cached build number; not thread-safe but called before threads start |
| `opterr`, `optind`, `optopt`, `optreset`, `optarg` | `getopt.c` | POSIX global state; not thread-safe; Win32-only |

No heap allocations; no global mutable state beyond the three items above.

---

## Quirks and invariants

- **`Q_strncpy` null-terminates.** Always writes a `'\0'` at `dst[size-1]`. Callers rely on this; `strncpy` (which does not guarantee termination) must never be silently substituted.
- **`Q_strlen(NULL) == 0`.** The macro short-circuits on NULL. This is load-bearing in many callers.
- **`Q_floor`/`Q_ceil` cast through `int`.** Deliberately truncating; not equivalent to `floorf`/`ceilf` for negative numbers. Used for world grid snapping; do not replace with standard versions without auditing every caller.
- **`PFILE_*` flags in `COM_ParseFileSafe`.** Seven flag bits control tokeniser behaviour (bracket handling, colon as token, hash as comment, quoted tokens, single quote, comma, newline). All combinations are exercised in map/entity/config parsing; the behaviour must remain byte-identical.
- **`matrixlib` depends on `com_model.h`.** `Matrix3x4_ConcatTransforms` and bone-related helpers reference `mstudiobone_t` and similar SDK structs. This is the only SDK-type dependency inside `public/`.
- **CRC32 output must be IEEE 802.3 compatible.** `CRC32_INIT_VALUE = 0xFFFFFFFF`, XOR `0xFFFFFFFF`. The `enginefuncs_t` slots are function pointers — the rewrite binds them to whatever implementation it chooses (including hardware-accelerated CRC32); no legacy source file needs to be preserved.
- **`utfstate_t` must be zero-initialised** before the first call to `Q_DecodeUTF8`/`Q_DecodeUTF16`. Callers that reuse the struct between codepoints without reset will silently corrupt output.
- **`ATLAS_MAX_SIZE = 1024`.** Hardcoded in `atlas_t.allocated[1024]`. Increasing it is a struct-layout break; decreasing it silently corrupts existing allocations.
- **`miniz` is vendored at version 3.0.0.** The build must not pull in a system zlib or a different miniz version alongside it; symbol collisions are possible.
- **`Q_buildnum_compat()` always returns 4529.** This is intentional; it represents the frozen legacy build number that some mods may test against.
- **`restrict` is erased in C++** via `#define restrict` in `crtlib.h`. This means the C++ rewrite cannot rely on restrict-based optimisations in these functions.

---

## Open questions

1. **`matrixlib` → `com_model.h` dependency.** Should the rewrite's matrix library take bone/attachment types as opaque spans, or keep the GoldSrc struct dependency? Eliminating it would make `public/` fully independent of `common/`.
2. **CRC32 binding strategy.** The `enginefuncs_t` slots are filled by the server subsystem, not by `public/`. Decide whether to (a) expose the new CRC32 functions directly at the right signature, (b) use inline lambdas/shims at the fill site, or (c) keep a thin `crclib`-compatible façade. All are valid; pick whichever keeps the utilities module cleanest.
3. **`utflib` scope.** The current API converts to CP1251/CP1252 for legacy console codepage support. Should the rewrite retain those conversion tables or treat all I/O as UTF-8 at the boundary and only convert at display time?
4. **`miniz` vendor strategy.** Should `xash3dpp/3rdparty/` carry its own miniz copy, or depend on a system zlib with a miniz compatibility shim? Either way, the ZIP-read API surface used by filesystem must be decided before the filesystem module is implemented.
5. **`getopt` necessity.** The rewrite will target C++17; `std::span` + a small argument parser might replace `getopt` entirely. Confirm whether any external tool or game DLL calls `getopt` directly (unlikely, but should be verified).
6. **Thread safety of `Q_timestamp`.** It uses `localtime` internally via a static buffer in some implementations. If the rewrite targets a multi-threaded host, this must be replaced with `localtime_r`/`localtime_s`.
