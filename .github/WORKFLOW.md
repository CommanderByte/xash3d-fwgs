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
analyse-threading          ← fix concurrency model first
    ↓
sweep-module               ← full compliance pass
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
| `analyse-threading` | After sweep | No |
| `document-architecture` | After analyse-threading | Yes |
| `finish-subsystem` | After document-architecture; gate before merge | No |
| `analyse-subsystem` | Before rewriting a legacy subsystem | Yes (docs only) |
| `analyse-modernization` | Optional future cleanup | No |
| `analyse-utility-consolidation` | Deduplication planning | No |
| `migrate-to-memory` | Memory migration pass | Yes |
