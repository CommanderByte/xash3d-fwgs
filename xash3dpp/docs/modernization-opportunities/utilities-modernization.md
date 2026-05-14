# Utilities Modernization Opportunities

> C++ standard in use: C++**20** (from `xash3dpp/CMakeLists.txt`)
> Boundary spec: derived from frozen headers
> ABI-frozen symbols in this subsystem: **None** — all utilities are `xash3dpp`-internal

## Summary

The utilities subsystem (`hash`, `atlas`, `utf`, `build`, `string`, `path`,
`matrix`, `swap`, `dynlib`) is largely modern: `std::array`, `std::span`,
`std::optional`, `std::numbers::pi`, and `enum class` with bitwise operators
are already in place after earlier modernization passes.  Remaining issues fall
into two groups: (1) legacy `const char *` interfaces paired with their modern
equivalents that are now deletable (2-J), and (2) a handful of residual C-array
locals, duplicate constants, and one critical missing implementation.

---

## High-priority opportunities

### H-1: `vec_to_yaw` and `vector_angles` are declared but never implemented

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/math.hpp` lines ~94–98
- **Current pattern**:
  ```cpp
  float vec_to_yaw( const Vec3 &v ) noexcept;
  Vec3  vector_angles( const Vec3 &fwd ) noexcept;
  ```
  No definition exists in any `.cpp` under `xash3dpp/src/utilities/`. Any
  translation unit that calls either function will fail to link.
- **Suggested replacement**: Implement both in `matrix.cpp`.  Legacy reference
  implementations (`SV_VecToYaw`, `VectorAngles`) live in
  `engine/server/sv_studio.c`.
  ```cpp
  float vec_to_yaw( const Vec3 &v ) noexcept
  {
      if( v.x == 0.0f && v.y == 0.0f ) return 0.0f;
      return std::atan2( v.y, v.x ) * ( 180.0f / static_cast<float>( std::numbers::pi ) );
  }

  Vec3 vector_angles( const Vec3 &fwd ) noexcept
  {
      float pitch, yaw;
      if( fwd.x == 0.0f && fwd.y == 0.0f )
      {
          yaw   = 0.0f;
          pitch = ( fwd.z > 0.0f ) ? 90.0f : 270.0f;
      }
      else
      {
          yaw   = std::atan2( fwd.y, fwd.x ) * ( 180.0f / static_cast<float>( std::numbers::pi ) );
          if( yaw < 0.0f ) yaw += 360.0f;
          const float xy = std::sqrt( fwd.x*fwd.x + fwd.y*fwd.y );
          pitch = std::atan2( fwd.z, xy ) * ( -180.0f / static_cast<float>( std::numbers::pi ) );
          if( pitch < 0.0f ) pitch += 360.0f;
      }
      return { pitch, yaw, 0.0f };
  }
  ```
- **Boundary-safe**: Yes
- **Rationale**: Missing definitions cause linker errors for any caller. This is
  a blocking gap, not a cosmetic issue.

---

## Medium-priority opportunities

### M-1: `DEG2RAD` duplicated in `matrix.cpp` (two `static constexpr` definitions)

- **File(s)**: `xash3dpp/src/utilities/matrix.cpp` lines ~86 and ~191
- **Current pattern**: `static constexpr float DEG2RAD = …` declared
  independently inside both `from_angles()` and `angle_vectors()`.
- **Suggested replacement**: Hoist to a single file-scope constant before both
  functions:
  ```cpp
  static constexpr float k_deg2rad = static_cast<float>( std::numbers::pi / 180.0 );
  ```
  Replace the two local definitions with uses of `k_deg2rad`.
- **Boundary-safe**: Yes
- **Rationale**: DRY; the value cannot drift between the two functions; naming
  `k_deg2rad` is consistent with the `k_` prefix convention already used for
  `k_crc32table` in `hash.cpp`.

### M-2: `atov(float*, const char*, size_t)` raw overload (2-J deletion)

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` line ~46;
  `xash3dpp/src/utilities/string.cpp` lines ~171–184
