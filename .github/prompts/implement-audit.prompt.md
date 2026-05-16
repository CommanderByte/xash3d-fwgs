---
name: "Implement audit fixes"
description: "Applies all structural violations found by /detail-audit to a module. Re-derives the violations internally (same six checks), applies every finding in severity order (BLOCKERs first, WARNINGs second), updates the boundary doc if one exists, builds, runs tests, and commits. Invoke this after reviewing /detail-audit output, or directly to run the full audit-fix-test-commit cycle without a review gate."
argument-hint: "module name, e.g. 'networking', 'cmd_cvar', 'filesystem'"
agent: agent
tools: [read, search, edit, execute, todo, Build_CMakeTools, RunCtest_CMakeTools]
model: claude-sonnet-4-6
---

# Implement Audit Fixes: `$ARGUMENTS`

Apply all structural violations found by the detail-audit process to the `$ARGUMENTS`
module. This prompt is the implementation half of `/detail-audit`.

Work autonomously from analysis through to commit. Do not ask for confirmation.

---

## Guardrails

- **Only fix what the audit identifies.** No opportunistic improvements.
- **Do not refactor logic, rename symbols, or reorder declarations** beyond what a
  structural rule requires.
- **Do not add docstrings or comments** to code you did not change for a rule reason.
- **If a file has zero violations, do not touch it.**
- **Stop at the module boundary.** Changes in another module's files require a
  separate audit for that module.
- This prompt does **not** re-check style rules (NODISCARD, NAMING_FN, ASSERTIONS,
  etc.) — run `sweep-module` for those.

---

## Step 1 — Produce violations table

Read `.github/prompts/detail-audit.prompt.md` and follow its **Steps 1 and 2**
(Inventory and Audit) for the `$ARGUMENTS` module exactly as written there.

If a violations table from a prior `/detail-audit $ARGUMENTS` run is already
present in the current conversation context, you may use it directly and skip
re-derivation. If the context is absent or uncertain, always re-derive.

Print the full violations table before proceeding. If the table is empty (zero
violations), say so explicitly and stop — nothing to commit.

---

## Step 2 — Apply fixes

Apply every fix from the table. Work through **BLOCKERs first**, WARNINGs second.
Batch independent edits with `multi_replace_string_in_file`.

**Fix patterns**:

- **CHECK-LIMITS — add to limits.hpp**:
  1. Open `xash3dpp/include/xash3dpp/limits.hpp`.
  2. Find or create the `// $ARGUMENTS subsystem` comment block.
  3. Add `#ifndef XASH_LIMIT_<NAME>` / `inline constexpr <type> <name> = <value>;` / `#endif`.
  4. In the original file, replace the literal with `::xash::limits::<name>`.

- **CHECK-HEADERS — move a private header**:
  1. Move the `.hpp` from `include/xash3dpp/$ARGUMENTS/` to
     `include/xash3dpp/private/$ARGUMENTS/`.
  2. Update all `#include` paths in `.cpp` files that include it.
  3. Verify no public header now transitively exposes the moved header.

- **CHECK-MEMORY — replace forbidden allocation**:
  - Replace `new T(args)` (non-pimpl) with `memory::pool_new<T>(pool, args)` when the
    pool handle is available in `InitParams`. If the pool handle does not yet exist,
    add a `// TODO: wire pool — pending Q-13 ALLOC_POLICY migration` comment, convert
    to `std::make_unique<T>` as a temporary measure, and record as an accepted WARNING.
  - Replace `delete ptr` with `memory::mem_free(ptr)`.
  - For hot-path `std::vector` / `std::deque` class members missing a
    `// @pre-reserved: <LIMIT_NAME>` annotation: add the annotation and ensure
    `.reserve(::xash::limits::<name>)` is called in the class's `init()` or constructor.

- **CHECK-STATS — add stats struct**:
  Add a minimal `<Subsystem>Stats` struct to the context header. Start with only the
  counters that already exist in the code (bytes in/out, packets sent/received, etc.).
  Do not add counters that do not yet have corresponding code paths.

- **CHECK-DI — fix dependency access**:
  Add the dependency to `<Subsystem>InitParams`. Store it in the pimpl struct. Replace
  the global/file-scope access with the stored pointer. Limit call-site changes to
  the module under audit.

- **CHECK-COMPAT — move inline guards to link-time selection**:
  Move the guarded block into a pair of `compat_<variant>.cpp` files selected via
  CMakeLists.txt. Keep the core logic file clean.

For any WARNING that cannot be mechanically fixed (e.g. a DI gap that requires
wiring not yet available), add a `// detail-audit: accepted — <reason>` comment on
the relevant declaration and record the deferral in the Step 5 summary.

---

## Step 3 — Boundary doc update

Check whether `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` exists.

If it does, scan the fix log from Step 2 for any of these structural changes:

| Change type | Update target in boundary doc |
|-------------|-------------------------------|
| New limit added to `limits.hpp` | "Fixed Limits" or "Wire Constants" section |
| Public header moved to `private/` | File inventory / header tree section |
| New or changed `InitParams` field | "Dependencies" or "Initialization" section |
| New `<Subsystem>Stats` struct or `stats()` | "Observability" or "Stats" section |

**Do not rewrite or restructure the boundary doc.** Make only the targeted additions
that reflect structural changes from Step 2. If a section does not exist and the
change warrants one, add a short new section at the end of the doc.

If the boundary doc does not exist, skip this step entirely.

---

## Step 4 — Build and test

```powershell
cmake -S xash3dpp -B build
cmake --build build --config Debug 2>&1 | Select-String "error C[0-9]|error:"

ctest --test-dir build -C Debug --output-on-failure -R "$ARGUMENTS" 2>&1 | Select-Object -Last 20
```

If there are build errors, trace them to the fix that caused them, correct the fix,
and rebuild. Do not weaken or delete tests. Do not add new tests in this prompt —
structural fixes should not require new test logic; if they do, note it in the
summary as a follow-up item.

---

## Step 5 — Commit

Stage `xash3dpp/` and any boundary doc in `xash3dpp/docs/boundaries/`:

```powershell
cd "c:\git\xash3d-fwgs"
git add -A xash3dpp/
git diff --cached --name-only | Where-Object { $_ -notlike "xash3dpp/*" }
```

If that second command produces any output, unstage those files.

Commit message format (project convention — `tag: description`, no Conventional Commits):

```
$ARGUMENTS: detail-audit structural fixes

- <one line per concrete change, grouped by CHECK-* category>
```

Verify the commit landed:

```powershell
git log --oneline -1
```

---

## Done Condition

Task is complete when:
1. All BLOCKER violations are resolved.
2. All WARNING violations are either fixed or explicitly accepted with a
   `// detail-audit: accepted — <reason>` comment.
3. Build succeeds and `100% tests passed`.
4. Boundary doc updated if applicable.
5. The commit is on the branch.
6. A final summary lists: every check run, violations found per check, how each
   was resolved (fixed / accepted / deferred), and any follow-up items noted.
