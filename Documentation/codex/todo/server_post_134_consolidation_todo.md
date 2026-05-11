# Server Post-134 Consolidation TODO

This TODO picks up after the Phase 115-134 server ownership sweep. The aim is
to convert the tested helper layer into clearer domains before continuing deep
runtime migration.

The guiding rule is still compatibility first:

1. keep legacy `SV_*`, game DLL ABI, protocol, save, and demo surfaces stable;
2. use tests and value objects to preserve known quirks;
3. consolidate only when a group has real domain ownership;
4. avoid broad file moves until build and include fallout is understood.

## Phase 135: Milestone 134 Server Progress Audit

Goal: summarize how far the engine server migration has progressed and where
the next phases should point.

- [x] Count the modern server helper, header, adapter, legacy, and test
  surfaces.
- [x] Identify domains that are well covered by tests.
- [x] Identify live runtime ownership that remains legacy-bound.
- [x] Recommend the next consolidation lane.

Evidence:
`Documentation/codex/modern/engine/milestone-134-server-progress-audit.md`.

## Phase 136: Server Domain Layout Plan

Goal: make the intended `src/engine/server` submodule layout explicit before
moving files.

- [x] Define the target directories for resources, messaging, game DLL bridge,
  client/session, runtime, world/physics, save/restore, and shared contracts.
- [x] Map current helper files to those domains and mark temporary facades.
- [x] Decide which include paths and Waf target lists would change during a
  physical move.
- [x] Document the no-move/no-rename boundaries for public ABI and legacy C
  adapters.

Evidence:
`Documentation/codex/modern/engine/server-domain-layout-plan.md`.

## Phase 137: Resource Transfer Domain Consolidation Pilot

Goal: use the best-covered resource helpers as the first consolidation pilot.

- [x] Review `resource_identity`, `resource_transfer_manifest`,
  `server_resource_catalog`, download/upload, consistency, customization,
  hot-resource, and reslist helpers as one domain.
- [x] Add or extend aggregate tests that exercise catalog-to-transfer flows.
- [x] Move or group only the modern resource-domain files if the Phase 136
  layout makes the change low-risk.
- [x] Keep HPAK, filesystem probes, resource linked lists, netchan fragments,
  and game DLL callbacks legacy-owned.

Evidence:
`Documentation/codex/modern/engine/resource-transfer-domain-pilot.md`,
`src/engine/server/resources/`, `src/include/engine/server/resources/`, and
`tests/engine/resource_transfer_manifest.cpp`.

## Phase 138: Server Messaging Domain Consolidation Pilot

Goal: consolidate message payload and envelope helpers without hiding live
packet-buffer ownership.

- [x] Review text, service, sound, static, voice, multicast, envelope, spawn
  handshake, event playback, and frame datagram helpers.
- [x] Add aggregate tests around representative complete messages plus
  recipient facts.
- [x] Expand shared adapter/result translation only where duplication is
  obvious.
- [x] Keep `sizebuf_t`, `MSG_*`, signon/datagram mutation, netchan sends, and
  rendered console ownership legacy-bound.

Evidence:
`Documentation/codex/modern/engine/server-messaging-domain-pilot.md`,
`src/engine/server/messaging/`, `src/include/engine/server/messaging/`, and
`tests/engine/server_message_envelope.cpp`.

## Phase 139: Game DLL Bridge Domain Consolidation Pilot

Goal: turn the broad game DLL bridge helper set into a clearer internal module
without changing callback table publication.

- [x] Map bridge helpers into lifecycle, ABI metadata, entities, messages,
  resources, movement, visibility/world-query, output, string-pool, and
  changelevel areas.
- [x] Add a domain-level test that proves cross-helper behavior without a live
  DLL.
- [x] Consider grouped implementation files only where they reduce adapter
  confusion.
- [x] Keep DLL lifetime, `enginefuncs_t` order, `svgame`, edict storage, and
  callback calls legacy-owned.

Evidence:
`Documentation/codex/modern/engine/game-dll-bridge-domain-pilot.md`,
`src/engine/server/game_dll/`, `src/include/engine/server/game_dll/`, flat
forwarding headers under `src/include/engine/server/`, and
`tests/engine/game_dll_bridge_domain.cpp`.

Validation:
Focused game DLL bridge targets passed 15/15; `.\waf.bat build --alltests`
passed 130/130; runtime smoke reached first frame in 0.508 seconds and stopped
with reason `command` on May 11 2026 at 14:14:40 local time.

## Phase 140: Client Session Domain Consolidation Pilot

Goal: clarify `sv_client.c` modernization boundaries before extracting more
client behavior.

- [x] Group existing client helpers around admission, session slots, userinfo,
  commands, transfer, voice, cvar query, and remote admin.
- [x] Identify which helpers should move under a future client/session module
  and which belong to resource or messaging domains instead.
