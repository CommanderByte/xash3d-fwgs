---
name: "Init — xash3dpp repo onboarding"
description: "Onboard to the xash3dpp rewrite repo. Run this at the start of any rewrite session."
agent: agent
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

## Current State

The `xash3dpp/` utilities module is complete and tested:

- **Library**: `xash3dpp_utilities` static library — `xash3dpp/src/utilities/` + headers in `xash3dpp/include/xash3dpp/utilities/`
- **Modules**: `atlas`, `build`, `dynlib`, `hash`, `math`, `matrix`, `path`, `string`, `swap`, `utf`
- **Tests**: `xash3dpp/tests/utilities/` — one `test_<module>.cpp` per module, CTest target `test_utilities`
- **Docs**: boundary notes in `xash3dpp/docs/boundaries/`; legacy survey in `xash3dpp/docs/legacy-survey/`
- **Build**: CMake at `xash3dpp/CMakeLists.txt`; C++20; no exceptions; no RTTI; build tree at `xash3dpp/build/`

No other subsystem has been started yet. Use `analyse-subsystem` to scope and begin the next module.
