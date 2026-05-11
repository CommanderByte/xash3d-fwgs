# Server Consolidation Roadmap TODO

This TODO expands the Phase 114 ownership checkpoint into the next practical
server phases. The goal is still the normal migration loop:

1. audit the legacy seam;
2. document behavior and compatibility boundaries;
3. add or extend focused unit tests;
4. introduce target-neutral C++ helpers;
5. route the smallest safe legacy call sites through C-compatible adapters;
6. run focused tests, full validation, and periodic runtime smoke checks.

The roadmap intentionally avoids a single broad server rewrite. It groups work
by domain so the current adapter-heavy surface can later collapse into useful
modules.

## Phase 115-117: Resource Transfer Domain

Legacy spread:

- `sv_init.c`: resource precache, resource indexes, startup resource list,
  voice codec payload.
- `sv_custom.c`: consistency checks, custom resource upload queues, HPAK/custom
  resource propagation.
- `sv_client.c`: download admission and client resource-list parsing.
- `sv_game.c`: game DLL resource callbacks.

Modern helpers already exist for resource identity, catalogs, resource
messages, reslists, downloads, uploads, consistency, customizations, hot
resources, and game DLL resource admission.

Planned path:

- Phase 115 audits the grouped boundary and names which helpers belong in a
  future `server/resources` module.
- Phase 116 adds aggregate tests and a small domain facade only if it reduces
  duplication between existing helpers.
- Phase 117 shrinks adapters only where the grouped boundary is clear.

Phase 115 outcome:

- Resource transfer is a real future domain, but the first consolidation should
  be aggregate tests plus a target-neutral resource manifest/list-view seam, not
  directory regrouping or adapter merging.
- `sv.resources[]`, client resource linked lists, HPAK, filesystem probes,
  transfer cvars, netchan fragments, and game DLL callbacks remain legacy-owned.
- Phase 116 should start by testing catalog entries, descriptor-list lookup,
  download decisions, and resource-row serialization against the same manifest.
  Evidence:
  `Documentation/codex/modern/engine/resource-transfer-consolidation-audit.md`.

## Phase 118-120: Server Messaging Domain

Legacy spread:

- `sv_game.c`: game DLL messages, reliable datagram selection, sound/static
  helper callbacks, multicast.
- `sv_cmds.c`: server print/stufftext broadcast helpers.
- `sv_client.c`: service messages, spawn handshakes, voice parsing.
- `sv_frame.c`: event emission and frame datagram payloads.

Modern helpers already cover text, service, sound, static, voice, multicast,
event playback, event logs, frame datagrams, and spawn handshakes.

Planned path:

- Phase 118 audits message domains and keeps in-game rendered console out of
  scope.
- Phase 119 adds aggregate message/envelope tests where payload helpers share
  command numbers, destination rules, or recipient gates.
- Phase 120 reviews whether a grouped messaging adapter is clearer than many
  small payload adapters.

Phase 118 outcome:

- Server messaging is a real future domain, but it should split into payload
  and envelope writers, destination and recipient policies, and game DLL bridge
  session state rather than one broad module.
- The rendered in-game console stays out of scope until a client/rendering
  console sink phase exists.
- Phase 119 should start with aggregate tests around representative complete
  messages plus destination/recipient vocabulary. Game DLL
  `pfnMessageBegin()` / `pfnMessageEnd()` and user-message rewrites stay for
  the game DLL bridge phases.
  Evidence:
  `Documentation/codex/modern/engine/server-messaging-consolidation-audit.md`.

## Phase 121-123: Game DLL Bridge Domain

Legacy spread:

- `sv_game.c` owns callback table publication, DLL load/unload, message
  sessions, user messages, resources, entity lifecycle, string pool,
  visibility/trace callbacks, movement callbacks, output callbacks, and
  changelevel/save intent.

Modern helpers already cover all of those policy islands, but they are still
arranged by extraction order.

Planned path:

- Phase 121 creates the submodule plan around bridge, lifecycle, entities,
  messages, resources, world queries, movement, output, string pool, and
  changelevel intent.
- Phase 122 adds cross-callback tests that validate grouped behavior without a
  live game DLL.
