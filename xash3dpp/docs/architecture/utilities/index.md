# utilities — File and Symbol Index

## CMake target

| Target | Type | `cxx_std` | External deps |
|--------|------|-----------|---------------|
| `xash3dpp_utilities` | STATIC | 23 | none |

## Public headers

All headers are under `xash3dpp/include/xash3dpp/utilities/`.

| Header | Namespace | Source file | Concept doc |
|--------|-----------|-------------|-------------|
| `string.hpp` | `xash::utilities` | `string.cpp` | [string-utils.md](./string-utils.md) |
| `path.hpp` | `xash::utilities` | `path.cpp` | [path-utils.md](./path-utils.md) |
| `math.hpp` | `xash::utilities` | header-only | [math.md](./math.md) |
| `matrix.hpp` | `xash::utilities` | `matrix.cpp` | [math.md](./math.md) |
| `hash.hpp` | `xash::utilities` | `hash.cpp` | [hash.md](./hash.md) |
| `utf.hpp` | `xash::utilities::utf` | `utf.cpp` | [encoding.md](./encoding.md) |
| `swap.hpp` | `xash::utilities` | header-only¹ | [encoding.md](./encoding.md) |
| `atlas.hpp` | `xash::utilities` | `atlas.cpp` | [misc.md](./misc.md) |
| `build.hpp` | `xash::utilities::build` | `build.cpp` | [misc.md](./misc.md) |
| `dynlib.hpp` | `xash::utilities` | `dynlib.cpp` | [misc.md](./misc.md) |
| `gameinfo_parser.hpp` | `xash` | `gameinfo_parser.cpp` | [misc.md](./misc.md) |

> ¹ `swap.hpp` is effectively header-only today. `swap_struct` is declared but
> has no implementation; `swap_bytes` is `inline`. A `swap.cpp` file should be
> added when `swap_struct` is implemented.

## Private headers

None. No `include/xash3dpp/private/utilities/` directory exists.

## Symbols by header

### `string.hpp`

| Symbol | Kind | Legacy |
|--------|------|--------|
| `strncpy(dst, src, size)` | function | `Q_strncpy` |
| `strlen(s)` | function (inline) | `Q_strlen` |
| `stricmp(a, b)` | function | `Q_stricmp` |
| `strnicmp(a, b, n)` | function | `Q_strnicmp` |
| `ci_less(a, b)` | function (inline) | — |
| `ci_equal(a, b)` | function (inline) | — |
| `to_lower(s&)` | function (inline) | — |
| `to_lower(sv)` | function (inline) | — |
| `snprintf(buf, size, fmt, ...)` | function | `Q_snprintf` |
| `vsnprintf(buf, size, fmt, va)` | function | `Q_vsnprintf` |
| `atoi(sv)` | function | `Q_atoi` |
| `atof(sv)` | function | `Q_atof` |
| `atoi(const char*)` | function (inline, null-safe) | `Q_atoi(NULL)` |
| `atof(const char*)` | function (inline, null-safe) | `Q_atof(NULL)` |
| `atov(out, sv)` | function | — |
| `strip_colors(in, out)` | function | `COM_StripColors` |
| `strip_colors(sv)` | function | `COM_StripColors` |
| `pretify_mem(bytes, decimals)` | function | `Q_pretifymem` |
| `match_pattern(text, pattern, …)` | function | `matchpattern_with_separator` |
| `TokenFlags` | enum class | `PFILE_*` flags |
| `parse_token(data, token, size, …)` | function | `COM_ParseFileSafe` |
| `Tokenizer` | class | — |
| `Tokenizer::Token` | struct | — |
| `trim_sv(sv, chars)` | function (inline) | strip leading/trailing chars; returns view into input |

### `path.hpp`