- [x] Add aggregate tests for admission/session/userinfo facts if they reduce
  repeated adapter code.
- [x] Keep connect/drop/spawn mutation, netchan, command execution, resource
  list mutation, voice packet reads, and cvar-query callbacks legacy-owned.

Evidence:
`Documentation/codex/modern/engine/client-session-domain-pilot.md`,
`src/engine/server/client/`, `src/include/engine/server/client/`, flat
forwarding headers under `src/include/engine/server/`, and
`tests/engine/client_session_domain.cpp`.

Validation:
`test_engine_client_session_domain` passed; focused moved-client targets passed
9/9; `.\waf.bat build --alltests` passed 131/131; runtime smoke reached first
frame in 0.494 seconds and stopped with reason `command` on May 11 2026 at
14:27:27 local time.

## Phase 141: Save Restore Value Objects

Goal: begin save/restore implementation work with pure values, not runtime
stream ownership.

- [x] Add tests for save admission snapshots, save version classification, or
  save-comment fallback selection.
- [x] Implement the smallest value object that can be tested without
  `SAVERESTOREDATA` mutation.
- [x] Route a tiny legacy call site only if it preserves console output and
  callback ordering exactly.
- [x] Keep game DLL field callbacks, `.HL?` filesystem effects, entity restore,
  and landmark transition mutation legacy-owned.

Evidence:
`Documentation/codex/modern/engine/save-restore-value-objects.md`,
`src/include/engine/server/save_restore_values.hpp`,
`src/engine/server/save_restore_values.cpp`, and
`tests/engine/save_restore_values.cpp`.

Validation:
`test_engine_save_restore_values` and `test_engine_save_restore_format`
passed; `.\waf.bat build --alltests` passed 132/132; runtime smoke reached
first frame in 0.481 seconds and stopped with reason `command` on
May 11 2026 at 14:40 local time. No legacy save/restore call site was routed
in this phase because callback reads, console output, raw field parsing, and
filesystem mutation remain legacy-owned.

## Phase 142: Server Adapter Inventory And Shrink Pass

Goal: reduce repeated glue while keeping adapters boring and explicit.

- [x] Inventory adapter-only duplicated patterns for bit-buffer result
  translation, resource snapshots, message recipient facts, and plain enum
  conversions.
- [x] Add shared adapter helpers only where the helper has one clear domain.
- [x] Avoid merging unrelated adapters just to reduce file count.
- [x] Run focused tests plus full validation if any adapter code changes.

Evidence:
`Documentation/codex/modern/engine/server-adapter-shrink-pass.md`.

Validation:
Documentation-only phase. `git diff --check` passed and
`scripts/phase-status.ps1 -PhaseNumber 142` reported 4 done, 0 open. No
adapter code changed, so focused adapter tests and full runtime validation were
not required.

## Phase 143: Server Header Boundary Audit

Goal: understand how to reduce `server.h` gravitational pull safely.

- [ ] Classify `server.h` contents as ABI-facing structs, private flags,
  declarations, constants, or runtime globals.
- [ ] Identify declarations already replaced by modern private headers.
- [ ] Propose a split plan that does not change struct layout or public
  include behavior.
- [ ] Defer actual splitting unless tests and build fallout are understood.

## Phase 144: World Trace Fixture Harness Plan

Goal: prepare fixtures before moving exact world/trace ownership.

- [ ] Define synthetic edict/model/area-node inputs needed for link, touch,
  hull, group-mask, and trace admission tests.
- [ ] Decide whether fixtures should live in modern C++ tests, legacy C tests,
  or a shared harness.
- [ ] Add one fixture skeleton only if it can compile without pulling the full
  engine runtime into the test.
- [ ] Keep BSP traversal, exact hull tests, trace globals, and `SV_Move()`
  legacy-owned.

## Phase 145: PMove And Usercmd Fixture Harness Plan

Goal: prepare PMove bridge fixtures before touching setup/finish or command
execution.

- [ ] Define usercmd packet, dropped-command, frozen-player, and unlag history
  fixtures.
- [ ] Define `playermove_t` setup/finish snapshots that can be compared safely.
- [ ] Identify callback mocks needed for PMove trace, contents, texture, and
  touch replay behavior.
- [ ] Keep `SV_RunCmd()`, PMove callback table publication, physent
  population, and touch replay legacy-owned.

## Phase 146: Server Runtime Smoke And Performance Checkpoint

Goal: turn the long server lane into a measured runtime checkpoint before more
deep server changes.

- [ ] Run focused/full validation after the consolidation docs are in place.
- [ ] Launch the game with `scripts/run-game.ps1` and manually start a new
  game.
- [ ] Record first-frame time from the validation smoke and the manual
  new-game result in `Documentation/codex/tasks.md`.
- [ ] Compare current startup evidence with the recent server-phase smoke
  timings and flag obvious regressions for later investigation.
