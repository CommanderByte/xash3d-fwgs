---
name: "Pre-PR gate"
description: "One-shot PR readiness check for a completed xash3dpp subsystem. Runs finish-subsystem checklist, a targeted compliance scan, and the reviewer agent in sequence. Reports a single SHIP / HOLD verdict. Run immediately before opening a pull request."
argument-hint: "subsystem name, e.g. 'cmd_cvar', 'filesystem', 'sound'"
agent: agent
tools: [read, search, run, terminal]
mode: agent
model: claude-sonnet-4-6
---

# Pre-PR Gate: `$ARGUMENTS`

Three checks, one verdict. Work through each phase in order. Do not open a PR
until the final verdict is SHIP.

---

## Phase 1 — Done checklist (finish-subsystem)

Verify every item below by reading the files. Report `[x]` (confirmed) or
`[ ]` (missing/incomplete).

**1. Boundary spec**
- [ ] `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` exists and covers:
      Responsibility, External ABI contracts, Interface table, Dependencies,
      Owned state, Quirks and invariants

**2. limits.hpp**
- [ ] All fixed buffer sizes, pool capacities, and count limits are in
      `xash3dpp/include/xash3dpp/limits.hpp` with `XASH_LIMIT_*` overrides
- [ ] No magic number literals in subsystem headers or source files

**3. STATS_TIERS**
- [ ] Either: `<Subsystem>Stats` struct + `stats() const noexcept` accessor exists
- [ ] Or: exempt (pure function namespace or init-once-at-startup) with
      `// no hot path — stats exempt` comment present

**4. NODISCARD**
- [ ] Every non-`void` public return carries `[[nodiscard]]`

**5. NAMING_FN / NAMING_ENUM**
- [ ] All member functions are `snake_case` (ABI-fixed `I<X>` vtable methods exempt)
- [ ] All `enum class` values are `PascalCase`, no `k` prefix

**6. Tests**
- [ ] Test files exist in `xash3dpp/tests/$ARGUMENTS/`
- [ ] Tests use `#include "../test_helpers.hpp"` — no local `#define CHECK`

**7. Architecture docs**
- [ ] `xash3dpp/docs/architecture/$ARGUMENTS/README.md` (or equivalent) exists
- [ ] Threading model is documented

**8. Tests pass**

```powershell
cd "c:\git\xash3d-fwgs\xash3dpp\build"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Debug --output-on-failure -R "test_$ARGUMENTS" 2>&1 | Select-Object -Last 10
```

- [ ] `100% tests passed` for the `test_$ARGUMENTS` target

---

## Phase 2 — Targeted compliance scan

Run these focused grep checks. Flag any hits as BLOCKER or WARNING.

```powershell
$src = "c:\git\xash3d-fwgs\xash3dpp\src\$ARGUMENTS"
$inc = "c:\git\xash3d-fwgs\xash3dpp\include\xash3dpp\$ARGUMENTS"

# ABI violations
Select-String -Path "$src\*","$inc\*" -Pattern "malloc|calloc|realloc|\bfree\b|^\s*new |\bdelete\b" -Recurse
# DLL boundary: string_view crossing extern "C"
Select-String -Path "$src\*","$inc\*" -Pattern 'extern\s+"C"' -Recurse
# Forbidden diagnostics
Select-String -Path "$src\*" -Pattern "\bprintf\b|\bfprintf\b|\bstd::cout\b" -Recurse
# Missing thread role assertion (stateful public functions)
Select-String -Path "$src\*" -Pattern "void.*::(init|shutdown|reset|flush|add|remove|register|unregister|set_|update_)" -Recurse
```

For each hit, classify:
- Raw allocation outside `memory/` → **BLOCKER**
- `extern "C"` boundary with `string_view` param → **BLOCKER**
- `printf`/`fprintf` outside `platform/` → **WARNING**
- Public mutating method missing `assert_thread_role` → **WARNING**

---

## Phase 3 — Reviewer sweep

Read the subsystem's public headers and the first 100 lines of each `.cpp` in
`xash3dpp/src/$ARGUMENTS/`. Apply the full reviewer checklist from
`.github/agents/xash3dpp-reviewer.agent.md` mentally.

Report any BLOCKERs or WARNINGs not already caught in Phases 1–2, using the
standard format:

```
[SEVERITY] <file>:<line-range>
Rule: <rule name>
Finding: <one sentence>
Suggestion: <optional fix>
```

---

## Verdict

```
$ARGUMENTS — pre-PR gate
Phase 1 checklist:  <N>/8 green
Phase 2 scan:       CLEAN | <N> hits
Phase 3 review:     CLEAN | <N> findings

BLOCKERs: <list or "none">
WARNINGs: <list or "none">

Verdict: SHIP | HOLD
```

**SHIP** requires: all 8 checklist items `[x]`, zero BLOCKERs, tests passing.
WARNINGs do not block shipping but should be recorded as follow-up issues.
