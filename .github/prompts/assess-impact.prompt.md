---
name: "Assess impact of a proposed change"
description: "Before making any cross-cutting change (design paradigm, naming convention, API signature, dependency), enumerate every affected file and subsystem in fix order. Read-only — no files are edited."
argument-hint: "description of the proposed change (e.g. 'rename enum class values from kFoo to Foo', 'add [[nodiscard]] to all bool returns', 'replace raw T* with pool_ptr in public APIs')"
agent: agent
tools: [read, search]
model: claude-sonnet-4-6
---

# Impact Assessment: $ARGUMENTS

Map the full blast radius of this change before anything is touched.
**Write everything to chat. Do not create or edit any files.**

---

## Step 1 — Classify the change

Determine which category applies:

| Category | Description |
|----------|-------------|
| **Convention** | How existing code should be written — naming, attributes, macro usage |
| **API** | A function signature, type definition, or interface shape |
| **Paradigm** | A cross-cutting architectural rule — ownership model, error handling, threading contract |
| **Dependency** | Adds or removes a dependency between subsystems |

State the category and in one sentence explain what is changing and why.

---

## Step 2 — Find all affected sites

Search `xash3dpp/` for every location the change will touch:

- Use grep and search to find all instances of the pattern being changed.
- For each match, record: file path, line range, and which subsystem it belongs to.
- Classify each match as one of:
  - **Must change** — directly implements the thing being changed
  - **Must verify** — calls or depends on the thing; may or may not need a change
  - **Docs only** — a boundary spec, design note, or README that references the pattern

Also search `xash3dpp/tests/` and `xash3dpp/docs/` separately — test and doc
changes are easy to forget and always appear at the end of a spiral.

---

## Step 3 — Check ABI exposure

For each affected file, check whether it is included by or touches:
- `engine/eiface.h`, `engine/edict.h` (game DLL interface)
- `engine/cdll_int.h`, `engine/cdll_exp.h` (client DLL interface)
- `common/`, `pm_shared/`, `engine/*.h` (shared SDK)

If any "must change" file touches a frozen ABI surface, flag it as **ABI RISK**.

---

## Step 4 — Produce a dependency-ordered fix list

List the changes in the order they must be made so that each step compiles cleanly
before the next begins:

1. **Core definitions** — headers, base types, macros (`include/xash3dpp/`)
2. **Implementations** — source files that depend on those headers (`src/`)
3. **Tests** — test files that exercise the changed API (`tests/`)
4. **Documentation** — boundary specs and design notes (`docs/`)

For each entry state: `file path` · `what to change` · `must change / verify / docs`.

---

## Step 5 — Scope estimate and commit plan

Print a summary table:

```
Total files to change:    N
Total files to verify:    N
Subsystems affected:      list them
ABI risk:                 yes / no
```

Then suggest how to slice the work into atomic commits, where each commit leaves
the build and tests in a green state:

```
Commit 1: <what it contains>
Commit 2: <what it contains>
...
```

Err on the side of more commits rather than fewer — one per subsystem is reasonable.

---

## Output format

Structure the chat output as:

1. **Change classification** — category + one-sentence description
2. **Affected sites table** — path, subsystem, classification (must/verify/docs)
3. **ABI risk** — yes/no with specific files if yes
4. **Ordered fix list** — numbered, with file path and what to do
5. **Scope estimate** — the summary table above
6. **Commit plan** — suggested atomic slices

Keep it scannable. The goal is a checklist the implementer can work through top-to-bottom
without backtracking.
