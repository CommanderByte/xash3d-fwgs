---
name: "Analyse C++ modernization opportunities"
description: "Audit a subsystem (legacy or xash3dpp) for C++ modernization opportunities — replacing C idioms with modern C++ equivalents — then document findings in xash3dpp/docs/modernization-opportunities/."
argument-hint: "subsystem name matching an existing boundary spec (e.g. public-utilities, filesystem, sound)"
agent: agent
tools: [read, search, edit, GetSymbolInfo_CppTools, GetSymbolReferences_CppTools]
model: claude-opus-4-7
---

# C++ Modernization Audit: $ARGUMENTS

You are auditing the **$ARGUMENTS** subsystem for places where C-style idioms can
be replaced by modern C++ equivalents **without breaking any external ABI contracts**.

This is an analysis and documentation task. Do **not** modify any source files.

## Scope rules (read first)

Before scanning any code, determine the ABI boundary for this subsystem:

- **Fixed external ABI** — game DLL and client DLL surfaces. These headers are
  frozen:
  - `engine/eiface.h`, `engine/edict.h` (game DLL)
  - `engine/cdll_int.h`, `engine/cdll_exp.h` (client DLL)
  - Shared SDK structures in `common/`, `pm_shared/`, `engine/*.h`
  - Anything `typedef`'d or `#define`'d in those files and used across the
    game/engine boundary must not change shape, calling convention, or name.

- **Engine-internal** — everything inside `xash3dpp/` that is not transitively
  exposed through the frozen headers. These are **free to modernize**.

When you are uncertain whether something crosses the boundary, mark it
**Needs verification** in the output rather than assuming it is safe.

---

## Step 1 — Locate the code

1. Search for all source and header files that belong to `$ARGUMENTS`:
   - Legacy tree (everything outside `xash3dpp/`)
   - New skeleton under `xash3dpp/src/` and `xash3dpp/include/` (if it exists)
2. Read the existing boundary spec at
   `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` for the list of fixed
   external contracts. If it does not exist, derive the boundary from the
   frozen headers directly.
3. Note the C++ standard in use: check `xash3dpp/CMakeLists.txt` for
   `CMAKE_CXX_STANDARD`. Features available at each level:
   - **C++17**: `std::string_view`, `std::optional`, `std::variant`,
     `if constexpr`, structured bindings, `std::filesystem`
   - **C++20**: `std::span`, `std::bit_cast`, `std::format`, concepts,
     `std::numbers`, ranges, `[[likely]]`/`[[unlikely]]`
   - **C++23**: `std::expected` (loader/parse error returns), `std::byteswap`
     (endian codecs), `std::to_underlying`, `std::mdspan`, `std::print`,
     `if consteval` — this repo builds at `CMAKE_CXX_STANDARD 23`, so these are
     available (verify MSVC x64+x86 `<mdspan>`/`<print>` support before relying
     on the newest two)

---

## Step 2 — Scan for C-style patterns

Go through each file in scope and flag every instance of the following
categories. For each finding record: **file + approximate line**, **current
pattern**, **suggested replacement**, and **boundary-safe** (Yes / No /
Needs verification).

### 2-A  String ownership — `char *` / `char[]` buffers

| Pattern | Suggested replacement |
|---------|----------------------|
| `char buf[N]` passed through call chains as an out-parameter | `std::string` return value |
| `static char buf[N]` returned from a function (hidden shared state) | `std::string` or `thread_local std::string` return |
| `char *` member of a class, manually `malloc`/`free`'d | `std::string` member |
| `strncpy` / `strcpy` / `sprintf` filling a local buffer before returning | `std::string` / `std::format` |
| `const char *` in-parameter where ownership is view-only | `std::string_view` |

### 2-B  Memory management — `malloc` / `free` / `realloc`

| Pattern | Suggested replacement |
|---------|----------------------|
| `malloc`+`free` for a fixed-size object | Constructor + destructor (RAII) or `std::make_unique` |
| `malloc`+`free` for a variable-length array | `std::vector<T>` |
| `realloc` growing a buffer | `std::vector::resize` / `std::vector::push_back` |
| `void *` cast after `malloc` | Typed allocation, remove cast |
| Manual `new`+`delete` without a wrapper | `std::unique_ptr` / `std::shared_ptr` |

### 2-C  Raw arrays

| Pattern | Suggested replacement |
|---------|----------------------|
| `T arr[N]` struct member where N is a compile-time constant | `std::array<T, N>` |
| `T *ptr; int count;` pairs representing a collection | `std::vector<T>` or `std::span<T>` |
| C-array passed as `T *` + `int len` | `std::span<T>` |
| `memset` / `memcpy` on a struct array member | Default-init or `std::fill` / `std::copy` |

