# Utilities Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`, `CMAKE_CXX_STANDARD 23`)
> Boundary spec: `docs/boundaries/utilities-boundary.md`
> ABI-frozen symbols in this subsystem: **None** — all utilities are `xash3dpp`-internal.
> The only external contract is that four CRC32 free functions can be bound
> into `enginefuncs_t` as function pointers via a fill-site shim; the
> implementation is otherwise unconstrained (boundary spec §External ABI).

> Refreshed 2026-07-20 (tree-wide modernization audit, Phase-2 adversarial
> pass, HEAD `cc73c054`). H-1, M-1, M-2, and L-1/L-2/L-3/L-5 were
> re-verified against source and are **CONFIRMED unchanged** since 2026-07-06.
> M-3 is **materially amended**: a full tree-wide caller census (not just a
> production/test split) found that only 5 of the 8 raw `(char*, size)` path
> overloads are dead everywhere; `fix_slashes` has a genuine production
> caller and must be kept; `strip_extension` should also be kept, because a
> *different, weaker* private reimplementation of it exists in
> `server/lifecycle/spawn.cpp` (new finding M-6) rather than that call site
> using this one. The open question narrows to one remaining overload,
> `file_base` — see M-3/Open Question 1. L-4 is re-scoped to match. M-4
> remains **RESOLVED** (HB-1, unchanged since 2026-07-19). Two **new**
> opportunities are added this pass: **M-5**, `utilities::append_le<T>` (a
> same-shape sibling of the existing `read_le`/`write_le`), proposed to
> absorb three duplicated `put_u16`/`put_u32`/`put_byte` lambda sets in
> `content/imagelib`'s codec save paths (L11 subtraction lens, SUB-8f) — a
> named, exists-in-tree consumer; and **M-6**, the `spawn.cpp`
> `strip_extension` duplicate above. The two implementation gaps
> (`swap_struct`, `gameinfo_parser`) remain untouched. The C++ standard
> reference is still accurate (C++23).

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
is now a pure deletion candidate, a path-overload family where a full-tree
census now separates "genuinely dead" from "hot-path-shaped and load-bearing"
(see M-3), a private duplicate of one of those kept path overloads living
outside the subsystem (M-6), one fixed-size stack buffer that could become
`std::format`, one small proposed addition with a named exists-in-tree
consumer (M-5, `append_le<T>`), and a handful of cosmetic
C-array-to-`std::array` and duplicate-attribute cleanups. There are also two
**implementation gaps** (`swap_struct` and the `gameinfo_parser` functions are
declared/stubbed but unimplemented) — these are noted separately because they
are missing code, not C-style code to modernize. Highest tier observed this
pass: **H-1** (unchanged); **M-6** (max Medium, two new this pass: M-5, M-6);
**L-5** (max Low, unchanged).

Non-obvious constraint found: much of the "C-looking" code here is
**deliberately** C-shaped and must stay that way. The `double`-precision trig in
`math.hpp`/`quaternion.cpp`/`matrix.cpp` is bit-parity-load-bearing (studio bone
math), the CRT mirrors (`strncpy`, `snprintf`, `parse_token`) intentionally
match libc signatures, and the raw `(char*, size)` path overloads are
documented as hot-path siblings of the `string_view` versions.

______________________________________________________________________

## High-priority opportunities

### H-1: Delete the raw `strip_colors(const char *, char *)` overload (2-J)

