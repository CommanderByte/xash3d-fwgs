# Utility Consolidation Plan — xash3dpp/src/filesystem

## Summary

8 clusters found across 8 source files.  
Estimated lines removed from call sites: **~145** (clusters 1–6 ~120 lines, cluster 7 ~6, cluster 8 ~19).  
Overall: two clusters (`ci_binary_find` + `archive_search_walk`) are the highest
priority — they are byte-for-byte identical across PAK and ZIP backends today and
will grow again when any new archive format (e.g. GRP, SIN) is added.
Clusters 7 (`to_lower`) and 8 (`path_join`) are low-effort sweeps whose utilities now exist.

---

## Clusters

### cluster-1: `ci_binary_find`

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/pak_backend.cpp` | ~125–140 | `PakBackend::find_entry` |
| `backends/zip_backend.cpp` | ~250–265 | `ZipBackend::find_entry` |

Status: **identical** — same `lower_bound` + `strnicmp` predicate + exact-match confirm.
Only the `Entry` struct type differs.

**Proposed extraction**

- Target file: `include/xash3dpp/private/filesystem/archive_helpers.hpp`
- Proposed signature:

```cpp
namespace xash::filesystem {

// Requires: T has a `std::string name` member.
// Returns pointer into 'entries' or nullptr.
template<typename T>
const T* ci_find_by_name( const std::vector<T>& entries,
                          std::string_view name ) noexcept
{
    auto it = std::lower_bound( entries.begin(), entries.end(), name,
        []( const T& e, std::string_view n ) {
            return xash::utilities::strnicmp( e.name.c_str(), n.data(),
                std::max( e.name.size(), n.size() ) + 1 ) < 0;
        } );
    if ( it == entries.end() ) return nullptr;
    if ( xash::utilities::strnicmp( it->name.c_str(), name.data(),
             std::max( it->name.size(), name.size() ) + 1 ) != 0 )
        return nullptr;
    return &*it;
}

} // namespace xash::filesystem
```

- Rationale: The function is a template because PAK and ZIP each have a private
  `Entry` struct. Both Entry types expose a `std::string name` member. Making
  the function a template (or using a concept) keeps it in a header and avoids
  requiring a shared `Entry` base class.
- Caveats / risks: The `std::max(...) + 1` sentinel trick must be preserved
  exactly — it forces `strnicmp` to compare past the shorter string's terminator
  so length mismatches are caught without a separate size check.

---

### cluster-2: `archive_search_walk`

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/pak_backend.cpp` | ~165–200 | `PakBackend::Search` inner loop |
| `backends/zip_backend.cpp` | ~305–330 | `ZipBackend::Search` inner loop |

Status: **identical** — iterate entries, try full name then strip trailing
`/component` repeatedly, deduplicate with a linear scan before pushing.

**Proposed extraction**

- Target file: `include/xash3dpp/private/filesystem/archive_helpers.hpp`
  (same header as cluster-1)
- Proposed signature:

```cpp
// Requires: T has a `std::string name` member.
// Mirrors legacy FS_Search_PAK / FS_Search_ZIP: each entry contributes its
// full name and every directory-prefix thereof to the result set.
template<typename T>
std::vector<std::string> archive_search_by_name(
    const std::vector<T>&  entries,
    std::string_view       pattern )
{
    using namespace xash::utilities;
    std::vector<std::string> results;

    for ( const auto& e : entries ) {
        std::string temp = e.name;
        while ( !temp.empty() ) {
            if ( match_pattern( temp, pattern, /*case_insensitive=*/true ) ) {
                if ( std::find( results.begin(), results.end(), temp )
                        == results.end() )
                    results.push_back( temp );
            }
            auto slash = temp.rfind( '/' );
            if ( slash == std::string::npos ) slash = temp.rfind( '\\' );
            if ( slash == std::string::npos ) break;
            temp.resize( slash );
        }
    }
    return results;
}
```

