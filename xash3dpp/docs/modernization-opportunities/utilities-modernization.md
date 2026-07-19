# Utilities Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`, `CMAKE_CXX_STANDARD 23`)
> Boundary spec: `docs/boundaries/utilities-boundary.md`
> ABI-frozen symbols in this subsystem: **None** — all utilities are `xash3dpp`-internal.
> The only external contract is that four CRC32 free functions can be bound
> into `enginefuncs_t` as function pointers via a fill-site shim; the
> implementation is otherwise unconstrained (boundary spec §External ABI).

> Refreshed 2026-07-06 (as-built pass). Re-scanned `src/utilities/**` and the
> public headers. **Status:** none of the previously-listed opportunities have
> been implemented since the last revision — H-1, M-1..M-3 and L-1..L-5 are all
> still open exactly as written, and the two implementation gaps (`swap_struct`,
> `gameinfo_parser`) remain (the latter confirmed by `stub_scan.py utilities`:
> 4 `// TODO` markers, all in `gameinfo_parser.cpp`). The C++ standard reference
> is still accurate (C++23). One **new** opportunity is added this pass (N-1,
> a latent `string_view` over-read in the `ci_less` comparator). Per-item
> as-built confirmations are inlined below.

## Summary

The utilities subsystem (`string`, `path`, `math`, `matrix`, `quaternion`,
`hash`, `utf`, `swap`, `atlas`, `build`, `dynlib`, `gameinfo_parser`) is
already highly modern after several prior passes: `std::array`, `std::span`,
`std::optional`, `std::string_view`, `enum class` with bitwise operators,
`std::numbers`, `std::byteswap`/`std::endian` (in `swap.hpp`), magic-static
initialisation, RAII hasher wrappers, and template-based MD5 rounds are all in
place. Every finding that the previous revision of this report ranked High or
Medium (missing `vec_to_yaw`/`vector_angles`, duplicate `DEG2RAD`, the raw
`atov`/`atoi`/`atof` overloads, the `Tokenizer` C-array buffer) has since been
resolved in the source.

What remains is a small tail: one clearly-superseded raw-buffer overload that
is now a pure deletion candidate, two fixed-size stack buffers / raw
pointer+length pairs that could become `std::string`/`std::span`, and a handful
of cosmetic C-array-to-`std::array` and duplicate-attribute cleanups. There are
also two **implementation gaps** (`swap_struct` and the `gameinfo_parser`
functions are declared/stubbed but unimplemented) — these are noted separately
because they are missing code, not C-style code to modernize.

Non-obvious constraint found: much of the "C-looking" code here is
**deliberately** C-shaped and must stay that way. The `double`-precision trig in
`math.hpp`/`quaternion.cpp`/`matrix.cpp` is bit-parity-load-bearing (studio bone
math), the CRT mirrors (`strncpy`, `snprintf`, `parse_token`) intentionally
match libc signatures, and the raw `(char*, size)` path overloads are
documented as hot-path siblings of the `string_view` versions.

______________________________________________________________________

## High-priority opportunities

