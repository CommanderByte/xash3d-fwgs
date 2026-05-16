# xash3dpp — Session Workflow

This document encodes the correct order of operations for common work patterns.
Following it prevents the "ant death spiral" — cascading reactive fixes caused by
making a change before understanding its blast radius.

---

## Starting a session

| Situation | First prompt to run |
|-----------|---------------------|
| First time in this repo | `init` |
| Resuming work | `status-and-next` |

After orienting, **pick exactly one chunk and commit to it before starting anything else.**
Open-ended sessions drift. Named, scoped sessions finish.

---

## Implementing a new subsystem

Run these prompts in order. Each one must complete cleanly before the next starts.

```text
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
