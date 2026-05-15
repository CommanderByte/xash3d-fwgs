# C++ Modernization Opportunities — `xash3dpp/src/filesystem`

**Standard**: C++20  
**Exceptions**: disabled (`/EHs-c-` / `-fno-exceptions`)  
**RTTI**: disabled  
**ABI boundary**: None — filesystem is fully internal; only `GetFSAPI` is exported.  
See `xash3dpp/docs/boundaries/filesystem-boundary.md`.

---

## Summary

| Tier   | Count | Key theme |
|--------|-------|-----------|
| High   | 2     | Stack-allocation hazards |
| Medium | 6     | Type safety, portability, verbosity |
| Low    | 5     | Readability and minor cleanup |

---

## High Priority

### H-1 — 64 KB stack allocation in `OsFile::Seek` backward path

**File**: `src/filesystem/file.cpp` ~L233  
**Category**: 2-C (raw array), safety hazard

```cpp
// Current
mz_uint8 sink[65536];
FsOffset discard = target - position_;
while (discard > 0 && !zlib_->done) {
    const std::size_t chunk = static_cast<std::size_t>(
        std::min<FsOffset>(discard, static_cast<FsOffset>(sizeof sink)));
    const FsOffset got = inflate_read(sink, chunk);
    ...
}
```

`ZlibState` already owns an `std::array<mz_uint8, 65536> in_buf{}` member. Seeking
backward (which requires reinflating from the beginning) can be done by draining into
`zlib_->in_buf` directly. This eliminates the duplicate 64 KB stack allocation and
keeps the scratch buffer with its logical owner.

**Proposed fix**: Replace `mz_uint8 sink[65536]` with `zlib_->in_buf.data()` /
`zlib_->in_buf.size()` in the discard loop. Requires no API change.

---

### H-2 — Fixed-size `wchar_t` and `char` stack buffers in `platform/win32.cpp`

**File**: `src/filesystem/platform/win32.cpp` — six sites  
**Category**: 2-A (char/wchar_t buffer), latent truncation bug

| Function | Variable | Size |
|----------|----------|------|
| `open_file` | `wchar_t wpath[4096]` | 8 KB |
| `open_memfd` | `wchar_t temp_dir[MAX_PATH]`, `wchar_t temp_file[MAX_PATH]` | both 520 B |
| `file_size` | `wchar_t wpath[4096]` | 8 KB |
| `file_time` | `wchar_t wpath[4096]` | 8 KB |
| `list_directory` | `wchar_t wpattern[4096]`, `char name_buf[4096]` | 8 KB each |
| `make_directory` | `wchar_t wpath[4096]` | 8 KB |
| `rename_file` | `wchar_t wfrom[4096]`, `wchar_t wto[4096]` | 8 KB each |
| `delete_file` | `wchar_t wpath[4096]` | 8 KB |

`to_wide(sv, out, cap)` silently returns 0 for any UTF-8 path whose UTF-16
expansion exceeds `cap - 1` (≈ 2047 wide chars). Callers guard with
`&& !path.empty()` — so such paths silently fail to open rather than
raising an error. For most current paths this is fine, but it is a latent
truncation bug as path depths grow.

**Proposed fix**: Replace `to_wide` with a helper that returns `std::wstring` using
a two-call `MultiByteToWideChar` pattern (first call with `0` to get the required
length, second call into a properly sized `std::wstring`):

```cpp
static std::optional<std::wstring> to_wide(std::string_view s) noexcept {
    if (s.empty()) return std::wstring{};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0,
        s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::nullopt;
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0,
        s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
```

Call sites become `auto wp = to_wide(path); if (!wp) return {};` and then use
`wp->c_str()`.

---

## Medium Priority

### M-1 — `::strnlen` POSIX extension in `pak_backend.cpp`

**File**: `src/filesystem/backends/pak_backend.cpp` L80  
**Category**: 2-C (non-standard C function), portability

```cpp
// Current
const std::size_t len = ::strnlen(de.name, sizeof(de.name));
```

`strnlen` is a POSIX extension, not part of the C++ standard library. It works on
all current targets (MSVC, glibc, Android NDK), but `<cstring>` does not guarantee
it under strict conformance. Replace with the standard range algorithm:

```cpp
// Proposed
const std::size_t len = static_cast<std::size_t>(
    std::find(std::begin(de.name), std::end(de.name), '\0') - de.name);
```

This is zero-overhead, fully standard, and equally readable with the `std::begin`/`std::end` ADL range.

---

### M-2 — `File::Seek` takes a raw `int` whence parameter

**File**: `include/xash3dpp/filesystem/file.hpp`,
          `src/filesystem/file.cpp`,
          `src/filesystem/platform/{win32,posix}.cpp`  
