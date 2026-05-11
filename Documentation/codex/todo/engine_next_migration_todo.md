# Engine Next Migration TODO

This TODO tracks the post-Phase-106 audit lane. The goal is to add small
enablers that make future server and engine migrations less adapter-heavy.

## Phase 107: Server Group Filter Policy

- [x] Inventory all `GROUP_OP_*`, `svs.groupop`, `svs.groupmask`, and
  `groupinfo` checks in server code.
  Evidence: `Documentation/codex/modern/engine/post-106-migration-audit.md`,
  `Documentation/codex/modern/engine/server-group-filter-policy.md`.
- [x] Add target-neutral group-filter predicates and tests for AND, NAND,
  zero-mask, and unknown-operation behavior.
  Evidence: `src/include/engine/server/server_group_filter.hpp`,
  `src/engine/server/server_group_filter.cpp`,
  `tests/engine/server_group_filter.cpp`.
- [x] Route only the smallest safe call sites through a C adapter.
  Evidence: `engine/server/server_group_filter_adapter.h`,
  `engine/server/server_group_filter_adapter.cpp`, `engine/server/sv_game.c`,
  `engine/server/sv_world.c`, `engine/server/sv_phys.c`,
  `engine/server/sv_pmove.c`.
- [x] Keep edict iteration, trace ownership, save context, and multicast/event
  writes legacy-owned.
  Evidence: `Documentation/codex/modern/engine/server-group-filter-policy.md`.
- [x] Run focused tests, full tests, and `+wait +wait` smoke timing.
  Evidence: `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_server_group_filter -StopRunningXash` passed; focused test
  passed, `.\waf.bat build --targets=xash` passed, full tests passed 114/114;
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 1.905 seconds and stopped with reason `command`.

## Phase 108: Map Validation And Landmark Result Policy

- [x] Baseline `SV_MapIsValid()` flag interpretation in changelevel, command,
  and save/load paths.
  Evidence: `Documentation/codex/modern/engine/server-map-validation-policy.md`.
- [x] Add target-neutral result classification for `MAP_IS_EXIST`,
  `MAP_HAS_LANDMARK`, and `MAP_INVALID_VERSION`.
  Evidence: `src/include/engine/server/server_map_validation.hpp`,
  `src/engine/server/server_map_validation.cpp`,
  `tests/engine/server_map_validation.cpp`.
- [x] Route safe flag interpretation through a C adapter while leaving map
  probing, BSP/header checks, and entity parsing legacy-owned.
  Evidence: `engine/server/server_map_validation_adapter.h`,
  `engine/server/server_map_validation_adapter.cpp`, `engine/server/sv_game.c`,
  `engine/server/sv_save.c`, `src/engine/server/server_command_lifecycle.cpp`.
- [x] Add tests for invalid version, missing map, existing map, smooth
  transition without landmark, and classic transition behavior.
  Evidence: `tests/engine/server_map_validation.cpp`.
- [x] Run focused tests, full tests, and `+wait +wait` smoke timing.
  Evidence: `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_server_map_validation -StopRunningXash` passed; focused test
  passed, `.\waf.bat build --targets=xash` passed, full tests passed 115/115;
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.506 seconds and stopped with reason `command`.

## Phase 109: Client Flag Predicate Policy

- [x] Group `FCL_*` uses by owner: fake client, HLTV, prediction/local weapons,
  frame resend/send/skip, resources, and consistency.
  Evidence: `Documentation/codex/modern/engine/server-client-flag-policy.md`.
- [x] Add typed client flag snapshot/predicate helpers for one owner at a time.
  Evidence: `src/include/engine/server/client_policy.hpp`,
  `src/engine/server/client_policy.cpp`, `tests/engine/client_policy.cpp`.
- [x] Start with fake-client and HLTV predicates because they appear in many
  server and game DLL paths.
  Evidence: `Documentation/codex/modern/engine/server-client-flag-policy.md`.
- [x] Avoid broad macro replacement in `server.h`.
  Evidence: `engine/server/server.h` remains unchanged; Phase 109 routes only
  `sv_query.c` through `client_policy_adapter`.
- [x] Run focused tests, full tests, and `+wait +wait` smoke timing.
  Evidence: `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_client_policy -StopRunningXash` passed; focused test passed,
  `.\waf.bat build --targets=xash` passed, full tests passed 115/115;
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.501 seconds and stopped with reason `command`.

## Phase 110: Server Event Playback Boundary Audit

- [x] Audit `SV_PlaybackEventFull()` ownership: event flags, invoker handling,
  group filtering, PVS/PHS masks, recipient iteration, client flags, reliability,
  and payload fields.
  Evidence: `Documentation/codex/modern/engine/server-event-playback-boundary.md`.
- [x] Decide which inputs can be snapshots and which must stay live legacy state.
  Evidence: `Documentation/codex/modern/engine/server-event-playback-boundary.md`.
