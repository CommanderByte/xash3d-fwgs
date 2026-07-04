# xash3dpp — Session Workflow

This document encodes the correct order of operations for common work patterns.
Following it prevents the "ant death spiral" — cascading reactive fixes caused by
making a change before understanding its blast radius.

---

## Starting a session

Route by current state — `whereami` (the `xash-tools` MCP tool, or
`& .venv\Scripts\python.exe xash3dpp\tools\whereami.py`) derives it and
suggests the next action:

| Situation | Do this first |
|-----------|---------------|
| Fresh clone / new machine | `.github/AGENT-SETUP.md`, then `whereami --doctor` |
| First time in this repo (human or agent) | `init`, then `whereami` |
| Returning after a gap (dormancy) | `whereami --doctor`; follow its `suggested_next` and `doc_pointers` |
| Resuming mid-chunk | `whereami` (read the last checkpoint note), then `status-and-next` if you need the full analysis |
| Just edited the workflow surface (`.github/`, adapters, tools, entry files) | `workflow_sync.py` (full stage) |

After orienting, **pick exactly one chunk and commit to it before starting anything else.**
Open-ended sessions drift. Named, scoped sessions finish.

---

## Implementing a new subsystem

Run these prompts in order. Each one must complete cleanly before the next starts.

```text
analyse-subsystem          ← recon: read the legacy code, produce the boundary spec
    ↓ commit deep-dive-*.md briefs to xash3dpp/docs/legacy-survey/ (committed-recon convention)
    ↓ promote blocking OQs via the crosswalk in decisions-architecture.md §3a
scaffold-subsystem
    ↓
plan-implementation        ← orders the TODO stubs into layers
    ↓
implement (write code)     ← follow the layer order from plan-implementation
+ write-unit-tests         ← parallel with implementation, not after
    ↓
sweep-module               ← only after code is feature-complete
    ↓
analyse-threading          ← documents the final threading model
    ↓
document-architecture      ← produces the reference docs
    ↓
finish-subsystem           ← read-only done checklist; must be SHIP-READY before merge
    ↓
pre-pr                     ← one-shot PR gate: checklist + scan + reviewer; must be SHIP before opening PR
    ↓
analyse-modernization      ← optional; future refactoring opportunities only
```

**Why this order matters:** sweep-module catches violations that are cheap to fix
while the code is fresh. analyse-threading after sweep ensures the threading model
is documented correctly, not speculatively.

**Scaffold gate:** scaffolding is **blocked while any row of the OQ
crosswalk (`decisions-architecture.md` §3a) lists `scaffold` in its Blocks
column** for the target subsystem. Resolve those decisions first (a
decision session), then scaffold. `whereami` reports the blocking rows.

---

## Session scoping & token budget

- **One chunk per session; one session per commit** (see MODEL-GUIDE's
  session-lifecycle section). A session's deliverable is concrete:
  analysis session → a committed doc; implementation session → a green
  commit; decision session → a register entry.
- **Read committed recon instead of re-deriving.** The legacy-survey deep
  dives and boundary specs exist so sessions don't re-read 20k lines of
  legacy C. If recon is missing, run `analyse-subsystem` — don't wing it.
- **Never hand-grep what the tools cover.** `compliance_scan.py`,
  `limits_scan.py`, `stub_scan.py`, `finish_check.py` produce the
  mechanical findings as JSON; spend reasoning on judgment, not searching.
- **Prefer MCP over shell** when the host exposes `xash-tools` (same data,
  fewer round-trips), and **prefer `cpp-lsp`** (definition/references)
  over grep dumps for symbol navigation — compile-accurate and far
  cheaper to interpret.
- **Subagents verify, they don't explore.** Spawn `xash3dpp-reviewer`,
  `abi-watchdog`, or `legacy-parity-auditor` for verification gates; do
  exploratory reading inline where you can act on it.

---

## Making a design or paradigm change

This is where spirals start. The fix: **always run `assess-impact` before touching anything.**

```text
assess-impact "<description of the change>"
    ↓
Read the output — decide whether to proceed
    ↓
Fix in dependency order (from the assess-impact output):
  1. Core headers / definitions
  2. Implementations
  3. Tests
  4. Documentation
    ↓
Commit after each subsystem is green (not at the end of the whole change)
```

**The rule:** if you don't know every file that needs to change before you start,
you're not ready to start. `assess-impact` produces that list.

---

## Changing an `I<X>` interface signature

Adding, removing, or reordering parameters on any `I<X>` pure-virtual method is
a **hand-armed grenade**: it compiles cleanly in each TU touched but silently
produces ODR mismatches or wrong-stub behaviour if any concrete implementation
or test stub is missed. See Q-17 in `decisions-architecture.md`.

```text
assess-impact "change I<X>::method signature"
    ↓
Fix in this exact order:
  1. I<X> interface header (pure-virtual declaration)
  2. All concrete implementations (class A : public I<X>)
  3. All test stubs (class StubX : public I<X> in test files)
  4. All direct call sites of the changed method
  5. Documentation referencing the signature
    ↓
Commit only when build + all tests are green
```

The blast radius always includes test stubs in files you did not write for this
change. A grep on the method name is not sufficient — run `assess-impact` first.

---

## Cleanup / compliance pass on an existing subsystem

```text
detail-audit               ← analysis only; read the report before proceeding
    ↓
implement-audit            ← applies the fixes from detail-audit
    ↓
analyse-threading          ← fix concurrency model
    ↓
sweep-module               ← full compliance pass (catches anything remaining)
    ↓
migrate-to-memory          ← if allocations aren't pool-backed yet
    ↓
analyse-modernization      ← optional, after compliance is clean
```

---

