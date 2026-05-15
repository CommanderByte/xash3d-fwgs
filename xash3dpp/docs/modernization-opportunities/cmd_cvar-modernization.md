# cmd_cvar Modernization Opportunities

> C++ standard in use: **C++20** (from `xash3dpp/CMakeLists.txt`)  
> Boundary spec: [`docs/boundaries/cmd_cvar-boundary.md`](../boundaries/cmd_cvar-boundary.md)  
> ABI-frozen symbols in this subsystem: `CvarAbi` layout, `CvarFlags` values,
> `CommandFn = void (*)()`, all `pfn*` signatures in `engine/eiface.h` /
> `engine/cdll_int.h` / `engine/menu_int.h`

## Summary

The subsystem is written in clean C++20 — no `NULL`, no `malloc`/`free`, no
raw `new`/`delete`, and all string inputs on the command-buffer side already
use `std::string_view`. The biggest remaining C-isms are structural: the
`CvarAbi::next` typed-as-`CvarAbi*` requirement forces eleven `reinterpret_cast`
sites that reach across the ABI boundary in every list traversal; and several
internal structs declare `const char *` for pool-owned strings, forcing
`const_cast<char *>` at every free site. The public registry API (`cvar_find`,
`cmd_add`, etc.) still takes `const char *` where `std::string_view` would be
safer. Everything else is low-priority polish.

## Implementation status

| Item | Status |
|------|--------|
| H-1 reinterpret\_cast helpers | **Implemented** |
| H-2 pool-owned fields → `char *` | **Implemented** (Command/AliasDef only; Cvar::def\_string/desc kept `const char *` — set from string literals at init) |
| M-1 cmd\_execute\_string stack buf | **Implemented** |
| M-2 Public API `string_view` | Remaining |
| M-3 bucket\_histogram `std::span` | **Implemented** |
| M-4 `utilities::snprintf` | **Implemented** |
| L-1 `std::array` for observers | Remaining |
| L-2 `std::array` for tok\_argv/buf | Remaining |
| L-3 `std::array` for local hist | Remaining |
| L-4 `std::array` for MapStats | Remaining |
| L-5 Compat tables `string_view` | Remaining |
| L-6 `std::strlen` in pool\_dup | Remaining |

---

## High-priority opportunities

### H-1: `reinterpret_cast` scatter for Cvar ABI list traversal

**Status: Implemented.** Added `cvar_list_next(Cvar *)`, `cvar_list_next(const Cvar *)`,
and `cvar_list_set_next(Cvar *, Cvar *)` to `context_impl.hpp`. All 10 call
sites in `cvar_ops.cpp` and `context_init.cpp` replaced. The `cvar_unlink`
tail-pointer trick (`Cvar **tail = reinterpret_cast<Cvar **>(&abi.next)`) was
rewritten using a `new_tail` pointer to avoid the type-punning alias entirely.
`cvar_get_list()` changed from `reinterpret_cast<CvarAbi*>(head)` to `&head->abi`.

- **File(s)**: `src/cmd_cvar/cvar_ops.cpp` lines ~75, ~100, ~120, ~250, ~260,
  ~290, ~310, ~320, ~355, ~365; `src/cmd_cvar/context_init.cpp` lines ~214, ~238
- **Current pattern**:

  ```cpp
  // 11+ occurrences across two files:
  Cvar *next = reinterpret_cast<Cvar *>(cv->abi.next);
  cv->abi.next = reinterpret_cast<CvarAbi *>(impl_->cvar_list_head);
  tail = reinterpret_cast<Cvar **>(&cv->abi.next);
  for (Cvar *cv = ...; cv; cv = reinterpret_cast<Cvar *>(cv->abi.next))
  ```

- **Suggested replacement**: Add two inline helpers to
  `include/xash3dpp/private/cmd_cvar/context_impl.hpp` (or a new
  `cvar_abi_list.hpp`):

  ```cpp
  inline Cvar *cvar_list_next(const Cvar *cv) noexcept {
      return reinterpret_cast<Cvar *>(cv->abi.next);
  }
  inline void cvar_list_set_next(Cvar *cv, Cvar *next) noexcept {
      cv->abi.next = reinterpret_cast<CvarAbi *>(next);
  }
  ```

  Every call site becomes `cvar_list_next(cv)` or `cvar_list_set_next(cv,
  next)`. The `reinterpret_cast` is reviewed and tested in one place only.