- Rationale: Identical logic repeated in two files; any future archive backend
  (GRP, SIN, HOG …) would copy it a third time. The dedup loop is O(n²) and
  should eventually be replaced with an `unordered_set`, making this the only
  place to fix.
- Caveats / risks: The `case_insensitive=true` is currently hard-coded in both
  callers (the `bool` parameter passed to `Search` is ignored). The shared
  function preserves this for now; pass the parameter through once the callers
  start using it.

---

### cluster-3: `ci_name_sort_comparator`

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/pak_backend.cpp` | constructor sort lambda | strnicmp-based |
| `backends/zip_backend.cpp` | constructor sort lambda | strnicmp-based |
| `ci_directory.cpp` | `ci_less` static helper + sort lambda | identical semantics |

Status: **functionally equivalent** — all sort `string`-named items CI-ascending.
The `ci_directory.cpp` version uses an explicit `ci_less` helper; the archive
versions inline the same comparator.

**Proposed extraction**

- Target file: `include/xash3dpp/utilities/string.hpp`
- Proposed additions:

```cpp
// Case-insensitive less-than and equality for string_view pairs.
// Suitable as std::sort / std::lower_bound predicates.
inline bool ci_less( std::string_view a, std::string_view b ) noexcept;
inline bool ci_equal( std::string_view a, std::string_view b ) noexcept;
```

- Rationale: `strnicmp` is already in `string.hpp`; `ci_less` / `ci_equal` are
  its natural boolean wrappers.  Moving them here removes the three private
  static duplicates and makes them available to the whole engine.
- Caveats / risks: The archive sort comparators currently use the
  `strnicmp(a, b, max(len)+1)` sentinel form; `ci_less` must preserve this to
  handle strings with embedded NULs (unlikely but present in legacy WAD names).
  Implementing as `strnicmp(a.data(), b.data(), max(a.size(), b.size()) + 1) < 0`
  is correct.

---

### cluster-4: `is_write_mode`

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/pak_backend.cpp` | `OpenFile` | `mode.find('w') \|\| mode.find('a')` |
| `backends/zip_backend.cpp` | `OpenFile` | identical |
| `backends/wad_backend.cpp` | `OpenFile` | identical |
| `backends/dir_backend.cpp` | `OpenFile` | same logic, stored in `is_write` bool |

Status: **identical** (3 out of 4; dir_backend follows same pattern).

**Proposed extraction**

- Target file: `include/xash3dpp/private/filesystem/i_search_backend.hpp`
  (already included by all backends)
- Proposed addition (inline free function or `constexpr`):

```cpp
inline bool is_write_mode( std::string_view mode ) noexcept {
    return mode.find( 'w' ) != std::string_view::npos
        || mode.find( 'a' ) != std::string_view::npos;
}
```

- Rationale: Two-line helper; does not need its own file. Adding it to the
  existing base header that every backend already includes is the zero-overhead
  option.
- Caveats / risks: None. The change is purely mechanical.

---

### cluster-5: `ci_equal_sv`

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/wad_backend.cpp` | `static iequal_sv(sv, sv)` | CI equality of two string_views |
| `ci_directory.cpp` | `static ci_equal(string&, sv)` | same semantics, slightly different arg types |

Status: **functionally equivalent** — both check `a.size() == b.size() && strnicmp == 0`.

**Proposed extraction**

- Subsumed by **cluster-3** (`ci_equal(sv, sv)` in `utilities/string.hpp`).
- Once cluster-3 is applied, both `iequal_sv` and `ci_equal` are deleted and
  replaced with `xash::utilities::ci_equal(a, b)`.
- Caveats / risks: `ci_equal` in `ci_directory.cpp` currently takes
  `const std::string& a` — callers pass it a `std::string`. The `string_view`
  overload in utilities accepts this implicitly.

---

### cluster-6: `MemFile`

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/wad_backend.cpp` | ~120–170 | Complete `File` subclass for `vector<byte>` data |

