# Post-146 Domain Consolidation TODO

This TODO starts after the Phase 146 runtime checkpoint. The goal is to turn
the modern server helper layer into coherent domains before attempting larger
runtime ownership moves.

Rules for this lane:

1. Add aggregate tests before shrinking adapters.
2. Consolidate only around real domains, not cosmetic file-count reduction.
3. Keep live `edict_t`, `sv_client_t`, `playermove_t`, `trace_t`, packet
   buffers, game DLL ABI, and filesystem ABI legacy-owned until fixtures prove
   the move is safe.
4. Keep focused tests even when aggregate tests are added.
5. Record smoke timing when runtime code or build wiring changes.

## Phase 147: Post-146 Migration Status And Roadmap

Goal: make the next route explicit before starting new code movement.

- [ ] Record current modern/legacy file-shape inventory.
- [ ] Identify what is actually modern-owned versus adapter-routed.
- [ ] Pick the next domain consolidation order.
- [ ] Update `Documentation/codex/tasks.md` with the next phase set.

## Phase 148: Resource Domain Aggregate Tests

Goal: prove resource helpers work together before adapter consolidation.

- [x] Audit current resource-domain helpers and adapters against
  `sv_custom.c`, `sv_client.c`, and `sv_game.c` call sites.
- [x] Add aggregate tests covering identity, catalog, consistency,
  download/upload admission, hot-resource decisions, and customization payload
  planning.
- [x] Keep HPAK mutation, filesystem reads/writes, reliable datagram ownership,
  and client/resource structs legacy-owned.
- [x] Run focused resource tests and full validation.

## Phase 149: Resource Adapter Shrink Pilot

Goal: reduce duplicated resource glue only where aggregate tests cover the
behavior.

- [x] Identify resource adapters that only translate plain values.
- [x] Add a small shared resource adapter utility or grouped adapter if it
  reduces duplication without hiding legacy ownership.
- [x] Avoid merging unrelated `sv_custom.c`, `sv_client.c`, and `sv_game.c`
  side effects.
- [x] Run focused resource tests, full validation, and smoke timing if runtime
  code changes.

## Phase 150: Messaging Domain Aggregate Tests

Goal: test message payload and recipient policy as a domain, not only as
individual payload writers.

- [x] Audit message helpers under `src/engine/server/messaging`.
- [x] Add aggregate tests for envelope selection, recipient policy, payload
  writer output, and event/frame-adjacent message planning.
- [x] Keep `sizebuf_t`, datagram ownership, signon buffers, client frames, and
  actual network sends legacy-owned.
- [x] Run focused messaging tests and full validation.

## Phase 151: Messaging Adapter Shrink Pilot

Goal: reduce repeated message adapter glue without obscuring packet-buffer
ownership.

- [x] Identify adapters that share recipient/envelope/string/byte writer
  patterns.
- [x] Add shared adapter glue only for repeated mechanical conversions.
- [x] Keep live datagram mutation and buffer lifetime in legacy call sites.
- [x] Run focused messaging tests, full validation, and smoke timing if
  runtime code changes.

## Phase 152: Game DLL Bridge Consolidation Map

Goal: regroup game DLL bridge helpers around the conceptual domains identified
after Phase 114.

- [x] Re-scan `sv_game.c` against modern `game_dll/` helpers.
- [x] Classify helpers by ABI, lifecycle, entities, messages, resources,
  world queries, movement, output, and string pool.
- [x] Decide which adapter groups can be consolidated without changing ABI
  publication order.
- [x] Keep callback table layout, DLL load/unload, edict storage, and game DLL
  function ordering legacy-owned.

## Phase 153: Game DLL Bridge Adapter Shrink Pilot

Goal: shrink one safe game DLL adapter cluster behind existing tests.

- [x] Pick one low-risk cluster from Phase 152.
- [x] Add or strengthen aggregate tests before moving glue.
- [x] Consolidate only mechanical conversion or repeated adapter calls.
- [x] Run focused game DLL bridge tests, full validation, and smoke timing.

## Phase 154: Client Session Aggregate Tests

Goal: prove client/session helpers combine cleanly before more `sv_client.c`
movement.

- [x] Add aggregate tests around admission, slot selection, rejection response,
  userinfo policy, command dispatch route, remote admin command classification,
  and query response behavior.
- [x] Keep packet reads, netchan state, downloads, movement packet parsing,
  and live client mutation legacy-owned.
- [x] Run focused client/session tests and full validation.
- [x] Record whether this makes a grouped client adapter worthwhile.

## Phase 155: Client Adapter Shrink Pilot

Goal: reduce duplicated client/session adapter glue where Phase 154 proves the
concept boundary.

- [x] Identify plain-value client adapters that can share conversion utilities.
- [x] Keep transfer, voice, movement, and netchan side effects separated.
- [x] Avoid creating a broad `sv_client.c` replacement facade.
- [x] Run focused client/session tests, full validation, and smoke timing.

## Phase 156: World And PMove Fixture Expansion

Goal: strengthen fixtures before live world/PMove ownership moves.

- [x] Extend world trace fixtures toward clip-admission path selection.
- [x] Extend PMove fixtures toward command replay and setup/finish comparison
  plans.
- [x] Keep exact hull traversal, PMove callbacks, physent population, and
  touch replay legacy-owned.
- [x] Run focused fixture tests and full validation.

## Phase 157: World Or PMove Micro-Extraction Pilot

Goal: move one fixture-backed world/PMove decision into modern code.

- [x] Choose exactly one decision covered by Phase 156 fixtures.
- [x] Route the legacy call site through the modern helper.
- [x] Keep live storage, traces, callbacks, and relinking legacy-owned.
- [x] Run focused tests, full validation, and smoke timing.

## Phase 158: Save Runtime Fixture Expansion

Goal: prepare save/load runtime movement with better fixtures.

- [x] Extend save fixtures beyond format/value parsing into save admission,
  comment/version decisions, entity patch planning, and manifest behavior.
- [x] Keep raw stream mutation, filesystem writes, game DLL field callbacks,
  and console output legacy-owned.
- [x] Run focused save tests and full validation.
- [x] Decide whether a save-domain aggregate test is ready.

## Phase 159: Engine Client And Render Boundary Audit

Goal: look outside the server for the next major modernization lane.

- [ ] Audit `engine/client`, `engine/client/dll_int`, menu interfaces, render
  boundaries, audio/video capture, and rendered console ownership.
- [ ] Identify low-risk value/policy candidates analogous to the early server
  phases.
- [ ] Avoid touching renderer or client prediction runtime without fixtures.
- [ ] Produce a recommended client/render/audio phase list.

## Phase 160: Domain Consolidation Checkpoint

Goal: pause after the first post-146 domain consolidation tranche.

- [ ] Compare adapter counts and modern domain files after Phases 147-159.
- [ ] Run full validation and runtime smoke timing.
- [ ] Decide whether to continue server consolidation, shift to client/render,
  or tackle deferred memory/platform/licensing work.
- [ ] Choose one simplification pilot: grouped adapter, grouped domain module,
  aggregate test, or README inventory cleanup.
- [ ] Move completed TODO items to `Documentation/codex/done/` where sensible.