- **Boundary-safe**: Yes — the cast is already correct and ABI-required; this
  just consolidates 11 open-coded copies into two reviewed, named helpers.
- **Rationale**: Any future reordering of `CvarAbi` fields, or any wrong
  substitution of `cv->abi.next` with a different field, silently miscompiles
  all 11 sites. Consolidation into a named helper makes the cast auditable in
  one location and documents the required invariant (`abi` must be first in
  `Cvar`). This was identified as "Cluster 2" in the consolidation plan and
  explicitly deferred; this report records it as the highest-value remaining
  mechanical improvement.

---

### H-2: `const char *` pool-owned fields force `const_cast` at every free site

**Status: Partially implemented.** `Command::name`, `Command::desc`, and
`AliasDef::value` changed to `char *`; all `const_cast` at their 8 free sites
removed. `cv->abi.name` no-op casts (already `char *` ABI field) also cleaned up.
`Cvar::def_string` and `Cvar::desc` kept as `const char *` — they are assigned
from `constexpr char[]` string literals at init time (before `cvar_register_engine`
pool-dups them), so changing to `char *` would require adding `const_cast` at
those init sites. Net: removed 8 casts, added 0.

- **File(s)**:
  - `include/xash3dpp/private/cmd_cvar/registry_types.hpp` lines ~29–32
    (`Command::name`, `Command::desc`); line ~39 (`AliasDef::value`)
  - `include/xash3dpp/cmd_cvar/cvar.hpp` lines ~153, ~155
    (`Cvar::desc`, `Cvar::def_string`)
  - Free sites: `src/cmd_cvar/cvar_ops.cpp` lines ~260–280 (`cvar_unlink`);
    `src/cmd_cvar/context_init.cpp` lines ~242–285 (`shutdown`);
    `src/cmd_cvar/cmd_ops.cpp` lines ~30, ~60, ~90 (`cmd_remove`, `cmd_unlink`)
- **Current pattern** (~10 occurrences):

  ```cpp
  memory::mem_free(const_cast<char *>(cmd->name));
  memory::mem_free(const_cast<char *>(cv->def_string));
  memory::mem_free(const_cast<char *>(al->value));
  ```

- **Suggested replacement — Option A** (preferred): Change the pool-owned
  pointer fields to `char *` in the internal structs, since they are always
  mutable (pool-allocated and pool-freed by the engine):

  ```cpp
  // registry_types.hpp
  struct Command {
      char        *name;   // pool-owned (was const char *)
      char        *desc;   // pool-owned (was const char *)
      ...
  };
  struct AliasDef {
      char        *value;  // pool-owned (was const char *)
      ...
  };
  // cvar.hpp — internal extensions only (not the ABI-frozen CvarAbi fields)
  char *def_string;        // was const char *
  char *desc;              // was const char *
  ```

  All `const_cast` calls at free sites disappear. Read paths are unaffected
  (implicit `char * → const char *` conversion on use).

- **Suggested replacement — Option B** (minimal, no struct change): Add a
  single free helper:

  ```cpp
  inline void mem_free_const(const char *p) noexcept {
      memory::mem_free(const_cast<char *>(p));
  }
  ```

  This does not remove the `const_cast` concept but concentrates the
  justification in one reviewed location.

- **Boundary-safe**: Yes — `Command`, `AliasDef`, and the non-ABI fields of
  `Cvar` are internal; no frozen header exposes them.
- **Rationale**: Ten scattered `const_cast<char *>` + `mem_free` calls obscure
  intent and make audits harder. Each site requires the reader to verify that
  the pointer is genuinely pool-owned before concluding the cast is safe.

---

## Medium-priority opportunities

### M-1: `cmd_execute_string` heap-allocates a `std::string` solely for null-termination

**Status: Implemented.** Changed to `std::memcpy` + manual null-terminator into
a `char buf[limits::cmd_line_max]` stack buffer. `#include <string>` removed from
`cmd_dispatch.cpp`.

- **File(s)**: `src/cmd_cvar/cmd_dispatch.cpp` lines ~185–188
- **Current pattern**:

  ```cpp
  void CmdCvarContext::cmd_execute_string(std::string_view text) noexcept
  {
      std::string line{ text };        // heap allocation just for '\0'
      dispatch_cmd(*impl_, *this, line.c_str(), true, 0);
  }
  ```

