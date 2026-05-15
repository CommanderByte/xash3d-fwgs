# Utility Consolidation Plan — cmd_cvar

## Summary

**3 clusters** found across 17 files (10 source, 7 headers).

| Cluster | Occurrences | Files | Est. lines saved | Priority |
|---------|------------|-------|-----------------|----------|
| `trim_sv` | 2 | 2 | ~10 code + eliminates a char[] scratch buffer | **Done** ✅ |
| `abi_list_filter` | 5 | 3 | ~25–30 (skeleton) | **Prerequisite done** ✅; template not extracted (see below) |
| `cstr_span_contains` | 2 | 1 (same file) | ~4 | **Done** ✅ |

**Overall assessment:** The subsystem is already well-structured. No systemic duplication
exists. The one clear win is extracting `trim_sv` into `utilities/string.hpp`, which serves
an existing gap (the header has no whitespace-trim function) and simplifies both call sites.
The linked-list rebuild cluster prerequisite (Cvar ABI cast encapsulation) has been
implemented as `cvar_list_next`/`cvar_list_set_next` helpers in `context_impl.hpp`. The
`abi_list_filter` template itself was not extracted — the ownership logic in `cvar_unlink`
is complex enough that the template would add abstraction cost without proportionate benefit,
and `cmd_remove`/`cmd_unlink` are already clean with the explicit pattern.

---

## Clusters

### `trim_sv` — whitespace trimming of string values

**Occurrences**

| File | Lines | Mechanism | Characters stripped |
|------|-------|-----------|-------------------|
| `src/cmd_cvar/cmd_ops.cpp` | ~140–147 | `string_view::remove_prefix / remove_suffix` | leading: ` \t` · trailing: ` \t\r` |
| `src/cmd_cvar/cvar_ops.cpp` | ~153–163 | pointer advance + `strlen` + index-backward NUL | leading: ` \t` · trailing: ` \t` |

The two occurrences differ in:

1. **Input type**: `std::string_view` vs `const char *` (requiring a temporary `char[]` buffer).
2. **Trailing CR**: `cbuf_split_push` also strips `\r` (text from network line endings);
   `cvar_set_direct` does not (cvar values are not newline-terminated).

Both implement the same semantic operation: "strip command-style whitespace from a user-supplied
string, with caller control over the exact strip set."

**Equivalence:** _Functionally equivalent_ for the leading-space subset; _nearly equivalent
with known difference_ (trailing `\r`) for the trailing-space subset.

**Deduplication payoff:**
- `cmd_ops.cpp`: 4 lines replaced by one call.
- `cvar_ops.cpp`: ~10 lines (buffer declaration + pointer advance + `strncpy` + `strlen` +
  back-scan loop) replaced by a `trim_sv` call + one `strncpy` copy — eliminates the
  `char trimmed[cmd_line_max]` VLA-adjacent stack buffer entirely.

---

**Proposed extraction**

- Target file: `xash3dpp/include/xash3dpp/utilities/string.hpp`
- Rationale: `string.hpp` is the existing home for all string primitives (`stricmp`,
  `strncpy`, `atof`, etc.) and already ships with `<string_view>`. A whitespace-trim is the
  most obvious missing entry. The function is header-only (two while loops), zero dependencies
  beyond `<string_view>`, and would be immediately useful to any future subsystem that
  processes user text.
- Public API? **Yes** — part of the `xash::utilities` public surface.
- Depends on: nothing outside `<string_view>`.

```cpp
// Strips leading and trailing characters in 'chars' from sv.
// Default strip set is ASCII space + tab.
inline std::string_view trim_sv(std::string_view sv,
                                 std::string_view chars = " \t") noexcept
{
    while (!sv.empty() && chars.find(sv.front()) != std::string_view::npos)
        sv.remove_prefix(1);
    while (!sv.empty() && chars.find(sv.back()) != std::string_view::npos)
        sv.remove_suffix(1);
    return sv;
}
```

**Call-site rewrites:**

`cbuf_split_push` (cmd_ops.cpp ~140–147):
```cpp
// Before
while (!piece.empty() && (piece.front() == ' ' || piece.front() == '\t'))
    piece.remove_prefix(1);
while (!piece.empty() && (piece.back() == ' ' || piece.back() == '\t' || piece.back() == '\r'))
    piece.remove_suffix(1);

// After
piece = utilities::trim_sv(piece, " \t\r");
```

