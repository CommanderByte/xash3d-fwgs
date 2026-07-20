# Utilities Boundary Spec

> Ports the legacy `public/` utility library. Renamed from
> `public-utilities-boundary.md` (6B S1) to the canonical
> `<subsystem>-boundary.md` form.

> Refreshed 2026-07-06 (as-built pass). The subsystem is **Complete**
> (`status_table.py`: 10 `.cpp`, 12/12 live tests) with four residual
> `// TODO` stub markers, all inside `gameinfo_parser.cpp`
> (`stub_scan.py utilities`). This pass reconciles the component list,
> owned-state table, and open questions against the shipped code, adds the
> `## Extension axes (Q-21)` section, and refreshes `## Threading` to the
> real annotation sites. Prior text is retained; superseded items carry a
> dated status line rather than being deleted.

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

> **As-built component map (2026-07-06).** The shipped `xash3dpp_utilities`
> target compiles ten translation units — `string`, `path`, `hash`, `matrix`,
> `quaternion`, `utf`, `atlas`, `build`, `dynlib`, `gameinfo_parser` — plus the
> header-only `math.hpp` and `swap.hpp`. Divergences from the legacy component
> list above:
>
> - **`quaternion` is a new component** (not a legacy `public/` file of its own):
>   it holds the pure-float studio bone-math kernel (`angle_quaternion_studio`,
>   `quaternion_slerp`, `from_origin_quat`, `angles_from_matrix`, `slerp_bones`)
>   promoted here from legacy `xash3d_mathlib.c`/`matrixlib.c` per
>   **content boundary OQ-5**. `matrix.cpp` deliberately stays `com_model`-free;
>   the struct-walking bone driver lives in `content` (`bone_solver.cpp`).
> - **`gameinfo_parser` is a new component** ported from legacy
>   `filesystem/filesystem.c` (`FS_ParseGameInfo`/`FS_WriteGameInfo`), **not**
>   from `public/`. It is pure text transforms with no FS/OS dependency; the
>   directory-scan half lives in `filesystem`. Its four parse/serialise/fixup
>   entries are still `// TODO` stubs (see Interface + Implementation gaps).
> - **`swaplib` → `swap.hpp`** (header-only, as in legacy). The descriptor-driven
>   `swap_struct` is **declared but not yet defined** (no `swap.cpp`); the typed
>   `read_le<T>`/`write_le<T>` path is implemented and preferred for new codecs.
> - **`getopt` was not ported.** No `getopt.*` exists under `xash3dpp/`; the
>   legacy Win32 POSIX shim is dropped (see Open question 5, now resolved).
> - **`miniz` is a sibling target, not a utilities component.** It builds as
>   `xash3dpp_miniz` (STATIC, from `../public/miniz.c`) in `xash3dpp/CMakeLists.txt`
>   and is consumed **PRIVATE** by `filesystem` and `content`; utilities does not
>   link it (the `target_link_libraries(... miniz)` line in the utilities
>   `CMakeLists.txt` is intentionally commented). Treat ZIP/deflate as a
>   filesystem/content concern, not a utilities export.

______________________________________________________________________

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

______________________________________________________________________

## Interface (what the rest of the engine calls)

