---
name: "Init — xash3dpp repo onboarding"
description: "Onboard to the xash3dpp rewrite repo. Run this at the start of any rewrite session."
agent: agent
tools: [read, search, xash-tools/*]
model: claude-haiku-4-5-20251001
---

# Repo Onboarding — xash3dpp

## Repository Layout

This repository has two distinct zones. Understand the boundary before doing anything else.

| Zone | Path | Purpose |
|------|------|---------|
| **Legacy** | Everything at the repo root (`engine/`, `filesystem/`, `public/`, `ref/`, `common/`, `pm_shared/`, `android/`, `3rdparty/`, `game_launch/`, `scripts/`, `src/`, `tests/`, `utils/`) | Original Xash3D FWGS C codebase. Authoritative reference for behaviour and ABI contracts. |
| **Rewrite** | `xash3dpp/` | New modular C++ project, structured as a self-contained future repo. All new work goes here. |

Documentation in `Documentation/codex/` is from a previous incremental-upgrade attempt. It is **legacy reference only** — treat its architecture decisions as notes, not requirements.

## The One Rule

> **Do not model the rewrite on the legacy code structure.** The legacy tree exists to read behaviour from, not to copy patterns into `xash3dpp/`.

When reading the legacy code, ask: *what does this do and what invariants must be preserved?* Not: *how is this structured?*

## Rewrite Principles

- External C ABI surfaces must be preserved (game DLL, client DLL, renderer, filesystem plugin, shared SDK headers in `common/`, `engine/*.h`, `pm_shared/`).
- Internal implementation language is C++. Exceptions and RTTI are disabled unless explicitly decided otherwise.
- Each new module must be independently buildable before the next one starts.
- Compatibility quirks are documented and preserved, not cleaned away silently.

## How to Work

1. **Analyse first.** Before writing any `xash3dpp/` code, read the relevant legacy subsystem using the available tools (semantic search, file reads, call hierarchy). Form a clear picture of the behaviour contract.
2. **Write a boundary spec.** What must the new module expose? What must it preserve? Capture this in `xash3dpp/docs/` before implementation.
3. **Implement in `xash3dpp/src/`.** Keep new code self-contained; do not reference legacy paths from build files.
4. **Verify against legacy behaviour.** Use the legacy code as the ground truth for any behavioural question.

Conventions, mandatory patterns, naming rules, and framework primitives are in
`.github/instructions/xash3dpp.instructions.md` — read that for the full coding standards.

## Current State

Do not rely on this prompt for a hand-maintained subsystem inventory. The
current implementation status, active chunk, blocking OQs, stub debt, and
workflow gates are derived by `whereami` and the status-table tool:

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\whereami.py --doctor
& .venv\Scripts\python.exe xash3dpp\tools\status_table.py --check --json
```

Read `xash3dpp/docs/implementation-plan.md` for the authoritative chunk plan.
Chunk numbers are frozen: Chunk 4 is a tombstone, networking is Chunk 2, and
new work follows the next active chunk reported by `whereami`.

**Common build setup**: CMake at `xash3dpp/CMakeLists.txt`; C++23;
no exceptions (`/EHs-c-`); no RTTI (`/GR-`); build tree at `xash3dpp/build/`.

**Environment/framework setup** lives in `.github/AGENT-SETUP.md`. For the
current-state brief (git, chunk status, gates, blocking OQs, last
checkpoint, suggested next action) run
`& .venv\Scripts\python.exe xash3dpp\tools\whereami.py --json`
*(MCP: xash-tools tool `whereami` — same data.)*