- Phase 123 tries one low-risk grouped adapter pilot if it makes `sv_game.c`
  easier to read without hiding ABI details.

Phase 121 outcome:

- The future physical layout should use `src/engine/server/game_dll/` with
  mirrored include folders for bridge, messaging, resources, entities,
  client-info, output, string-pool, changelevel, world-query, and movement
  submodules.
- Do not move files yet. First add aggregate tests for cross-callback behavior,
  starting with the game DLL message bridge because the supporting message
  session, user-message registry, payload, multicast, and envelope helpers
  already exist.
- Keep real DLL lifetime, callback table publication, edict/private-data
  storage, string-base memory, `sv.multicast`, netchan writes, trace/world,
  movement, save/restore streams, and live output sinks legacy-owned.
  Evidence:
  `Documentation/codex/modern/engine/game-dll-bridge-submodule-plan.md`.

Phase 122 outcome:

- The first aggregate bridge test targets game DLL messaging, not loader or
  edict ownership.
- `game_dll_message_bridge` adds user-message begin request and active
  registration resend payload helpers; `sv_game.c`, `gEngfuncs`, live DLL
  load/unload, edict storage, string base, and `sv.multicast` remain
  legacy-owned.
- This gives Phase 123 a concrete basis for deciding whether a small grouped
  messaging adapter is useful.
  Evidence:
  `Documentation/codex/modern/engine/game-dll-message-bridge-aggregate.md`.

Phase 123 outcome:

- The pilot groups the message-session and user-message registration adapter
  implementations in `engine/server/game_dll_message_bridge_adapter.cpp`.
- The C headers and exported adapter function names remain split and stable,
  so `sv_game.c` call sites and game DLL callback table publication stay
  compatibility-owned.
  Evidence:
  `Documentation/codex/modern/engine/game-dll-message-bridge-adapter-pilot.md`.

## Phase 124-126: Client And Session Domain

Legacy spread:

- `sv_client.c` mixes admission, challenges, connectionless handling, fake
  clients, userinfo/rates, command dispatch, downloads/uploads, voice, cvar
  query responses, rcon/redirects, movement parsing, and debug entity commands.
- `sv_query.c` and `sv_main.c` consume parts of the same client state.

Modern helpers already cover user-agent validation, source query payloads,
connection responses, connectionless classification, challenge windows, client
flags, userinfo policy, client command dispatch, downloads, uploads, and voice
relay.

Planned path:

- Phase 124 audits the client/session split and chooses the least tangled
  next seam.
- Phase 125 adds a session slot/population helper and tests only around plain
  values.
- Phase 126 splits transfer, voice, cvar-query, and remote-admin follow-up
  work so `sv_client.c` does not become the next hidden mega-module.

Phase 124 outcome:

- `sv_client.c` should split into admission, session slots/population, spawn
  handshake, userinfo, client command, transfer/resource, voice, cvar-query,
  remote-admin, and movement-packet areas.
- The least tangled Phase 125 seam is a client-session slot/population helper
  using plain snapshots for player/bot counts, first-free-slot selection, and
  heartbeat-relevant population decisions.
- Full connect/drop/spawn, userinfo duplicate-name mutation, movement packet
  parsing, cvar-query callbacks, resource-list parsing, rcon redirects, and
  enttools should wait for their own fixtures or domains.
  Evidence:
  `Documentation/codex/modern/engine/client-session-boundary-audit.md`.

Phase 125 outcome:

- `client_session_slots` now owns plain snapshot decisions for player/bot
  counts, first-free-slot selection, and master heartbeat population changes.
- `sv_client.c` routes only those scans through
  `engine/server/client_session_slots_adapter.*`; slot mutation, netchan,
  cvars, edicts, game DLL callbacks, connect/drop side effects, and packet
  sends remain legacy-owned.
  Evidence:
  `Documentation/codex/modern/engine/client-session-slots.md`.

Phase 126 outcome:

- Downloads and client resource-list parsing should stay with the
  resource-transfer domain because live filesystem, HPAK, fragment, and
  resource-list effects dominate.
- Voice relay already has a focused helper, so the remaining live work is
  message reads, physics callbacks, recipient iteration, and datagram mutation.
