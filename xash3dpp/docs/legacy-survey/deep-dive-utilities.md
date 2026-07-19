# Deep Dive: Legacy `public/` Utility Library — Structs, Constants, Algorithms & Quirks

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the portable, dependency-free utility
library at the repository root (`public/`, with two ports pulled from
`common/` and `filesystem/`) that the `xash3dpp` `utilities` subsystem replaces.
Narrow-and-exact companion to the wide `public-common-sdk.md` summary. Line
numbers are against the working tree on that date; behaviour references, not
design constraints. Everything here is **legacy** — read for behaviour and the
hash/wire-format contracts, not as a rewrite blueprint.*

Primary sources:

- `public/crtlib.h` + `public/crtlib.c` — string/path CRT mirror, tokenizer
- `public/crclib.h` + `public/crclib.c` — CRC32, MD5, `CRC32_BlockSequence`
- `public/matrixlib.c` — 3×4 / 4×4 matrix math (bone/attachment dependent)
- `public/xash3d_mathlib.h` + `.c` — vector macros, quaternion + studio bone math
- `public/atlas.h` + `public/atlas.c` — strip-based 2-D atlas packer
- `public/swaplib.h` — reflection-driven struct byte-swap (header-only)
- `public/utflib.c` — UTF-8/UTF-16 decode + CP1251/CP1252 tables
- `public/build.c` + `public/build.h` (+ generated `build_vcs.c`) — build number
- `public/getopt.h` + `public/getopt.c` — Win32 POSIX `getopt` shim
- `public/dllhelpers.c` — export-table clear/validate
- `public/miniz.c` — vendored zlib/deflate/ZIP (built standalone)
- `filesystem/filesystem.c` — `FS_ParseGameInfo`/`FS_WriteGameInfo` (gameinfo text)

**Global assumptions:** all on-disk integers/floats are **little-endian**;
byte-swapping happens only under `#if XASH_BIG_ENDIAN`. No `#pragma pack` on any
struct in this library. `vec_t` is `float`; `vec3_t` is `float[3]`
(`common/xash3d_types.h`).

---

## 1. `crtlib` — string / path CRT mirror

### 1.1 Tokenizer flags (`crtlib.h:47-66`)

`COM_ParseFileSafe` is driven by seven flag bits; the port mirrors them 1:1 as
`utilities::TokenFlags`:

```c
#define PFILE_NO_BRACKETS_AS_TOKEN     ( 1U << 0 )  // {,},(,) not standalone tokens
#define PFILE_COLON_AS_TOKEN           ( 1U << 1 )  // ':' is its own token
#define PFILE_HASH_AS_COMMENT          ( 1U << 2 )  // '#' begins a line comment
#define PFILE_NO_QUOTED_TOKENS         ( 1U << 3 )  // '"' has no special meaning
#define PFILE_NO_SINGLE_QUOTE_AS_TOKEN ( 1U << 4 )
#define PFILE_NO_COMMA_AS_TOKEN        ( 1U << 5 )
#define PFILE_NEWLINE_AS_TOKEN         ( 1U << 6 )  // '\n' returned as a token
```

All combinations are exercised across map / entity / config / KV parsing; the
tokenizer behaviour must remain byte-identical.

### 1.2 Load-bearing string quirks

- **`Q_strncpy` always null-terminates** — writes `'\0'` at `dst[size-1]`.
  Never substitutable by libc `strncpy` (which does not).
- **`Q_strlen(NULL) == 0`** — the macro short-circuits on NULL; many callers
  rely on it.
- **`#define restrict` erases the keyword in C++** (`crtlib.h:69`) — the port
  cannot rely on restrict-based optimisation. `Q_memor` (`crtlib.h:107`) uses
  `XASH_RESTRICT`.
- **`Q_floor`/`Q_ceil` cast through `int`** — deliberately truncating; not
  equivalent to `floorf`/`ceilf` for negatives (world-grid snapping).

### 1.3 `dllfunc_t` (`crtlib.h:142`)

```c
typedef struct { const char *name; void **func; } dllfunc_t;
void     ClearExports( const dllfunc_t *funcs, size_t num_funcs );
qboolean ValidateExports( const dllfunc_t *funcs, size_t num_funcs );
```