Status: **single occurrence today**, but:
- `backends/android_backend.cpp` loads entire assets into memory and currently
  returns an `OsFd`-backed file via a memfd; a `MemFile` would be simpler and
  avoid the syscall.
- Any future archive backend that fully decompresses into memory before serving
  reads (e.g. a custom compressed format) would need the same class.

**Proposed extraction**

- Target file: `include/xash3dpp/private/filesystem/mem_file.hpp`
- Proposed interface:

```cpp
namespace xash::filesystem {

// In-memory File — owns a vector<byte> and serves reads from it.
// Suitable for archive backends that load whole lumps (WAD, custom formats).
class MemFile final : public File {
public:
    explicit MemFile( std::vector<std::byte> data );

    FsOffset Read( std::span<std::byte> buf )        override;
    FsOffset Write( std::span<const std::byte> buf ) override;  // always -1
    FsOffset Seek( FsOffset offset, int whence )     override;
    FsOffset Tell()   const                          override;
    FsOffset Length() const                          override;
    bool     Eof()    const                          override;
    void     Flush()        override {}

    std::optional<std::string> Gets()        override;
    int                        Getc()        override;
    void                       UnGetc( int c ) override;

private:
    std::vector<std::byte> data_;
    FsOffset               len_;
    FsOffset               pos_    = 0;
    int                    ungetc_ = EOF;
};

} // namespace xash::filesystem
```

- Rationale: `MemFile` is a complete, self-contained 50-line class with no
  external dependencies beyond `File` base. Embedding it in `wad_backend.cpp`
  makes it invisible to other backends. A header-only implementation keeps
  the move cheap.
- Caveats / risks: The implementation is trivially moveable into a header.
  `Gets()` in the current `wad_backend.cpp` version strips `\r` — the shared
  version should match `OsFile::Gets()` semantics (skip `\r`, stop at `\n`
  or EOF).

---