- Cvar query responses should wait for a game DLL/client-query bridge.
- The only new route-through is `remote_admin_command`, which covers rcon
  enable/password action and quoted command reconstruction while redirects,
  command execution, logging, and packet sends remain legacy-owned.
  Evidence:
  `Documentation/codex/modern/engine/client-transfer-voice-admin-split.md`.

## Phase 127-128: Runtime Configuration And Operator Commands

Legacy spread:

- `sv_main.c` owns server cvars, movevars, timeout/frame loops, packet reads,
  master heartbeat, final messages, and shutdown.
- `sv_cmds.c` owns operator commands, map/load/save/changelevel commands,
  status/info commands, and command registration lifecycle.

Modern helpers already cover read-only cvar snapshots and server command
lifecycle decisions, but live command/cvar mutation remains legacy-owned.

Planned path:

- Phase 127 audits server runtime configuration and determines where read-only
  snapshots reduce adapter churn.
- Phase 128 audits operator commands and looks for command-table or command
  lifecycle helpers that are genuinely reusable.

Phase 127 outcome:

- `sv_main.c` cvar registration, movevar mutation, packet reads, frame
  sequencing, master heartbeats, final messages, and shutdown remain
  legacy-owned.
- `SV_CheckTimeouts()` now routes only timeout and pause-release decisions
  through `server_timeout_policy`, using plain request values built from the
  legacy client slot, cvar reads, entity flags, and computed drop points.
- The policy helper deliberately does not own cvar mutation, local-address
  detection, client drops, zombie state mutation, or pause toggles.
  Evidence:
  `Documentation/codex/modern/engine/server-runtime-configuration-boundary.md`.

Phase 128 outcome:

- `sv_cmds.c` command registration, command callbacks, console output,
  filesystem probes, save/load/map effects, info-string mutation, cvar
  mutation, client lookup, and shutdown effects remain legacy-owned.
- `server_operator_command_policy` now routes only argument-policy decisions
  for `kick`, `serverinfo`, and `localinfo`.
- The selected seam is reusable because it models operator command admission
  and target classification without owning `Cmd_*` registration or command
  execution side effects.
  Evidence:
  `Documentation/codex/modern/engine/server-operator-command-boundary.md`.

## Phase 129-130: Frame Snapshot Domain

Legacy spread:

- `sv_frame.c` owns packet entity selection, baseline deltas, event emission,
  ping emission, clientdata, reliable update fanout, datagram sending, and
  inactive-client handling.

Modern helpers already cover frame datagram gates, event playback decisions,
visibility capacities, group filtering, and client flag predicates.

Planned path:

- Phase 129 audits snapshot/frame ownership and documents what cannot move
  before packet-entity fixtures exist.
- Phase 130 adds pure snapshot/delta planning tests only if they can avoid live
  `edict_t`, `client_frame_t`, and packet-entity mutation.

Phase 129 outcome:

- Packet entity selection, client frame mutation, baseline scoring, event queue
  mutation, ping stat lookup, clientdata callbacks, netchan sends, and inactive
  client transition effects remain legacy-owned in `sv_frame.c`.
- Existing modern helpers already cover datagram transfer gates, event emit
  count clamping, and portal viewentity capacity.
- Phase 130 should pilot only a packet-entity delta cursor/header planner from
  plain old/new entity numbers and delta-frame freshness facts. Full snapshot
  building, `MSG_WriteDeltaEntity()`, `SV_FindBestBaseline()`, and circular
  packet-entity storage remain out of scope.
  Evidence:
  `Documentation/codex/modern/engine/server-frame-snapshot-boundary.md`.

Phase 130 outcome:

- `server_packet_entities_delta` now owns only packet-entity header selection
  and sorted old/new cursor actions for `SV_EmitPacketEntities()`.
- `sv_frame.c` still owns `svs.packet_entities`, `client_frame_t` mutation,
  baseline selection, `MSG_WriteDeltaEntity()`, stale-delta diagnostics, and
  old-edict removal checks.
- The helper adds fixture coverage for packet-entity ordering without moving
  the snapshot builder or delta writer.
  Evidence:
  `Documentation/codex/modern/engine/server-packet-entities-delta.md`.

## Phase 131-133: World, Physics, And PMove

