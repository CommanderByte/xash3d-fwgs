---
name: "Document xash3dpp submodule architecture"
description: "Produce or update technical architecture documentation for a xash3dpp submodule. Creates docs/architecture/<module>/ with README.md (overview), index.md (concept/file index), and one detailed .md per major concept or source file. If docs already exist, audits them for staleness and updates rather than overwrites. Use when a subsystem is complete enough to document, when onboarding needs a reference, or when the subsystem has changed and docs need refreshing."
argument-hint: "submodule name, e.g. 'memory', 'filesystem', 'utilities'"
agent: agent
tools: [read, search, edit]
model: claude-sonnet-4-6
---

# Document architecture for: `$ARGUMENTS`

Produce or update a structured reference for the **$ARGUMENTS** subsystem.
All output goes under `xash3dpp/docs/architecture/$ARGUMENTS/`.

---

## Step 1 — Inventory: source code and existing docs

Read **both** the source and any existing documentation in parallel so you can
compare them before writing anything.

### 1a — Read the source

1. **Public headers** — `xash3dpp/include/xash3dpp/$ARGUMENTS/` and any
   root-level headers that belong to this module.
2. **Private/internal headers** — `xash3dpp/include/xash3dpp/private/$ARGUMENTS/`
3. **Implementations** — `xash3dpp/src/$ARGUMENTS/` (all `.cpp` files)
4. **Tests** — `xash3dpp/tests/$ARGUMENTS/` (to understand the tested surface)
5. **CMakeLists** — `xash3dpp/src/$ARGUMENTS/CMakeLists.txt` for build deps
   and target shape.
6. **Existing boundary and legacy docs** — `xash3dpp/docs/boundaries/` and
   `xash3dpp/docs/legacy-survey/` for prior analysis.
7. **Existing architecture docs as structural reference** — the completed docs
   for `memory/`, `filesystem/`, and `utilities/` under
   `xash3dpp/docs/architecture/` illustrate the expected output structure,
   level of detail, and cross-linking conventions. Read one of them before
   writing if you are creating docs for a new subsystem.

While reading, note:
- The module's single responsibility and non-goals
- Major types, their ownership, and their lifecycles
- Thread-safety guarantees (mutexes, atomics, single-owner contracts)
- The public API surface vs. the internal machinery
- How this module connects to xash3dpp_memory, xash3dpp_utilities, and other deps
- Anything that differs from the legacy engine behaviour

### 1b — Read existing architecture docs (if any)

Check whether `xash3dpp/docs/architecture/$ARGUMENTS/` already exists:

- If it **does not exist**: proceed to Step 2 in "create" mode for all files.
- If it **does exist**: read every `.md` file in it, then proceed to Step 2 in
  "update" mode. In update mode, treat each existing file as a draft that may
  be partially correct, partially stale, or missing new content.

Also check for a legacy flat file at
`xash3dpp/docs/architecture/$ARGUMENTS.md` — if found, its content should be
folded into the new directory structure and the flat file updated with a
redirect note.

---

## Step 2 — Audit existing docs against source (update mode only)

Skip this step if you are in create mode.

For each existing doc file, identify:

**Stale content** — things the doc says that are no longer true:
- Types, functions, or fields that have been renamed, moved, or removed
- Descriptions of behaviour that has changed (e.g. a threading model that was
  revised, a pool that was added, a mutex that replaced an atomic)
- CMake target names, dependency lists, or file paths that have changed

**Missing content** — things the source has that the docs do not mention:
- New types or functions added since the doc was written
- New source files or headers not listed in the index
- New CMake targets or optional build features
- New threading guarantees or invariants established by recent changes

**Inconsistencies between doc files** — contradictions across pages:
- A type described differently in `README.md` vs. its concept page
- A lifecycle rule stated in one concept page that conflicts with another
- Cross-links that point to pages or anchors that do not exist

Produce a short internal tally (you do not need to write this to disk):

```
STALE:   <list of items to correct>
MISSING: <list of items to add>
INCONSISTENT: <list of contradictions to resolve>
```

Use this tally to guide every edit in Steps 3–6.

---