- **Current pattern**: A raw-pointer overload exists alongside the modern span
  overload:
  ```cpp
  void atov( float *out, const char *s, std::size_t n ) noexcept; // old
  void atov( std::span<float> out, std::string_view s ) noexcept; // new
  ```
  Zero call sites for the raw overload exist anywhere in `xash3dpp/`.
- **Suggested replacement**: Delete the `(float*, const char*, size_t)` overload
  from both the header and `string.cpp`.  Call sites (when they arrive) use
  `atov( std::span{arr}, sv )`.
- **Boundary-safe**: Yes — engine-internal, no callers.
- **Rationale**: Two overloads for the same operation with different safety
  profiles invites accidental use of the unsafe one.

### M-3: `strip_colors(const char*, char*)` raw overload (2-J deletion)

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` line ~51;
  `xash3dpp/src/utilities/string.cpp` lines ~206–215
- **Current pattern**: Raw in+out-buffer overload alongside `std::string` return:
  ```cpp
  void        strip_colors( const char *in, char *out ) noexcept; // old
  std::string strip_colors( std::string_view in ) noexcept;       // new
  ```
  Zero call sites for the raw overload exist in `xash3dpp/`.
- **Suggested replacement**: Delete the raw overload; keep only the
  `std::string_view` → `std::string` version.
- **Boundary-safe**: Yes
- **Rationale**: Same argument as M-2; eliminates a shared mutable output buffer.

### M-4: `atoi` / `atof` accept `const char *` with defensive null guards

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` lines ~44–45;
  `xash3dpp/src/utilities/string.cpp` lines ~123–165
- **Current pattern**:
  ```cpp
  int   atoi( const char *s ) noexcept;
  float atof( const char *s ) noexcept;
  ```
  Both guard `if (!s || !*s) return 0;` and `skip_spaces` also null-guards.
  The test in `tests/utilities/test_string.cpp` line 40 explicitly passes
  `nullptr` and expects `0`.
- **Suggested replacement**: Add `std::string_view` overloads that remove the
  null checks; keep the `const char *` overloads as thin forwarders for legacy
  callers passing null:
  ```cpp
  int   atoi( std::string_view s ) noexcept;
  float atof( std::string_view s ) noexcept;
  // Legacy compat — forwards to string_view overload; handles null.
  inline int   atoi( const char *s ) noexcept { return atoi( s ? std::string_view{s} : std::string_view{} ); }
  inline float atof( const char *s ) noexcept { return atof( s ? std::string_view{s} : std::string_view{} ); }
  ```
  Update `atov(std::span<float>, std::string_view)` to call the new string_view
  `atof` overload directly; remove the pointer-arithmetic walk from
  `atov(std::span<float>, …)`.
- **Boundary-safe**: Yes
- **Rationale**: The modern overloads become the canonical interface; new callers
  never see the null-guard defensive code.  The legacy wrappers keep the
  existing test passing without change.

### M-5: `pretify_mem` uses `char val[32]` / `char buf[48]` local C arrays

- **File(s)**: `xash3dpp/src/utilities/string.cpp` lines ~237–262
- **Current pattern**:
  ```cpp
  char val[32];
  std::snprintf( val, sizeof val, … );
  char buf[48];
  char *o = buf;
  …
  return std::string( buf );
  ```
- **Suggested replacement**: Use `std::string` throughout and `std::format`
  for the numeric portion:
  ```cpp
  std::string pretify_mem( float value, int decimals ) noexcept
  {
      …
      std::string val = decimals <= 0 || is_integral
          ? std::format( "{}", static_cast<int>( value + 0.5f ) )
          : std::format( "{:.{}f}", static_cast<double>( value ), decimals );
      // comma-insertion pass using string operations
      …
      return val + " " + suffix;
  }
  ```
  The inner comma-insertion loop remains but operates on `std::string` rather
  than a fixed-size `char[]`.
- **Boundary-safe**: Yes
- **Rationale**: Eliminates two fixed-size stack buffers that could overrun if
  `decimals` is very large or `value` is extreme.  `std::format` is also
  exception-safe for compile-time checked format strings.

### M-6: `number_from_date` uses pointer arithmetic on `string_view::data()`