| Component | Key symbols | Notes |
|---|---|---|
| `crtlib` | `Q_strncpy`, `Q_strncat`, `Q_strcmp/stricmp`, `Q_snprintf`, `Q_vsnprintf`, `COM_ParseFileSafe`, `COM_FileBase`, `COM_FileExtension`, `COM_DefaultExtension`, `COM_ReplaceExtension`, `COM_ExtractFilePath`, `COM_FileWithoutPath`, `COM_StripExtension`, `COM_PathSlashFix`, `Q_atoi`, `Q_atof`, `Q_atov`, `Q_timestamp`, `matchpattern_with_separator` | Custom CRT replacing libc string ops; `restrict` keyword stripped in C++ via macro |
| `crclib` | `CRC32_Init`, `CRC32_ProcessBuffer`, `CRC32_ProcessByte`, `CRC32_Final`, `CRC32_BlockSequence`, `MD5Init`, `MD5Update`, `MD5Final` | Four CRC32 functions bound into `enginefuncs_t`; a shim or direct export handles the ABI binding. MD5 is engine-internal. |
| `matrixlib` | `Matrix3x4_*`, `Matrix4x4_*` | **As-built:** `matrix.hpp`/`matrix.cpp` operate on value-type `Vec3`/`Matrix3x4`/`Matrix4x4`; the `com_model.h` dependency was removed (bone math split to `quaternion`, see below). |
| `xash3d_mathlib` | `DotProduct`, `CrossProduct`, `VectorNormalize`, `AngleVectors`, `VectorAngles`, `AngleQuaternion`, `QuaternionSlerp`, `R_ConcatTransforms`, plus ~60 macros | **As-built:** `math.hpp` (header-only `constexpr`/templates); the macro soup is replaced by typed free functions. |
| `quaternion` *(new, as-built)* | `angle_quaternion_studio`, `quaternion_slerp`, `from_origin_quat`, `angles_from_matrix`, `slerp_bones` | Pure-float studio bone-math kernel promoted from legacy `xash3d_mathlib.c`/`matrixlib.c` per content **OQ-5**; bit-exact to Q-18 goldens. No `com_model` dependency; struct-walking driver lives in `content`. |
| `miniz` | `mz_compress`, `mz_uncompress`, `tinfl_decompress`, `tdefl_compress`, ZIP read/write | Vendored single-file library; used by filesystem and network |
| `utflib` | `Q_DecodeUTF8`, `Q_DecodeUTF16`, `Q_EncodeUTF8`, `Q_UTF8Length`, `Q_UTF16ToUTF8`, `Q_UnicodeToCP1251`, `Q_UnicodeToCP1252` | Stateful decoder via `utfstate_t`; zero-init required |
| `atlas` | `Atlas_Init`, `Atlas_AllocBlock` | Strip-based packer; max atlas 1024×1024 (`limits::atlas_max_size`); output `x, y` offsets |
| `getopt` | `getopt` + `optarg/optind/opterr/optopt/optreset` | **Not ported to `xash3dpp/`** — legacy Win32 POSIX shim dropped (Open question 5, resolved). Listed for legacy reference only. |
| `swaplib` → `swap.hpp` | `swap_struct`, `SwapField`, `read_le<T>`, `write_le<T>` | Header-only. `read_le`/`write_le` (fold to `std::byteswap`) implemented and preferred; descriptor-driven `swap_struct` **declared but not yet defined** (no `swap.cpp`). |
| `build` | `Q_buildnum`, `Q_buildnum_iso`, `Q_buildnum_compat`, `g_buildcommit`, `g_buildbranch`, `g_buildcommit_date` | `build_vcs.c` supplies VCS strings at link time; `build.c` computes day-offset from `g_buildcommit_date` |
| `dllhelpers` → `dynlib.hpp` | `clear_exports`, `validate_exports`, `ExportEntry` | Used by all plugin loaders to resolve export tables after `LoadLibrary`/`dlopen`; `void **slot` erasure is deliberate. |
| `gameinfo_parser` *(new, as-built)* | `parse_gameinfo_txt`, `parse_liblist_gam`, `serialise_gameinfo`, `apply_gameinfo_fixups` | Pure text transforms ported from legacy `filesystem/filesystem.c` (**not** `public/`); directory scan stays in `filesystem`. Parse/serialise/fixup-dll-path bodies are **`// TODO` stubs** today (`apply_gameinfo_fixups` does implement the budget clamps). |

______________________________________________________________________

## Dependencies (what this module calls)

- **`common/xash3d_types.h`** — `byte`, `vec_t`, `vec2_t`…`matrix4x4`, `qboolean`, `poolhandle_t`; required by every component.
- **`common/build.h`** — compiler feature macros (`RETURNS_NONNULL`, `FORMAT_CHECK`, `MAYBE_ALIGNED`, `XASH_WIN32`, etc.).
- **`common/port.h`** — used by `matrixlib.c` for platform alignment.
- **`common/com_model.h`** — used by `matrixlib.c` for bone/attachment data types (`mstudio*`).
- **C standard library** — `<string.h>`, `<stdarg.h>`, `<math.h>`, `<ctype.h>`, `<stdio.h>` (for `sscanf` in `build.c`).
- **No engine subsystems** — no filesystem, no console, no memory pools.

______________________________________________________________________

## Owned state

| Symbol | Location | Notes |
|---|---|---|
| `build::commit`, `build::branch`, `build::commit_date` | `build_vcs.cpp` (generated) | `extern const std::string_view`; populated at link/configure time from VCS. Immutable after static init. |
| magic-static in `build::number()` | `build.cpp` | One-time cached build-number offset; **magic-static** (C++11 thread-safe init guard), read-only afterward. **Supersedes 2026-07-06** the legacy `static int b` in `Q_buildnum()`. |