## Step 3 — Identify the concepts

Group the source into logical **concepts** — cohesive topics that each deserve
their own documentation page. Good concepts are:

| Kind | Example |
|------|---------|
| Core type / class | `PoolBucket`, `ISearchBackend`, `OsFile` |
| Subsystem feature | pool lifecycle, archive mounting, backend dispatch |
| Cross-cutting concern | threading model, memory ownership, error handling |
| Platform abstraction | OS I/O layer, Android AAsset integration |
| Build / CMake shape | target layout, optional features |

Aim for 3–8 concepts. If a single source file maps cleanly to one concept,
use the file name; if several files share one idea, group them.

In **update mode**: compare the concept list derived from the current source
against the set of existing concept pages. Identify:
- Concepts that need a new page (source has grown)
- Pages whose concept no longer exists or was folded into another (mark for
  removal or merging — do not silently leave orphan pages)
- Pages that still exist and only need patching

---

## Step 4 — Write or update `README.md` (overview)

**Create mode**: create `xash3dpp/docs/architecture/$ARGUMENTS/README.md`
using the template below.

**Update mode**: read the existing `README.md`, then edit it to:
- Correct any stale purpose/goals/invariants identified in Step 2
- Add any new design goals or invariants introduced by recent changes
- Update the "Architecture at a glance" section and ASCII diagram if the
  component structure has changed
- Sync the concept index at the bottom to match the actual set of pages after
  Step 6 runs

Template (for create mode; use as a reference checklist in update mode):

```markdown
# $ARGUMENTS — Architecture Overview

> **Source**: `xash3dpp/src/$ARGUMENTS/`  
> **Public API**: `xash3dpp/include/xash3dpp/$ARGUMENTS/`  
> **Legacy reference**: `<path in legacy tree, or "n/a — new subsystem">`

## Purpose

One paragraph: what problem this module solves and what it explicitly does
*not* do.

## Design goals

Bullet list (3–6 items): the principles that shaped every design decision.
Example: "thread-safe for concurrent reads", "no exceptions", "pool-backed
allocations for engine-wide stats".

## Key invariants

Bullet list of correctness rules that must never be violated. Example: "pool
must be destroyed *after* all backends are freed", "OpenFile may be called
concurrently; AddGameDirectory may not".

## Relationship to legacy code

Short paragraph: what changed, what was preserved, what was deliberately
discarded.

## Architecture at a glance

A short prose description of the major layers or components, followed by an
ASCII diagram if one clarifies the structure.

## Index of concepts

Link to each page in this directory (auto-populated from Step 6):

- [index.md](./index.md) — full file/symbol index
- [concept-1.md](./concept-1.md) — short label
- …
```

---

## Step 5 — Write or update `index.md` (file/symbol index)

**Create mode**: create `xash3dpp/docs/architecture/$ARGUMENTS/index.md`
using the template below.

**Update mode**: edit the existing `index.md` to:
- Add rows for any new headers, source files, types, or CMake targets
- Remove or correct rows for renamed/deleted symbols found in Step 2
- Keep the table structure intact; do not reformat rows that are still correct

Template:

```markdown
# $ARGUMENTS — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `$ARGUMENTS.hpp` | `xash::$ARGUMENTS` | `<class>`, `<enum>`, `<function>` |
| … | … | … |

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/$ARGUMENTS/foo.hpp` | … |
| … | … |

## Source files

| File | Responsibility |
|------|---------------|
| `src/$ARGUMENTS/foo.cpp` | … |
| … | … |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `Foo` | class | `foo.hpp` | … |
| … | … | … | … |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_$ARGUMENTS` | STATIC | … | … |
| … | … | … | … |
```

---

## Step 6 — Write or update one page per concept

For each concept identified in Step 3:

**Create mode**: create
`xash3dpp/docs/architecture/$ARGUMENTS/<concept-slug>.md` using the template
below.

**Update mode**: edit the existing concept page to:
- Correct stale descriptions (renamed fields, changed thread-safety guarantees,
  updated lifecycle rules, etc.)