| Symbol | Kind | Legacy |
|--------|------|--------|
| `file_base(src, dst, size)` | function | `COM_FileBase` |
| `file_extension(sv)` | function | `COM_FileExtension` |
| `default_extension(path, ext, size)` | function | `COM_DefaultExtension` |
| `replace_extension(path, ext, size)` | function | `COM_ReplaceExtension` |
| `extract_dir(path, dst, size)` | function | `COM_ExtractFilePath` |
| `filename(sv)` | function | `COM_FileWithoutPath` |
| `strip_extension(sv)` | function | `COM_StripExtension` |
| `fix_slashes(path, size)` | function | `COM_FixSlashes` |
| `remove_line_feed(s)` | function | `COM_RemoveLineFeed` |
| `trim_space(sv)` | function | `COM_TrimSpace` |
| `file_base(sv)` | function (`std::string`-returning) | — |
| `default_extension(sv, ext)` | function (`std::string`-returning) | — |
| `replace_extension(sv, ext)` | function (`std::string`-returning) | — |
| `extract_dir(sv)` | function (`std::string`-returning) | — |
| `fix_slashes(sv)` | function (`std::string`-returning) | — |
| `path_join(a, b)` | function | — |
| `path_join(a, b, c)` | function | — |

### `math.hpp` (header-only)

| Symbol | Kind | Notes |
|--------|------|-------|
| `vec_t` | type alias | `float` |
| `Vec2` | struct | `.x`, `.y` |
| `Vec3` | struct | `.x`, `.y`, `.z`; members: `.dot()`, `.length()`, `.normalized()`, `+=`, `-=`, `*=` |
| `Vec4` | struct | `.x`, `.y`, `.z`, `.w` |
| `PITCH`, `YAW`, `ROLL` | constants | 0, 1, 2 |
| `dot(a, b)` | function | — |
| `cross(a, b)` | function | — |
| `length(v)` | function | — |
| `normalize(v)` | function | — |
| `AngleVectors` | struct | `{Vec3 fwd, right, up}` — named return type |
| `angle_vectors(angles)` | function | returns `AngleVectors` |
| `vec_to_yaw(v)` | function | — |
| `vector_angles(fwd)` | function | no `up` parameter |
| `rint(f)` | function (constexpr) | — |
| `is_nan(f)` | function (constexpr) | — |

### `matrix.hpp`

| Symbol | Kind | Notes |
|--------|------|-------|
| `Matrix3x4` | struct | `std::array<std::array<float,4>,3>`, affine; `.data()` → `float*` |
| `Matrix3x4::identity()` | static function | returns identity matrix |
| `Matrix4x4` | struct | `std::array<std::array<float,4>,4>`, projective; `.data()` → `float*` |
| `Matrix4x4::identity()` | static function | returns identity matrix |
| `transform_point(m, p)` | function | — |
| `rotate_vector(m, v)` | function | — |
| `concat(a, b)` | function | — |
| `invert_ortho(m)` | function | — |
| `from_angles(origin, angles)` | function | origin + Euler angles → affine |
| `to_matrix3x4(m4)` | function | — |
| `perspective(fov, aspect, near, far)` | function | — |
| `look_at(eye, target, up)` | function | — |
| `operator*(Matrix3x4, Matrix3x4)` | operator | — |
| `operator*(Matrix4x4, Matrix4x4)` | operator | — |

### `hash.hpp`

| Symbol | Kind | Notes |
|--------|------|-------|
| `Crc32` | type alias | `std::uint32_t` |
| `CRC32_INIT` | constant | `0xFFFFFFFFu` — initial CRC state |
| `crc32_init(state)` | function (constexpr) | pass-by-reference initialise |
| `crc32_update(state, data, len)` | function | buffer update |
| `crc32_update(state, byte)` | function | single-byte update |
| `crc32_final(state)` | function (constexpr) | — |
| `crc32(data, len)` | function | one-shot |
| `crc32_block_sequence(base, length, seq)` | function | sequence-keyed CRC; returns `uint8_t` |
| `Md5State` | struct | — |
| `md5_init(state)` | function | — |
| `md5_update(state, data, len)` | function | — |
| `md5_final(state)` | function | returns `array<uint8_t,16>` |
| `Crc32Hasher` | class | RAII, chained `.update()` |
| `Md5Hasher` | class | RAII, chained `.update()` |

### `utf.hpp` (namespace `xash::utilities::utf`)