### 2-D  Resource handles

| Pattern | Suggested replacement |
|---------|----------------------|
| File `HANDLE` / `FILE *` with manual `fclose`/`CloseHandle` in error paths | RAII wrapper or `std::unique_ptr` with custom deleter |
| Library handle opened/closed in the same scope with early returns | RAII wrapper |
| `bool initialized; … if (!initialized) { init(); initialized=true; }` | C++11 magic static or `std::once_flag` |

### 2-E  Enums and flags

| Pattern | Suggested replacement |
|---------|----------------------|
| `#define FLAG_X (1<<N)` bitmask used in C++ code | `enum class Flags : unsigned` + bitwise operators |
| `int flags` parameter accepting OR-combined `#define` values | Typed `enum class` parameter |
| Plain `enum Foo` (not `enum class`) in C++ files | `enum class Foo` where it is internal only |

### 2-F  Function pointers and callbacks

| Pattern | Suggested replacement |
|---------|----------------------|
| `typedef void (*Callback)(…)` stored as a member | `std::function<void(…)>` or a virtual interface |
| Raw function-pointer table (vtable emulation) | Abstract base class or `std::variant` + visitor |

### 2-G  Casts

| Pattern | Suggested replacement |
|---------|----------------------|
| C-style cast `(T)expr` | `static_cast<T>`, `reinterpret_cast<T>`, or `std::bit_cast<T>` |
| `(void *)` used to erase type | Template or `std::any` / `std::variant` |

### 2-H  Output parameters

| Pattern | Suggested replacement |
|---------|----------------------|
| `bool Foo(T *out_val)` returning success + value via pointer | `std::optional<T>` return |
| `int Foo(T *out_a, T *out_b)` returning two results via pointers | `std::pair<T,T>` / named struct return |

### 2-I  Miscellaneous

| Pattern | Suggested replacement |
|---------|----------------------|
| `NULL` | `nullptr` |
| `0` used as null pointer | `nullptr` |
| `printf`-style formatting for string construction | `std::format` (C++20) or `std::ostringstream` |
| Manual `min`/`max` macros | `std::min` / `std::max` with `<algorithm>` |
| `typedef struct Foo { … } Foo;` in a C++ file | Plain `struct Foo { … };` |

### 2-J  Functions that C++ now makes redundant (delete, don't just replace)

These are **small helpers whose sole purpose was to paper over a missing
language or library feature**. When the underlying feature is now available,
the helper is not just rewritable — it is **deletable**. All call sites
become a direct expression in the same change.

Ask at each such function: *"If I delete this, can every call site be written
as a single idiomatic C++ expression?"* If yes, the function is a deletion
candidate.

Common patterns:

| Pattern | Why it existed | Modern replacement (call sites become…) |
|---------|---------------|----------------------------------------|
| `has_flag(f, mask)` | enum lacked `operator&` | `(f & mask) != Enum::None` — add operators to the enum |
| `is_power_of_two(n)` | no stdlib support | `std::has_single_bit(n)` (`<bit>`, C++20) |
| `custom_clamp(v, lo, hi)` | `std::clamp` unavailable | `std::clamp(v, lo, hi)` |
| `starts_with(s, prefix)` / `ends_with(s, suffix)` on `const char*` | pre-C++20 | `std::string_view(s).starts_with(prefix)` |
| `safe_strlen(p)` / `null_safe_len(p)` | protect against null | `p ? std::string_view(p).size() : 0` or accept `string_view` directly |
| `bit_cast_helper<T>(src)` / memcpy-based type punning | `std::bit_cast` unavailable | `std::bit_cast<T>(src)` (C++20) |
| `array_count(arr)` / `ARRAY_SIZE` macro | no `std::size` | `std::size(arr)` or `std::span(arr).size()` |
| Single-expression math wrapper (`deg2rad(x)`, `rad2deg(x)`) | readability | inline constant + multiply at call site, or `std::numbers::pi` |
| `toggle(bool &b)` or similar trivial one-liner wrapper | style | delete; call sites write the expression directly |

**How to report a deletion candidate** (in Step 4):
note that the function itself disappears, list the number of call sites that
change, and show a before/after example. Prioritise by call-site count — a
helper called in 20 places is a bigger win than one called once.

### 2-K  Extension posture (Q-21)

Read `xash3dpp/docs/design/extension-goals.md` and flag violations of its
door rules — these are modernization findings even when the code is
otherwise idiomatic C++:

