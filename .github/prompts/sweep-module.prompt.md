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

## Already-Swept Modules (skip if $ARGUMENTS matches one of these)

The following modules were fully swept in commit `e14152ee` and are already
compliant. If `$ARGUMENTS` is one of them, report "already compliant — nothing
to do" and stop.

| Module | Swept commit |
|--------|-------------|
| `filesystem` | e14152ee |
| `cmd_cvar` | 75c383da |
| `memory` | e14152ee |
| `platform` | e14152ee |
| `utilities` | e14152ee |

---

## Guardrails — Read Before Touching Any File

These constraints prevent the sweep from drifting into unrelated work.
Violating them is worse than leaving a minor violation in place.

**Only fix what the audit in Step 2 identifies. Nothing else.**

- **Do not add docstrings, comments, or type annotations** to code you did
  not change for a compliance reason.
- **Do not refactor logic** — no restructuring of control flow, no extraction
  of helpers, no reordering of declarations beyond what a rule requires.
- **Do not rename a symbol** unless it violates QE or QF exactly. Do not
  "improve" names that are already compliant.
- **Do not change test assertions or test logic** — only the macro style
  (QK). If a test was passing before, it must pass after with identical
  semantics.
- **Do not add error handling** for scenarios the existing code cannot reach.
- **If a file has zero violations, do not touch it.** Even a trivial
  whitespace change generates noise in the diff.
- **Stop at the module boundary.** Do not follow a rename into a different
  module's files unless that module is the current `$ARGUMENTS`.

---

## Reference Documents (read before touching any file)

1. **Conventions** (auto-applied but re-read the critical sections):
   [`xash3dpp/.github/instructions/xash3dpp.instructions.md`](../instructions/xash3dpp.instructions.md)
2. **Design Paradigms Round 2** (QA–QK):
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

Read each file in the module and record every violation under the rules below.
Work through all categories before making any changes.

---

### Round 1 — Structural Rules

#### R1-Pimpl — Pimpl move pattern (Q-3)

Applies only to classes that own `std::unique_ptr<Impl>` or a raw `Impl*`.

Rule: in the **header**, declare (do not default) the destructor and move
operations. In the **.cpp** (where `Impl` is complete), define them with
`= default`. Writing `= default` in the header triggers premature
`unique_ptr<Impl>` destructor instantiation.

```cpp
// Header — incomplete Impl:
~MyClass();
MyClass(MyClass&&) noexcept;
MyClass& operator=(MyClass&&) noexcept;

// .cpp — Impl complete:
MyClass::~MyClass()                          = default;
MyClass::MyClass(MyClass&&) noexcept         = default;
MyClass& MyClass::operator=(MyClass&&) noexcept = default;
```

Flag: `= default` on destructor or move operations *in the header* when `Impl`
is forward-declared.

#### R1-ErrorReturn — Error return diagnostics (Q-5)

Rule: every function that returns a failure indicator (`bool`, `optional<T>`,
`T*` nullable) must emit a `platform::log` diagnostic **before** returning
failure. Silent failures are forbidden.

Exception: `optional<T>` returning `nullopt` for a "not found" query (not an
error) is explicitly silent by contract — do not add a log there.

Flag: any `return false;`, `return nullptr;`, `return std::nullopt;` in `src/`
that is not preceded by a `platform::log` call, where the return represents an
error (not a normal "not found").

#### R1-Ownership — Ownership markers in public APIs (Q-9)

Rule: raw `T*` in a public API means "borrowed reference with engine lifetime".
It must be documented with `// @lifetime: engine` on the declaration.
`std::make_unique<Impl>()` is the **only** allowed `new` expression.

Flag:
- Raw `T*` return in a public header without `// @lifetime: engine`
- Any `new T(` expression that is not `std::make_unique<Impl>()`
- Any `delete` expression

#### R1-NoGlobals — No new mutable global state (Q-4)

Rule: no new mutable `static` or `extern` variables at file scope in `src/`.
State must live in context-object members or pool-allocated structs.

Exception: `memory` subsystem's global pool registry is the documented
singleton — do not flag it.

Flag: any new `static <type> g_` or file-scope mutable variable in `src/`
that was not present in the already-swept subsystems.

#### R1-InitParams — Named params struct for non-trivial init (Q-4)

Rule: any `init()` / constructor that takes two or more arguments from the
caller must accept a `<Subsystem>InitParams` struct, not positional args.

Flag: any public `init(sv, sv, ...)` with more than one `string_view`/`int`
positional parameter where no params struct exists.

---

### Round 2 — Code Style Rules

#### QA — `[[nodiscard]]` completeness

Rule: `[[nodiscard]]` is the **default** for every non-`void` return.

Flag any non-void function declaration in public and private headers missing
`[[nodiscard]]`. Does **not** apply to: `void` returns; `operator<<`; setters
that are universally called for side-effects only.