No heap allocations; no global mutable state beyond the two items above.

> **Superseded 2026-07-06:** the legacy owned-state entries below no longer
> exist in the port. `getopt` was not ported, so its POSIX globals
> (`opterr`/`optind`/`optopt`/`optreset`/`optarg`) are gone; the `build.c`
> `static int b` cache became the `build::number()` magic-static. Retained for
> legacy cross-reference only:
>
> | Symbol | Location | Notes |
> |---|---|---|
> | `static int b` | `build.c :: Q_buildnum()` | One-time cached build number; not thread-safe but called before threads start |
> | `opterr`, `optind`, `optopt`, `optreset`, `optarg` | `getopt.c` | POSIX global state; not thread-safe; Win32-only |

______________________________________________________________________

## Quirks and invariants

- **`Q_strncpy` null-terminates.** Always writes a `'\0'` at `dst[size-1]`. Callers rely on this; `strncpy` (which does not guarantee termination) must never be silently substituted.
- **`Q_strlen(NULL) == 0`.** The macro short-circuits on NULL. This is load-bearing in many callers.
- **`Q_floor`/`Q_ceil` cast through `int`.** Deliberately truncating; not equivalent to `floorf`/`ceilf` for negative numbers. Used for world grid snapping; do not replace with standard versions without auditing every caller.
- **`PFILE_*` flags in `COM_ParseFileSafe`.** Seven flag bits control tokeniser behaviour (bracket handling, colon as token, hash as comment, quoted tokens, single quote, comma, newline). All combinations are exercised in map/entity/config parsing; the behaviour must remain byte-identical.
- **`matrixlib` depends on `com_model.h`.** `Matrix3x4_ConcatTransforms` and bone-related helpers reference `mstudiobone_t` and similar SDK structs. This is the only SDK-type dependency inside `public/`.
- **CRC32 output must be IEEE 802.3 compatible.** `CRC32_INIT_VALUE = 0xFFFFFFFF`, XOR `0xFFFFFFFF`. The `enginefuncs_t` slots are function pointers — the rewrite binds them to whatever implementation it chooses (including hardware-accelerated CRC32); no legacy source file needs to be preserved.
- **`utfstate_t` must be zero-initialised** before the first call to `Q_DecodeUTF8`/`Q_DecodeUTF16`. Callers that reuse the struct between codepoints without reset will silently corrupt output.
- **`ATLAS_MAX_SIZE`** (= `limits::atlas_max_size` = 1024). ABI-frozen: matches `atlas_t.allocated[1024]`. Increasing it is a struct-layout break; decreasing it silently corrupts existing allocations.
- **`miniz` is a sibling target, not a utilities export.** *(Refreshed 2026-07-06)* It builds as `xash3dpp_miniz` (STATIC, from `../public/miniz.c`) and is linked **PRIVATE** by `filesystem`/`content` only. The utilities `CMakeLists.txt` keeps its miniz link commented out. The build must not pull in a system zlib or a second miniz alongside it; symbol collisions are possible. (Spin-off note: moves under `xash3dpp/3rdparty/` when that tree is populated — implementation-plan D0.)
- **`Q_buildnum_compat()` always returns 4529.** This is intentional; it represents the frozen legacy build number that some mods may test against.
- **`restrict` is erased in C++** via `#define restrict` in `crtlib.h`. This means the C++ rewrite cannot rely on restrict-based optimisations in these functions.

______________________________________________________________________

## Open questions

1. **`matrixlib` → `com_model.h` dependency.** Should the rewrite's matrix library take bone/attachment types as opaque spans, or keep the GoldSrc struct dependency? Eliminating it would make `public/` fully independent of `common/`.
   > **Resolved 2026-07-06:** as-built `matrix.cpp` is `com_model`-free — the studio bone kernel was split into the new `quaternion` component (content OQ-5) which takes value-type `Vec3`/`Vec4`/`Matrix3x4` spans, not SDK structs. Utilities no longer depends on `com_model.h`.
