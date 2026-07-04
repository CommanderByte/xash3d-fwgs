---
name: "Finish-subsystem checklist"
description: "Read-only done checklist for a completed xash3dpp subsystem. Reports [x]/[ ] status to chat. Run after document-architecture and before merging."
argument-hint: "subsystem name, e.g. 'sound', 'renderer', 'networking'"
agent: agent
tools: [read, search, execute]
model: claude-haiku-4-5-20251001
---

# Done Checklist: `$ARGUMENTS`

The canonical 9-section checklist is computed by the repo tool (single
source, shared with pre-pr Phase 1):

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\finish_check.py $ARGUMENTS --run-tests --json
```

Your job is interpretation, not re-derivation:

1. Report each of the 9 items as `[x]` (pass) or `[ ]` (fail) with a one-line
   note taken from the tool's evidence.
2. For every `needs-judgment` item, resolve it yourself by reading the cited
   files — e.g.:
   - **limits** (item 2): classify each magic/shadow candidate per the
     limits-audit exemptions (math constants, wire-frozen discriminators);
     only unclassified tunables make the item `[ ]`.
   - **stats** (item 3): if neither a `Stats` struct nor the
     `// no hot path — stats exempt` comment exists, check whether the
     subsystem actually qualifies for exemption before failing it.
   - **compat/satellite** (item 9): confirm the boundary spec documents the
     Q-11 verdict for any sub-feature.
3. Do not guess: every `[x]` must be backed by tool evidence or a file you
   actually read.

## Summary

After resolving all items, output:

```text
$ARGUMENTS — done checklist summary
Passed: <N>/9 sections fully green
Blocked: <list any [ ] items that are BLOCKERs>
Warnings: <list any [ ] items that are non-blocking>
Verdict: SHIP-READY | NEEDS-WORK
```

`SHIP-READY` requires all 9 sections fully `[x]`.

---

### Manual fallback (no Python available)

Verify by reading files: 1) boundary spec exists at
`xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` covering Responsibility /
ABI contracts / Interface / Dependencies / Owned state / Quirks; 2) fixed
sizes live in `limits.hpp` with `XASH_LIMIT_*` overrides and no magic
literals remain in the subsystem; 3) `<Subsystem>Stats` + `stats() const
noexcept`, or the documented exemption comment; 4) `[[nodiscard]]` on every
non-void public return; 5) `snake_case` member functions (ABI-fixed `I<X>`
exempt) and `PascalCase` enum values without `k` prefix; 6) tests exist in
`xash3dpp/tests/$ARGUMENTS/`, include `../test_helpers.hpp`, no local
`#define CHECK`; 7) `docs/architecture/$ARGUMENTS/` exists and documents
threading; 8) `ctest --preset debug -R test_$ARGUMENTS` is 100% green;
9) no engine-wide compat type, Q-11 satellite verdicts documented, no direct
OS socket call outside `src/platform/*/os_socket.cpp`.
