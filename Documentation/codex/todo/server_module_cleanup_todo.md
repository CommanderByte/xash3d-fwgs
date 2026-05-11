# Server Module Cleanup TODO

This TODO tracks the cleanup lane after Phase 160. The goal is to turn the
modern server layer into readable game-engine modules instead of a collection
of tiny migration artifacts.

Rules for this lane:

1. Look at each submodule before collapsing it.
2. Prefer game-engine terminology: Session, Content, Replication, Game API,
   Simulation, Savegame, Runtime, Shared.
3. Keep focused tests when they protect ABI order, protocol bytes, save
   compatibility, or hard-to-debug quirks.
4. Collapse tests only after an aggregate test covers the same behavior.
5. Keep live server state, packet buffers, game DLL ABI, traces, PMove, and
   save streams legacy-owned unless a dedicated fixture-backed phase moves
   them.
6. Cleanup phases should be chunky. This lane is meant to reduce ceremony, not
   generate a new phase for every moved file.
7. Each phase should start with a move/collapse map, a focused test matrix, and
   a reference scan for include paths, Waf targets, scripts, and task evidence.

## Phase 161: Server Module Cleanup Roadmap

Goal: define the cleanup vocabulary and per-submodule collapse criteria.

- [x] Audit `client`, `resources`, `messaging`, `game_dll`, and the flat
  server layer.
- [x] Decide the game-engine terminology to use in docs and future modules.
- [x] Define test-collapse rules.
- [x] Define aggressive scan and batching techniques.
- [x] Add the next cleanup phases to the main task list.

Evidence: `Documentation/codex/modern/engine/server-module-cleanup-plan.md`.

## Phase 162: Flat Server Module Rehome

Goal: move the remaining flat modern server helpers into real modules in one
coherent pass.

- [x] Create the needed `shared/`, `runtime/`, `save/`, and `world/` source and
  include directories.
- [x] Move obvious shared constraints, runtime shell helpers, savegame helpers,
  and simulation/world helpers into their modules.
- [x] Decide whether `source_query` and `netapi_info` move into `client/` or a
  query submodule, and do the move if the owner is clear.
- [x] Update includes, READMEs, and Waf references where needed.
- [x] Run focused affected tests, full validation, and smoke timing if runtime
  wiring changes.

Evidence: `Documentation/codex/modern/engine/flat-server-module-rehome.md`.

## Phase 163: Submodule Collapse Pass

Goal: inspect each existing module and collapse only the parts that are clearer
together than apart.

- [x] Review Session/client helpers and decide whether admission/query pieces
  should group further.
- [x] Review Content/resource helpers and collapse duplicated consistency or
  transfer setup only where aggregate tests protect it.
- [x] Review Replication/messaging helpers and decide whether tiny payload
  writers or snapshot-adjacent helpers should group.
- [x] Review Game API/game DLL helpers and decide whether message
  bridge/session/registry or entity lifecycle/parse pieces should share
  implementation or setup.
- [x] Run focused module tests and full validation.

Phase 163 decisions are recorded in
`Documentation/codex/modern/engine/submodule-collapse-pass.md`.

## Phase 164: Test Suite Consolidation Pass

Goal: reduce test target noise without making failures harder to diagnose.

- [ ] Pick the modules with the strongest aggregate coverage.
- [ ] Move duplicated setup into module-specific test support headers.
- [ ] Merge selected tiny tests into aggregate/module tests and update
  `src/wscript`.
- [ ] Keep focused tests for ABI order, protocol byte layouts, save formats,
  and tricky compatibility quirks.
- [ ] Run focused module tests and full validation.

## Phase 165: Adapter And Common Glue Cleanup

Goal: remove repeated adapter and common-structure boilerplate where it is
mechanical, while keeping risky side effects visible.

- [ ] Review adapter shared helpers for Session, Content, Replication, and
  Game API.
- [ ] Move common plain structs/enums into module headers only when multiple
  helpers genuinely share them.
- [ ] Avoid a giant `server_adapter.cpp` or broad `Server` facade.
- [ ] Keep live legacy ownership obvious at the call site.
- [ ] Run focused adapter/module tests, full validation, and smoke timing if
  runtime route-through code changes.

## Phase 166: Module Cleanup Checkpoint

Goal: pause after the ambitious cleanup lane and decide whether it actually
made the tree simpler.

- [ ] Recount modern server files, headers, adapters, and tests.
- [ ] Compare before/after readability, validation cost, and target noise.
- [ ] Record whether the next lane should continue server cleanup or switch to
  client/render fixtures.
- [ ] Run full validation and runtime smoke timing.
- [ ] Move completed cleanup TODOs to `done/` where sensible.