> **Re-verified 2026-07-20.** Unchanged since the last pass: the raw overload
> is still declared at `string.hpp:125` and defined at `string.cpp:208-219`,
> and a tree-wide grep (`src/`, `include/`, `tests/`) still finds exactly two
> callers — `tests/utilities/test_string.cpp:144` and `:147` — both inside
> `test_strip_colors()`, which already exercises the `string_view` overload
> immediately below. `blast_radius` is exactly 2. Still not an ABI obligation
> (`COM_StripColors` is not an `eiface`/engine-table slot) and unrelated to the
> subsystem's HB-2 fenced kernel (double-precision studio math, not string
> handling).

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/string.hpp` line 125
  (raw decl); `xash3dpp/src/utilities/string.cpp` lines 208–219 (raw impl)
  and its `std::string_view` sibling further down the same file;
  `xash3dpp/tests/utilities/test_string.cpp` lines 144, 147 (the only two
  callers, both in `test_strip_colors()`).

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

> **Re-verified 2026-07-20.** Unchanged: `string.cpp:236-248` still round-trips
> through `char val[32]` via `snprintf` then `strchr`/`strlen`, before a
> character-by-character copy into the `std::string` result. Not part of the
> studio-math fence — `pretify_mem` is number formatting.

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

> **Re-verified 2026-07-20.** Unchanged: signature is still
> `(const std::uint8_t *, int length, int sequence)` and a tree-wide grep
> still finds **zero callers**, production or test — only the declaration,
> definition, and doc cross-references mention the name. Confirmed
> `gold-plate: speculative` — this stays a recorded shape constraint (open
> question 2 below), never work, until a demo/resource-integrity caller
> exists.

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

### M-3: Raw in-place `(char *, size)` path mutators duplicate the `string_view` API (amended 2026-07-20)

> **AMENDED.** The prior pass treated all 8 raw overloads as one undifferentiated
> census question. A full tree-wide caller grep (not just production vs. test)
> now splits them three ways, and one of the three groups **must not be
> deleted** — the opposite of the previous framing's implicit "delete once
> confirmed dead" default. This changes what M-3 actually recommends; the open
> question it fed (Open Question 1, below) is now mostly resolved.

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/path.hpp` lines 15–52
  (raw overloads) vs. 59–65 (`std::string` overloads);
  `xash3dpp/src/utilities/path.cpp` (raw impls at lines 36, 82, 94, 101, 144,
  153, 162, 175); `xash3dpp/tests/utilities/test_path.cpp`;
  `xash3dpp/src/server/lifecycle/precache.cpp` line 37.

- **Current pattern**: Every path helper exists twice — a raw caller-owned-buffer
  form and an owning `string_view` → `std::string` form:

  ```cpp
  void file_base( const char *path, char *out, std::size_t size ) noexcept;   // raw
  ...
  [[nodiscard]] std::string file_base( std::string_view path );               // owning
  ```

  A tree-wide grep of all 8 raw names (`file_base`, `default_extension`,
  `replace_extension`, `extract_dir`, `strip_extension`, `fix_slashes`,
  `remove_line_feed`, `trim_space`) against every `.cpp`/`.hpp` in `xash3dpp/`
  splits them into three groups:

  1. **Zero callers anywhere, including their own tests (5):**
     `default_extension`, `replace_extension`, `extract_dir`,
     `remove_line_feed`, `trim_space`. `replace_extension`'s raw form even
     calls `default_extension`'s raw form internally (`path.cpp:98`), so both
     halves of that internal chain are dead together.
  2. **One live production caller (1):** `fix_slashes( char * )`. Called from
     `prepare_name()` in `src/server/lifecycle/precache.cpp:37` over
     `dst` — a `char[k_qpath]` stack array reference parameter
     (`precache.cpp:32`), the `SV_ModelIndex`/`SV_SoundIndex`
     name-preparation path. Switching this
     call to the `string_view` overload (`path.cpp:214`) would add a heap
     allocation per precache name where today there is none — this is exactly
     the "hot path" the header comment claims, verified rather than assumed.
  3. **One test-only caller each in `utilities` itself (2):** `file_base`,
     `strip_extension` — each called exactly once, from its own unit test in
     `test_path.cpp`, with no production caller of the *utilities* function.
     `strip_extension` is a partial exception: `server/lifecycle/spawn.cpp`
     has its own private reimplementation of the same operation instead of
     calling this one (see M-6) — so "no production caller" is true of the
     utilities entry point, but the operation itself is production-used
     elsewhere under a different, weaker implementation.

