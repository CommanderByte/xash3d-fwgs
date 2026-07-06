---
name: "Pre-PR gate"
description: "One-shot PR readiness check for a completed xash3dpp subsystem. Runs finish-subsystem checklist, a targeted compliance scan, and the reviewer agent in sequence. Reports a single SHIP / HOLD verdict. Run immediately before opening a pull request."
argument-hint: "subsystem name, e.g. 'cmd_cvar', 'filesystem', 'sound'"
agent: agent
tools: [read, search, execute, xash-tools/*]
model: claude-sonnet-4-6
---

# Pre-PR Gate: `$ARGUMENTS`

Three checks, one verdict. Work through each phase in order. Do not open a PR
until the final verdict is SHIP.

The mechanical work is done by the repo tools (`python` = the repo venv,
`.venv\Scripts\python.exe`); your job is to run them, interpret the JSON, and
apply judgment where an item says `needs-judgment` or a finding is marked
`candidate-*`.

---

## Phase 1 — Done checklist (finish-subsystem)

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\finish_check.py $ARGUMENTS --run-tests --json
```
*(MCP: xash-tools tool `finish_check` — same data.)*

This emits the canonical 10-section done checklist (boundary spec, limits
(QO-classified), stats, nodiscard, naming, tests + macros, architecture docs
+ threading, ctest run, compat/satellite, lifecycle + annotation discipline
(Q-22/QN)) as `pass | fail | needs-judgment` items — the same checklist the
`finish-subsystem` prompt reports, from one source.

- Report each item as `[x]` (pass) or `[ ]` (fail), with the evidence line.
- For every `needs-judgment` item, do the judgment now by reading the cited
  evidence (e.g. classify magic-number candidates per the limits-audit
  exemptions; verify the Q-11 verdict paragraph exists in the boundary spec)
  and resolve it to `[x]` or `[ ]`.

## Phase 2 — Targeted compliance scan

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\compliance_scan.py $ARGUMENTS --checks prepr --json
```
*(MCP: xash-tools tool `compliance_scan` — same data.)*

Classify the findings:

- `blocker` severity (raw allocation outside `memory/`, `string_view` at an
  `extern "C"` boundary, direct OS socket call outside the platform layer)
  → **BLOCKER**
- `printf`/`fprintf` outside `platform/` → **WARNING**
- `candidate-*` findings (thread-assert, extern-C string_view proximity) →
  read the flagged site; confirm or discard, then classify per the rule's
  hint. A confirmed missing `assert_thread_role` on a public mutator →
  **WARNING**.

## Phase 3 — Reviewer sweep

Read the subsystem's public headers and the first 100 lines of each `.cpp` in
`xash3dpp/src/$ARGUMENTS/`. Apply the reviewer charter from
`.github/agents/xash3dpp-reviewer.agent.md` — **supply the Phase 2 JSON as
the reviewer's mechanical pre-pass** so it spends its effort on the [J]
judgment checks, not re-grepping the [M] set.

Report any BLOCKERs or WARNINGs not already caught in Phases 1–2, using the
standard format:

```text
[SEVERITY] <file>:<line-range>
Rule: <rule name>
Finding: <one sentence>
Suggestion: <optional fix>
```

---

## Verdict

```text
$ARGUMENTS — pre-PR gate
Phase 1 checklist:  <N>/9 green
Phase 2 scan:       CLEAN | <N> hits
Phase 3 review:     CLEAN | <N> findings

BLOCKERs: <list or "none">
WARNINGs: <list or "none">

Verdict: SHIP | HOLD
```

**SHIP** requires: all 9 checklist items `[x]`, zero BLOCKERs, tests passing.
WARNINGs do not block shipping but should be recorded as follow-up issues.

Then record a checkpoint (`checkpoint` MCP tool or
`xash3dpp\tools\checkpoint.py`): `step="pre-pr"`, note includes the
SHIP/HOLD verdict.

---

### Manual fallback (no Python available)

Phase 1: read `.github/prompts/finish-subsystem.prompt.md`'s fallback list
and verify each section by hand; run
`ctest --preset debug -R "test_$ARGUMENTS"` from `xash3dpp/` with the
VS2022-bundled ctest. Phase 2: grep `src/$ARGUMENTS` + the subsystem include
tree for `malloc|calloc|realloc|\bfree\b|^\s*new |\bdelete\b`,
`extern "C"` (then check `string_view` in those files),
`\bprintf\b|\bfprintf\b|std::cout`,
`void.*::(init|shutdown|reset|flush|add|remove|register|unregister|set_|update_)`
(check `assert_thread_role` in the body),
`IEngineCompatPolicy|GlobalCompatPolicy|unified_compat`, and
`::(socket|bind|sendto|recvfrom|WSAStartup|getaddrinfo)\b`.