`cvar_set_direct` FCVAR_NOEXTRAWHITESPACE block (cvar_ops.cpp ~152–162):
```cpp
// Before
char trimmed[limits::cmd_line_max];
if (cv->abi.flags & FCVAR_NOEXTRAWHITESPACE) {
    while (*value == ' ' || *value == '\t') ++value;
    utilities::strncpy(trimmed, value, sizeof(trimmed));
    std::size_t len = utilities::strlen(trimmed);
    while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t'))
        trimmed[--len] = '\0';
    value = trimmed;
}

// After
char trimmed[limits::cmd_line_max];
if (cv->abi.flags & FCVAR_NOEXTRAWHITESPACE) {
    const auto sv = utilities::trim_sv(std::string_view{ value });
    utilities::strncpy(trimmed, sv.data(), std::min(sv.size() + 1, sizeof(trimmed)));
    trimmed[sv.size() < sizeof(trimmed) ? sv.size() : sizeof(trimmed) - 1] = '\0';
    value = trimmed;
}
```

> **Note on `strncpy` with non-NUL-terminated `string_view`:** `trim_sv` returns a view into
> the original buffer, which _is_ NUL-terminated past the end of the view. The `strncpy` call
> safely copies only `sv.size()` chars. The explicit NUL assignment is belt-and-suspenders;
> `utilities::strncpy` already guarantees NUL termination.

**Caveats / risks:**
- `std::string_view::find` on single characters is `O(chars.size())` per character, but the
  strip sets are ≤ 3 chars — no measurable difference from the explicit comparisons.
- The `cvar_ops.cpp` rewrite eliminates `char trimmed[]` for the no-extra-whitespace case
  _only if_ the `FCVAR_NOEXTRAWHITESPACE` branch is taken. The array declaration must remain
  outside the `if` block until the branch is confirmed the only user, or be moved inside the
  `if` (safe, since `value` inside the `if` is always pointed into `trimmed` before use).

---

### `abi_list_filter` — ABI-chained linked-list rebuild

**Occurrences**

| File | Function | Lines | List type | Operation |
|------|----------|-------|-----------|-----------|
| `src/cmd_cvar/cmd_ops.cpp` | `cmd_remove` | ~61–74 | `Command*` via `abi_next` | Remove by pointer identity |
| `src/cmd_cvar/cmd_ops.cpp` | `cmd_unlink` | ~84–101 | `Command*` via `abi_next` | Filter by `flags & mask` |
| `src/cmd_cvar/cvar_ops.cpp` | `cvar_unlink` | ~262–295 | `Cvar*` via cast `abi.next` | Filter by `owner_flags & mask` |
| `src/cmd_cvar/context_init.cpp` | `alias` handler | ~140–151 | `AliasDef*` via `abi_next` | Remove by pointer identity |
| `src/cmd_cvar/context_init.cpp` | `unalias` handler | ~167–175 | `AliasDef*` via `abi_next` | Remove by pointer identity |

**Pattern skeleton** (Command / AliasDef):
```cpp
T *new_head = nullptr;
T **tail    = &new_head;
for (T *p = head; p; ) {
    T *next = p->abi_next;
    if (keep(p)) {
        *tail = p; p->abi_next = nullptr; tail = &p->abi_next;
    } else {
        free_resources(p);  // varies per type
    }
    p = next;
}
head = new_head;
```

**Equivalence:** _Functionally equivalent_ skeleton; _intentionally different_ free-resource
logic per type. `Command` and `AliasDef` are identical in list-traversal structure. `Cvar` is
**nearly equivalent with a known difference**: because `CvarAbi::next` is typed `CvarAbi *`
(for legacy DLL ABI), the loop requires:

```cpp
Cvar *next = reinterpret_cast<Cvar *>(cv->abi.next);       // next pointer
tail       = reinterpret_cast<Cvar **>(&cv->abi.next);      // tail pointer
```

This `reinterpret_cast` between `CvarAbi **` and `Cvar **` is technically undefined behaviour
under strict aliasing (the two pointer types are not layout-compatible in the C++ type system,
even if they happen to be the same width). It works in practice with MSVC and Clang under
`-fno-strict-aliasing`, but it is a latent code smell.

**Deduplication payoff:** ~8 lines of skeleton per site × 4 sites (Command + AliasDef) =
~32 lines saved, minus ~12 for the template = **~20 lines net** for the cleanly-typed cases.
The `Cvar` case cannot be cleanly templated until the `reinterpret_cast` is resolved.

---

**Proposed extraction** _(blocked — see prerequisite below)_

- Target file: `xash3dpp/include/xash3dpp/private/cmd_cvar/context_impl.hpp` (inline
  template helpers, available to all implementation TUs)
- Public API? **No** — private, cmd_cvar TUs only.
- Depends on: `registry_types.hpp` (for `Command`, `AliasDef`).