- **Suggested replacement**: Delete the 5 group-1 overloads outright (2-J) —
  `default_extension`, `replace_extension`, `extract_dir`, `remove_line_feed`,
  `trim_space` — together with the now-orphaned internal call from
  `replace_extension`'s raw form into `default_extension`'s raw form
  (`path.cpp:94-98`). **Keep `fix_slashes( char * )`**: it has a genuine
  production caller over a fixed stack buffer and deleting it would introduce
  an allocation on a precache-time path with no behavioural upside. **Keep
  `strip_extension( char * )` too**, but not as-is: M-6 finds a divergent
  private copy of it in `server/lifecycle/spawn.cpp`; consolidate that copy
  onto this one instead of deleting either. `file_base` is now the sole
  overload with a genuinely open fate — see Open Question 1.

- **Boundary-safe**: Yes for the 5-overload deletion (dead code, no call-site
  ambiguity left). Yes for keeping `fix_slashes` and `strip_extension`.
  NeedsVerification only for `file_base` (Open Question 1).

- **Rationale**: The previous framing risked deleting a genuine hot-path
  overload along with the dead ones because "audit for remaining raw-form call
  sites" had not yet been done exhaustively. Doing that census first turns a
  single all-or-nothing decision into a safe majority deletion (5 of 8), two
  confirmed keeps (`fix_slashes`, `strip_extension`), and one genuinely open
  overload (`file_base`, 1 of 8, not 8).

### M-4: `ci_less` comparator over-reads past `string_view` bounds (new 2026-07-06)

> **RESOLVED 2026-07-19 (HB-1, consolidation audit).** `string.hpp` now
> implements a bounded three-way `ci_compare(string_view, string_view)`
> (view-size-bounded lexicographic compare, ASCII A-Z fold, length
> tiebreak) and `ci_less`/`ci_equal` forward to it. Ordering for
> NUL-terminated inputs is unchanged; the over-read on non-terminated
> views is gone. Covered by `test_ci_compare` in
> `tests/utilities/test_string.cpp` (non-terminated slice + embedded-NUL
> cases). The filesystem M-7 consumer inherits the fix through `ci_less`.
> Retained below for the record.

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

### M-5: Add `utilities::append_le<T>` to absorb three duplicated codec-write lambda sets (new 2026-07-20)

- **File(s)**: `xash3dpp/include/xash3dpp/utilities/swap.hpp` lines 50–80
  (existing `read_le`/`write_le`, the shape to mirror);
  `xash3dpp/src/content/imagelib/codec_bmp.cpp` lines 363–371;
  `xash3dpp/src/content/imagelib/codec_tga.cpp` lines 278–279;
  `xash3dpp/src/content/imagelib/codec_wad.cpp` lines 178–190.

- **Current pattern**: Three of imagelib's four save-path codecs each define
  their own local `put_u16`/`put_u32`/`put_byte` lambda trio that appends to a
  `std::vector<std::byte> &out`, all three functionally identical modulo
  variable names:

  ```cpp
  // codec_bmp.cpp:363-371 (near-identical bodies also in codec_tga.cpp,
  // codec_wad.cpp)
  auto put_u16 = [&]( std::uint16_t v ) {
      out.push_back( std::byte{ static_cast<std::uint8_t>( v ) } );
      out.push_back( std::byte{ static_cast<std::uint8_t>( v >> 8 ) } );
  };
  auto put_u32 = [&]( std::uint32_t v ) { /* same shape, 4 bytes */ };
  auto put_byte = [&]( std::uint8_t v ) { out.push_back( std::byte{ v } ); };
  ```

  `swap.hpp` already has the read-side and pointer-write-side of this exact
  operation (`read_le<T>`, `write_le<T>`) but nothing that appends to a
  growable buffer, which is what every save path actually needs.

- **Suggested replacement**: Add one function beside the existing pair:

  ```cpp
  template<std::integral T>
  void append_le( std::vector<std::byte> &out, T v ) noexcept
  {
      const std::size_t at = out.size();
      out.resize( at + sizeof( T ) );
      write_le<T>( out.data() + at, v );
  }
  ```

  Replace the three local lambda trios with `ut::append_le<std::uint16_t>(out, v)` /
  `ut::append_le<std::uint32_t>(out, v)` / `ut::append_le<std::uint8_t>(out, v)`
  at their call sites.