| Symbol | Kind | Notes |
|--------|------|-------|
| `DecodeState` | struct | must be zero-initialised |
| `decode_utf8(state, byte)` | function | streaming |
| `decode_utf16(state, unit)` | function | streaming |
| `encode_utf8(codepoint)` | function | returns `pair<array<char,4>, size_t>` |
| `length(sv)` | function | codepoint count |
| `utf16_to_utf8(dst, src)` | function | bulk convert |
| `to_cp1251(cp)` | function | Unicode → Windows-1251 |
| `to_cp1252(cp)` | function | Unicode → Windows-1252 |
| `Utf8Decoder` | class | RAII streaming wrapper |
| `Utf16Decoder` | class | RAII streaming wrapper |

### `swap.hpp`

| Symbol | Kind | Notes |
|--------|------|-------|
| `swap_bytes(p, size)` | function (inline) | 2/4/8 bytes |
| `SwapField` | struct | descriptor for `swap_struct` |
| `swap_struct(data, fields)` | function | **not yet implemented** |

### `atlas.hpp`

| Symbol | Kind | Notes |
|--------|------|-------|
| `Atlas::ATLAS_MAX_SIZE` | constant | 1024 (ABI-frozen) |
| `Atlas(size)` | constructor | `explicit`; `size ≤ ATLAS_MAX_SIZE` |
| `Atlas::Block` | struct | `{x, y}` |
| `Atlas::alloc(w, h)` | function | returns `optional<Block>` |
| `Atlas::size()` | function | usable edge length |
| `Atlas::max_height()` | function | current high-water mark |
| `Atlas::clear()` | function | reset all allocations |

### `build.hpp` (namespace `xash::utilities::build`)

| Symbol | Kind | Notes |
|--------|------|-------|
| `number()` | function | days since 2015-04-01 |
| `number_from_date(iso_date)` | function | — |
| `COMPAT_NUMBER` | constant | 4529 (frozen) |
| `commit` | extern `string_view` | from generated `build_vcs.cpp` |
| `branch` | extern `string_view` | from generated `build_vcs.cpp` |
| `commit_date` | extern `string_view` | from generated `build_vcs.cpp` |

### `dynlib.hpp`

| Symbol | Kind | Notes |
|--------|------|-------|
| `ExportEntry` | struct | `{std::string_view name, void **slot}` |
| `clear_exports(span<const ExportEntry>)` | function | zero all slots |
| `validate_exports(span<const ExportEntry>)` | function | returns true if all non-null |

### `gameinfo_parser.hpp` (namespace `xash`)

| Symbol | Kind | Notes |
|--------|------|-------|
| `parse_gameinfo_txt(content, gamefolder)` | function | **stub** — returns `nullopt` |
| `parse_liblist_gam(content, gamefolder)` | function | **stub** — returns `nullopt` |
| `serialise_gameinfo(info)` | function | **stub** — returns `""` |
| `apply_gameinfo_fixups(info)` | function | clamps budgets to safe ranges |

## Tests

| Test executable | Covers |
|-----------------|--------|
| `test_atlas` | `Atlas::alloc`, `reset`, `max_height` |
| `test_build` | `build::number`, `number_from_date`, `COMPAT_NUMBER` |
| `test_dynlib` | `clear_exports`, `validate_exports` |
| `test_gameinfo_parser` | `apply_gameinfo_fixups` (stubs not tested) |
| `test_hash` | `crc32`, `crc32_block_sequence`, MD5, RAII wrappers |
| `test_math` | Vec operations, angle helpers, `is_nan`, `rint` |
| `test_matrix` | Matrix3x4/4x4 concat and point-transform |
| `test_path` | All path functions, both raw and `std::string` APIs |
| `test_string` | `strncpy`, `stricmp`, `snprintf`, `atoi/atof`, `match_pattern`, `Tokenizer` |
| `test_swap` | `swap_bytes` (2/4/8 bytes); `swap_struct` not tested |
| `test_utf` | `decode_utf8/16`, `encode_utf8`, `utf16_to_utf8`, codepage maps, RAII wrappers |