#### QE — Naming: `snake_case` member functions

Rule: all member function names are `snake_case`.
Exception: `I<X>` vtable methods that match a legacy ABI (e.g. `IFilesystem::Open`)
are exempt and must **not** be renamed.

Flag any member function with a capital letter that is not ABI-fixed.

#### QF — `enum class` values: `PascalCase`, no `k` prefix

Rule: `enum class` values use `PascalCase` — no `k` prefix.

Flag any `enum class` value starting with lowercase `k`.

#### QH — No `assert()` from `<cassert>`

Rule: use `XASH_ASSERT` / `XASH_FATAL` from `<xash3dpp/platform/assert.hpp>`.

Flag any `#include <cassert>` or bare `assert(` call.

#### QI — No `printf`/`fprintf` for diagnostics; no raw `console::write` for C++ subsystem errors

Rule:
- `printf`, `fprintf`, `std::cout` — **always** flag; replace with `platform::log`.
- `platform::console::write` — **allowed and encouraged** for intentional
  game-console output (e.g., the `echo` command, `cvarlist`, `cmdlist`,
  progress/status messages directed at the player or developer).
- `platform::console::write` for **C++ subsystem diagnostic messages**
  (pool allocation failures, internal errors, severe warnings) — flag;
  replace with `platform::log( LogLevel::Error/Warning, "subsystem", ... )`.

`platform::log`'s default implementation routes through `console::write`
internally, so using it does not suppress the message — it just adds the
structured `[tag][LEVEL]:` prefix and the routing hook.

Flag: `printf`, `fprintf`, `std::cout` anywhere in `src/`. Also flag
`console::write` used to report a C++ error/warning condition rather than
to produce user-visible console output.

#### QK — Tests must use `test_helpers.hpp`

Rule: every test file must `#include "test_helpers.hpp"` and use `CHECK` /
`CHECK_EQ` / `REQUIRE` from it. No local `#define CHECK`.

Flag any test file that defines its own `CHECK`/`REQUIRE` or does not include
`test_helpers.hpp`.

#### Forbidden patterns (flag any occurrence in `src/`)

| Forbidden | Use instead |
|-----------|-------------|
| `malloc`, `calloc`, `realloc`, `free` | `mem_alloc` / `mem_free` |
| bare `new` / `delete` | `pool_new<T>` / `mem_free`; `std::make_unique<Impl>()` only exception |
| `fopen`, `fclose`, `FILE *`, `CreateFile`, `open()` | `IFilesystem` interface |
| `strlen`, `strcpy`, `strcmp`, `sprintf` | `utilities::` equivalents |
| `#include <cstring>`, `#include <cstdio>` | flag for review |

---

### Threading — Model Rules

These checks apply **only to stateful subsystems** (pimpl classes with an
`init()`/`shutdown()` lifecycle). Skip for pure-function namespaces
(`utilities`, `platform` free functions) — they are inherently thread-safe by
being stateless.

#### TH-Role — Thread role assertions on main-thread-only public functions

Rule: every public method of a stateful subsystem that must only be called from
the main thread must call `assert_thread_role(ThreadRole::Main)` as its first
statement. The call is a no-op in release builds.

```cpp
#include <xash3dpp/platform/thread_role.hpp>

void MySubsystem::mutate() noexcept
{
    platform::assert_thread_role( platform::ThreadRole::Main );
    // ...
}
```

Flag: any public `init()`, `shutdown()`, or state-mutating method on a stateful
class that lacks `assert_thread_role(ThreadRole::Main)`.

Do **not** add thread assertions to: query/read-only functions that take
`const` context, stateless free functions, or background-thread callbacks.

#### TH-Const — Query functions take `const` context (thread-safety by signature)

Rule: functions that only read subsystem state must take `const MySubsystem&`
(or `const Impl&` internally) — never access mutable state from a function
that may be called from a worker thread.

Flag: any non-`const` member function that only reads state and could safely
be `const`.

#### TH-NoGlobalRead — No mutable global reads in potentially-parallel paths

Rule: a function callable from a worker thread must not read unguarded
mutable global state. All shared state must be accessed under a lock or be
`std::atomic`.

Flag: file-scope mutable variables read in functions that are not
main-thread-only.

---

## Step 3 — Fix All Violations

Apply every fix found in Step 2. All independent fixes may be batched with
`multi_replace_string_in_file`. Never make speculative changes beyond what
the audit identified.

**Round 1 fixes:**
- **R1-Pimpl**: Move `= default` from the header to the `.cpp` for destructor and move
  ops of pimpl classes. Declare (no `= default`) in the header.
- **R1-ErrorReturn**: Add `platform::log( LogLevel::Error, "<subsystem>", "..." )` before
  each silent `return false` / `return nullptr` that represents a real error.
  Add `#include <xash3dpp/platform/log.hpp>` if not already present.