- **File(s)**: `xash3dpp/src/utilities/build.cpp` lines ~30–75
- **Current pattern**:
  ```cpp
  const char *date = iso_date.data();
  const int y0 = digit( *date++ );
  …
  if( y0 < 0 || *date++ != '-' ) …
  ```
  Pointer increments through a `std::string_view` backing store are technically
  valid but unusual and bypass the `string_view` API entirely.
- **Suggested replacement**: Use indexed subscript access instead:
  ```cpp
  const int y0 = digit( iso_date[0] ), y1 = digit( iso_date[1] ),
            y2 = digit( iso_date[2] ), y3 = digit( iso_date[3] );
  if( iso_date[4] != '-' ) return -1;
  …
  ```
  Since `size() != 10` is checked up front, all indexed accesses are safe.
- **Boundary-safe**: Yes
- **Rationale**: Eliminates raw pointer manipulation; the indexed form makes the
  "YYYY-MM-DD" pattern visually obvious and can be verified at a glance.

---

## Low-priority / cosmetic opportunities

### L-1: `k_cp1251_table` is a C array — should be `constexpr std::array`

- **File(s)**: `xash3dpp/src/utilities/utf.cpp` lines ~170–179
- **Current**: `static const uint16_t k_cp1251_table[64] = { … };`
- **Replacement**: `static constexpr std::array<std::uint16_t, 64> k_cp1251_table{ … };`
- **Boundary-safe**: Yes
- **Rationale**: Consistent with all other table-like data in the codebase
  (`k_crc32table` aside — that one is already `constexpr`).

### L-2: `find_extension` private helper still uses raw `const char *` walk

- **File(s)**: `xash3dpp/src/utilities/path.cpp` lines ~18–31
- **Current**: `static const char *find_extension( const char *path )` —
  pointer-walking null-guarded helper used only by the legacy raw-buffer
  overloads of `strip_extension` and `default_extension`.
- **Replacement**: Remove `find_extension` entirely once the raw-buffer
  overloads are removed (if that happens); or rewrite as a `std::string_view`
  helper to match `file_extension(std::string_view)`.
- **Boundary-safe**: Yes
- **Rationale**: The `std::string_view`-based `file_extension` already
  duplicates its logic cleanly.  This is a maintenance hazard.

### L-3: `Tokenizer::buf_` is a raw `char[]` member

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` lines ~130–131
- **Current**: `char buf_[MAX_TOKEN]{};`
- **Replacement**: `std::array<char, MAX_TOKEN> buf_{};`
  Access via `buf_.data()` in `Tokenizer::next()`.
- **Boundary-safe**: Yes
- **Rationale**: Makes `sizeof(Tokenizer::buf_)` unnecessary; `buf_.data()` is
  explicit about the pointer extraction.

### L-4: `month_prefix` / `month_days` C arrays in `build.cpp`

- **File(s)**: `xash3dpp/src/utilities/build.cpp` lines ~68–71
- **Current**: `constexpr int month_prefix[13] = { … }; constexpr int month_days[12] = { … };`
- **Replacement**: `constexpr std::array<int, 13>` / `std::array<int, 12>`.
- **Boundary-safe**: Yes
- **Rationale**: Consistent style; array access out-of-bounds is caught by
  `std::array` in debug builds.

### L-5: `SwapField::subdef` is a raw pointer — no null-as-optional idiom

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/swap.hpp` lines ~44–48
- **Current**: `const SwapField *subdef;` — `nullptr` means "not a sub-struct".
- **Replacement**: `std::span<const SwapField> subdef{};` — empty span is the
  "not a sub-struct" sentinel.  `size < 0` check becomes `!subdef.empty()`.
- **Boundary-safe**: Yes — `SwapField` is engine-internal.
- **Rationale**: Removes a raw pointer with a sentinel convention; `std::span`
  expresses "optional array" more clearly.

---

## Out of scope / ABI-frozen

- `char *strncpy( char *, const char *, std::size_t )` — the signature mirrors
  the standard `::strncpy`; changing it would break drop-in compatibility for
  code that calls it via `Q_strncpy`.
