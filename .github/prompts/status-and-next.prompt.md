---
name: "Implementation status and next steps"
description: "Scan the xash3dpp/ tree, determine what is implemented vs. stub-only, and print a prioritised 'what to do next' plan to chat based on the legacy engine dependency structure. No files are written."
agent: agent
tools: [read, search, execute]
model: claude-haiku-4-5-20251001
---

# xash3dpp — Implementation Status and Next Steps

Analyse the current state of the rewrite and recommend what to tackle next.
**Write everything to chat. Do not create or edit any files.**

---

## Step 1 — Determine what is implemented

Generate the status table (`python` = the repo venv,
`.venv\Scripts\python.exe`):

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\status_table.py --markdown
& .venv\Scripts\python.exe xash3dpp\tools\status_table.py --check --json
```

The table classifies each `xash3dpp/src/` directory as **Complete** (has
`.cpp` files and tests), **Partial** (`.cpp` but no tests), or **Skeleton**
(CMakeLists only). `--check` diffs against the status table maintained in
`xash3dpp/docs/implementation-plan.md` — report any drift it finds. For
Partial subsystems, `stub_scan.py <subsystem> --json` gives the TODO/test
detail. The "Complete" label is structural — cross-check the
implementation-plan chunk status before treating a subsystem as done.

Manual fallback: count `.cpp` files per `src/` directory and check
`include/xash3dpp/<sub>/` + `tests/<sub>/` presence by hand.

---

## Step 2 — Read the legacy dependency map

Read `xash3dpp/docs/legacy-survey/overview.md` for the subsystem dependency
graph and the list of cross-cutting pain points.

Also read any boundary specs that exist in `xash3dpp/docs/boundaries/` and any
legacy survey docs in `xash3dpp/docs/legacy-survey/` for subsystems that are
still in Skeleton or Partial state.

Build a mental model of which subsystems block which:

- What does each unimplemented subsystem need before it can start?
- Which subsystems are leaves (no inbound dependencies from other
  unimplemented subsystems) and can therefore be started independently?
- Which subsystems are the heaviest "hub" nodes that many others depend on?

---

## Step 3 — Score each unimplemented subsystem

For each Skeleton or Partial subsystem, estimate:

| Factor | Description |
|--------|-------------|
| **Isolation** | Can it be built and tested without other unimplemented modules? (High = yes) |
| **Foundational** | Do many other modules depend on it? (High = yes) |
| **Complexity** | How many legacy source files make up this subsystem? How much shared global state does it own? (High = gnarly) |
| **ABI risk** | Does it touch a FROZEN ABI surface (game DLL, client DLL, pm_shared)? (High = risky) |

Consult the relevant `engine/` and legacy-survey docs for each subsystem's
file count and god-object footprint.

---

## Step 4 — Produce the "not too evil" work plan

Print a recommended implementation order to chat.  Group related work into
chunks sized so that each chunk:

- Can be completed in a single focused session (roughly: one boundary spec +
  one CMakeLists + one `.hpp` + one `.cpp` + one test harness = one chunk)
- Builds and tests cleanly before the next chunk starts
- Does not depend on any chunk that has not yet been completed

For each chunk, state:

```
## Chunk N — <name>

**Subsystems**: list of src/ directories  
**Depends on**: completed subsystems this chunk requires  
**Legacy reference**: key files in engine/ to read first  
**Complexity note**: one sentence on the hardest part  
**ABI surfaces touched**: e.g. "none", "cmd/cvar plugin table", "game DLL eiface.h"  
**Deliverable**: what "done" looks like (test target green, specific API surface implemented)
```

Use the following ordering heuristics:

1. **Foundation services first** — cmd/cvar, networking primitives.  Without
   these, server and client cannot be meaningfully started.
2. **Server before client** — the server is more isolated; it only needs the
   game DLL ABI.  The client adds sound, rendering, input, and UI — all of
   which are larger and more platform-dependent.
3. **Plugin / renderer last** — the renderer ABI is fully internal and can be
   redesigned freely.  It is also the most platform-specific and the least
   important for a dedicated-server milestone.
4. **Isolatable helpers can be slotted in anywhere** — if a chunk is a
   self-contained helper with no dependents (e.g. save/restore, HPAK) it can
   be picked up between bigger pieces.

---

## Step 5 — Print warnings and open questions

List any known landmines or design decisions that will need an explicit choice
before the corresponding chunk can start:

- **pm_shared determinism** — float vs. fixed-point decision needed before
  physics chunk.
- **Renderer ABI shape** — the legacy `ref_api.h` is internal; the rewrite
  needs to decide on Vulkan/Metal/D3D12/GL before the renderer chunk.
- **Networking protocol compatibility** — does the rewrite aim to be
  wire-compatible with GoldSrc clients during development?
- Any others found in the legacy-survey pain-points list.

---

## Output format

Structure the chat output as:

1. **Status table** — one row per subsystem
2. **Dependency graph summary** — short prose, no more than a paragraph
3. **Recommended work plan** — numbered chunks as described in Step 4
4. **Open questions / landmines** — bulleted list

Keep each chunk description short. The goal is a plan someone can use to pick
up the next piece of work, not a full design document.