`void **func` is intentional type erasure — each slot points at a
function-pointer variable filled after `LoadLibrary`/`dlopen`.

---

## 2. `crclib` — CRC32 + MD5

### 2.1 CRC32 constants (`crclib.h:27-40`)

```c
#define CRC32_INIT_VALUE 0xFFFFFFFFUL
#define CRC32_XOR_VALUE  0xFFFFFFFFUL   // IEEE 802.3
CRC32_Init(p):  *p = 0xFFFFFFFF
CRC32_Final(c): return c ^ 0xFFFFFFFF
```

`crc32table[256]` is the standard reflected IEEE 802.3 table. `CRC32_Init`,
`CRC32_ProcessBuffer`, `CRC32_ProcessByte`, `CRC32_Final` are the four functions
bound into `enginefuncs_t` (game-DLL ABI) as function pointers.

### 2.2 `CRC32_BlockSequence` (`crclib.c:148-172`) — quirk-heavy

Sequence-keyed short hash used "for proxy protecting" (demo/resource integrity):

```c
byte CRC32_BlockSequence( byte *base, int length, int sequence )
{
    char buffer[64]; uint32_t le[2]; int off;
    if( sequence < 0 ) sequence = abs( sequence );
    if( length > 60 )  length = 60;            // hard clamp
    memcpy( buffer, base, length );
    off = sequence % 0x3FC;                     // 0x3FC = 1020
    le[0] = LittleLong( crc32table[off / 4] );
    le[1] = LittleLong( crc32table[off / 4 + 1] );
    memcpy( buffer + length, (char *)le + ( off & 3 ), 4 );  // 4 sequence-keyed bytes
    length += 4;
    CRC32_Init(&CRC); CRC32_ProcessBuffer(&CRC, buffer, length); CRC = CRC32_Final(CRC);
    return (byte)CRC;                           // truncated to one byte
}
```

Quirks that must be preserved byte-for-byte if this is reimplemented:
`length > 60` clamp; `abs(sequence)`; `off = sequence % 1020`; the four
sequence-keyed table bytes appended after the payload with a `& 3` sub-word
offset; **the result is the low byte of the final CRC**.

### 2.3 MD5 (`crclib.h:22-26`, engine-internal)

```c
typedef struct { uint buf[4]; uint bits[2]; uint in[16]; } MD5Context_t;
// init: buf = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}; bits = {0,0}
```

Note the legacy context stores `in` as **`uint[16]`** (64 bytes as 16 words);
`MD5SwapBlock` byte-swaps them under big-endian before `MD5Transform`. MD5 is
never exposed to game DLLs.

---

## 3. `matrixlib` + `xash3d_mathlib` — matrix, vector, studio bone math

- **`matrixlib.c`** implements `Matrix3x4_*` / `Matrix4x4_*` over `matrix3x4` /
  `matrix4x4` C arrays. `Matrix3x4_ConcatTransforms` and the bone helpers
  reference `mstudiobone_t` etc. — the **only** `com_model.h` dependency inside
  `public/`.
- **`xash3d_mathlib.h`** is ~60 macros (`DotProduct`, `CrossProduct`,
  `VectorNormalize`, `VectorMA`, …) plus inline functions; no `.c` linkage for
  the macros. `xash3d_mathlib.c` carries the heavier routines: `AngleVectors`,
  `VectorAngles`, `AngleQuaternion`, `QuaternionSlerp` /
  `QuaternionSlerpNoAlign` / `QuaternionAlign`, `R_StudioSlerpBones`,
  `R_ConcatTransforms`, `SinCos`.
- **Bit-parity is load-bearing.** The studio bone path runs `double`-precision
  trig (`SinCos`, `acos`, `atan2`, `sqrt`) with float narrowing on store; the
  arithmetic ordering must not be "simplified" or bone animation drifts from
  GoldSrc. The `AngleQuaternion` studio branch takes Euler angles in **radians**
  with the half-angle `ROLL→sy / YAW→sp / PITCH→sr` variable mapping.
