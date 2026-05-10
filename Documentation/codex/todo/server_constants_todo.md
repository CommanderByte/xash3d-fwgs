# Server Constants And Constraints TODO

This TODO starts the post-Phase-100 server constants lane. The intent is to
make server-only limits, flags, and magic values visible as modern contracts
before route-through work touches live server ownership.

## Scope Start

- Mirror legacy server-only constants into typed modern C++ contracts.
- Add tests that compare values against legacy adapters when practical.
- Document each constant as ABI/layout, protocol/wire, save-format, gameplay,
  or private implementation detail.
- Prefer pure helpers before route-through.

## Scope End

- Do not change `server.h` struct layout.
- Do not replace protocol constants without wire-format tests.
- Do not move edict/client/server storage ownership in this lane.
- Do not change savegame versions, game DLL ABI tables, or packet formats.

## Phase 101: Server Constants And Constraints Inventory

- [x] Inventory server-only constants and classify by compatibility role.
- [x] Add a modern `server_limits` contract for safe mirror constants.
- [x] Add tests that compare modern constants to legacy values through a tiny
  adapter where needed.
- [x] Document which legacy macros are layout-sensitive and should not be
  replaced directly.
- [x] Run focused tests and full validation after the constants contract.
  Evidence: Phase 101 validation passed with `test_engine_server_limits`,
  `xash`, 109/109 tests, and a 0.533s first-frame smoke.

## Phase 102: Server Challenge Window Policy

- [x] Baseline challenge-window behavior in `sv_client.c`.
  Evidence: `Documentation/codex/modern/engine/server-challenge-window-policy.md`.
- [x] Extract time-window calculation as a pure helper.
  Evidence: `src/include/engine/server/server_challenge_policy.hpp`,
  `src/engine/server/server_challenge_policy.cpp`.
- [x] Add tests for boundary seconds and repeat-window behavior.
  Evidence: `tests/engine/server_challenge_policy.cpp`.
- [x] Keep challenge salt storage, hashing, packets, and rejection output
  legacy-owned.
  Evidence: only `SV_ChallengePolicy_TimeWindow()` and
  `SV_ChallengePolicy_PreviousTimeWindow()` route through modern code;
  `SV_GetChallenge()`, `Netchan_OutOfBandPrint()`, and
  `SV_RejectConnection()` remain in `engine/server/sv_client.c`.
- [x] Run focused tests and full validation after the route-through.
  Evidence: Phase 102 validation passed with
  `test_engine_server_challenge_policy`, `xash`, 110/110 tests, and a 0.498s
  first-frame smoke.

## Phase 103: Server Lifecycle Limits Policy

- [ ] Baseline maxclient bounds, singleplayer/multiplayer update-backup
  selection, `SV_SPAWN_TIME`, and client entity count calculation.
- [ ] Add pure helpers and tests for those lifecycle limits.
- [ ] Keep spawn, activate/deactivate, baselines, and entity allocation
  legacy-owned.

## Phase 104: Server Movement Constraint Constants

- [ ] Baseline server movement type constants, clip-plane limit, and movement
  epsilon.
- [ ] Add modern constants/tests that make server physics constraints distinct
  from GL and pm_shared constants with similar names.
- [ ] Keep `SV_MoveStep`, `SV_FlyMove`, world collision, and player movement
  loops legacy-owned.

## Phase 105: Visibility Leaf And View Constraint Policy

- [ ] Baseline `MAX_ENT_LEAFS(ext)`, `MAX_VIEWENTS`, and related visibility
  capacity behavior.
- [ ] Add a shared modern capacity helper and tests.
- [ ] Keep edict leaf arrays, PVS/PAS, packet entity selection, and world
  linking legacy-owned.

## Phase 106: Runtime Route-Through Review

- [ ] Review constants mirrored in Phases 101-105.
- [ ] Choose the safest small route-through call sites.
- [ ] Explicitly mark layout-sensitive macros as legacy-owned.
- [ ] Run focused tests, full tests, and runtime smoke before any route-through
  commit.