```cpp
// Remove a single node from an ABI-chained list (T must have T* abi_next).
// Returns the new head.  Calls free_fn(node) on the removed node.
template<typename T, typename FreeFn>
T* abi_list_remove_one(T *head, T *target, FreeFn free_fn) noexcept
{
    T *new_head = nullptr;
    T **tail    = &new_head;
    for (T *p = head; p; ) {
        T *next = p->abi_next;
        if (p == target) {
            free_fn(p);
        } else {
            *tail = p; p->abi_next = nullptr; tail = &p->abi_next;
        }
        p = next;
    }
    return new_head;
}

// Filter an ABI-chained list (T must have T* abi_next).
// Returns the new head.  Calls free_fn(node) for each node where !keep(node).
template<typename T, typename KeepFn, typename FreeFn>
T* abi_list_filter(T *head, KeepFn keep, FreeFn free_fn) noexcept
{
    T *new_head = nullptr;
    T **tail    = &new_head;
    for (T *p = head; p; ) {
        T *next = p->abi_next;
        if (keep(p)) {
            *tail = p; p->abi_next = nullptr; tail = &p->abi_next;
        } else {
            free_fn(p);
        }
        p = next;
    }
    return new_head;
}
```

**Prerequisite — fix the `Cvar` list-traversal UB first:**

**Done.** `cvar_list_next(Cvar *)`, `cvar_list_next(const Cvar *)`, and
`cvar_list_set_next(Cvar *, Cvar *)` were added to `context_impl.hpp` (H-1 in the
modernization report). All 10 inline `reinterpret_cast` sites in `cvar_ops.cpp` and
`context_init.cpp` replaced. The `reinterpret_cast<Cvar **>(&cv->abi.next)` tail-pointer
trick in `cvar_unlink` was rewritten using an explicit `new_tail` pointer.

**Template extraction decision: not extracted.** `cvar_unlink` has six distinct
free-path branches (USER_CREATED, DLL_WRAPPER with separate wrapper cleanup, DLL-owned
with owner-flags check, etc.); the per-node logic is too complex for a one-size-fits-all
template lambda. `cmd_remove` and `cmd_unlink` were cleaned up in-place (now use the same
explicit pattern documented above) and are readable without a template abstraction.

If a future cleanup pass adds more list types, extracting `abi_list_filter` at that point
would be worthwhile.

---

### `cstr_span_contains` — linear scan of a `const char*` array for exact match

**Occurrences**

| File | Function | Lines | Notes |
|------|----------|-------|-------|
| `src/cmd_cvar/compat_goldsrc.cpp` | `is_filterable_exempt` | ~87–90 | Scans `kFilterableExemptions` |
| `src/cmd_cvar/compat_goldsrc.cpp` | `is_overridable_command` | ~95–98 | Scans `kOverridableCommands` |

**Equivalence:** _Identical_ — both are `for (const char *s : arr) { if (strcmp(s, needle)==0) return true; } return false;`.

**Deduplication payoff:** ~4 lines saved (2 loops × 2 removed lines each).

---

**Actual implementation** (differs from the proposal below)

When L-5 of the modernization pass changed `kFilterableExemptions` and `kOverridableCommands`
from `constexpr const char *[]` to `constexpr std::array<std::string_view, N>`, the
`cstr_span_contains` template was simultaneously updated to take `std::string_view needle`
and use `s == needle` (string_view equality) instead of `std::strcmp`. The `<cstring>`
include was removed and replaced with `<array>` + `<string_view>`.

The final helper in `compat_goldsrc.cpp`:

```cpp
template<typename Arr>
static constexpr bool cstr_span_contains(const Arr &arr, std::string_view needle) noexcept
{
    for (std::string_view s : arr)
        if (s == needle) return true;
    return false;
}
```

The proposed form below (using `const char *const (&arr)[N]` + `std::strcmp`) was superseded
by this approach.

---

**Proposed extraction**

- Target file: anonymous namespace in `src/cmd_cvar/compat_goldsrc.cpp` — **not** a shared
  utility file; the pattern is too specific to warrant a public or even internal header.
- Public API? **No.**
- Depends on: `<cstring>` (already included).

```cpp
// File-local helper: exact case-sensitive match against a constexpr string array.
template<std::size_t N>
static constexpr bool cstr_span_contains(const char *const (&arr)[N],
                                          const char *needle) noexcept
{
    for (const char *s : arr) {
        if (std::strcmp(s, needle) == 0) return true;
    }
    return false;
}
```

**Caveats / risks:** None. This is a trivial local refactor — it reduces visual noise in two
method bodies and makes the intent explicit. No API surface change.

---

## Application order

1. **`trim_sv`** — **Done.** Added to `utilities/string.hpp`; `cmd_ops.cpp` and `cvar_ops.cpp` updated.
2. **`cstr_span_contains`** — **Done.** File-local template in `compat_goldsrc.cpp`; updated to
   use `std::array<std::string_view>` + `==` as part of L-5.
3. **`cvar_list_next` / `cvar_list_set_next` accessors** — **Done** (H-1). Added to `context_impl.hpp`;
   replaced all `reinterpret_cast<Cvar *>(cv->abi.next)` occurrences.
4. **`abi_list_filter` / `abi_list_remove_one`** — **Not extracted** (see cluster notes above).
   `cmd_remove`, `cmd_unlink`, `alias`/`unalias` handlers kept as explicit code.