### cluster-7: `to_lower` (manual ASCII tolower loop)

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/wad_backend.cpp` | `normalise_name()` ~L110 | Lowercase name built from `char[16]` WAD field |
| `backends/wad_backend.cpp` | Constructor ~L130 | Lowercase `stem_` after `file_base()` |
| `backends/wad_backend.cpp` | `lookup()` ~L252 | Lowercase `name` after `strip_extension(filename(...))` |

Status: **identical** — all three are the same `for (char& c : s) c = std::tolower(...)` loop.

**Proposed extraction**

- Target file: `include/xash3dpp/utilities/string.hpp` (**already added**)
- Signature:
  ```cpp
  inline void to_lower( std::string& s ) noexcept;
  inline std::string to_lower( std::string_view s );
  ```
- Replacement at each site: `xash::utilities::to_lower(name);`
  (`wad_backend.cpp` already includes `<xash3dpp/utilities/string.hpp>`; no new include required.)
- Rationale: Pure ASCII lowercase is used only in `wad_backend.cpp` today, but the
  need arises wherever legacy WAD / BSP names are normalised. Centralising it in
  `string.hpp` makes the helper available to future backends without further headers.
- Caveats / risks: None. The `ascii_lower` trick (`'A'–'Z'` range only) already used
  in `string.cpp` for `stricmp` is equivalent; `to_lower` uses the same range check so
  locale-dependent `std::tolower` behaviour is not introduced.
- Estimated lines removed: **6** (three 2-line loops → three 1-line calls)

---

### cluster-8: `path_join` (manual `dir + "/" + rel` concatenation)

**Occurrences**

| File | Lines | Notes |
|------|-------|-------|
| `backends/android_backend.cpp` | L23–27 | Private `join_path` helper — full reimplementation |
| `backends/android_backend.cpp` | L121 | Inline ternary `dir_sv.empty() ? entry : dir + '/' + entry` |
| `backends/android_backend.cpp` | L153 | Inline `dir_sv + '/' + entry` inside recursive call |
| `backends/dir_backend.cpp` | L88 | Write path: `root_ + "/" + path` |
| `backends/dir_backend.cpp` | L92 | Read path: `root_ + "/" + resolved` |
| `backends/dir_backend.cpp` | L108 | `FileTime`: `root_ + "/" + resolved` |
| `backends/dir_backend.cpp` | L116 | `FindFile`: `root_ + "/" + resolved` |
| `backends/dir_backend.cpp` | L120 | `FindFile` confirm: `root_ + "/" + resolved` |
| `backends/dir_backend.cpp` | L154 | Search result: `resolved_dir + "/" + move(n)` |
| `ci_directory.cpp` | L89 | `get_or_populate`: `root_ + "/" + dir` (guarded by `dir.empty()`) |
| `filesystem.cpp` | L96, L101, L103, L108 | `rootdir/rodir + '/' + game dirs` |
| `filesystem.cpp` | L117 | `root + '/' + entry` |
| `filesystem.cpp` | L157 | `dir + '/' + entry` |
| `filesystem.cpp` | L258, L315, L347, L349 | `source_path + '/' + path` |

Status: **functionally equivalent** — all perform a separator-guarded string join.
`android_backend.cpp::join_path` is an **identical reimplementation** of `path_join`.
The inline sites are near-equivalent (the ternary on L121 matches `path_join`'s empty-check exactly).

**Proposed extraction**

- Target file: `include/xash3dpp/utilities/path.hpp` + `src/utilities/path.cpp` (**already added**)
- Signature:
  ```cpp
  std::string path_join( std::string_view dir, std::string_view rel );
  ```
- Required new includes:
  - `backends/android_backend.cpp`: add `#include <xash3dpp/utilities/path.hpp>` and
    remove the private `join_path` helper; update the two remaining inline sites.
  - `backends/dir_backend.cpp`: add `#include <xash3dpp/utilities/path.hpp>`; replace
    the 6 `root_ + "/"` patterns and the `resolved_dir + "/"` Search pattern.
  - `ci_directory.cpp`: add `#include <xash3dpp/utilities/path.hpp>`; replace
    the ternary `dir.empty() ? root_ : root_ + "/" + dir` with `path_join(root_, dir)`.
  - `filesystem.cpp`: already includes `path.hpp`; replace the 9 inline sites.
- Rationale: ~17 sites across 4 files. The `android_backend.cpp` private helper is a
  direct duplication. The `dir_backend.cpp` joins are all `root_ + "/" + x` where
  `root_` is guaranteed slash-free (trimmed in constructor), making `path_join`
  semantically equivalent. The `filesystem.cpp` sites all join two `std::string` /
  `std::string_view` values with no special-casing needed.
- Caveats / risks:
  - `dir_backend.cpp` L154 passes `std::move(n)` as the second argument; wrapping in
    `path_join` copies `n` before the move. Convert to `path_join(resolved_dir, n)`
    then `std::move` the result: `results.push_back(path_join(resolved_dir, n))` —
    `path_join` returns by value so `push_back` moves it anyway via NRVO.
  - `ci_directory.cpp` uses `std::string&` vs `std::string_view` — implicit conversion
    is fine.
- Estimated lines removed: **~19** (1 private helper function + ~17 inline sites become
  single-expression calls; some multi-line constructions collapse to one line)

---

## Application order (suggested)

| Priority | Cluster | Effort | Payoff |
|----------|---------|--------|--------|
| 1 | `ci_binary_find` (1) + `archive_search_walk` (2) | Medium | High — removes 60+ duplicated lines, future-proofs |
| 2 | `is_write_mode` (4) | Low | Low — 3 trivial replacements |
| 3 | `ci_equal_sv` + `ci_name_sort_comparator` (3+5) | Low-medium | Medium — cleans up private statics, enriches utilities |
| 4 | `MemFile` (6) | Medium | Quality — enables android_backend simplification |
| 5 | `to_lower` (7) | Low | Low — 3 identical loops in one file |
| 6 | `path_join` (8) | Low-medium | Medium — 17 sites, removes private helper in android_backend |
