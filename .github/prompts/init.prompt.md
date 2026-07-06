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

Six subsystems are complete and tested.

### `xash3dpp_utilities` — complete
- **Library**: `xash3dpp/src/utilities/`; headers in `xash3dpp/include/xash3dpp/utilities/`
- **Modules**: `atlas`, `build`, `dynlib`, `hash`, `math`, `matrix`, `path`, `string`, `swap`, `utf`
- **Tests**: `xash3dpp/tests/utilities/` — one `test_<module>.cpp` per module; CTest target `test_utilities`

### `xash3dpp_memory` — complete
- **Library**: `xash3dpp/src/memory/`; public headers in `xash3dpp/include/xash3dpp/memory/`;
  private headers in `xash3dpp/include/xash3dpp/private/memory/`
- **Design**: pool accounting facade over `malloc`/`free`; 128-slot registry;
  `SlotState` atomic CAS for concurrent `create_pool`; `std::atomic<OomHandler>`
  for the OOM callback
- **Tests**: `xash3dpp/tests/memory/`

### `xash3dpp_filesystem` — complete
- **Library**: `xash3dpp/src/filesystem/`; headers in `xash3dpp/include/xash3dpp/filesystem/`
- **Design**: `Filesystem` pimpl class; `VFileSystem009Adapter` is the legacy
  ABI shim (wraps `Filesystem` directly); PAK/WAD/ZIP archive backends;
  `ISearchBackend` vtable for backend dispatch; case-insensitive directory
  cache; `std::shared_mutex` guards the search-path deque
- **Tests**: `xash3dpp/tests/filesystem/`

### `xash3dpp_platform` — complete (sockets layer pending)
- **Library**: `xash3dpp/src/platform/`; public headers in
  `xash3dpp/include/xash3dpp/platform/`
- **Design**: Single Porting Layer — all OS-specific code lives here.
  Win32 and POSIX backends for `sys` (time, sleep, env), `console` (stdin
  reader), and `crash` (signal/exception handler); Android JNI bootstrap via
  `std::call_once`
- **Pending**: `IPlatformSockets` / `OsSocket` RAII socket layer is **not yet
  implemented**. Requirements are in
  `xash3dpp/docs/architecture/platform/sockets.md`. This is a hard prerequisite
  for Chunk 4 (networking). Do not call BSD/Winsock socket APIs directly from
  any subsystem outside `src/platform/*/os_socket.cpp`.
- **Tests**: `xash3dpp/tests/platform/`

### `xash3dpp_core` — complete
- **Library**: `xash3dpp/src/core/`; public headers in
  `xash3dpp/include/xash3dpp/core/`; private headers in
  `xash3dpp/include/xash3dpp/private/core/`
- **Design**: Cross-cutting singletons shared by all subsystems — structured
  logging (`core::log`, `core::logf`, `LogLevel` enum), assertion macros
  (`XASH_ASSERT`, `XASH_FATAL`), and thread-role registration
  (`core::register_thread_role`, `core::assert_thread_role`); log sink writes
  via `platform::console::write()`
- **Tests**: `xash3dpp/tests/core/`

### `xash3dpp_cmd_cvar` — complete
- **Library**: `xash3dpp/src/cmd_cvar/`; public headers in
  `xash3dpp/include/xash3dpp/cmd_cvar/`; private headers in
  `xash3dpp/include/xash3dpp/private/cmd_cvar/`
- **Design**: Single `CmdCvarContext` pimpl class; pimpl move ctor/dtor defined
  in `context.cpp` (not `= default` in header); `XASH_GOLDSRC_COMPAT` CMake
  option selects `compat_goldsrc.cpp` vs `compat_null.cpp` at link time —
  zero `#ifdef` in core; `CircularBuffer<T,N>` private template for change log
- **Tests**: `xash3dpp/tests/cmd_cvar/`

**Common build setup**: CMake at `xash3dpp/CMakeLists.txt`; C++23;
no exceptions (`/EHs-c-`); no RTTI (`/GR-`); build tree at `xash3dpp/build/`.

**Environment/framework setup** lives in `.github/AGENT-SETUP.md`. For the
current-state brief (git, chunk status, gates, blocking OQs, last
checkpoint, suggested next action) run
`& .venv\Scripts\python.exe xash3dpp\tools\whereami.py --json`
*(MCP: xash-tools tool `whereami` — same data.)*