- **`QuaternionSlerpNoAlign`** has three branches: `acos`/`sin` interior,
  linear `(1-t, t)` near-parallel, and an antipodal fallback (perpendicular
  quaternion + the `i<3` partial blend). `t` is **not** clamped inside slerp
  (caller's contract).

---

## 4. `atlas` — strip-based 2-D packer

### 4.1 Struct + constant (`atlas.h:20-30`)

```c
#define ATLAS_MAX_SIZE 1024
typedef struct atlas_s { int allocated[ATLAS_MAX_SIZE]; int size; int max_height; } atlas_t;
```

`ATLAS_MAX_SIZE` is **ABI-structural** — it fixes `allocated[1024]`. Increasing
it is a struct-layout break; decreasing corrupts existing allocations. The port
pins it to `limits::atlas_max_size`.

### 4.2 Algorithm (`atlas.c:17-63`)

`Atlas_AllocBlock(w,h)` scans columns for the lowest strip that fits `w`
contiguous columns without exceeding the running `best` height; on success it
raises every one of the `w` columns to `best + h` and returns `(x, y=best)`.
Returns `false` when `best + h > size`. `max_height` tracks the tallest column.
`Atlas_Init` zeroes `allocated` and sets `size`.

---

## 5. `swaplib` — reflection-driven struct swap (header-only)

### 5.1 Descriptor (`swaplib.h:21-29`)

```c
typedef struct swap_struct_def_s {
    uint32_t offset;   // field byte offset
    int32_t  size;     // 2/4/8; NEGATIVE => sub-struct (subdef, |size| entries)
    struct swap_struct_def_s *subdef;
    uint32_t count;    // array element count (0 => single field)
    uint32_t stride;   // element stride
} swap_struct_def_t;
```

- **`size < 0` is the recursion sentinel**: `subdef` points at a nested table of
  `-size` entries, applied `count` (or 1) times at `offset + j*stride`.
- `swap_field_` byte-reverses widths 2/4/8 via explicit pairwise swaps
  (`switch(size)`; `default` is a no-op — arbitrary widths are silently
  skipped).
- Macros `swap_struct` / `swap_array` swap **unconditionally**; callers are told
  to prefer the `le_`/`be_` conditional macros. Comment flags a strict-aliasing
  break in `swap_struct_`.

---

## 6. `build` — build number

`Q_buildnum_iso(date)` (`build.c:20-42`) parses `"YYYY-MM-DD"`, converts to a
day count from year-1900 using the `mond[12]` days-per-month table
(`{31,28,31,30,...}`), adds a leap day when `year%4==0 && month>1`, then subtracts
**41728** (the day index of Apr 1 2015) — so the build number is *days since
2015-04-01*, or `-1` on parse failure. `Q_buildnum()` (`build.c:52-63`) caches
the result in a `static int b`. `build.h` also exposes `Q_buildnum_compat()`
which **always returns 4529** (frozen legacy build some mods test against). VCS
strings (`g_buildcommit`, `g_buildbranch`, `g_buildcommit_date`) come from the
generated `build_vcs.c`.

---

## 7. `utflib`, `getopt`, `dllhelpers`, `miniz`

- **`utflib.c`** — stateful UTF-8/UTF-16 decode via `utfstate_t` (**must be
  zero-initialised** before first `Q_DecodeUTF8`/`Q_DecodeUTF16`; reuse without
  reset corrupts output). Carries CP1251/CP1252 conversion tables for legacy
  console codepage support (`Q_UnicodeToCP1251`/`CP1252`).
- **`getopt.c/.h`** — Win32 POSIX `getopt` shim with the standard mutable
  globals `optarg`/`optind`/`opterr`/`optopt`/`optreset`; a no-op guard on
  non-Windows. Not thread-safe.
- **`dllhelpers.c`** — `ClearExports`/`ValidateExports` over a `dllfunc_t[]`.
- **`miniz.c`** — vendored single-file zlib/deflate + ZIP reader/writer; used by
  the filesystem archive backends and networking compression, not by the rest
  of `public/`.

---

## 8. As-built mapping (legacy → `xash3dpp/utilities`)

| Legacy piece | `xash3dpp` home | Notes / status |
|---|---|---|
| `crtlib` strings (`Q_strncpy`, `Q_snprintf`, `Q_atoi/atof/atov`, `COM_StripColors`, `COM_ParseFileSafe`, `matchpattern`) | `utilities/string.{hpp,cpp}` | `TokenFlags` mirrors `PFILE_*`; `Tokenizer` wraps `parse_token`. Complete. |
| `crtlib` paths (`COM_FileBase`, `COM_FileExtension`, `COM_ExtractFilePath`, `COM_PathSlashFix`, …) | `utilities/path.{hpp,cpp}` | Raw `(char*,size)` + owning `string_view→string` overloads. Complete. |
| `crclib` CRC32 + MD5 | `utilities/hash.{hpp,cpp}` | `crc32_*` free fns + `Crc32Hasher`/`Md5Hasher` RAII; `Md5State` uses `array<u8,64> in`. `crc32_block_sequence` present (raw ptr+len). Complete. |
| `matrixlib` (value-type portion) | `utilities/matrix.{hpp,cpp}` | Value-type `Matrix3x4`/`Matrix4x4`/`Vec3`; **`com_model.h` dependency removed**. |
| `xash3d_mathlib` macros | `utilities/math.hpp` | Header-only `constexpr`/templates; macro soup replaced. |
| `xash3d_mathlib` + `matrixlib` studio bone math (`AngleQuaternion`, `QuaternionSlerp`, `R_StudioSlerpBones`, `Matrix3x4_FromOriginQuat`, `Matrix3x4_AnglesFromMatrix`) | `utilities/quaternion.{hpp,cpp}` | **New component** promoted per content **OQ-5**; bit-exact to Q-18 goldens. Struct-walking bone driver lives in `content/bone_solver.cpp`. |
| `utflib` | `utilities/utf.{hpp,cpp}` | `Utf8Decoder`/`Utf16Decoder` value types + CP1251/CP1252 tables. Complete. |
| `atlas` | `utilities/atlas.{hpp,cpp}` | `Atlas` value type; `ATLAS_MAX_SIZE = limits::atlas_max_size`. Complete. |
| `swaplib` | `utilities/swap.hpp` (header-only) | `SwapField` mirrors `swap_struct_def_t`; typed `read_le<T>`/`write_le<T>` added (preferred). **`swap_struct` declared but not defined** — implementation gap. |
| `build` + `build_vcs` | `utilities/build.{hpp,cpp}` + generated `build_vcs.cpp` | `build::number()` magic-static; `COMPAT_NUMBER = 4529`. Complete. |
| `dllhelpers` (`dllfunc_t`) | `utilities/dynlib.{hpp,cpp}` | `ExportEntry` + `clear_exports`/`validate_exports`. Complete. |
| `getopt` | *not ported* | Dropped; command-line parsing is a launcher/host concern. |
| `miniz` | `xash3dpp_miniz` STATIC target (from `../public/miniz.c`) | Sibling target linked PRIVATE by `filesystem`/`content`; **not** a utilities export. Relocates to `xash3dpp/3rdparty/` (spin-off D0). |
| `FS_ParseGameInfo`/`FS_WriteGameInfo` (`filesystem.c`) | `utilities/gameinfo_parser.{hpp,cpp}` | **New component** (ported from `filesystem/`, not `public/`); pure text transforms. Parse/serialise/dll-path-derive bodies are **`// TODO` stubs** (4 markers); budget-clamp fixups implemented. |

---

## 9. Quirk / parity checklist (for any future reimplementation)

1. `Q_strncpy` null-terminates; `Q_strlen(NULL)==0`; `restrict` erased in C++.
2. Seven `PFILE_*` tokenizer bits — all combinations byte-identical.
3. CRC32 = IEEE 802.3, init/XOR `0xFFFFFFFF`.
4. `CRC32_BlockSequence`: `length>60` clamp, `abs(sequence)`, `off = seq % 1020`,
   4 table bytes appended at `& 3` offset, **low byte** of final CRC returned.
5. MD5 init constants + `uint[16]` block layout.
6. Studio bone math is **double-precision, ordering-load-bearing**; radians-direct
   `AngleQuaternion`; slerp does not clamp `t`.
7. `ATLAS_MAX_SIZE = 1024` is struct-ABI; atlas packer is lowest-fitting-strip.
8. `swaplib` `size<0` = sub-struct recursion; `default` width is a silent no-op.
9. Build number = days since 2015-04-01 (`-41728` offset); compat = `4529`.
10. `utfstate_t`/decoders require zero-init before first use.
