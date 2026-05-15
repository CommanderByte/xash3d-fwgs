---
name: "Sweep module for design compliance"
description: "Audit and fix an existing xash3dpp module for full compliance with the Round 2 design paradigms: [[nodiscard]], naming conventions, forbidden patterns, test_helpers.hpp migration, and thread assertions. Rebuilds and re-runs all tests. Commits when clean."
argument-hint: "module name, e.g. 'utilities', 'memory', 'platform', 'cmd_cvar', 'filesystem'"
agent: agent
tools: [read, search, edit, run, terminal]
mode: agent
---

# Design Compliance Sweep: `$ARGUMENTS`

Perform a **full compliance sweep** of the `$ARGUMENTS` module in `xash3dpp/`.
Work autonomously from start to finish. Do not ask for confirmation — make all
fixes directly. The task is complete when every test passes and the changes are
committed.

---

## Reference Documents (read before touching any file)

1. **Conventions** (auto-applied but re-read the critical sections):
   [`xash3dpp/.github/instructions/xash3dpp.instructions.md`](../instructions/xash3dpp.instructions.md)
2. **Round 2 cleanup plan**:
   [`xash3dpp/docs/design/round2-cleanup-plan.md`](../../xash3dpp/docs/design/round2-cleanup-plan.md)
3. **Design Paradigms Round 2** (QA–QK):
   [`xash3dpp/docs/design/design-paradigms-round2.md`](../../xash3dpp/docs/design/design-paradigms-round2.md)

---

## Step 1 — Inventory

List every file in the module:
- `xash3dpp/include/xash3dpp/$ARGUMENTS/` — public headers
- `xash3dpp/include/xash3dpp/private/$ARGUMENTS/` — private headers (if present)
- `xash3dpp/src/$ARGUMENTS/` — implementation files
- `xash3dpp/tests/$ARGUMENTS/` — test files

---

## Step 2 — Compliance Audit

Read each file and record every violation in each category below.

### QA — `[[nodiscard]]` completeness

Rule: `[[nodiscard]]` is the **default** for every non-`void` return. Omit only
with a documented reason.

Check every function declaration in public and private headers. Flag any non-void
return that is missing `[[nodiscard]]`. Exceptions that do NOT need it:
- `void` returns
- Setter-style functions where the return value is universally ignored (e.g. chained
  builder methods — uncommon in this codebase)
- `operator<<` overloads

### QE — Naming: `snake_case` member functions

Rule: all member function names are `snake_case`.
Exception: `I<X>` vtable methods that match a legacy ABI (e.g. `IFilesystem::Open`)
are exempt.

Flag any member function with a capital letter in its name that is not ABI-fixed.

### QF — `enum class` value names: `PascalCase`, no `k` prefix

Rule: `enum class` values use `PascalCase` with no `k` prefix.
Example: `AllocStrategy::System` not `AllocStrategy::kSystem`.

Flag any `enum class` value starting with a lowercase `k`.

### QH — No `assert()` from `<cassert>`

Rule: use `XASH_ASSERT` / `XASH_FATAL` from `<xash3dpp/platform/assert.hpp>`.
Flag any `#include <cassert>` or bare `assert(` call.

### QI — No `printf`/`fprintf`/`console::write` for diagnostics

Rule: use `platform::log( LogLevel::..., "subsystem", ... )`.
Flag any `printf`, `fprintf`, `console::write`, or `std::cout` used for error/info
output in `src/` files.

### QK — Tests use `test_helpers.hpp`

Rule: every test file must `#include "test_helpers.hpp"` (or the path-adjusted
relative include) and use `CHECK` / `CHECK_EQ` / `REQUIRE` from it. No ad-hoc
macro redefinitions.

Flag any test file that defines its own `CHECK` macro or does not include
`test_helpers.hpp`.

### Forbidden patterns (flag any occurrence in `src/`)

- `malloc`, `calloc`, `realloc`, `free` — use `mem_alloc` / `mem_free`
- bare `new` / `delete` — use `pool_new<T>` / `mem_free`; `std::make_unique<Impl>()` is the only exception
- `fopen`, `fclose`, `FILE *`, `CreateFile`, `open()` — use `IFilesystem`
- `strlen`, `strcpy`, `strcmp`, `sprintf` — use `utilities::` equivalents
- `#include <cstring>`, `#include <cstdio>` in `src/` — flag for review

---

## Step 3 — Fix All Violations

Apply every fix found in Step 2. For each fix:

- **QA**: Add `[[nodiscard]]` before the return type on the declaration.
- **QE**: Rename via the language server rename tool (`vscode_renameSymbol`) to
  update all call sites atomically. Then verify the build still references the
  new name everywhere.
- **QF**: Rename enum values using `vscode_renameSymbol`; update all call sites.
- **QH**: Replace `#include <cassert>` with `#include <xash3dpp/platform/assert.hpp>`
  and replace `assert(cond)` with `XASH_ASSERT(cond, "message")` or `XASH_FATAL(cond, "message")`.
- **QI**: Replace diagnostic output with `platform::log`. Add
  `#include <xash3dpp/platform/log.hpp>` if not already present.
- **QK**: Replace the ad-hoc macro block with `#include "../../test_helpers.hpp"` (adjust
  relative path). Remove the old `#define CHECK` / `#define REQUIRE` lines.
- **Forbidden patterns**: Replace with the approved equivalent. Document with an inline
  comment if the substitution is non-obvious.

Make all independent fixes in parallel using `multi_replace_string_in_file` where
possible. Never make speculative changes beyond what the audit found.

---

## Step 4 — Build and Test

```powershell
cd "c:\git\xash3d-fwgs\xash3dpp\build\Debug"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . 2>&1 | Select-String "error C[0-9]|error:"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Debug --output-on-failure -j1 2>&1 | Select-Object -Last 20
```

If there are build errors, read the error output, trace the root cause, fix it,
and rebuild. Repeat until the build is clean.

If any test fails, read its output, trace the cause, fix the test or the source,
rebuild, and re-run. Do not weaken assertions or skip tests.

**Expected outcome**: `100% tests passed`.

---

## Step 5 — Commit

Stage only `xash3dpp/` changes:

```powershell
cd "c:\git\xash3d-fwgs"
git add -A xash3dpp/
git diff --cached --name-only | Where-Object { $_ -notlike "xash3dpp/*" }
```

If that second command produces any output, unstage those files with
`git restore --staged <file>` before committing.

Commit message format:

```
refactor($ARGUMENTS): apply Round 2 design compliance

- <list each rule fixed: QA, QE, QF, QH, QI, QK>
- <one line per concrete change>
```

---

## Done Condition

The task is complete when:
1. `100% tests passed` in the CTest run.
2. The commit is made with only `xash3dpp/` files staged.
3. No violations from Step 2 remain in the module.
