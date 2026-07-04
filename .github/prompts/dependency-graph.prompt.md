---
name: "Dependency graph — EngineContext init order audit"
description: "Read all InitParams structs and EngineContext member declarations to build the subsystem dependency graph, check for cycles, and verify that member init order in EngineContext is consistent with the dependency direction. Read-only."
agent: agent
tools: [read, search, execute, GetSymbolInfo_CppTools, GetSymbolReferences_CppTools]
model: claude-haiku-4-5-20251001
---

# EngineContext Dependency Graph Audit

Map every subsystem's declared dependencies and verify the EngineContext
initialisation order is consistent. **Read-only — no files are modified.**

---

## Step 1 — Collect all InitParams structs

Run the edge scanner (`python` = the repo venv, `.venv\Scripts\python.exe`):

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\dep_scan.py --json
```
*(dep_scan is CLI-only by design — no MCP twin.)*

It returns the `*InitParams` inventory, cross-namespace dependency edges,
and any mutual-reference cycles. (Manual fallback: grep
`struct \w+InitParams` over `xash3dpp/include/**/*.hpp`.)

For each struct, read its definition and record:

| Subsystem | InitParams struct | Fields (subsystem references) |
|-----------|------------------|-------------------------------|
| ...       | ...              | ...                           |

A field is a **dependency** if its type is another subsystem's context class,
interface, or a reference to another subsystem's resource (pool handle, etc.).

---

## Step 2 — Read EngineContext member order

Locate `EngineContext` (search for `class EngineContext` in
`xash3dpp/include/xash3dpp/`). Read the class definition and list every
member in declaration order:

| # | Member name | Type | Subsystem |
|---|-------------|------|-----------|
| 1 | ...         | ...  | ...       |

Declaration order is init order — C++ initialises members in the order they
appear in the class body, not in the initialiser list.

---

## Step 3 — Build the dependency graph

Combine Steps 1 and 2 into a directed graph:

- **Node**: each subsystem
- **Edge A → B**: subsystem A's `InitParams` references B (A depends on B; B
  must be initialised before A)

Print the graph as an adjacency list:

```
memory       → (none)
platform     → (none)
utilities    → (none)
filesystem   → memory, utilities
cmd_cvar     → memory, utilities
...
```

---

## Step 4 — Check for cycles

A dependency cycle means no valid init order exists. Detect cycles using a
depth-first traversal of the graph from Step 3.

If a cycle exists:
- List the cycle path (e.g. `A → B → C → A`)
- Mark it **BLOCKER** — the cycle must be broken before EngineContext can
  initialise correctly

If no cycle: report "Dependency graph is acyclic ✓"

---

## Step 5 — Verify EngineContext member order

For each edge A → B (A depends on B), verify that B appears **before** A in
the EngineContext member list from Step 2.

| Dependency | B init position | A init position | Order |
|------------|----------------|----------------|-------|
| A depends on B | #N | #M | ✓ correct (N < M) / ✗ WRONG (N > M) |

Any row with N > M is a **BLOCKER** — A will be initialised before its
dependency B, which is a use-before-init bug.

---

## Step 6 — Check for undeclared dependencies

Scan each subsystem's `.cpp` files for subsystem references that are NOT
declared in its `InitParams`. A subsystem that accesses another subsystem
without declaring it as a dependency in InitParams is using a hidden implicit
dependency.

```powershell
# Example: find references to other subsystem namespaces in filesystem sources
Select-String -Path "c:\git\xash3d-fwgs\xash3dpp\src\**\*.cpp" -Pattern "xash::(memory|utilities|platform|cmd_cvar)::" -Recurse
```

For each hit, verify the accessed subsystem is declared in the source
subsystem's `InitParams`. If not, flag as **WARNING — undeclared dependency**.

---

## Report

```
EngineContext Dependency Graph
==============================

Subsystems mapped: <N>
Edges (dependencies): <N>

Graph:
<adjacency list from Step 3>

Cycle check: PASS — acyclic | FAIL — <cycle path>

Init order violations:
<table from Step 5, or "None — all dependencies initialised before dependents ✓">

Undeclared dependencies:
<list from Step 6, or "None found ✓">

BLOCKERs: <list or "none">
WARNINGs:  <list or "none">
```
