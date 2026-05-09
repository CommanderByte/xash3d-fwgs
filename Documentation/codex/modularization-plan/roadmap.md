# Modular C++ Migration Roadmap

## Goal

Move implementation internals toward smaller, testable C++ modules without
breaking the existing game DLL, client DLL, renderer, filesystem, tool, and
platform contracts.

This is not a rewrite plan for a new engine. It is a compatibility-preserving
modernization plan for this fork.

## Non-Goals

- Do not redesign public SDK headers as C++ headers.
- Do not expose STL types, exceptions, or C++ ownership rules through C ABI
  boundaries.
- Do not change protocol structs, edict layout, save/restore formats, or
  renderer API tables as early modernization work.
- Do not start with a cross-tree style conversion.

## Phase 0: Baseline And Tooling

Purpose: make current behavior easy to rebuild and verify before changing
internals.

Work items:

- Keep the Windows SDL2 plus `hlsdk-portable` smoke-test recipe current.
- Add equivalent Linux smoke-test notes once a local run is available.
- Record Waf configure flags used for each platform.
- Generate and preserve a local `compile_commands.json` workflow.
- Identify the fastest build and smoke-test commands for day-to-day refactors.

Acceptance criteria:

- Dedicated server build succeeds.
- Windows client build succeeds.
- Runtime reaches menu or first frame with known-good assets.
- Known warnings are listed and triaged separately from refactor behavior.

## Phase 1: Compatibility Boundary Map

Purpose: mark the borders that C++ internals must not accidentally change.

Work items:

- Document game DLL exports and engine callback tables.
- Document client DLL exports and engine callback tables.
- Document renderer DLL entry points and ref API tables.
- Document filesystem module exports and `VFileSystem009` expectations.
- Label headers as public ABI, shared internal, or private implementation.

Acceptance criteria:

- Each boundary has an owner document under this folder.
- Each boundary lists structs, callbacks, and functions that must remain
  C-compatible.
- Each boundary lists risky behavior quirks discovered during code reading.

## Phase 2: Test Harness And Manual Recipes

Purpose: add enough verification to make small internal moves less spooky.

Work items:

- Add focused filesystem tests for search path ordering, case handling, archive
  precedence, and path normalization.
- Add public utility tests around path, string, byte order, and parsing helpers.
- Add a minimal runtime smoke-test checklist for menu and map startup.
- Add optional screenshot or log assertions later, once launch behavior is
  scriptable.

Acceptance criteria:

- Filesystem changes can be validated without manually launching the game.
- Public utility changes have focused regression coverage.
- Runtime smoke tests are written down and reproducible.

## Phase 3: Filesystem Pilot

Purpose: prove the internal C++ migration pattern in a subsystem with existing
backend boundaries.

Work items:

- Keep exported `FS_*` behavior and filesystem tables stable.
- Wrap one backend implementation in a C++ internal type.
- Preserve `searchpath_t` callback behavior while reducing direct global
  coupling.
- Convert cleanup paths to RAII inside implementation files only.
- Measure whether compile dependencies improve or degrade.

Acceptance criteria:

- Existing and new filesystem tests pass.
- Search path printout and lookup ordering match the baseline.
- Engine still reaches first frame with the Windows smoke-test runtime.
- No public header requires C++ to include.

## Phase 4: Utility And Platform Ownership

Purpose: reduce repeated manual lifetime handling before touching larger engine
state.

Work items:

- Introduce internal wrappers for dynamic libraries, files, and temporary
  buffers where public ABI is not affected.
- Group platform services behind small internal interfaces.
- Keep existing platform entry points as wrappers.
- Avoid broad dependency injection until tests need it.

Acceptance criteria:

- Resource cleanup becomes local and deterministic in converted files.
- Platform-specific behavior remains isolated behind existing build flags.
- No platform loses compiler support due to C++ feature choices.

## Phase 5: Renderer Internals

Purpose: carve renderer state into clearer ownership units after smoke testing
is strong enough.

Work items:

- Map global renderer state in `gl_local.h`.
- Identify texture registry, GL state cache, and frame/view state boundaries.
- Introduce internal types such as `GlState`, `TextureManager`, and
  `RenderFrame` behind the existing renderer API.
- Add screenshot or log-based smoke checks before moving behavior.

Acceptance criteria:

- Renderer DLL exports remain stable.
- Visual smoke tests match baseline closely enough for manual review.
- GL state cleanup and texture ownership are easier to reason about.

## Phase 6: Host, Client, And Server Facades

Purpose: prepare the largest stateful systems for gradual internal ownership
without moving compatibility-critical storage too early.

Work items:

- Add thin `HostContext`, `ClientContext`, and `ServerContext` facades around
  existing globals.
- Move initialization and shutdown ownership first.
- Convert local helper functions before exported or cross-module functions.
- Keep `Host_*`, `CL_*`, and `SV_*` entry points as wrappers.

Acceptance criteria:

- Frame startup and shutdown behavior stays identical.
- Game and client DLL callbacks see the same data and ordering.
- Protocol, entity, save/restore, and client frame structures are unchanged.

## Decision Log

| Date | Decision | Notes |
| --- | --- | --- |
| 2026-05-09 | Use incremental internal C++ modernization, not a big-bang rewrite. | Preserve C ABI surfaces and prove the pattern with smaller pilots first. |
| 2026-05-09 | Use filesystem as first serious pilot candidate. | Existing backend shape makes it lower risk than host/client/server state. |

## Backlog Seeds

- Add `boundary-game-dll.md`.
- Add `boundary-client-dll.md`.
- Add `boundary-renderer.md`.
- Add `boundary-filesystem.md`.
- Add `cpp-build-policy.md`.
- Add `runtime-smoke-tests.md`.