- [x] Define the first event playback helper boundary after Phases 107 and 109.
  Evidence: `Documentation/codex/modern/engine/server-event-playback-boundary.md`.
- [x] No runtime route-through is required in this audit phase.
  Evidence: documentation-only phase; `git diff --check` passed.

## Phase 111: Server Event Playback Policy Helper

- [x] Add target-neutral event admission and recipient decision helpers.
  Evidence: `src/include/engine/server/server_event_playback_policy.hpp`,
  `src/engine/server/server_event_playback_policy.cpp`.
- [x] Add tests for `FEV_NOTHOST`, host/local-weapons suppression, fake clients,
  no direct HLTV/spectator filtering, reliability, and group filtering.
  Evidence: `tests/engine/server_event_playback_policy.cpp`.
- [x] Route decisions while keeping PVS/PHS mask generation, message writes, and
  recipient iteration legacy-owned.
  Evidence: `engine/server/sv_game.c`, `engine/server/sv_frame.c`,
  `Documentation/codex/modern/engine/server-event-playback-policy.md`.
- [x] Run focused tests, full tests, and `+wait +wait` smoke timing.
  Evidence: `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_server_event_playback_policy -StopRunningXash` passed; focused
  target passed, `xash` built, full tests passed 116/116, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.511 seconds and stopped with reason `command`.

## Phase 112: Read-Only Cvar Snapshot Pilot

- [x] Inventory server helpers that only need read-only cvar values.
  Evidence: `Documentation/codex/modern/engine/read-only-cvar-snapshot.md`.
- [x] Add a small target-neutral value snapshot type for numeric, boolean, and
  string cvar reads without owning the cvar registry.
  Evidence: `src/include/engine/cvar_snapshot.hpp`,
  `src/engine/cvar_snapshot.cpp`, `tests/engine/cvar_snapshot.cpp`.
- [x] Route one low-risk cvar-heavy helper through adapter-provided snapshots.
  Evidence: `engine/server/sv_main.c` routes `SV_ProcessUserAgent()` input
  device cvar reads through `engine/server/server_cvar_snapshot_adapter.*`.
- [x] Keep registration, mutation, callbacks, command bindings, and archived
  persistence legacy-owned.
  Evidence: `Documentation/codex/modern/engine/read-only-cvar-snapshot.md`.
- [x] Run focused tests, full tests, and `+wait +wait` smoke timing.
  Evidence: `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_cvar_snapshot -StopRunningXash` passed; focused target passed,
  `xash` built, full tests passed 117/117, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.516 seconds and stopped with reason `command`.

## Phase 113: Model And Visibility Service Boundary Audit

- [x] Audit `engine/common/mod_bmodel.c`, `sv_world.c`, `sv_phys.c`, and game
  DLL trace/visibility callbacks for model, hull, PVS/PAS, and leaf ownership.
  Evidence: `Documentation/codex/modern/engine/model-visibility-service-boundary.md`.
- [x] Identify read-only snapshot seams that could support a future
  model/visibility service without moving BSP storage.
  Evidence: `Documentation/codex/modern/engine/model-visibility-service-boundary.md`.
- [x] Decide whether fixture tests can cover PVS/PAS and hull behavior before
  any runtime route-through.
  Evidence: `Documentation/codex/modern/engine/model-visibility-service-boundary.md`;
  documentation-only audit phase, `git diff --check` passed.

## Phase 114: C++ Ownership Consolidation Checkpoint

- [x] Review modern server helpers since the server constants lane and classify
  them as temporary facade, behavior owner, or reusable domain concept.
  Evidence: `Documentation/codex/modern/engine/server-cpp-ownership-consolidation.md`.
- [x] Identify where old `sv_*.c` grouping still helps and where it mixes
  separate concepts.
  Evidence: `Documentation/codex/modern/engine/server-cpp-ownership-consolidation.md`.
- [x] Identify adapter-reduction candidates without creating a single
  catch-all adapter layer.
  Evidence: grouped game DLL bridge, server resource, server messaging,
  client/session, and world adapter candidates are listed in
  `Documentation/codex/modern/engine/server-cpp-ownership-consolidation.md`.
- [x] Keep existing compatibility tests intact and defer aggregate concept-level
  tests until grouped files actually move.
  Evidence: documentation-only audit phase, `git diff --check` passed.

## Later Candidates

- Resource transfer consolidation once catalog, reslist, consistency,
  download, upload, customization, and hot-resource helpers are ready to group.
- Server messaging consolidation once text, service, sound, static, voice,
  multicast, event, and frame-datagram helpers are ready to group.
- Game DLL bridge consolidation once callback, message, resource, entity,
  string-pool, visibility, and load/lifecycle helpers are ready to group.
- Client/session consolidation once admission, userinfo, command, transfer,
  voice, and remote-admin helpers are ready to group.
- Runtime save/restore owner audit after map and entity policies mature.
- Rendered console sink/router work after client/render ownership is selected.
- Memory pool modernization in Phase 990.