- **Suggested replacement**: Stack-copy with `utilities::strncpy`:

  ```cpp
  void CmdCvarContext::cmd_execute_string(std::string_view text) noexcept
  {
      char buf[limits::cmd_line_max];
      utilities::strncpy(buf, text.data(),
                         text.size() + 1 < sizeof(buf) ? text.size() + 1 : sizeof(buf));
      dispatch_cmd(*impl_, *this, buf, true, 0);
  }
  ```

  Alternatively, change `dispatch_cmd` to accept `std::string_view` and build
  the per-fragment null-terminated copy only at fragment boundaries (already
  done for `line_copy` internally; the outer conversion becomes unnecessary).

- **Boundary-safe**: Yes — `cmd_execute_string` is internal.
- **Rationale**: With `/EHs-c-` (no exceptions), a `std::string` constructor
  failure produces a `std::bad_alloc` that cannot be caught. Using a stack
  buffer matches the no-allocation contract the rest of the dispatch path
  honours (all other callers pass already-null-terminated strings).

---

### M-2: Public registry API takes `const char *` where `std::string_view` is natural

**Status: Remaining.**

- **File(s)**: `include/xash3dpp/cmd_cvar/context.hpp` lines ~80–180
- **Current pattern**:

  ```cpp
  Cvar        *cvar_find(const char *name) noexcept;
  void         cvar_set (const char *name, const char *value, ...) noexcept;
  bool         cmd_exists(const char *name) const noexcept;
  // (and six other name-lookup methods)
  ```

- **Suggested replacement**: Accept `std::string_view` for all view-only name
  and value parameters. Call sites with `const char *` literals or variables
  are unaffected (implicit conversion). Internal call chains that ultimately
  need null-terminated strings for `pool_dup` or `CmdHashMap::find` add a
  `sv.data()` / local-copy step only once at the boundary.
- **Boundary-safe**: Yes — `context.hpp` is not a frozen header. `std::string_view`
  is implicitly constructible from `const char *`, so all existing call sites
  continue to compile unchanged.
- **Rationale**: Five methods on the command-buffer side already use
  `std::string_view` (`cbuf_add_text`, `cbuf_insert_text`, `cbuf_stuff_text`,
  `cbuf_clear`-adjacent, `cmd_execute_string`). Completing the migration removes
  the inconsistency and prevents accidental passing of unterminated spans to
  internal C-string APIs.

---

### M-3: `CmdHashMap::bucket_histogram` takes a raw pointer + length

**Status: Implemented.** Changed to `std::span<std::size_t>`. `<span>` added to
`cmd_hash_map.hpp`. Call site in `context_misc.cpp` simplified to `map.bucket_histogram(hist)`.

- **File(s)**: `include/xash3dpp/private/cmd_cvar/cmd_hash_map.hpp` line ~138
- **Current pattern**:

  ```cpp
  void bucket_histogram(std::size_t *out, std::size_t count) const noexcept;
  ```

- **Suggested replacement**:

  ```cpp
  void bucket_histogram(std::span<std::size_t> out) const noexcept;
  ```

  `std::span` carries the length and is constructible from `std::array` or raw
  arrays; the caller in `context_misc.cpp` passes `hist` + `limits::cvar_hash_buckets`
  and would become `bucket_histogram(hist)`.

- **Boundary-safe**: Yes — debug-only method, not exposed outside the subsystem.
- **Rationale**: C-array + length is pattern 2-C in the audit; `std::span` is
  the C++20 replacement and reduces accidental mismatch between the array size
  and the explicit length argument.

---

### M-4: `std::snprintf` used directly instead of `utilities::snprintf`

**Status: Implemented.** All three sites (`context_init.cpp` cmdlist/cvarlist
handlers, `context_misc.cpp` `dump_hash_stats`) changed to `utilities::snprintf`.
Orphaned `#include <cstdio>` removed from both files.

- **File(s)**:
  - `src/cmd_cvar/context_init.cpp` lines ~204, ~222 (cmdlist, cvarlist handlers)
  - `src/cmd_cvar/context_misc.cpp` line ~118 (dump_hash_stats)
- **Current pattern**:

  ```cpp
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%zu command(s)\n", count);
  platform::console::write(buf);
  ```

