# Round 2 Cleanup Plan — Applying Design Paradigms to Existing Subsystems

> **Purpose**: Identify every concrete change needed across the five completed
> subsystems to bring them in line with the Round 2 design paradigms decisions.
> **Reference**: `design-paradigms-round2.md` (QA–QK)
> **Policy**: Changes are batched by subsystem and tagged with the rule they fix.
> They should be made opportunistically — i.e. when a subsystem is next opened for
> another reason — unless marked "before Chunk 2" (required for correctness).

---

## 0. Changes Required **Before Chunk 2**

These two items are not cosmetic — downstream subsystems depend on them:

| Item | Rule | Status |
|------|------|--------|
| Implement `platform/log.hpp` + `log.cpp` | QI | ✅ Done this session |
| Implement `platform/assert.hpp` | QH | ✅ Done this session |

All new code written from this point forward must call `platform::log` instead of
`console::write` or any `printf` variant, and must use `XASH_ASSERT`/`XASH_FATAL`
instead of `assert()` from `<cassert>`.

---

## 1. `platform` subsystem

### 1.1 `private/platform/assert_main.hpp`

| File | Change | Rule |
|------|--------|------|
| `include/xash3dpp/private/platform/assert_main.hpp` | Replace `assert(...)` with `XASH_FATAL(...)` after including `platform/assert.hpp` | QH |

Current code:
```cpp
#include <cassert>
// …
assert( ( id == std::thread::id{} || id == std::this_thread::get_id() ) &&
        "platform main-thread-only function called from a worker thread" );
```

Replace with:
```cpp
#include <xash3dpp/platform/assert.hpp>
// …
XASH_FATAL( id == std::thread::id{} || id == std::this_thread::get_id(),
            "platform main-thread-only function called from a worker thread" );
```

### 1.2 `tests/platform/` test files

| File | Change | Rule |
|------|--------|------|
| `test_platform.cpp`, `test_console.cpp`, `test_os_io.cpp`, `test_crash.cpp` | Replace ad-hoc `CHECK` macro with the standard `CHECK`/`REQUIRE` set from `test_helpers.hpp` once that file is created | QK |

> Note: `test_helpers.hpp` itself is a prerequisite. Create it in
> `xash3dpp/tests/` once any existing test is being edited anyway.

---

## 2. `memory` subsystem

### 2.1 `include/xash3dpp/memory/memory.hpp`

| Item | Change | Rule |
|------|--------|------|
| `enum class AllocStrategy { kSystem, kArena, kNullPool }` | Rename values to `System`, `Arena`, `NullPool` when the file is next edited | QF |
| `enum class PoolTag { kProcess, kSession, kFrame }` | Rename values to `Process`, `Session`, `Frame` when the file is next edited | QF |

These renames are the only grandfathered exceptions in the codebase; removing the
`k` prefix makes them consistent with `LogLevel::Error`, `AllocStrategy::Arena` etc.

Callers to update after the rename (grep `kSystem\|kArena\|kNullPool\|kProcess\|kSession\|kFrame`):
- `src/memory/memory.cpp`
- `tests/memory/test_memory.cpp`
- Any subsystem that calls `memory::create_pool(…, PoolTag::kProcess)`

---

## 3. `filesystem` subsystem

### 3.1 `include/xash3dpp/filesystem/filesystem.hpp`

| Item | Change | Rule |
|------|--------|------|
| Missing `[[nodiscard]]` on all non-void returns | Add `[[nodiscard]]` to: `IFilesystem::Open`, `IFilesystem::Close`, `IFilesystem::Read`, `IFilesystem::Seek`, `IFilesystem::Tell`, `IFilesystem::Eof`, `IFilesystem::Stat`, `IFilesystem::FindFirst`, `IFilesystem::FindNext`, `create_filesystem()` | QA |
| `Filesystem::Init`, `Open`, `Read`, `Seek`, `Tell`, `Eof`, `Stat` etc. | Rename to `init`, `open`, `read`, `seek`, `tell`, `eof`, `stat` on the `Filesystem` implementation class (not on `IFilesystem` vtable which is ABI-fixed) | QE |

> **Important**: The `IFilesystem` vtable method names (e.g. `Open`, `Close`) must
> **not** change — they are matched by VFileSystem009 adapter code. Only the
> `Filesystem` concrete class and any other non-vtable methods need renaming.

### 3.2 `src/filesystem/*.cpp`

| Item | Change | Rule |
|------|--------|------|
| Any `printf`/`fprintf` diagnostic calls | Replace with `platform::log(LogLevel::Error, "filesystem", …)` | QI |
| Any `assert()` from `<cassert>` | Replace with `XASH_ASSERT` / `XASH_FATAL` | QH |

### 3.3 `tests/filesystem/test_filesystem.cpp`

| Item | Change | Rule |
|------|--------|------|
| Ad-hoc `CHECK` macro definition | Replace with `test_helpers.hpp` standard macros | QK |

---

## 4. `cmd_cvar` subsystem

### 4.1 `include/xash3dpp/cmd_cvar/cmd_cvar.hpp`

| Item | Change | Rule |
|------|--------|------|
| Missing `[[nodiscard]]` on `cvar_find`, `cvar_get_*`, `cmd_find`, `cmd_token_*` | Add `[[nodiscard]]` | QA |

### 4.2 `src/cmd_cvar/cmd_cvar.cpp`

| Item | Change | Rule |
|------|--------|------|
| Any `printf`/`fprintf` diagnostic calls | Replace with `platform::log` | QI |
| Any `assert()` | Replace with `XASH_ASSERT` / `XASH_FATAL` | QH |

---

## 5. `utilities` subsystem

### 5.1 `include/xash3dpp/utilities/*.hpp`

| Item | Change | Rule |
|------|--------|------|
| Review for missing `[[nodiscard]]` on return values | Add where appropriate | QA |

---

## 6. Create `tests/test_helpers.hpp`

This is a pure addition — no subsystem is blocked on it, but it standardises
the `CHECK`/`REQUIRE` macros that every test currently re-defines locally.

Standard macro set (from design-paradigms-round2.md QK):

```cpp
CHECK(expr)           // non-fatal; logs failure and continues
CHECK_EQ(a, b)        // non-fatal equality
CHECK_NE(a, b)        // non-fatal inequality
CHECK_STREQ(a, b)     // non-fatal C-string equality
CHECK_LT(a, b)        // non-fatal less-than
CHECK_LE(a, b)        // non-fatal less-or-equal
REQUIRE(expr)         // fatal; prints failure and calls std::exit(1)
```

All macros print `FAIL [file:line]: <expr>` to stdout on failure.

---

## 7. Opportunistic Application Schedule

| When to apply | Items |
|---------------|-------|
| **Before Chunk 2 (required)** | §0 — `log.hpp`, `assert.hpp` ✅ |
| **Next edit of `memory/`** | §2.1 — enum value rename (QF) |
| **Next edit of `filesystem/`** | §3.1 `[[nodiscard]]`, §3.2 `platform::log` / `XASH_ASSERT`, §3.3 test macros (QA, QE, QI, QH, QK) |
| **Next edit of `cmd_cvar/`** | §4.1 `[[nodiscard]]`, §4.2 `platform::log` (QA, QI, QH) |
| **Next edit of `platform/`** | §1.1 `assert_main.hpp` (QH) |
| **First test file edited** | §6 — create `test_helpers.hpp`, migrate that file's macros (QK) |
| **New code only** | All Round 2 rules apply from line 1; no exemptions |