### H-1: Delete the raw `strip_colors(const char *, char *)` overload (2-J)

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` line ~104;
  `xash3dpp/src/utilities/string.cpp` lines ~205–216 (raw impl) and ~500–515
  (`std::string_view` impl).

- **Current pattern**: Two overloads coexist —

  ```cpp
  void        strip_colors( const char *in, char *out ) noexcept;  // raw, caller-owned out buffer
  std::string strip_colors( std::string_view in ) noexcept;        // owning return
  ```

  The raw overload writes into a caller-supplied buffer with no size argument
  (a latent overflow if `out` is smaller than `in`). Its **only** caller is its
  own unit test (`tests/utilities/test_string.cpp::test_strip_colors`, which
  passes a `char out[32]`); no production code uses it.

- **Suggested replacement**: Delete the raw overload from the header and
  `string.cpp`. Update the two raw-buffer assertions in `test_strip_colors` to
  use the `std::string_view` → `std::string` version (the test already exercises
  that overload immediately below).

- **Boundary-safe**: Yes — engine-internal, no production callers.

- **Rationale**: Removes a sizeless shared-output-buffer API whose safe
  replacement already exists and is already the one every real caller uses.
  Classic 2-J deletion: the function disappears and the single call site becomes
  one idiomatic expression.

______________________________________________________________________

## Medium-priority opportunities

### M-1: `pretify_mem` fixed `char val[32]` stack buffer → `std::format`

- **File(s)**: `xash3dpp/src/utilities/string.cpp` lines ~237–262.

- **Current pattern**:

  ```cpp
  char val[32];
  if( is_integral )
      std::snprintf( val, sizeof val, "%d", static_cast<int>( value + 0.5f ) );
  else
      std::snprintf( val, sizeof val, "%.*f", decimals, static_cast<double>( value ) );
  const char *dot = std::strchr( val, '.' );
  ...
  ```

  The result is already a `std::string`; only the numeric formatting step still
  round-trips through a fixed C buffer plus `strchr`/`strlen` walking.

- **Suggested replacement**: Format the numeric portion with `std::format`
  (C++23) into a `std::string`, then run the existing thousands-separator pass
  over that string instead of over `char val[]`:

  ```cpp
  const std::string num = is_integral
      ? std::format( "{}", static_cast<int>( value + 0.5f ) )
      : std::format( "{:.{}f}", static_cast<double>( value ), decimals );
  const std::size_t dot = num.find( '.' );
  const int span = static_cast<int>( dot == std::string::npos ? num.size() : dot );
  ```

- **Boundary-safe**: Yes.

- **Rationale**: Removes the last fixed-size stack buffer in `string.cpp` and
  the `strchr`/`strlen` pointer walk; `std::format` is bounds-safe and its
  format string is checked at compile time.

### M-2: `crc32_block_sequence(const std::uint8_t *, int, int)` → `std::span`

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/hash.hpp` line ~45;
  `xash3dpp/src/utilities/hash.cpp` (function body, C-array + `memcpy`).

- **Current pattern**: A raw `base` pointer plus a separate `int length` (with
  an internal `if( length > 60 ) length = 60;` clamp) describe the input block:

  ```cpp
  std::uint8_t crc32_block_sequence( const std::uint8_t *base, int length, int sequence ) noexcept;
  ```

- **Suggested replacement**: Accept `std::span<const std::uint8_t> block` (the
  length travels with the pointer; the `> 60` clamp becomes
  `block = block.first( std::min<std::size_t>( block.size(), 60 ) )`). The
  internal `std::array<std::uint8_t, 64> buffer` staging is already modern.

- **Boundary-safe**: Needs verification — this reproduces legacy
  `CRC32_BlockSequence`, used for demo/resource integrity hashes on the wire.
  The *hash output* must stay byte-identical; the *signature* is engine-internal
  and free to change once the (currently zero) call sites are known.

- **Rationale**: Ties length to the buffer, eliminating the mismatched-length
  footgun; parity is preserved because only parameter passing changes.

### M-3: Raw in-place `(char *, size)` path mutators duplicate the `string_view` API

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/path.hpp` lines ~15–52
  (raw overloads) vs ~60–72 (`std::string` overloads);
  `xash3dpp/src/utilities/path.cpp` throughout.

- **Current pattern**: Every path helper exists twice — a raw caller-owned-buffer
  form and an owning `string_view` → `std::string` form:

  ```cpp
  void file_base( const char *path, char *out, std::size_t size ) noexcept;   // raw
  ...
  [[nodiscard]] std::string file_base( std::string_view path );               // owning
  ```

  The header comments the raw set as intentionally kept "for hot paths", but the
  known production callers (`filesystem/backends/wad_backend.cpp`,
  `platform/{win32,posix}/sys.cpp`) all use the `std::string`/`string_view`
  forms.

- **Suggested replacement**: Audit for remaining raw-form call sites. For each
  raw overload with zero non-test callers, delete it (2-J); the private
  `find_extension(const char *)` helper (path.cpp line ~17) then also becomes
  deletable (see L-4). Keep only the raw forms a measured hot path actually
  needs.

- **Boundary-safe**: Needs verification — depends on the raw-form call-site
  census across the whole `xash3dpp/` tree, including subsystems not yet written.

- **Rationale**: Two overloads per operation, one of them a sizeless/manual
  buffer form, doubles the surface and invites accidental use of the unsafe one.
  Deletion-driven simplification once callers are confirmed.

### M-4: `ci_less` comparator over-reads past `string_view` bounds (new 2026-07-06)

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` lines ~50–56.