| Pattern | Door rule violated |
|---------|--------------------|
| New file-scope mutable state / singleton outside the subsystem's documented ABI exceptions ("Module statics" table) | P-3 context-first |
| Function reachable from DLL callbacks or service frontends that reads state from a global instead of a context parameter | P-3 context-first |
| Free function taking a whole runtime aggregate while reading/writing only one sub-aggregate (orchestrators exempt) | P-5 narrowest-state |
| Debug/introspection code reaching into subsystem internals where a typed surface (`EntityView`, observers, stats tiers) exists or should be extended | P-4 typed surfaces |
| Cross-thread state access not using an inbox / published-snapshot design | P-1 / P-2 |

### 2-L  Design paradigm — C dispatch / state that should be OOP

Beyond idiom-level swaps, flag **design-level** shapes: C `switch`-on-tag
dispatch and stateful file-scope globals that the register makes classes or
polymorphic hierarchies. Sanctioned by Q-22 (state-with-invariants → class),
P-7 (pool-owned RAII classes), and Q-11 (open-set features → registered
implementations), with the seam precedents already in the tree (`ISearchBackend`,
`IProtocolDriver`, `EntityView`).

| Pattern | Suggested replacement |
|---------|----------------------|
| File-scope globals + free functions mutating them (a de-facto object) | A class owning the state, invariants as private members (Q-22) |
| `switch (tag)` over a **closed, frozen** set of variants | `std::variant<...>` + visitor (no RTTI, exhaustiveness-checked) |
| `{ ext, fnptr }` table over an **open** set (formats, backends) | An `I<Thing>` interface + registry (open-closed; `ISearchBackend` shape) |
| Raw `void *` / offset-walked frozen struct handed across a seam | A non-owning typed view class over it (`EntityView`-over-`entvars_t`) |
| Manual `create`/`destroy` + owning raw pointer | Pool-owned class, P-7 `create_<thing>` factory + `operator delete` |

**Constraints — keep this from becoming OOP-maximalism**: engine targets are
`/GR-` (no RTTI → no `dynamic_cast`; prefer `variant` + visitor for closed sets)
and `/EHs-c-` (no throwing constructors → a factory returning `std::expected`).
**Orchestrators stay procedural (Q-22)**: top-level flow / lifecycle sequences
remain free functions that *drive* these classes; OOP is for stateful aggregates
and open dispatch sets, not everything.

---

## Step 3 — Prioritise

Group findings into three tiers:

| Tier | Criteria |
|------|----------|
| **High** | Removes a safety hazard (shared mutable buffer, manual lifetime, naked owning pointer) or eliminates significant boilerplate in a hot call path; deletes a helper function that is now entirely redundant (category 2-J); or replaces a file-scope-global cluster with an encapsulating class (category 2-L) |
| **Medium** | Improves readability/type-safety with low risk (enum class, nullptr, std::array, optional) |
| **Low** | Cosmetic improvement, debatable style gain, or requires touching frozen ABI |

**Extension bump (Q-21)**: a finding that also opens or protects a door in
`xash3dpp/docs/design/extension-goals.md` is promoted **one tier** and tagged
`[EXT:G-n]` / `[EXT:P-n]` so the extension-relevant backlog is filterable.

---

## Step 4 — Write the modernization report

Create the file:

`xash3dpp/docs/modernization-opportunities/$ARGUMENTS-modernization.md`

Use this structure:

```markdown
# <Subsystem> Modernization Opportunities

> C++ standard in use: C++**XX** (from `xash3dpp/CMakeLists.txt`)
> Boundary spec: `docs/boundaries/<subsystem>-boundary.md` (or "derived from frozen headers")
> ABI-frozen symbols in this subsystem: <list or "None">

## Summary

One short paragraph describing the overall state: how C-heavy it is, what the
biggest wins would be, and any non-obvious constraints found.

## High-priority opportunities

For each item:

### H-1: \<short title\>

- **File(s)**: `path/to/file.c` lines ~NN–MM
- **Current pattern**: (brief description or short code snippet)
- **Suggested replacement**: (brief description or short code snippet)
- **Boundary-safe**: Yes / No / Needs verification
- **Rationale**: Why this matters (safety, clarity, performance).

(repeat H-2, H-3, …)

## Medium-priority opportunities

(same per-item format, M-1, M-2, …)

## Low-priority / cosmetic opportunities

(same per-item format, L-1, L-2, … — or a compact table if there are many)

## Out of scope / ABI-frozen

List any patterns found that *look* modernizable but must not change because
they cross the fixed external ABI boundary.

## Open questions

Things that need a design decision before the change can be made safely
(e.g. "does the sound thread call this path? if so, `std::string` allocation
may not be acceptable").
```

Do not modify any source files. The report is the only deliverable.
