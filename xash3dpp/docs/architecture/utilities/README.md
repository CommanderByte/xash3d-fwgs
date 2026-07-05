# utilities — Architecture Overview

> **Source**: `xash3dpp/src/utilities/`\
> **Public API**: `xash3dpp/include/xash3dpp/utilities/`\
> **Legacy reference**: `public/crtlib.c`, `public/crclib.c`, `public/matrixlib.c`,
> `public/atlas.c`, `public/utflib.c`, `public/swaplib.h`, `public/build.c`,
> `public/dllhelpers.c`, `filesystem/filesystem.c` (gameinfo parser)

## Purpose

The utilities module is a **dependency-free collection of pure computational
helpers**. It provides the string, path, math, hash, encoding, and miscellaneous
building-block primitives that every other xash3dpp subsystem uses.

It does **not** touch the filesystem, the memory subsystem, the network layer,
or any operating-system resource. Every function is `noexcept` and stateless (or
carries only caller-owned value-type state). There are no global mutable objects
except for the magic-static build number cached by `build::number()`.

## Design goals

- **No heap allocation for long-lived state**: functions return by value or write
  into caller-provided buffers. The few functions that return `std::string` do so
  for convenience in cold/config code; hot paths always have a `(char*, size_t)`
  overload.
- **No exceptions, no RTTI**: compiled with `/EHs-c- /GR-`.
- **`noexcept` everywhere**: all functions in this module are `noexcept` unless
  they return a `std::string` (which may throw `std::bad_alloc` via stdlib).
- **Dual API** for string/path operations: a raw `(char*, size_t)` form for
  engine-internal hot paths, and a `std::string`-returning form for callers that
  prefer the convenience.
- **Zero external dependencies**: `xash3dpp_utilities` links only the C++
  standard library (target declares `cxx_std_23`). It does not link
  `xash3dpp_memory` or any other xash3dpp target.
- **Legacy fidelity**: each function preserves the observable contract of its
  `Q_*` / `COM_*` predecessor. Legacy function names and numeric outputs are
  documented in every header.

## Key invariants

- `swap_struct` is declared in `swap.hpp` but has **no implementation yet**. Only
  `swap_bytes` (inline) is currently callable. Do not add the test until a
  `swap.cpp` is provided.
- `build::commit`, `build::branch`, and `build::commit_date` are
  `[[gnu::weak]]` `const std::string_view` symbols; they read `"(unknown)"` or
  `"1970-01-01"` unless `build_vcs.cpp` (generated at configure time) is linked.
- `Atlas::ATLAS_MAX_SIZE` derives from `limits::atlas_max_size` (default 1024) to match the legacy
  struct layout. Changing it breaks serialised atlas coordinates.
- `gameinfo_parser.cpp` functions (`parse_gameinfo_txt`, `parse_liblist_gam`,
  `serialise_gameinfo`) are **stubs** returning `std::nullopt`/empty. They are
  compile-time placeholders; the real implementation is a TODO.
- `GameInfo` and `parse_gameinfo_txt` live in namespace `xash`, not
  `xash::utilities`. `gameinfo_parser.hpp` depends on the package-root
  `xash3dpp/gameinfo.hpp`.

## Threading

The module is thread-agnostic: pure functions plus caller-owned value types,
no global mutable state (see the boundary spec's Threading section). Every
public header carries a `@thread-safety:` contract line (QN). Nothing here
asserts a thread role — utilities sit below the subsystems that have one.

## Relationship to legacy code

The legacy `public/` directory contained a flat mix of C string macros
(`Q_strncpy`, `Q_snprintf`, `Q_atoi`, …) interleaved with engine-specific
concerns. The rewrite:

- Replaces C macros with proper C++20 functions and `inline` helpers.
- Adds `std::string_view` overloads wherever legacy code took `const char *`.
- Replaces `vec_t[3]` raw arrays with typed `Vec3`/`Vec2`/`Vec4` structs.
- Replaces raw `float[3][4]` matrix arrays with `Matrix3x4`/`Matrix4x4` structs
  that carry their data contiguously for C-API interop via `.data()`.
- Replaces the bare `crc32state` / `MD5Context` C structs with RAII hasher
  classes (`Crc32Hasher`, `Md5Hasher`).
- Preserves `ATLAS_MAX_SIZE = 1024` and `build::COMPAT_NUMBER = 4529` for
  backward compatibility with map/demo tooling that hard-codes them.
- Intentionally drops `Q_strtol`/`Q_strtof` (use `atoi`/`atof` instead) and the
  colour-indexed string tables from `crtlib.c` (moved to the renderer).

## Architecture at a glance

All symbols are pure functions or value-type classes; there is no subsystem
initialisation or shutdown. Callers `#include` only the headers they need.

```text
 ┌──────────────────────────────────────────────────────────────────┐
 │  xash3dpp_utilities  (STATIC, no deps beyond C++20 stdlib)       │
 │                                                                  │
 │  string.hpp / .cpp  ──  safe bounded string ops, tokenizer       │
 │  path.hpp / .cpp    ──  COM_* path helpers, dual API             │
 │  math.hpp           ──  Vec2/3/4, scalar helpers (header-only)   │
 │  matrix.hpp / .cpp  ──  Matrix3x4, Matrix4x4, angle utilities    │
 │  hash.hpp / .cpp    ──  CRC32 (IEEE 802.3), MD5, RAII wrappers   │
 │  utf.hpp / .cpp     ──  UTF-8/16 streaming decode/encode         │
 │  swap.hpp           ──  byte-order swap (header-only; swap_struct │
 │                          stub — no impl yet)                     │
 │  atlas.hpp / .cpp   ──  2D strip-based texture atlas packer      │
 │  build.hpp / .cpp   ──  build number, VCS metadata               │
 │  dynlib.hpp / .cpp  ──  DLL export table helpers                 │
 │  gameinfo_parser.hpp/ ── GameInfo parse/serialise (TODO stubs)   │
 │    .cpp                                                          │
 └──────────────────────────────────────────────────────────────────┘
```

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [string-utils.md](./string-utils.md) — bounded string ops, numeric conversion, tokenizer, wildcards
- [path-utils.md](./path-utils.md) — COM\_\* path helpers, dual raw/`std::string` API
- [math.md](./math.md) — Vec2/3/4, Matrix3x4/4x4, angle and projection helpers
- [hash.md](./hash.md) — CRC32, MD5, RAII hasher wrappers
- [encoding.md](./encoding.md) — UTF-8/16 streaming decode/encode, codepage tables, byte-swap
- [misc.md](./misc.md) — atlas packer, build metadata, dynlib export helpers, gameinfo parser