- Raw `file_base(const char*, char*, size_t)` and related path functions — kept
  intentionally as "hot-path" C-API overloads alongside the string-returning
  versions.  Per the header comment they are not slated for removal.
- `void *` in `ExportEntry::slot` — function pointer type erasure for a generic
  export table; typed generics would require templates throughout the loader,
  which changes the callers' code significantly.

---

## Open questions

1. **`vec_to_yaw` / `vector_angles` intent (H-1)**: Are these deliberately left
   unimplemented as stubs for a future content-loaders subsystem, or were they
   simply overlooked when `angle_vectors` was added?  If the former, the
   declarations should be removed or guarded with `// not yet implemented`.

2. **`atoi(nullptr)` contract (M-4)**: The unit test explicitly exercises
   `atoi(nullptr) == 0`.  Is this a deliberate legacy-compat guarantee for
   engine code paths that may receive null config tokens, or a test artefact?
   The answer determines whether the `const char *` overloads can be removed
   entirely or must be kept as forwarders.

3. **`Utf8Decoder::feed` U+0000 vs invalid-byte ambiguity**: Both a valid U+0000
   byte (0x00) and an isolated continuation byte (0x80–0xBF) return
   `optional{0}`.  The comment says "optional{0} for invalid byte sequences"
   but this is the same value as valid U+0000.  If callers need to detect
   invalid sequences, the return type should be `std::expected<uint32_t,
   Utf8Error>` (or a wrapper enum).  Needs design decision.


> C++ standard in use: C++**20** (from `xash3dpp/CMakeLists.txt`)
> Boundary spec: derived from frozen headers (no `utilities-boundary.md` found)
> ABI-frozen symbols in this subsystem: **None** — all utilities are engine-internal

## Summary

The subsystem is in good shape after a recent modernization pass that covered raw
struct arrays (`std::array`), C-string ownership (`std::string_view`), flag helpers
(`TokenFlags` operators + `has_flag` deletion), and span-based API surfaces.
What remains are: one macro (`MD5S`) that should be a function template, three
scattered raw local arrays inside `hash.cpp`, two functions with legacy pointer
idioms (`angle_vectors` out-params, `md5_load_block`/`md5_transform` decayed-array
params), one missed magic-π constant in `matrix.cpp`, and two deletion candidates
(`xash::utilities::clamp`, `utf::length` raw-pointer overload).

---

## High-priority opportunities

### H-1: `#define MD5S(…)` multi-statement macro → `constexpr` function template

- **File**: `xash3dpp/src/utilities/hash.cpp` lines ~149–153 (64 call sites at lines 171–208)
- **Current pattern**:
  ```cpp
  #define MD5S(f, w, x, y, z, data, s)  \
      (w) += f((x),(y),(z)) + (data);   \
      (w)  = md5_rotl((w),(s));         \
      (w) += (x)
  ```
  Used as `MD5S(md5_f1, a,b,c,d, w[0]+0xd76aa478u, 7);` across 64 lines.
- **Suggested replacement**: C++20 NTTP (non-type template parameter) function template — `md5_f1/f2/f3/f4` are already `constexpr` free functions and qualify as NTTPs:
  ```cpp
  template<auto F>
  static constexpr void md5_step(
      std::uint32_t &w, std::uint32_t x, std::uint32_t y, std::uint32_t z,
      std::uint32_t data, int s ) noexcept
  {
      w += F( x, y, z ) + data;
      w  = md5_rotl( w, s );
      w += x;
  }
  // call site: md5_step<md5_f1>( a, b, c, d, w[0]+0xd76aa478u, 7 );
  ```
  The macro is already `#undef`'d after `md5_transform` — discipline is right, but a typed function template removes the multi-statement expansion hazard entirely and is debuggable.
- **Boundary-safe**: Yes (internal `static` helpers)
- **Rationale**: Multi-statement macros cannot be stepped into, hide type errors, and expand in surprising ways if the argument is a side-effecting expression. 64 call sites become typesafe function calls.

---

## Medium-priority opportunities

### M-1: `perspective()` still has a magic-π constant — inconsistency with `from_angles()`