- Add sections for new types, operations, or invariants
- Fix broken cross-links (anchors or filenames that no longer exist)
- Do not rewrite sections that are still accurate — minimal, targeted edits
  preserve reviewer confidence

For concepts that were **removed** from the source: add a deprecation notice
at the top of the page and a link to wherever the concept moved, rather than
silently deleting the file. Only delete the file if the concept was truly
eliminated with no successor.

For concepts that are **new**: create the page from scratch using the template.

Template (for new pages; use as a checklist when updating):

```markdown
# <Concept name>

> **Defined in**: `<header>` / `<source file(s)>`  
> **Namespace**: `xash::<subsystem>`

## Overview

One or two paragraphs explaining what this concept is, why it exists, and how
it fits into the larger module.

## <Type name / function group>

For each major public or internal type, or cohesive group of functions:

### Fields / members

| Name | Type | Role |
|------|------|------|
| `foo_` | `std::atomic<std::size_t>` | … |
| … | … | … |

### Key operations

Describe the most important operations (methods, free functions) with:
- A one-line summary of what it does
- Pre-conditions and post-conditions if non-obvious
- Thread-safety guarantee (e.g. "lock-free, may be called from any thread")
- Any interaction with other concepts (e.g. "calls `mem_alloc` on the pool")

### Lifecycle / ownership

Describe how instances are created, transferred, and destroyed. Note who owns
the memory (pool-allocated, stack, unique_ptr, etc.) and what cleanup order
is required. Name the Q-22 shape explicitly where it applies: pool-owned
classes (`create_<thing>` factory + operator-delete pair), pimpl exceptions,
orchestrator free functions, documented module statics (the Q-2 exception
table pattern), and the QN annotations readers should trust
(`@lifetime:`, `@thread-safety:`).

## Threading model

Describe the synchronisation strategy for this concept:
- Which operations are thread-safe and which are single-threaded
- Which mutexes / atomics protect which fields
- Any lock-ordering constraints

## Error handling

How failures are signalled (nullptr return, assert, fallback behaviour).
Note any OOM paths.

## Edge cases and invariants

Bullet list of non-obvious rules, known limitations, or things that look
wrong but are intentional.

## See also

Links to related concept pages and legacy reference files.
```

Prefer depth over breadth on each page — a reader should be able to
understand the concept fully without reading the source.

---

## Step 7 — Cross-consistency pass

After all files have been written or updated, do a final consistency sweep:

1. **README concept index** — every `.md` in the directory (except `README.md`
   and `index.md`) must have a matching entry in README's index. Add or remove
   entries as needed.

2. **index.md completeness** — every public header, private header, source
   file, and CMake target that exists on disk must appear in a table row.
   Every row must refer to something that still exists.

3. **Cross-links** — for every Markdown link in any page that points to
   another page in this directory, verify the target file and anchor exist.
   Fix broken links.

4. **Thread-safety coverage** — every concept page whose concept has shared
   mutable state must have a "Threading model" section. Add a stub section to
   any page that is missing one.

5. **Legacy flat file** — if `xash3dpp/docs/architecture/$ARGUMENTS.md`
   exists, update it to contain only:
   ```markdown
   > This document has been superseded by the directory
   > [docs/architecture/$ARGUMENTS/](./$ARGUMENTS/README.md).
   ```

---

## Final checklist

- [ ] All source files read before writing any docs
- [ ] Stale/missing/inconsistent tally produced (update mode)
- [ ] `README.md` reflects current module purpose, goals, and invariants
- [ ] `index.md` lists every header, source file, type, and CMake target
- [ ] Every concept from Step 3 has a page; no orphan pages remain
- [ ] No concept page copies more than a few lines of code verbatim
- [ ] Thread-safety addressed on every concept with shared mutable state
- [ ] All cross-links between pages resolve correctly
- [ ] Legacy flat file redirects to the directory (if it existed)

---

## Step 8 — Commit

Once the Final checklist is green:

```
git add xash3dpp/docs/architecture/$ARGUMENTS/
git commit -m "$ARGUMENTS: write architecture docs"
```

Commit message bullets:

- List the doc files created or updated (README.md, index.md, concept pages).
- If in update mode, note which sections were stale and what was corrected.
