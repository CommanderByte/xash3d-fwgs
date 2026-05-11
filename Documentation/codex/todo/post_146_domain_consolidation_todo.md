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

- [ ] Audit current resource-domain helpers and adapters against
  `sv_custom.c`, `sv_client.c`, and `sv_game.c` call sites.
- [ ] Add aggregate tests covering identity, catalog, consistency,
  download/upload admission, hot-resource decisions, and customization payload
  planning.
- [ ] Keep HPAK mutation, filesystem reads/writes, reliable datagram ownership,
  and client/resource structs legacy-owned.
- [ ] Run focused resource tests and full validation.

## Phase 149: Resource Adapter Shrink Pilot

Goal: reduce duplicated resource glue only where aggregate tests cover the
behavior.

- [ ] Identify resource adapters that only translate plain values.
- [ ] Add a small shared resource adapter utility or grouped adapter if it
  reduces duplication without hiding legacy ownership.
- [ ] Avoid merging unrelated `sv_custom.c`, `sv_client.c`, and `sv_game.c`
  side effects.
- [ ] Run focused resource tests, full validation, and smoke timing if runtime
  code changes.

## Phase 150: Messaging Domain Aggregate Tests

Goal: test message payload and recipient policy as a domain, not only as
individual payload writers.

- [ ] Audit message helpers under `src/engine/server/messaging`.
- [ ] Add aggregate tests for envelope selection, recipient policy, payload
  writer output, and event/frame-adjacent message planning.
- [ ] Keep `sizebuf_t`, datagram ownership, signon buffers, client frames, and
  actual network sends legacy-owned.
- [ ] Run focused messaging tests and full validation.

## Phase 151: Messaging Adapter Shrink Pilot

Goal: reduce repeated message adapter glue without obscuring packet-buffer
ownership.

- [ ] Identify adapters that share recipient/envelope/string/byte writer
  patterns.
- [ ] Add shared adapter glue only for repeated mechanical conversions.
- [ ] Keep live datagram mutation and buffer lifetime in legacy call sites.
- [ ] Run focused messaging tests, full validation, and smoke timing if
  runtime code changes.

## Phase 152: Game DLL Bridge Consolidation Map

Goal: regroup game DLL bridge helpers around the conceptual domains identified
after Phase 114.

- [ ] Re-scan `sv_game.c` against modern `game_dll/` helpers.
- [ ] Classify helpers by ABI, lifecycle, entities, messages, resources,
  world queries, movement, output, and string pool.
- [ ] Decide which adapter groups can be consolidated without changing ABI
  publication order.
- [ ] Keep callback table layout, DLL load/unload, edict storage, and game DLL
  function ordering legacy-owned.

## Phase 153: Game DLL Bridge Adapter Shrink Pilot

Goal: shrink one safe game DLL adapter cluster behind existing tests.

- [ ] Pick one low-risk cluster from Phase 152.
- [ ] Add or strengthen aggregate tests before moving glue.
- [ ] Consolidate only mechanical conversion or repeated adapter calls.
- [ ] Run focused game DLL bridge tests, full validation, and smoke timing.

## Phase 154: Client Session Aggregate Tests

Goal: prove client/session helpers combine cleanly before more `sv_client.c`
movement.

- [ ] Add aggregate tests around admission, slot selection, rejection response,
  userinfo policy, command dispatch route, remote admin command classification,
  and query response behavior.
- [ ] Keep packet reads, netchan state, downloads, movement packet parsing,
  and live client mutation legacy-owned.
- [ ] Run focused client/session tests and full validation.
- [ ] Record whether this makes a grouped client adapter worthwhile.

## Phase 155: Client Adapter Shrink Pilot

Goal: reduce duplicated client/session adapter glue where Phase 154 proves the
concept boundary.

- [ ] Identify plain-value client adapters that can share conversion utilities.
- [ ] Keep transfer, voice, movement, and netchan side effects separated.
- [ ] Avoid creating a broad `sv_client.c` replacement facade.
- [ ] Run focused client/session tests, full validation, and smoke timing.

## Phase 156: World And PMove Fixture Expansion

Goal: strengthen fixtures before live world/PMove ownership moves.

- [ ] Extend world trace fixtures toward clip-admission path selection.
- [ ] Extend PMove fixtures toward command replay and setup/finish comparison
  plans.
- [ ] Keep exact hull traversal, PMove callbacks, physent population, and
  touch replay legacy-owned.
- [ ] Run focused fixture tests and full validation.

## Phase 157: World Or PMove Micro-Extraction Pilot

Goal: move one fixture-backed world/PMove decision into modern code.

- [ ] Choose exactly one decision covered by Phase 156 fixtures.
- [ ] Route the legacy call site through the modern helper.
- [ ] Keep live storage, traces, callbacks, and relinking legacy-owned.
- [ ] Run focused tests, full validation, and smoke timing.

## Phase 158: Save Runtime Fixture Expansion

Goal: prepare save/load runtime movement with better fixtures.

- [ ] Extend save fixtures beyond format/value parsing into save admission,
  comment/version decisions, entity patch planning, and manifest behavior.
- [ ] Keep raw stream mutation, filesystem writes, game DLL field callbacks,
  and console output legacy-owned.
- [ ] Run focused save tests and full validation.
- [ ] Decide whether a save-domain aggregate test is ready.

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
- [ ] Move completed TODO items to `Documentation/codex/done/` where sensible.