- **File**: `xash3dpp/src/utilities/matrix.cpp` line ~155
- **Current pattern**:
  ```cpp
  // inside perspective():
  static constexpr float DEG2RAD = static_cast<float>( 3.14159265358979323846 / 180.0 );
  ```
  `from_angles()` in the same file was already updated to `std::numbers::pi / 180.0`; `perspective()` was missed.
- **Suggested replacement**:
  ```cpp
  static constexpr float DEG2RAD = static_cast<float>( std::numbers::pi / 180.0 );
  ```
- **Boundary-safe**: Yes
- **Rationale**: Consistency with the rest of the file; `std::numbers::pi` is the platform-canonical value and removes an easy mistype vector.

### M-2: Raw local C arrays in `crc32_block_sequence` → `std::array`

- **File**: `xash3dpp/src/utilities/hash.cpp` lines ~113–117
- **Current pattern**:
  ```cpp
  std::uint8_t buffer[64];
  std::memcpy( buffer, base, static_cast<std::size_t>( length ) );
  …
  std::uint8_t le8[8];
  le8[0] = …; … le8[7] = …;
  std::memcpy( buffer + length, le8 + ( off & 3 ), 4 );
  ```
- **Suggested replacement**:
  ```cpp
  std::array<std::uint8_t, 64> buffer{};
  std::array<std::uint8_t, 8>  le8{};
  ```
  Subsequent indexing and `std::memcpy( buffer.data() + length, le8.data() + (off & 3), 4 )` remain identical.
- **Boundary-safe**: Yes (purely local, function-private)
- **Rationale**: `std::array` provides bounds-checked `operator[]` in debug mode, value-initialises to zero, and matches the pattern used everywhere else in the subsystem.

### M-3: `md5_load_block` and `md5_transform` use decayed-array parameters

- **File**: `xash3dpp/src/utilities/hash.cpp` lines ~155, 163
- **Current pattern**:
  ```cpp
  static void md5_load_block( std::uint32_t w[16], const std::uint8_t in[64] ) noexcept
  static void md5_transform ( std::uint32_t buf[4], const std::uint8_t in[64] ) noexcept
  ```
  Both array parameters silently decay to raw pointers. The local `std::uint32_t w[16]` at line 166 is also a raw C array.
- **Suggested replacement**:
  ```cpp
  static void md5_load_block( std::span<std::uint32_t, 16>       w,
                               std::span<const std::uint8_t, 64>  in ) noexcept
  static void md5_transform ( std::span<std::uint32_t, 4>        buf,
                               std::span<const std::uint8_t, 64>  in ) noexcept
  ```
  Local `w` → `std::array<std::uint32_t, 16> w{}`. Call sites from `md5_update`/`md5_final` already hold `std::array` members and can pass `.data()` or construct a span directly.
- **Boundary-safe**: Yes (file-private helpers)
- **Rationale**: Fixed-extent `std::span<T, N>` makes the size constraint visible to the compiler and eliminates silent pointer decay. Pairs naturally with the `std::array` members of `Md5State`.

### M-4: `angle_vectors` uses three nullable output pointers → named return struct

- **File**: `xash3dpp/include/xash3dpp/utilities/math.hpp` lines ~91–94,
  `xash3dpp/src/utilities/matrix.cpp` (implementation)