- **Boundary-safe**: Yes — engine-internal, little-endian byte layout is
  unchanged (folds to the same `std::memcpy`/`std::byteswap` `write_le` already
  uses); not part of the HB-2 double-precision-studio-math fence.

- **Rationale**: Named, exists-in-tree consumer per the anti-gold-plating rule
  (`consumer_status: exists-in-tree`, 3 call sites today) — this is not a
  speculative addition. Found by the L11 subtraction lens (SUB-8f) alongside
  five other tree-wide duplication clusters; `append_le` is the one of the six
  whose home is `utilities` (L0, every codec already links it). Removes ~15
  lines of duplicated lambda bodies and, unlike the three local copies, gets
  bounds-safe growth for free from `std::vector::resize`.

### M-6: `server/lifecycle/spawn.cpp` reimplements `utilities::strip_extension` locally instead of calling it (new 2026-07-20)

- **File(s)**: `xash3dpp/src/server/lifecycle/spawn.cpp` lines 64–75 (local
  reimplementation), line 310 (its one call site); vs.
  `xash3dpp/src/utilities/path.cpp` lines 17–30 (`find_extension`, the
  path-separator-aware helper) and 144–151 (`utilities::strip_extension`,
  the function being duplicated).

- **Current pattern**: `spawn.cpp` defines its own anonymous-namespace
  `strip_extension( char *path )` that scans for the last `'.'` and truncates
  there — the same operation `utilities::strip_extension( char * )` already
  provides — instead of calling the utilities function:

  ```cpp
  // spawn.cpp:64-75
  // COM_StripExtension (public/crtlib.c): truncate at the last '.' (legacy
  // scans backward for the first '.' — i.e. the last one — with no path-
  // separator check; map names carry no directory here so the quirk is inert).
  void strip_extension( char *path ) noexcept
  {
      int last_dot = -1;
      for ( int i = 0; path[i] != '\0'; ++i )
          if ( path[i] == '.' )
              last_dot = i;
      if ( last_dot >= 0 )
          path[last_dot] = '\0';
  }
  ```

  The two implementations are **not** byte-for-byte equivalent:
  `utilities::strip_extension` goes through `find_extension`, which resets its
  dot-tracking at every `/`/`\\` (a dot inside a directory component does not
  count as an extension separator); `spawn.cpp`'s local copy scans the whole
  string with no path-separator awareness. The in-code comment already
  acknowledges the divergence and argues it is inert because the one call site
  (`spawn.cpp:312`, on `rt.level.name`, a bare map name with no directory
  component) never exercises it — but that argument lives in a comment, not in
  a shared implementation, so nothing prevents the next caller of this local
  copy from being a path with a directory component.

- **Suggested replacement**: Delete the local `strip_extension` and call
  `xash::utilities::strip_extension( rt.level.name )` at the existing call
  site (`spawn.cpp:312`); it is `noexcept` with the same `char *` signature,
  so the substitution is direct. This removes the divergent behaviour rather
  than continuing to reason about why it happens not to matter today.

- **Boundary-safe**: Yes — both functions are engine-internal string
  manipulation; `utilities::strip_extension` is exactly the safer (path-aware)
  superset of what `spawn.cpp`'s copy does for the one input it currently
  receives.

- **Rationale**: A second, behaviourally-narrower private copy of a function
  that already exists in the shared utility library is exactly the pattern
  `decisions-architecture.md §4.3`'s "second copy goes in a shared header on
  first duplication, not the fifth" rule targets — found here on the *second*
  copy, before it had the chance to reach a fifth. It also strengthens the
  answer to Open Question 1: `utilities::strip_extension( char * )` is not
  merely "test-only" — a second, weaker inline copy of its logic already
  exists in production, which argues for consolidating onto the utilities
  version rather than deleting it.