1. **CRC32 binding strategy.** The `enginefuncs_t` slots are filled by the server subsystem, not by `public/`. Decide whether to (a) expose the new CRC32 functions directly at the right signature, (b) use inline lambdas/shims at the fill site, or (c) keep a thin `crclib`-compatible façade. All are valid; pick whichever keeps the utilities module cleanest.
1. **`utflib` scope.** The current API converts to CP1251/CP1252 for legacy console codepage support. Should the rewrite retain those conversion tables or treat all I/O as UTF-8 at the boundary and only convert at display time?
1. **`miniz` vendor strategy.** Should `xash3dpp/3rdparty/` carry its own miniz copy, or depend on a system zlib with a miniz compatibility shim? Either way, the ZIP-read API surface used by filesystem must be decided before the filesystem module is implemented.
   > **Resolved 2026-07-06:** ships as the `xash3dpp_miniz` STATIC target built from `../public/miniz.c`, linked PRIVATE by `filesystem`/`content`. The relocation into `xash3dpp/3rdparty/` is tracked as spin-off D0 in `implementation-plan.md`; the ZIP-read surface is owned by the filesystem archive backends, not utilities.
1. **`getopt` necessity.** The rewrite will target C++17; `std::span` + a small argument parser might replace `getopt` entirely. Confirm whether any external tool or game DLL calls `getopt` directly (unlikely, but should be verified).
   > **Resolved 2026-07-06:** `getopt` was **not ported** — no `getopt.*` exists under `xash3dpp/`. Command-line parsing is a launcher/host concern; no game DLL or tool links it. (The C++ standard is now C++23, not C++17 as this question assumed.)
1. **Thread safety of `Q_timestamp`.** It uses `localtime` internally via a static buffer in some implementations. If the rewrite targets a multi-threaded host, this must be replaced with `localtime_r`/`localtime_s`.

______________________________________________________________________

## Threading

> Refreshed 2026-07-06 (as-built pass). Verified against the shipped headers
> and `src/utilities/**`.

The xash3dpp port is thread-agnostic by construction: every entry point is a
pure function or a method on a caller-owned value type (`Tokenizer`, `Atlas`,
`Crc32Hasher`, `Md5Hasher`, `Utf8Decoder`/`Utf16Decoder`). There is no global
mutable state (the `build::number()` magic-static is initialise-once, read-only
after). Per QN, each public header carries a `@thread-safety:` contract line;
instance types are confined to their owner's thread.

Per the analyse-threading classification, every symbol here is one of two
roles — **pure/stateless** (safe from any thread, no synchronisation) or
**owner-confined value type** (single-thread-per-instance; caller supplies
external synchronisation if shared). There are **zero `assert_thread_role`
sites** in the subsystem, and that is correct: a thread-role assert would be
false precision on a library with no thread affinity of its own.

| Symbol / type | Class | Notes |
|---|---|---|
| `string`, `path`, `hash`, `math`, `matrix`, `quaternion`, `utf`, `swap`, `dynlib` free functions | Pure / stateless | No shared state; operate on caller-owned buffers/values. `@thread-safety:` on each header. |
| `Tokenizer`, `Atlas`, `Crc32Hasher`, `Md5Hasher`, `Utf8Decoder`/`Utf16Decoder` | Owner-confined value type | Confine each instance to its owner's thread; no interior locks. |
| `Atlas::clear`, `Tokenizer::reset` | Owner-confined (mutator) | Carry `// compliance-allow(thread-assert)` (atlas.cpp:17, string.cpp:495) — the two mutators that would otherwise trip the "documents-but-never-asserts" QN check; assert is intentionally omitted. |
| `build::number()` magic-static | Initialise-once, read-only | C++11 magic-static guard (`build.cpp:69-70`); safe concurrent first-call, plain load afterward. |
| `build::commit`/`branch`/`commit_date` | Immutable after static init | `extern const std::string_view` from generated `build_vcs.cpp`. |

**Follow-up flag (not a source edit here):** `gameinfo_parser` is pure text
transforms once implemented, so it stays in the pure/stateless row; no thread
concern is added by finishing its stubs.

## Role & parity

- **Role:** role-neutral substrate — vector / matrix / string / CRC helpers under
  every role. No cross-role parity obligation, **except** the double-precision
  studio math helpers, which are in the HB-2 float-exact set because they feed
  the shared-deterministic studio hull / trace path (see content, map_loader).

## Extension axes (Q-21)

> Added 2026-07-06 (as-built pass). Evaluated against
> `docs/design/extension-goals.md` (G-1..G-5, P-1..P-8).