## Commit discipline

- **One commit per stable state** — every commit should leave the build and tests green.
- **One commit per subsystem** when a change spans multiple subsystems.
- **Never batch** a full spiral into one commit at the end. Small commits give rollback points.
- Commit message format: `tag: short description` — see `CONTRIBUTING.md`.
- **Record a checkpoint** (`checkpoint` MCP tool or
  `xash3dpp\tools\checkpoint.py`) at every commit, handoff, or
  interruption — it is what `whereami` shows the next session.

---

## Tooling (xash3dpp/tools/)

Deterministic scripts do the mechanical work; prompts invoke them and
interpret the JSON. Invocation:
`& .venv\Scripts\python.exe xash3dpp\tools\<script>.py … --json` — or,
preferred when available, the `xash-tools` MCP tool of the same name
(identical `data`; `status_table` is exposed as `status`; `dep_scan` is
CLI-only by design). Full table + envelope spec: `xash3dpp/tools/README.md`.

| Script | Purpose | Consumed by |
|--------|---------|-------------|
| `whereami.py` | session ground-truth brief + `--doctor` env checks | session start, dormancy recovery |
| `checkpoint.py` | record advisory intent | every commit / handoff / interruption |
| `build.py` / `test.py` / `refresh_compile_db.py` | build, ctest, clangd DB | sweep-module, implement-audit, retriever, bisect, pre-pr |
| `compliance_scan.py` | reviewer [M] checks as JSON | pre-pr, sweep-module, detail-audit, reviewer pre-pass |
| `limits_scan.py` / `stub_scan.py` / `status_table.py` / `dep_scan.py` | limits, TODO/stubs, status table, dependency edges | limits-audit, plan-implementation, status-and-next, dependency-graph |
| `finish_check.py` | the 9-section done checklist as data | finish-subsystem, pre-pr |
| `workflow_sync.py` | drift gate over the whole workflow surface | after ANY workflow-surface edit |

---

## Prompts at a glance

| Prompt | When to use | Edits files? |
|--------|-------------|-------------|
| `init` | Session start (first time) | No |
| `status-and-next` | Session start (resuming) | No |
| `assess-impact` | Before any cross-cutting change | No |
| `scaffold-subsystem` | New subsystem | Yes |
| `plan-implementation` | After scaffolding | No |
| `write-unit-tests` | During implementation | Yes |
| `sweep-module` | After feature-complete | Yes |
| `detail-audit` | Structural violation scan — read-only, produces report | No |
| `implement-audit` | Apply fixes from a detail-audit run | Yes |
| `analyse-threading` | After sweep | No |
| `document-architecture` | After analyse-threading | Yes |
| `finish-subsystem` | After document-architecture; done checklist | No |
| `pre-pr` | After finish-subsystem; final gate before opening PR | No |
| `analyse-subsystem` | Before rewriting a legacy subsystem | Yes (docs only) |
| `analyse-modernization` | Optional future cleanup | Yes (docs only) |
| `analyse-utility-consolidation` | Deduplication planning | No |
| `migrate-to-memory` | Memory migration pass | Yes |
| `retriever` | Enforce one rule across the entire codebase until clean | Yes |
| `dependency-graph` | Verify EngineContext init order has no cycles | No |
| `bisect` | Find the commit that introduced a regression | No |
| `limits-audit` | Check all magic numbers are in limits.hpp | No |
| `abi-watchdog` | Verify xash3dpp/ does not conflict with frozen ABI surfaces | No |
| `xash3dpp-reviewer` | Full correctness + ABI safety review of xash3dpp/ code | No |
| `legacy-parity-auditor` | Adversarial behavioural-parity audit vs the legacy C reference, before the finish-subsystem gate | No |

---

## Related reference docs

| Document | What it covers |
|---|---|
| `PROMPT-GUIDE.md` | Frontmatter spec, tool tiers, adapters, and model selection for all `.prompt.md` files |
| `AGENT-SETUP.md` | Per-framework setup matrix, human quickstart, env overrides, dormancy recovery |
| `instructions/xash3dpp.instructions.md` | Mandatory C++ patterns and conventions for `xash3dpp/` |
| `xash3dpp/docs/design/decisions-architecture.md` | Structural paradigms: ownership, error returns, interfaces (Q-1 through Q-18) + the OQ crosswalk |
| `xash3dpp/docs/design/decisions-style.md` | Naming, `[[nodiscard]]`, logging, test conventions (QA through QM) |
| `xash3dpp/tools/README.md` | The deterministic tooling: scripts, MCP tools, envelope, state conventions |

---

## Edge cases

**Tests fail after `implement-audit`**

Do not push forward. Revert the failing hunk (`git checkout -- <file>`), note the
violation as `UNRESOLVED` in the audit run, and move on. Never fix a structural
violation at the cost of breaking a test.

**`detail-audit` flags a WARNING you disagree with**

mark it `DEFERRED` in the report with a one-sentence justification, then add a comment
in the source at the relevant site: `// @audit-deferred: <reason>`. Record the
deferral in the subsystem’s boundary doc `## Known Deviations` section.

**Skipping phases**

- Skip `detail-audit` / `implement-audit` for brand-new subsystems — they have no legacy
  violations. Proceed directly with `sweep-module` after the first working implementation.
- Skip `migrate-to-memory` if the subsystem was scaffolded with `scaffold-subsystem`
  (it already uses `create_pool` from day one).
- Skip `analyse-modernization` unless the subsystem is stable and will not change
  significantly in the next sprint.

**`assess-impact` returns dozens of files**

Do not proceed with the change in a single session. Break it into per-subsystem
sub-tasks, each with its own `assess-impact` → implement → commit cycle.