______________________________________________________________________

## Low-priority / cosmetic opportunities

| ID | File(s) | Current | Suggested | Boundary-safe | Rationale |
|----|---------|---------|-----------|---------------|-----------|
| L-1 | `src/utilities/hash.cpp` lines ~16–79 | `static constexpr std::uint32_t k_crc32table[256] = { … };` (C array) | `static constexpr std::array<std::uint32_t, 256> k_crc32table{ … };` | Yes | Matches the `std::array` convention already used for `k_cp1251_table` (utf.cpp) and MD5 state; `[]` indexing is unchanged. |
| L-2 | `include/xash3dpp/utilities/math.hpp` line ~22 | `[[nodiscard]] [[nodiscard]] constexpr vec_t dot( … )` — attribute written twice | Remove the duplicate `[[nodiscard]]`. | Yes | Harmless but obviously accidental; some compilers warn on repeated attributes. |
| L-3 | `include/xash3dpp/utilities/swap.hpp` lines ~86–92 | `const SwapField *subdef{};` — `nullptr` means "not a sub-struct", paired with `int32_t size` where `< 0` flags recursion | `std::span<const SwapField> subdef{};` — empty span is the sentinel; `size < 0` test becomes `!subdef.empty()` | Yes (engine-internal) | Removes a raw pointer + magic-sign convention. **Defer until `swap_struct` is implemented** (see Implementation gaps) so the descriptor shape is designed once. |
| L-4 | `src/utilities/path.cpp` line 17 | `static const char *find_extension( const char *path )` — path-separator-aware pointer walk, called from `default_extension` (path.cpp:87, dead per M-3) and `strip_extension` (path.cpp:148, kept per M-3/M-6) | Cannot be deleted outright — `strip_extension` is being kept, not removed. Delete only the now-dead call from `default_extension`'s raw form when M-3's 5-overload deletion lands; `find_extension` itself survives as `strip_extension`'s helper. | Yes | Amended 2026-07-20: the prior revision assumed both callers would go away together (M-3's original all-8 framing); M-3's amendment keeps `strip_extension`, so `find_extension` is retained too. |
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

1. **M-3 raw path-overload census — narrowed 2026-07-20.** The `path.hpp`
   header states the raw `(char*, size)` overloads exist "for hot paths." The
   full tree-wide caller census now answers this for 7 of the 8: 5 are dead
   everywhere and should be deleted, `fix_slashes` is a genuine hot path and
   must be kept, and `strip_extension` should be kept and become the
   consolidation target for M-6's duplicate. The **only** overload still
   undecided is `file_base( const char *, char *, std::size_t )`
   (`path.hpp:15`) — it has exactly one caller, its own unit test
   (`test_path.cpp:63`), and no production caller anywhere. Delete it and fold
   the test into the `string_view` form (matching H-1's precedent), or keep it
   with an explicit "reserved for a hot path that does not exist yet"
   justification comment. Recommend deletion — there is no reasoning here that
   was not already true for the 5 confirmed-dead overloads.

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

1. **ANSWERED 2026-07-20 — no, `utilities` does not gain a generic
   `find_by_key(span, key)` helper.** The L2 registry-unification lens raised
   whether the canonical `{key, payload}` dispatch-table shape (used by
   filesystem's `k_archive_types`/`k_wad_types`, sound's `k_audio_codecs`, and
   proposed for imagelib and the future renderer backend selector) should
   share one small lookup template in `utilities` (L0, every one of those
   subsystems already links it), rather than each site keeping its own 4-line
   `for` loop. Recommendation: **no** — 4 call sites of a 4-line loop do not
   justify a new generic API in a tree with deliberately zero `concept`s and
   zero CRTP, and it fails the anti-gold-plating rule (no day-one consumer
   beyond "fewer lines" — every existing call site already works). Revisit
   only when a 5th keyed-dispatch table appears (the renderer backend selector
   is the next candidate, OBL-13-9). Recorded here so this subsystem does not
   re-propose it independently.