- **Current pattern**:
  ```cpp
  void angle_vectors( const Vec3 &angles,
                      Vec3 *fwd, Vec3 *right, Vec3 *up ) noexcept;
  ```
  Each output pointer is optional (callers pass `nullptr` for components they don't need).
- **Suggested replacement**: Define a plain aggregate and return it:
  ```cpp
  struct AngleVectors { Vec3 fwd, right, up; };
  AngleVectors angle_vectors( const Vec3 &angles ) noexcept;
  ```
  Callers that only need one component name the binding explicitly: `auto [fwd, r, u] = angle_vectors(a);`.
- **Boundary-safe**: Yes (engine-internal)
- **Open question**: If callers want to pass only `fwd` they currently skip allocating the others. The struct return always computes all three. Check whether any hot-path caller intentionally skips two components; if so, consider three separate functions (`fwd_vector`, `right_vector`, `up_vector`) instead.
- **Rationale**: Removes nullable-out-pointer idiom, enables structured bindings, makes the return layout explicit and value-typed.

### M-5: `xash::utilities::clamp` is redundant — `std::clamp` is already available (2-J deletion)

- **File**: `xash3dpp/include/xash3dpp/utilities/math.hpp` line ~52
- **Current pattern**:
  ```cpp
  template<typename T>
  constexpr T clamp( T v, T lo, T hi ) noexcept { return v < lo ? lo : v > hi ? hi : v; }
  ```
- **Why it exists**: Predates C++17 in the original codebase.
- **Modern replacement**: `std::clamp<T>(v, lo, hi)` — available since C++17, already `constexpr`, in `<algorithm>` which is already included in `math.hpp`.
- **Deletion cost**: 0 external call sites currently exist inside `xash3dpp/`. The function can be removed with no downstream changes. Any future caller just writes `std::clamp`.
- **Boundary-safe**: Yes
- **Rationale**: Dead helper. Keeping it risks future callers unknowingly using the shadow name instead of the stdlib version, which could cause subtle ADL surprises.

### M-6: `utf::length(const char *)` should accept `std::string_view`

- **File**: `xash3dpp/include/xash3dpp/utilities/utf.hpp` line ~43,
  `xash3dpp/src/utilities/utf.cpp` line ~133
- **Current pattern**:
  ```cpp
  std::size_t length( const char *s ) noexcept; // null-checks internally
  ```
- **Suggested replacement**:
  ```cpp
  std::size_t length( std::string_view s ) noexcept;
  ```
  Drop the `if( !s ) return 0;` guard — `string_view` is never a null pointer.
- **Boundary-safe**: Yes
- **Rationale**: Consistent with the rest of the API surface that was already migrated to `string_view`. Callers with a known non-null `const char*` construct `string_view` at the call site (zero cost).

---

## Low-priority / cosmetic opportunities

| # | File | Pattern | Replacement |
|---|------|---------|-------------|
| L-1 | `path.cpp` ~line 17 | `find_extension` internal helper uses `std::strlen` + pointer arithmetic over `const char*` | Rewrite using `std::string_view` range operations; eliminates the `std::strlen` call |
| L-2 | `utf.cpp` ~line 178 | `to_cp1251` / `to_cp1252` codepage lookups use a manual `for(size_t i=0; i<64; ++i)` linear scan | `std::ranges::find_if` on the table, or keep as-is (table is tiny; no performance concern) |
| L-3 | `string.cpp` ~line 21 | `skip_spaces` helper returns `const char*` — only used internally by `atoi`/`atof` | Acceptable as-is; not exposed in the header. Low value change |

---

## Out of scope / ABI-frozen

None. The entire utilities subsystem is engine-internal (`xash::utilities` / `xash::utilities::utf` / `xash::utilities::build` namespaces). No symbol in these files is directly `extern "C"` exported or referenced through the frozen `engine/eiface.h` / `cdll_int.h` surfaces.

`crc32_block_sequence` is called through `enginefuncs_t::pfnCRC32_Block` at the shim layer — the C++ implementation signature is **not** part of the frozen ABI.

---

## Open questions

1. **`angle_vectors` hot-path callers (M-4)**: Does any performance-sensitive call site deliberately pass `nullptr` for two of the three outputs to avoid computing them? If yes, three focused free functions (`fwd_vector`, `right_vector`, `up_vector`) are a better split than a struct return.

2. **`md5_step` NTTP (H-1)**: GCC and Clang accept `constexpr` free-function pointers as NTTPs in C++20. MSVC support is solid from VS 2019 16.9+. Verify the CI toolchain version before landing.

3. **`xash::utilities::clamp` vs. unqualified `clamp` (M-5)**: After deletion, any code in `namespace xash::utilities` that writes unqualified `clamp(…)` will fall through to `std::clamp` via ADL if `<algorithm>` is included. Confirm no file in the tree does this accidentally before deleting the helper.