Utilities is a **pure-function / owner-confined value-type library with no
subsystem state, no seams, and no ABI freeze** (its only external contract is
four CRC32 slots bound into `enginefuncs_t` by the *server*, via a fill-site
shim — utilities itself owns nothing frozen). That posture makes most door
rules **not applicable by construction** rather than by choice: there is no
file-scope mutable state to keep out (P-3), no aggregate to narrow (P-5), no
service to spin out (P-6). The doors it *does* keep open are the introspection
substrate (P-4) that every future service reuses and the annotation discipline
(P-8) it already satisfies.

| Goal / primitive | Applies? | Verdict / door kept |
|------------------|----------|---------------------|
| **P-3** context-first, no new file-scope state | **N/A — nothing to keep** | Already stateless bar the two read-only build statics (magic-static + generated `string_view`s). No mutation channel exists to leak; the door is structurally shut. |
| **P-4** typed introspection surfaces | **Yes — enabling substrate** | The hashers, `Tokenizer`, `swap`/`read_le`, and `pretify_mem`/`match_pattern` are the value-type primitives MCP (G-1), debug-thread (G-3), and overlay (G-4) frontends compose. Door rule: keep them value-typed and header-declared so a service *extends* by calling, never by poking internals. |
| **P-1 / P-2** inbox / snapshot reads | **Consumer, not owner** | Utilities has no sim state to publish; its pure functions are trivially callable from any snapshot/inbox consumer. No door obligation. |
| **P-5** narrowest-state signatures | **Already minimal** | Free functions already take the smallest value/span they touch (`std::span`, `std::string_view`); no god-aggregate exists. `crc32_block_sequence`/path raw overloads are the only wider-than-needed signatures (see modernization M-2/M-3). |
| **P-6** services are satellites | **N/A (leaf dep)** | Utilities is a leaf everything links *toward*; it never links toward a service. `xash3dpp_miniz` is a separate STATIC target (Q-11-clean). |
| **P-7** pool-owned RAII lifecycle | **N/A** | No heap ownership; value types are stack/caller-owned, `pool_new` not used. |
| **P-8** annotation discipline | **Yes — satisfied** | Every header carries `@thread-safety:`; the two non-asserting mutators are `compliance-allow(thread-assert)`-marked (documents-and-marks, not documents-but-never-asserts). |
| **G-1** in-engine MCP service | Enabling substrate (see P-4) | No utilities-owned state to expose; the value-typed hashers/`Tokenizer`/format helpers are what an MCP frontend composes. Door obligation identical to P-4: stay value-typed and header-declared. |
| **G-2** game ABI v2 | Door-keep (thin) | The CRC32 free functions are bound as raw `enginefuncs_t` slots today; a v2 ABI would bind the same callables through a context-carrying descriptor. No utilities change is forced — the shim moves, the implementation does not. |
| **G-3** dedicated debug thread | N/A (stateless leaf) | Pure functions are callable from any thread by construction; utilities publishes no sim state a debug thread would snapshot. No door obligation beyond staying stateless. |
| **G-4** expanded in-game debugging | Enabling substrate (see P-4) | `pretify_mem`/`match_pattern`/hashers are overlay building blocks; no utilities-side surface to add. |
| **G-5** scripting runtime | Door-keep | `match_pattern`, the hashers, `Tokenizer`, and `swap` are the kind of cold-path primitives a tooling VM binds; keeping them `noexcept` and exception-free suits the `/EHs-c- /GR-` isolated-island constraint. No API change needed now. |
| **Q-11** satellite / **Q-12** compat | No | Single concrete implementation, no compat variance, no policy injection (score 0). See Q-11 verdict below. |

**Net:** utilities keeps its doors open by *staying a stateless leaf*. The one
active recommendation surfaced by this axis review is to preserve the
value-typed, header-declared introspection primitives (P-4) as the shared
substrate rather than letting a future debug/MCP frontend grow private helpers —
a design note, not a source change.

## Constant classification (QO)

The literals in this module are **algorithm constants, frozen** — CRC32
polynomial/init values (IEEE 802.3), MD5 round constants and shifts, UTF
decode-state masks, codepage tables, date-digit ranges. They are not tunable
capacities (limits.hpp) nor behavioural knobs (cvars); per QO they stay as
in-code constants next to the algorithms that define them. `ATLAS_MAX_SIZE`
is the one structural capacity and already lives in `limits.hpp`
(`atlas_max_size`, ABI-frozen at 1024).

## Q-11 satellite verdict

Not a satellite candidate: no compat variance, no policy injection, no
protocol variants — a single concrete implementation serving every consumer
(Q-11 score 0). Interfaces are deliberately absent (Q-22 seam rule: no
speculative `I*` seams in a pure-function library).