- **Suggested replacement — Option A** (preferred, C++20):
  Since `platform::console::write` accepts `std::string_view`, use
  `std::format` directly and eliminate the intermediate buffer:

  ```cpp
  platform::console::write(std::format("{} command(s)\n", count));
  ```

  Note: `std::format` allocates and may throw `std::bad_alloc`. Evaluate
  acceptability under the no-exceptions constraint (see Open Questions).

- **Suggested replacement — Option B** (safe): Replace `std::snprintf` with
  `utilities::snprintf`, which guarantees null-termination even on truncation:

  ```cpp
  char buf[64];
  utilities::snprintf(buf, sizeof(buf), "%zu command(s)\n", count);
  platform::console::write(buf);
  ```

- **Boundary-safe**: Yes — all three sites are in built-in command handlers
  and debug utilities.
- **Rationale**: `utilities::snprintf` is the project-standard safe wrapper;
  using raw `std::snprintf` in three places is inconsistent. The `std::format`
  option is the clean C++20 path but requires verifying allocation acceptability.

---

## Low-priority / cosmetic opportunities

| # | File(s) | Current | Suggested | Status |
|---|---------|---------|-----------|--------|
| L-1 | `context_impl.hpp` line ~51 | `ObserverEntry observers[limits::cmd_observer_max]` | `std::array<ObserverEntry, limits::cmd_observer_max>` | Remaining |
| L-2 | `context_impl.hpp` lines ~73–74 | `const char *tok_argv[k_max_argc]`, `char tok_argsBuffer[cmd_line_max]` | `std::array<const char*, k_max_argc>`, `std::array<char, cmd_line_max>` | Remaining |
| L-3 | `context_misc.cpp` ~L110 | `std::size_t hist[limits::cvar_hash_buckets] = {}` | `std::array<std::size_t, limits::cvar_hash_buckets> hist {}` | Remaining |
| L-4 | `context_misc.cpp` ~L112 | `MapStats maps[]` (local array) | `std::array<MapStats, 3> maps` | Remaining |
| L-5 | `compat_goldsrc.cpp` lines ~37–57 | `constexpr const char *kFilterableExemptions[]`, `kOverridableCommands[]` | `constexpr std::array<std::string_view, N>` | Remaining |
| L-6 | `context_impl.hpp` ~L122 | `utilities::strlen(src)` in `pool_dup` — null guard already above | `std::strlen(src)` (src guaranteed non-null at that point) | Remaining |

---

## Out of scope / ABI-frozen

| Symbol / pattern | Why it must not change |
|------------------|----------------------|
| `CvarAbi` field order and sizes | `cvar_t`-compatible layout; game DLLs dereference `name`, `string`, `flags`, `value`, `next` by offset. Any reorder silently breaks all DLL builds. |
| `CvarAbi::next` typed as `CvarAbi *` | `cvar_t::next` is `struct cvar_s *` in legacy headers; DLLs cast the list head to `cvar_t *` and walk `.next` directly. The engine-side `Cvar *` must be the same object, hence `reinterpret_cast` is required and cannot be removed — only encapsulated (H-1). |
| `CvarFlags` numeric values | Frozen by `common/cvardef.h`. Values are compared by game DLL code compiled against the SDK. The enum wrapper itself is free to change shape but the integer values are frozen. |
| `CommandFn = void (*)()` | ABI-compatible with legacy `xcommand_t`. Cannot become `std::function` (different calling convention and layout). |
| `pfnCvar_RegisterVariable`, `pfnCVarGetPointer`, `pfnCvar_DirectSet`, `pfnAddServerCommand`, all `pfnRegister*` / `pfnGetCvar*` / `pfnAddCommand` / `pfnClientCmd` / `pfnServerCmd` | Defined in `engine/eiface.h`, `engine/cdll_int.h`, `engine/menu_int.h` — fully frozen SDK headers. |

---

## Open questions

1. **`std::format` under `/EHs-c-`**: `std::format` can throw `std::bad_alloc`
   and `std::format_error`. With MSVC's `/EHs-c-` the throw cannot be caught.
   Determine whether the project's OOM strategy (abort) makes this acceptable
   for non-critical display paths (`cmdlist`, `cvarlist`, `dump_hash_stats`),
   or whether `utilities::snprintf` (already chosen for M-4) is the permanent
   solution. `std::format` remains an option once the no-exceptions stance is
   re-evaluated.
