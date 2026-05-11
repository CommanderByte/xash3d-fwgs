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
- Phase 125 adds admission/session helpers and tests only around plain values.
- Phase 126 splits transfer, voice, cvar-query, and remote-admin follow-up
  work so `sv_client.c` does not become the next hidden mega-module.

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