- **R1-Ownership**: Add `// @lifetime: engine` comment to raw `T*` returns in public
  headers. Replace any `new T(` (non-pimpl) with `pool_new<T>( pool, ... )`.
  Replace any `delete ptr` with `mem_free( ptr )`.
- **R1-NoGlobals**: Move file-scope mutable state into the pimpl struct. If the state
  is truly process-singleton (like the memory pool registry), document it with a
  comment; do not remove it.
- **R1-InitParams**: Define a `<Subsystem>InitParams` struct and change the function
  signature to accept it. Update all call sites in the module.

**Round 2 fixes:**
- **QA**: Add `[[nodiscard]]` before the return type on the declaration.
- **QE**: Rename using `replace_string_in_file` / `multi_replace_string_in_file`
  directly — do **not** use `vscode_renameSymbol` for names that are common
  English words or are shared by multiple symbols (e.g. `init`, `open`, `read`,
  `write`, `close`). `vscode_renameSymbol` confuses semantically unrelated symbols
  that happen to share a name, producing a cascade of wrong renames. Search for
  all usages with `grep_search` first, then replace each site explicitly.
  Verify the build after any rename.
- **QF**: Rename enum values the same way as QE (direct `replace_string_in_file`,
  not `vscode_renameSymbol`). Grep for every usage of `EnumType::old_name` first.
- **QH**: Replace `#include <cassert>` with `#include <xash3dpp/platform/assert.hpp>`.
  Replace `assert(cond)` with `XASH_ASSERT(cond)` (one argument — no message)
  or `XASH_FATAL(cond, "message")` (two arguments, always-on) for invariants
  that must abort in release builds.
- **QI**: Replace `printf`/`fprintf`/`std::cout` with `platform::log`. For
  `platform::console::write`: keep it when it is intentional game-console
  output (echo, cmdlist, cvarlist, player-visible messages); replace it with
  `platform::log( LogLevel::Error/Warning, "<subsystem>", "..." )` only when
  it is reporting a C++ subsystem error or internal diagnostic condition.
  Add `#include <xash3dpp/platform/log.hpp>` if not already present.
- **QK**: Replace the ad-hoc macro block with `#include "../../test_helpers.hpp"`
  (adjust relative path depth). Remove old `#define CHECK` / `#define REQUIRE` lines.
- **Forbidden patterns**: Replace with the approved equivalent. Add an inline comment
  if the substitution is non-obvious.

**Threading fixes:**
- **TH-Role**: Add `platform::assert_thread_role( platform::ThreadRole::Main );` as
  the first statement of each flagged public method. Add
  `#include <xash3dpp/platform/thread_role.hpp>` at the top of the `.cpp` file.
- **TH-Const**: Add `const` qualifier to the flagged method signature and propagate
  through the Impl accessor chain.
- **TH-NoGlobalRead**: Guard the unprotected read under the subsystem's existing lock,
  or move the variable into a pool-allocated struct.

---

## Step 4 — Build and Test

```powershell
# cmake --build runs from the Visual Studio build output directory
cd "c:\git\xash3d-fwgs\xash3dpp\build\Debug"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . 2>&1 | Select-String "error C[0-9]|error:"

# CTest must run from the CMake build root (one level up), not the config subdirectory
cd "c:\git\xash3d-fwgs\xash3dpp\build"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Debug --output-on-failure -j1 2>&1 | Select-Object -Last 20
```

If there are build errors, read the error output, trace the root cause, fix it,
and rebuild. Repeat until the build is clean.

If any test fails, read its output, trace the cause, fix the test or the source,
rebuild, and re-run. Do not weaken assertions or skip tests.

**Expected outcome**: `100% tests passed`.

---

## Step 4b — Sync boundary doc

After all code fixes are applied, check whether the boundary spec is still
accurate:

```
xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md
```

If the file exists, scan it for any symbol names, enum values, function
signatures, or structural descriptions that were changed by the fixes in Step 3
(e.g. a renamed enum value, a new `[[nodiscard]]`, a removed global).
Update only the lines that are factually wrong — do not rewrite prose or
restructure sections.

If no boundary spec exists, skip this step.

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
refactor($ARGUMENTS): apply Round 1+2 and threading compliance

- <list each rule fixed: R1-Pimpl, R1-ErrorReturn, R1-Ownership, QA, QE, QF, QH, QI, QK, TH-Role, TH-Const>
- <one line per concrete change>
```

Only list rules that actually had violations. Omit rules where no changes were needed.

**After committing, verify the commit actually landed** — execution_subagent
fabricates git output. Read `.git/refs/heads/<branch>` directly and confirm
the hash changed from what it was before the commit.

---

## Done Condition

The task is complete when:
1. `100% tests passed` in the CTest run.
2. The commit is made with only `xash3dpp/` files staged.
3. No violations from Step 2 remain in the module.
4. The boundary spec (if it exists) reflects any renamed symbols or changed signatures.