- **Current pattern**: the case-insensitive ordering comparator forwards to the
  C-string `strnicmp` with a length one past the *longer* view:

  ```cpp
  [[nodiscard]] inline bool ci_less( std::string_view a, std::string_view b ) noexcept
  {
      const std::size_t n = ( a.size() > b.size() ? a.size() : b.size() ) + 1;
      return strnicmp( a.data(), b.data(), n ) < 0;   // reads up to n bytes from each
  }
  ```

  `strnicmp` reads up to `n` bytes from `a.data()`/`b.data()`, but a
  `std::string_view` is **not guaranteed null-terminated** — a view into the
  middle of a larger buffer (e.g. a token slice, or `trim_sv` output) has no
  `'\0'` at `data()+size()`. When the two views differ in length, the shorter
  one is read `> size()` bytes, which is a latent out-of-bounds read (OWASP
  buffer-over-read) on any non-terminated view. Today's callers happen to pass
  null-terminated backing strings, which is why it has not surfaced.

- **Suggested replacement**: implement the comparison in terms of the views'
  own sizes rather than a C-string length, e.g. a bounded lexicographic
  compare over `std::min(a.size(), b.size())` lowercased bytes with a
  length-tiebreak, or `std::ranges::lexicographical_compare` with a
  case-insensitive predicate. `ci_equal` (immediately below) already does the
  bounded thing correctly (`a.size() != b.size()` early-out, then
  `strnicmp(..., a.size())`) and is the shape to mirror.

- **Boundary-safe**: Yes — engine-internal comparator; behaviour for
  null-terminated inputs is unchanged, the fix only removes the over-read on
  non-terminated views.

- **Rationale**: Turns a `string_view`-shaped API that secretly requires
  null-termination into one that is actually safe for arbitrary views — the
  whole point of taking `string_view`. **Recommendation only; no source edit is
  made in this pass** (analysis/doc task). Flag for the cross-cutting synthesis
  as a repeated "C-string function fed a non-terminated `string_view`" pattern
  to sweep across subsystems.

______________________________________________________________________

## Low-priority / cosmetic opportunities

| ID | File(s) | Current | Suggested | Boundary-safe | Rationale |
|----|---------|---------|-----------|---------------|-----------|
| L-1 | `src/utilities/hash.cpp` lines ~16–79 | `static constexpr std::uint32_t k_crc32table[256] = { … };` (C array) | `static constexpr std::array<std::uint32_t, 256> k_crc32table{ … };` | Yes | Matches the `std::array` convention already used for `k_cp1251_table` (utf.cpp) and MD5 state; `[]` indexing is unchanged. |
| L-2 | `include/xash3dpp/utilities/math.hpp` line ~22 | `[[nodiscard]] [[nodiscard]] constexpr vec_t dot( … )` — attribute written twice | Remove the duplicate `[[nodiscard]]`. | Yes | Harmless but obviously accidental; some compilers warn on repeated attributes. |
| L-3 | `include/xash3dpp/utilities/swap.hpp` lines ~86–92 | `const SwapField *subdef{};` — `nullptr` means "not a sub-struct", paired with `int32_t size` where `< 0` flags recursion | `std::span<const SwapField> subdef{};` — empty span is the sentinel; `size < 0` test becomes `!subdef.empty()` | Yes (engine-internal) | Removes a raw pointer + magic-sign convention. **Defer until `swap_struct` is implemented** (see Implementation gaps) so the descriptor shape is designed once. |
| L-4 | `src/utilities/path.cpp` line ~17 | `static const char *find_extension( const char *path )` — null-guarded pointer walk used only by the raw path overloads | Delete alongside the raw path overloads (M-3), or re-express as a `std::string_view` helper mirroring `file_extension`. | Yes | Duplicates `file_extension(std::string_view)` logic; a maintenance hazard while both exist. |
| L-5 | `include/xash3dpp/utilities/swap.hpp` lines ~24–41 | `swap_bytes( void *p, std::size_t size )` — runtime `switch(size)` byte reversal | Leave as-is for the dynamic-width reflection path; new codecs should prefer the already-modern templated `read_le<T>`/`write_le<T>` (which fold to `std::byteswap`). | Yes | No change needed; documented so it is not mistaken for an oversight. The typed path already exists. |

