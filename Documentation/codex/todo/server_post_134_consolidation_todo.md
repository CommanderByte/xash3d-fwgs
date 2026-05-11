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

- [ ] Define the target directories for resources, messaging, game DLL bridge,
  client/session, runtime, world/physics, save/restore, and shared contracts.
- [ ] Map current helper files to those domains and mark temporary facades.
- [ ] Decide which include paths and Waf target lists would change during a
  physical move.
- [ ] Document the no-move/no-rename boundaries for public ABI and legacy C
  adapters.

## Phase 137: Resource Transfer Domain Consolidation Pilot

Goal: use the best-covered resource helpers as the first consolidation pilot.

- [ ] Review `resource_identity`, `resource_transfer_manifest`,
  `server_resource_catalog`, download/upload, consistency, customization,
  hot-resource, and reslist helpers as one domain.
- [ ] Add or extend aggregate tests that exercise catalog-to-transfer flows.
- [ ] Move or group only the modern resource-domain files if the Phase 136
  layout makes the change low-risk.
- [ ] Keep HPAK, filesystem probes, resource linked lists, netchan fragments,
  and game DLL callbacks legacy-owned.

## Phase 138: Server Messaging Domain Consolidation Pilot

Goal: consolidate message payload and envelope helpers without hiding live
packet-buffer ownership.

- [ ] Review text, service, sound, static, voice, multicast, envelope, spawn
  handshake, event playback, and frame datagram helpers.
- [ ] Add aggregate tests around representative complete messages plus
  recipient facts.
- [ ] Expand shared adapter/result translation only where duplication is
  obvious.
- [ ] Keep `sizebuf_t`, `MSG_*`, signon/datagram mutation, netchan sends, and
  rendered console ownership legacy-bound.

## Phase 139: Game DLL Bridge Domain Consolidation Pilot

Goal: turn the broad game DLL bridge helper set into a clearer internal module
without changing callback table publication.

- [ ] Map bridge helpers into lifecycle, ABI metadata, entities, messages,
  resources, movement, visibility/world-query, output, string-pool, and
  changelevel areas.
- [ ] Add a domain-level test that proves cross-helper behavior without a live
  DLL.
- [ ] Consider grouped implementation files only where they reduce adapter
  confusion.
- [ ] Keep DLL lifetime, `enginefuncs_t` order, `svgame`, edict storage, and
  callback calls legacy-owned.

## Phase 140: Client Session Domain Consolidation Pilot

Goal: clarify `sv_client.c` modernization boundaries before extracting more
client behavior.

- [ ] Group existing client helpers around admission, session slots, userinfo,
  commands, transfer, voice, cvar query, and remote admin.
- [ ] Identify which helpers should move under a future client/session module
  and which belong to resource or messaging domains instead.
- [ ] Add aggregate tests for admission/session/userinfo facts if they reduce
  repeated adapter code.
- [ ] Keep connect/drop/spawn mutation, netchan, command execution, resource
  list mutation, voice packet reads, and cvar-query callbacks legacy-owned.

## Phase 141: Save Restore Value Objects

Goal: begin save/restore implementation work with pure values, not runtime
stream ownership.

- [ ] Add tests for save admission snapshots, save version classification, or
  save-comment fallback selection.
- [ ] Implement the smallest value object that can be tested without
  `SAVERESTOREDATA` mutation.
- [ ] Route a tiny legacy call site only if it preserves console output and
  callback ordering exactly.
- [ ] Keep game DLL field callbacks, `.HL?` filesystem effects, entity restore,
  and landmark transition mutation legacy-owned.

## Phase 142: Server Adapter Inventory And Shrink Pass

Goal: reduce repeated glue while keeping adapters boring and explicit.

- [ ] Inventory adapter-only duplicated patterns for bit-buffer result
  translation, resource snapshots, message recipient facts, and plain enum
  conversions.
- [ ] Add shared adapter helpers only where the helper has one clear domain.
- [ ] Avoid merging unrelated adapters just to reduce file count.
- [ ] Run focused tests plus full validation if any adapter code changes.

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
