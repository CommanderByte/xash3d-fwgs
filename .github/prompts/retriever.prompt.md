---
name: "Retriever — single-rule codebase enforcement"
description: "Hunt down every violation of one specific rule across the entire xash3dpp/ tree and fix them all — code, tests, and docs. Loops until the rescan comes back empty or the pass limit is reached. Narrow scope, unlimited depth."
argument-hint: "RULE_NAME: one-sentence description of what changed. E.g. 'ERROR_RETURN: logging required at public API boundary only, not in private helpers'"
agent: agent
tools: [read, search, edit, run, terminal]
mode: agent
model: claude-sonnet-4-6
---

# Retriever: `$ARGUMENTS`

You have one job. Find every violation of the rule described in `$ARGUMENTS`
across all of `xash3dpp/` and fix them — in code, tests, and documentation.
Then scan again. Repeat until the scan comes back empty.

Do not fix anything else. Not even things you notice along the way.

---

## Parse the argument

Split `$ARGUMENTS` on the first `:` to extract:

- **RULE** — the rule name (e.g. `ERROR_RETURN`)
- **CHANGE** — what the rule now requires (e.g. `logging required at public API
  boundary only, not in private helpers`)

Read the full rule definition from the relevant design doc before proceeding:

- Architecture rules (PIMPL_MOVE, ERROR_RETURN, OWNERSHIP, DI_PARAMS, etc.):
  [`xash3dpp/docs/design/decisions-architecture.md`](../../xash3dpp/docs/design/decisions-architecture.md)
- Style rules (NODISCARD, NAMING_FN, NAMING_ENUM, ASSERTIONS, LOGGING, etc.):
  [`xash3dpp/docs/design/decisions-style.md`](../../xash3dpp/docs/design/decisions-style.md)
- Threading rules (TH-Role, TH-Const, TH-GLOBALS):
  [`xash3dpp/docs/design/threading-model.md`](../../xash3dpp/docs/design/threading-model.md)
- Stats rules (STATS_TIERS):
  [`xash3dpp/docs/design/debug-stats-design.md`](../../xash3dpp/docs/design/debug-stats-design.md)

---

## Guardrails — read before touching anything

- **One rule only.** If you notice other violations while scanning, record them
  as a NOTE at the end. Do not fix them.
- **No logic changes.** Fixing a log-call site is not an invitation to restructure
  the function around it.
- **No doc rewrites.** Update only the lines that are factually wrong given the
  rule change. Do not improve prose or restructure sections.
- **Build must stay green after every pass.** If a fix breaks the build, revert
  it and record it as UNRESOLVED.
- **Pass limit: 6.** If violations remain after 6 fix-and-rescan cycles, stop,
  report what is left, and explain why they could not be resolved automatically.

---

## Phase 1 — Map (read-only)

Search `xash3dpp/` exhaustively for every site that is affected by the CHANGE.
Use multiple grep patterns — violations may appear in different forms.

For each hit, record:

| # | File | Line | Kind | Notes |
|---|------|------|------|-------|
| 1 | ... | ... | ADD / REMOVE / UPDATE_DOC / VERIFY | ... |

**Kind values:**

- `ADD` — rule now requires something that is missing here
- `REMOVE` — rule no longer requires something that is present here (excess)
- `UPDATE_DOC` — a doc, comment, or prompt references the old rule behaviour
- `VERIFY` — looks like it might be affected but needs a human read to decide

Do not make any changes during this phase.

Print the full table to chat before continuing.

---

## Phase 2 — Triage

For each `VERIFY` row, read the file at that line and classify it as one of the
other kinds, or `SKIP` (genuinely not affected). Update the table.

Print the final triage table (no `VERIFY` rows remaining) before continuing.

---

## Phase 3 — Fix

Work through the table in this order:
1. Source files (`xash3dpp/src/`, `xash3dpp/include/`)
2. Test files (`xash3dpp/tests/`)
3. Documentation (`xash3dpp/docs/`, `.github/`)

For each row, apply the minimal change the rule requires:

- **ADD**: insert the required element (e.g. `platform::log(...)` before a
  public-API failure return). Add any missing `#include` at the top of the file.
- **REMOVE**: delete or replace the excess element (e.g. remove a `platform::log`
  call that the rule no longer requires from a private helper).
- **UPDATE_DOC**: update only the sentence or line that references the old behaviour.

After fixing each file, record the row as `DONE`.

---

## Phase 4 — Build and test

```powershell
cd "c:\git\xash3d-fwgs\xash3dpp\build\Debug"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . 2>&1 | Select-String "error C[0-9]|error:"

cd "c:\git\xash3d-fwgs\xash3dpp\build"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Debug --output-on-failure -j1 2>&1 | Select-Object -Last 20
```

If the build fails, trace the error to the fix that caused it, revert that fix,
mark the row `UNRESOLVED (build break)`, and continue with the remaining rows.

If a test fails, trace it to the fix, revert if necessary, mark `UNRESOLVED
(test failure)`, and continue.

Do not proceed to Phase 5 until the build and all tests are green.

---

## Phase 5 — Rescan

Repeat Phase 1 using the same grep patterns. If any new violations appear (not
in the previous `UNRESOLVED` list), add them to the table and loop back to
Phase 3.

If the rescan is empty (or only `UNRESOLVED` rows remain), proceed to Phase 6.

Track the pass count. If this is pass 6, stop here and report.

---

## Phase 6 — Commit

For each subsystem that had at least one `DONE` fix, create one commit:

```powershell
cd "c:\git\xash3d-fwgs"
git add -A xash3dpp/
git diff --cached --name-only | Where-Object { $_ -notlike "xash3dpp/*" -and $_ -notlike ".github/*" }
```

If the second command produces output, unstage those files first.

Commit message format:

```
refactor(<subsystem>): enforce RULE across <subsystem>

- <one line per concrete change>
```

For documentation-only changes, use `doc(<location>):` instead of `refactor`.

After committing, verify the commit landed by reading `.git/refs/heads/<branch>`.

---

## Done — final report

Print a summary:

```
Retriever — RULE complete
Passes:   <N>
Fixed:    <count> sites across <count> files
Committed: <list of commit hashes and subsystems>
Unresolved: <list of UNRESOLVED rows with reason, or "none">
Side-notes: <any other violations noticed but not fixed, per guardrail>
```