______________________________________________________________________

## Out of scope / ABI-frozen

These look modernizable but are deliberately C-shaped and should **not** change:

- **`strncpy( char *, const char *, std::size_t )`** (string.hpp/.cpp) — a CRT
  mirror (`Q_strncpy`) that always null-terminates. Callers rely on the exact
  libc-like signature; changing it defeats its drop-in purpose.
- **`snprintf` / `vsnprintf`** (string.hpp) — variadic CRT mirrors; carry
  `compliance-allow(nodiscard-missing)` for `Q_snprintf`/`Q_vsnprintf` parity.
- **`parse_token( const char *data, char *token, std::size_t, … )`**
  (string.hpp) — the single-step tokeniser is the hot inner primitive that the
  `Tokenizer` class wraps; its `char*` output buffer is intentional. The
  ergonomic `Tokenizer` (already `std::array`-backed, `std::optional`-returning)
  is the recommended surface.
- **CRC32 free functions** (`crc32_init/update/final`) — bound into
  `enginefuncs_t` as function pointers via a fill-site shim (boundary spec).
  Their signatures mirror the frozen game-DLL slots.
- **`const void *` byte-buffer parameters** in `crc32_update` / `md5_update` —
  generic "hash these bytes" APIs; the `std::span<const std::byte>` ergonomic
  form already exists on the `Crc32Hasher`/`Md5Hasher` wrappers.
- **`void **slot` in `ExportEntry`** (dynlib.hpp) — deliberate type erasure for
  a heterogeneous export table filled after `LoadLibrary`/`dlopen`. Typing it
  would require templating every plugin loader; the erased slot is the point.

______________________________________________________________________

## Implementation gaps (not modernization — missing code)

Flagged so they are not mistaken for C-style code awaiting a rewrite:

- **`swap_struct( void *, std::span<const SwapField> )`** is declared in
  `swap.hpp` but has **no definition** (no `swap.cpp` in
  `src/utilities/CMakeLists.txt`; `tests/utilities/test_swap.cpp` and the
  architecture docs both note this). Nothing calls it, so there is no linker
  hazard today. When it is written, adopt L-3 (`std::span` `subdef`) at the same
  time.
- **`gameinfo_parser`** functions (`parse_gameinfo_txt`, `parse_liblist_gam`,
  `serialise_gameinfo`, and the `dll_path`/`title` derivation in
  `apply_gameinfo_fixups`) are `// TODO` stubs returning `std::nullopt`/`{}`.
  Unfinished implementation, not modernization scope.

______________________________________________________________________

## Open questions

1. **M-3 raw path-overload census.** The `path.hpp` header states the raw
   `(char*, size)` overloads exist "for hot paths." Are any actually on a
   measured hot path, or are they legacy scaffolding that every current caller
   already bypasses via the `std::string` forms? The answer decides whether M-3
   is a clean deletion or a keep-with-justification.

1. **`crc32_block_sequence` wire callers (M-2).** No caller exists in the tree
   yet. Before changing its signature to `std::span`, confirm the intended
   demo/resource-integrity call sites so the hash output contract is pinned by a
   test first.

1. **UTF decoder `0` overload — valid U+0000 vs invalid sequence.** Both
   `decode_utf8`/`decode_utf16` (and the `Utf8Decoder::feed`/`Utf16Decoder::feed`
   wrappers) return `0` / `optional{0}` for a genuine U+0000 **and** for an
   invalid byte sequence. If any consumer must distinguish "valid NUL" from
   "malformed input", the return type should become
   `std::expected<std::uint32_t, Utf8Error>` (C++23). Needs a design decision
   before the encoding boundary is finalised.

1. **`swap_struct` descriptor shape (L-3).** When `swap_struct` is implemented,
   should `SwapField::subdef` be a `std::span<const SwapField>` (empty = leaf)
   instead of a raw pointer + `size < 0` sentinel? Decide at implementation time
   so the reflection table is designed once.
