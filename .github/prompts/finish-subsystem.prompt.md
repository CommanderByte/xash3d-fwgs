---
name: "Finish-subsystem checklist"
description: "Read-only done checklist for a completed xash3dpp subsystem. Reports [x]/[ ] status to chat. Run after document-architecture and before merging."
argument-hint: "subsystem name, e.g. 'sound', 'renderer', 'networking'"
agent: agent
tools: [read, search]
model: claude-haiku-4-5-20251001
---

# Done Checklist: `$ARGUMENTS`

Verify every item below by reading the files — do not guess. Report each item
as `[x]` (confirmed) or `[ ]` (missing/incomplete), followed by a one-line
note. Output nothing else until the summary at the end.

---

## 1. Boundary spec

- [ ] `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` exists
- [ ] Spec covers: Responsibility, External ABI contracts, Interface table,
      Dependencies, Owned state, Quirks and invariants

## 2. limits.hpp

- [ ] Any fixed buffer sizes, pool capacities, or count limits for `$ARGUMENTS`
      are defined in `xash3dpp/include/xash3dpp/limits.hpp` with the
      `#ifndef XASH_LIMIT_* / inline constexpr / #else / #endif` override pattern
- [ ] No magic number literals in the subsystem's headers or source files

## 3. STATS_TIERS

- [ ] Either: a `<Subsystem>Stats` struct exists with appropriate tiers and a
      `stats() const noexcept` accessor on the context class
- [ ] Or: subsystem qualifies for exemption (pure function namespace with no mutable
      state, **or** init-once-at-startup utility) and has a `// no hot path — stats exempt`
      comment at the context class declaration. Note: call frequency alone does not qualify.

## 4. NODISCARD (QA)

- [ ] Every non-`void` public function return carries `[[nodiscard]]`
      (or has a documented reason for the omission at the declaration site)

## 5. NAMING_FN / NAMING_ENUM (QE / QF)

- [ ] All member functions are `snake_case` (except ABI-fixed `I<X>` vtable methods)
- [ ] All `enum class` values are `PascalCase` with no `k` prefix

## 6. Tests

- [ ] Test file(s) exist in `xash3dpp/tests/$ARGUMENTS/`
- [ ] Tests use `#include "../test_helpers.hpp"` (no local `#define CHECK`)
- [ ] All tests pass (`100% tests passed` in CTest for the `test_$ARGUMENTS` target)

## 7. Architecture docs

- [ ] `xash3dpp/docs/architecture/$ARGUMENTS/README.md` (or equivalent) exists
- [ ] Threading model for the subsystem is documented (which functions are
      main-thread-only, which are thread-safe, any background-thread patterns)

## 8. No BLOCKERs

- [ ] No open BLOCKER findings from the reviewer agent (ABI violations,
      `throw`/`try`/`catch`, `malloc`/`new` outside pimpl, `string_view` at DLL
      boundary, game DLL call off main thread, etc.)

## 9. Compat and satellite design (Q-11 / Q-12)

- [ ] No engine-wide compat policy type (`IEngineCompatPolicy`, a unified compat
      struct shared across subsystems, etc.) — each subsystem with behavioural quirks
      owns its own `ICompatPolicy` or feature-specific variant (Q-12).
- [ ] Sub-features that scored ≥ 2 on the Q-11 satellite test have their own CMake
      target and are not bundled into the parent library. Verdict is documented in
      the boundary spec.
- [ ] No direct OS socket call (`::socket`, `::bind`, `::sendto`, `::recvfrom`,
      `WSAStartup`) outside `src/platform/*/os_socket.cpp`.

---

## Summary

After checking all items, output:

```
$ARGUMENTS — done checklist summary
Passed: <N>/9 sections fully green
Blocked: <list any [ ] items that are BLOCKERs>
Warnings: <list any [ ] items that are non-blocking>
Verdict: SHIP-READY | NEEDS-WORK
```

`SHIP-READY` requires all 9 sections fully `[x]`.