Legacy spread:

- `sv_world.c`: area tree, links, touch triggers, hull selection, traces,
  contents, surface/texture trace, light styles.
- `sv_phys.c`: entity think, impact, pusher/toss/step physics, gravity, fog,
  physics API callbacks.
- `sv_move.c`: monster movement.
- `sv_pmove.c`: player-move setup, unlag, physent population, run command.

Modern helpers already cover movement constants, visibility capacities, group
filtering, and game DLL movement/visibility admission. They do not own live
collision, hull, BSP, or PMove state.

Planned path:

- Phase 131 audits world link/touch boundaries and fixture needs.
- Phase 132 audits trace/physics fixtures before any route-through.
- Phase 133 audits PMove as a bridge boundary and defers risky live movement
  changes until fixtures exist.

Phase 131 outcome:

- `sv_world.c` area-node storage, edict link lists, trigger touch callbacks,
  water brush content checks, group-filter callers, and collision traversal
  remain legacy-owned.
- `server_world_link_policy` now owns only split-axis, split-distance, link
  child, and recursive traversal-mask decisions from plain values.
- Further route-through needs synthetic edict/list fixtures, trigger brush hull
  fixtures, water content fixtures, and callback-order tests.
  Evidence:
  `Documentation/codex/modern/engine/server-world-link-boundary.md`.

Phase 132 outcome:

- `sv_world.c` still owns exact hull selection, brush/studio/custom clipping,
  portal CSG, `SV_Move()`, `SV_MoveNoEnts()`, trace globals, and area-list
  collision traversal.
- `sv_phys.c` still owns live think/touch/blocked callbacks, fly/toss/step/
  pusher physics, velocity mutation, water transitions, and the external
  physics API table.
- `sv_move.c` still owns bottom checks, monster stepping, chase-direction
  ordering, world-only movement, point-contents checks, relinking, and
  partial-ground compatibility.
- `server_physics_routing_policy` now owns only `MOVETYPE_*` to physics
  handler dispatch, pusher candidate, and pushed-entity precise-blocking
  predicates.
- Further route-through needs synthetic trace fixtures, hull/model fixtures,
  pusher restore-stack fixtures, water/velocity fixtures, and game DLL/physics
  callback mocks.
  Evidence:
  `Documentation/codex/modern/engine/server-world-physics-fixture-audit.md`.

Phase 133 outcome:

- `sv_pmove.c` still owns the PMove callback table, `playermove_t` setup and
  finish, physent/visent/moveent population, player trace callbacks, unlag
  edict relinking, touch replay, and `SV_RunCmd()` command execution.
- `sv_client.c` still owns delta-compressed usercmd parsing, dropped-command
  sequencing, frozen-player filtering, and calls into `SV_RunCmd()`.
- `server_pmove_bridge_policy` now owns only PMove unlag admission, player
  edict-index range checks, interpolant-use gates, teleport threshold,
  latency/lerp target-time calculations, and interpolation fraction clamps.
- Further route-through needs command packet fixtures, playermove setup/finish
  snapshots, physent population fixtures, PMove callback mocks, unlag
  frame-history fixtures, and touch replay fixtures.
  Evidence:
  `Documentation/codex/modern/engine/server-pmove-bridge-boundary.md`.

## Phase 134: Runtime Save/Restore Owner Audit

Legacy spread:

- `sv_save.c` owns save/load streams, landmark transitions, token tables, game
  DLL field serialization, entity patch files, and bundled save files.

Modern helpers already parse fixture formats and model changelevel intent, but
runtime ownership remains legacy-owned.

Planned path:

- Phase 134 audits which save/restore concepts can become modern value types
  without changing save file compatibility.
- Runtime stream mutation, game DLL field descriptors, and filesystem side
  effects stay legacy-owned until a later implementation phase.

## Validation Rhythm

- Documentation-only phases run `git diff --check`.
- Implementation phases use `scripts/run-phase-validation.ps1` with the
  focused target when practical.
- Every few implementation phases, launch the game with `scripts/run-game.ps1`
  and record the manual new-game result.
- Record first-frame time from the `+wait +wait` smoke evidence whenever the
  phase validation script runs it.
