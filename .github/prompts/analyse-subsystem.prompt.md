---
name: "Analyse legacy subsystem"
description: "Read a legacy subsystem and produce a boundary spec in xash3dpp/docs/ before any rewrite work starts."
argument-hint: "subsystem name (e.g. filesystem, sound, networking, renderer)"
agent: agent
tools: [read, search, edit]
model: claude-sonnet-4-6
---

# Analyse Legacy Subsystem: $ARGUMENTS

You are preparing to rewrite the **$ARGUMENTS** subsystem. Your job is to read the
legacy code thoroughly and produce a boundary spec document — not to write any
implementation code yet.

## Step 1 — Locate the legacy code

Search for all source files related to `$ARGUMENTS` in the legacy tree (everything
outside `xash3dpp/`). Use semantic search and grep to find:
- Primary implementation files
- Headers that declare its public interface
- Callers that depend on it from other subsystems
- Any platform-specific variants under `engine/platform/`

## Step 2 — Read and map the subsystem

For each file found, read enough to answer:
- What is the single responsibility of this subsystem?
- What does it expose to the rest of the engine (functions, structs, globals)?
- What does it depend on from other subsystems?
- What global or static state does it own?
- Are there any quirks, bug-compatibility flags, or non-obvious invariants?

## Step 3 — Identify the external boundary

Determine which parts of this subsystem touch the fixed external ABI surfaces:
- Game DLL callbacks (`engine/eiface.h`, `engine/edict.h`)
- Client DLL callbacks (`engine/cdll_int.h`, `engine/cdll_exp.h`)
- Shared SDK structures (`common/`, `pm_shared/`, `engine/*.h`)

Everything else is internal and free to redesign.

## Step 4 — Write the boundary spec

Create `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` with the following sections:

```markdown
# <Subsystem> Boundary Spec

## Responsibility
One paragraph: what this module does and what it does not do.

## External ABI contracts
List any game DLL / client DLL hooks this subsystem implements or drives.
If none, say "None — fully internal."

## Interface (what the rest of the engine calls)
Table of key functions/types the new module must expose.

## Dependencies (what this module calls)
List of other subsystems this module depends on, and why.

## Owned state
List of significant global/static state this module owns today.

## Quirks and invariants
Bullet list of non-obvious behaviours, bug-compatibility flags, and ordering
constraints that the rewrite must preserve.

## Satellite components
Sub-features of this subsystem that may belong in a separate CMake target.
Apply the Q-11 separation test from `decisions-architecture.md §Q-11` to each
candidate. Record the verdict (same target / separate target) with one-line rationale.
| Candidate feature | Score (0-5) | Verdict |
|-------------------|-------------|---------|
| ...               | ...         | ...     |

## Open questions
Things that need a design decision before implementation can start.
```

Do not start any implementation. The spec is the deliverable.