**Category**: 2-E (raw integer "enum"), API type safety

```cpp
// Current
virtual FsOffset Seek(FsOffset offset, int whence) = 0;
```

All callers pass `SEEK_SET`, `SEEK_CUR`, or `SEEK_END` — C macros from `<cstdio>`.
Define a scoped enum in `file.hpp` and replace the `int` parameter:

```cpp
enum class SeekOrigin : int {
    Begin   = 0,  // SEEK_SET
    Current = 1,  // SEEK_CUR
    End     = 2,  // SEEK_END
};

virtual FsOffset Seek(FsOffset offset, SeekOrigin origin) = 0;
```

Remove the `#include <cstdio>` pulled in solely for these macros from `file.cpp`.
Internal `platform::seek` still takes `int whence` (OS API) — convert at the call
site with `static_cast<int>(origin)`.

---

### M-3 — `inflate_read(void* out, size_t n)` raw void pointer

**File**: `src/filesystem/file.cpp` (private method)  
**Category**: 2-F (function pointer / C-style signature), type clarity

```cpp
// Current — internal method
FsOffset OsFile::inflate_read( void* out, std::size_t n );
```

Since this is a private method and `out` always receives either `buf_.data()` or
`buf.data()` (both `std::byte*`) or `zlib_->in_buf.data()` (`mz_uint8*`), tighten
to `std::span<mz_uint8>` (matching miniz's native type):

```cpp
FsOffset OsFile::inflate_read( std::span<mz_uint8> out );
```

Call sites update from `inflate_read(ptr, n)` to `inflate_read({ptr, n})`.  
The `mz_uint8*` cast at the `mz_stream` assignment becomes unnecessary.

---

### M-4 — `reinterpret_cast<const char*>(buf.data())` byte-to-string conversion in `filesystem.cpp`

**File**: `src/filesystem/filesystem.cpp` L122, L134  
**Category**: 2-G (cast), repeated unsafe-looking pattern

```cpp
// Current — appears twice
const std::string text{ reinterpret_cast<const char*>(gi.data()), gi.size() };
```

`std::byte*` → `const char*` via `reinterpret_cast` is permitted by [basic.lval] but
looks unsafe to reviewers. Centralise in a small local helper (or a utility in
`utilities/string.hpp`) to document intent:

```cpp
// Proposed helper (inline in string.hpp or as a local lambda)
inline std::string bytes_as_string( std::span<const std::byte> s ) noexcept {
    return { reinterpret_cast<const char*>( s.data() ), s.size() };
}
```

---

### M-5 — Remaining manual 3-segment path joins in `FindLibrary`

**File**: `src/filesystem/filesystem.cpp` L~392–400  
**Category**: 2-I (miscellaneous), consistency with established `path_join` utility

```cpp
// Current — multi-segment; path_join is not applied here
const std::string candidate = impl_->rootdir + '/' + g.gamefolder
                            + '/' + g.dll_path + '/' + std::string{name};
```

Two such chains exist in `FindLibrary` (dll_path sub-dir branch and gamedir-root
branch). These were intentionally excluded from the Cluster 8 sweep because
`path_join` is a two-argument function. Extend `path_join` with a variadic overload
or use chained calls:

```cpp
const std::string candidate =
    path_join(path_join(path_join(impl_->rootdir, g.gamefolder), g.dll_path), name);
```

Or add a three-argument overload:
```cpp
std::string path_join( std::string_view a, std::string_view b, std::string_view c );
```

---

### M-6 — WAD / PAK magic constants use verbose bit-shift form

**Files**: `src/filesystem/backends/wad_backend.cpp` L31–38,
           `src/filesystem/backends/pak_backend.cpp` L28–30  
**Category**: 2-G (cast), readability

```cpp
// Current
static constexpr std::uint32_t k_WAD2 =
    (static_cast<std::uint32_t>('2') << 24) |
    (static_cast<std::uint32_t>('D') << 16) |
    (static_cast<std::uint32_t>('A') <<  8) | 'W';
```

C++20 `std::bit_cast` produces the equivalent value on little-endian targets
(xash's only supported endianness) from a readable character literal array:

```cpp
static constexpr std::uint32_t k_WAD2 =
    std::bit_cast<std::uint32_t>(std::array<char,4>{'W','A','D','2'});
```

**Note**: Only valid on little-endian platforms. Add a static_assert or keep the
current form if big-endian support is ever planned.

---

## Low Priority

### L-1 — `#pragma pack` in `zip_backend.cpp`

**File**: `src/filesystem/backends/zip_backend.cpp`  
**Category**: 2-I (miscellaneous)

`#pragma pack(push,1)` / `#pragma pack(pop)` is compiler-specific but is supported
by all targeted compilers (MSVC, GCC, Clang). The only portable C++ alternative
would be `[[gnu::packed]]` (GCC/Clang only) or manual `std::memcpy`-based deserialisation.
Given universal `#pragma pack` support, this is acceptable as-is. **No action needed.**

---

### L-2 — Unnamed padding bytes `pad0/pad1` in `DiskLump` (wad_backend.cpp)

**File**: `src/filesystem/backends/wad_backend.cpp`  
**Category**: 2-I (miscellaneous)

```cpp
struct DiskLump {
    ...
    std::uint8_t  attribs;  // compression/type flags
    std::uint8_t  pad0;     // unused
    std::uint16_t pad1;     // unused
};
```

These could be marked `[[maybe_unused]]` but that attribute applies to declarations,
not struct members. The names `pad0`/`pad1` are already clear. Alternatively, use
unnamed bit-fields:

```cpp
std::uint8_t  : 8;   // attribs pad
std::uint16_t : 16;  // unused
```

Low value change. **Optional only.**

---

### L-3 — `any(SearchPathFlags)` free function could become `operator bool`

**File**: `include/xash3dpp/filesystem/search_path_flags.hpp`  
**Category**: 2-J (deletion candidate)

`any(f)` wraps `static_cast<uint32_t>(f) != 0`. Adding `operator bool` to the enum
class removes the helper and makes `if (f & Flag::X)` natural:

```cpp
constexpr bool operator bool(SearchPathFlags f) noexcept { ... } // not valid C++
// Instead:
constexpr explicit operator bool(SearchPathFlags f); // also not valid for enum class
```

Actually `operator bool` cannot be defined for an `enum class` — `any()` is the
correct idiomatic approach for scoped enums. **No change needed.**

---

### L-4 — `SEEK_SET` / `SEEK_CUR` / `SEEK_END` macros at internal call sites

**File**: `src/filesystem/file.cpp`, `src/filesystem/backends/*.cpp`  
**Category**: 2-E, cross-references M-2

Once M-2 (`SeekOrigin` enum) is implemented, all internal `platform::seek(fd, off,
SEEK_SET)` calls should be updated to use `static_cast<int>(SeekOrigin::Begin)` or a
local alias, eliminating the `#include <cstdio>` solely for these macros.

---

### L-5 — Arithmetic `static_cast` noise in binary-format readers

**Files**: `backends/wad_backend.cpp`, `backends/pak_backend.cpp`,
           `backends/zip_backend.cpp`  
**Category**: 2-G (cast)

Every disk-struct field requires `static_cast<std::int64_t>`, `static_cast<std::uint32_t>`,
etc. to satisfy C++20 signed/unsigned comparison warnings and to widen types for math.
These casts are correct and necessary given that the on-disk format structures use
narrow fixed-width types. A thin `narrow_cast<>` helper (no-op in release, assert-checked
in debug) would add safety but increase verbosity. **Acceptable as-is** given the
explicit intent of the casts.

---

## Already Modern — No Action Needed

| Pattern | Location | Status |
|---------|----------|--------|
| `OsFd` RAII wrapper | `os_fd.hpp` | ✅ Move-only RAII, no leaks |
| `ZlibState` RAII | `file.cpp` | ✅ `mz_inflateEnd` in destructor |
| `std::array<std::byte, limits::filesystem_file_buffer_size> buf_{}` | `file.cpp` | ✅ Value-initialised fixed buffer |
| `std::shared_mutex` for path list | `filesystem.cpp` | ✅ Reader/writer lock |
| `ISearchBackend` virtual interface | `i_search_backend.hpp` | ✅ Replaces legacy fn-ptr vtable |
| `SearchPathFlags` scoped enum | `search_path_flags.hpp` | ✅ Typed bitmask, no raw `int` flags |
| `std::optional` return values | throughout | ✅ No out-parameters for file lookups |
| `std::unique_ptr<File>` | throughout | ✅ No raw owning pointers |
| `std::vector<std::byte>` for buffers | throughout | ✅ No raw `malloc`/`free` |
| `std::string_view` parameters | throughout | ✅ No raw `const char*` in new APIs |

---

## Application Order

When implementing, apply in this order to minimise merge conflicts:

1. **H-1** — replace `sink[65536]` in `OsFile::Seek`
2. **H-2** — dynamic `to_wide` in win32.cpp
3. **M-1** — `strnlen` → range algorithm
4. **M-3** — `inflate_read` void* → span
5. **M-2** — `SeekOrigin` enum in `file.hpp`; update L-4 alongside
6. **M-5** — variadic `path_join`; remove remaining manual joins in `FindLibrary`
7. **M-4** — `bytes_as_string` helper
8. **M-6** — bit_cast magic constants
9. **L-2** — unnamed padding (cosmetic, batch with other wad edits)
