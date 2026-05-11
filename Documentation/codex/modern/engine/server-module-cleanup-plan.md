# Server Module Cleanup Plan

Phase 161 sets up the next cleanup lane after the Phase 160 checkpoint. The
goal is to make the modern server tree read like a game engine, not like a
chronological fossil record of every tiny C seam we extracted.

This is deliberately more ambitious than another narrow helper phase, but it
still keeps compatibility discipline: no public ABI changes, no live server
state ownership moves without fixtures, and no broad runtime rewrite hidden
inside a directory shuffle.

## Terminology

Use game-engine terms for the architecture, while keeping path names stable
unless a rename clearly improves comprehension.

| Engine concept | Current or likely path | Responsibility |
| --- | --- | --- |
| Session | `client/` | Connected clients, admission, rejection, userinfo, timeout, commands, query responses, remote admin. |
| Content | `resources/` | Resource identity, precache catalog, custom resources, consistency, uploads, downloads, reslists. |
| Replication | `messaging/` plus future `snapshot/` | Server-to-client payloads, recipient policy, multicast, event playback, frame datagram, packet entity deltas. |
| Game API | `game_dll/` | Game DLL ABI metadata, lifecycle, entities, messages, resources, movement callbacks, visibility/trace, output, string pool. |
| Simulation | future `world/` | Area links, world trace policy, physics routing, movement constraints, PMove bridge fixtures. |
| Savegame | future `save/` | Save format fixtures, save admission, comments, manifest, entity patch plans, later stream ownership. |
| Runtime | future `runtime/` | Server shell, lifecycle/operator command policy, filters, event-log formatting, cvar/command shell decisions. |
| Shared | future `shared/` | Server-wide limits, compatibility constants, group filters, map validation, cross-domain constraints. |

Avoid generic enterprise names such as `Manager`, `Service`, `Processor`, or
`Handler` unless the object truly owns state or lifecycle. Prefer the domain
noun and a direct verb.

## Current Submodule Scan

### `client/` / Session

Current shape:

- 9 source files, 791 lines;
- 9 headers, 383 lines.

Files:

- `client_command_dispatch`
- `client_policy`
- `client_session_slots`
- `connection_response`
- `connectionless_classifier`
- `remote_admin_command`
- `server_challenge_policy`
- `server_timeout_policy`
- `user_agent_policy`

Collapse direction:

- Keep this module, but consider renaming documentation terminology to
  "Session" rather than spreading more "client policy" nouns.
- Keep `client_policy` as the home for flag/rate/userinfo predicates.
- Consider grouping `connection_response`, `connectionless_classifier`, and
  `server_challenge_policy` under an admission/handshake surface if future
  phases add more connection code.
- Move `source_query` and `netapi_info` here if we decide that query response
  payloads are session-facing rather than shared network utilities.
- Keep `remote_admin_command` separate from normal client commands; it has
  different security and redirect ownership.

Test collapse:

- Keep the focused tests for now.
- Add or extend one `client_session_domain` aggregate when more files move.
- Collapse tiny one-topic tests only after the aggregate covers the same edge
  cases and Waf target churn is intentional.

### `resources/` / Content

Current shape:

- 9 source files, 1114 lines;
- 9 headers, 423 lines.

Files:

- `resource_identity`
- `resource_transfer_manifest`
- `server_consistency_list`
- `server_consistency_policy`
- `server_download_policy`
- `server_hot_resource`
- `server_reslist_policy`
- `server_resource_catalog`
- `server_upload_queue`

Collapse direction:

- Treat this as the first mature domain.
- Promote `resource_transfer_manifest` into a real Content-domain aggregate if
  it can express catalog/download/upload/resource-message flow without owning
  HPAK or filesystem mutation.
- Do not merge `resource_identity`; it is a reusable vocabulary object.
- `server_consistency_list` and `server_consistency_policy` may become one
  consistency unit if tests show little independent value.
- `server_download_policy`, `server_upload_queue`, `server_hot_resource`, and
  `server_reslist_policy` can remain small until a content-transfer aggregate
  removes duplication.

Test collapse:

- `resource_domain.cpp` is the right aggregate anchor.
- The next cleanup can reduce duplicated fixture construction by moving common
  descriptors into a resource test-support header.
- Do not delete narrow tests until the aggregate asserts their quirks.

### `messaging/` / Replication

Current shape:

- 14 source files, 1391 lines;
- 14 headers, 872 lines.

Files:

- `server_customization_message`
- `server_event_playback_policy`
- `server_frame_datagram`
- `server_message_envelope`
- `server_multicast_policy`
- `server_packet_entities_delta`
- `server_resource_message`
- `server_service_messages`
- `server_sound_message`
- `server_spawn_handshake`
- `server_static_messages`
- `server_text_messages`
- `server_userinfo_message`
- `server_voice_relay`

Collapse direction:

- Split the concept, not necessarily the folder yet:
  - protocol payload writers;
  - recipient/delivery policy;
  - frame/snapshot replication.
- Keep `server_message_envelope` as shared replication vocabulary.
- Consider a future `snapshot/` submodule for `server_frame_datagram` and
  `server_packet_entities_delta` once snapshot fixtures grow.
- Keep payload writers small where their protocol bytes are independent.
- Collapse only the smallest text/service/userinfo payload writers if a
  `protocol_messages` unit would be clearer than many tiny files.

Test collapse:

- `server_messaging_domain.cpp` is the aggregate anchor.
- Common byte-buffer assertions should move into a test-support helper rather
  than being copied across payload tests.
- Do not hide exact byte layout tests inside an overly broad integration test.

### `game_dll/` / Game API

Current shape:

- 15 source files, 3027 lines;
- 15 headers, 1547 lines.

Files:

- `game_dll_changelevel_policy`
- `game_dll_client_info_policy`
- `game_dll_enginefuncs`
- `game_dll_entity_lifecycle`
- `game_dll_entity_parse`
- `game_dll_load_policy`
- `game_dll_message_bridge`
- `game_dll_message_session`
- `game_dll_movement_policy`
- `game_dll_output_policy`
- `game_dll_payload_policy`
- `game_dll_resource_policy`
- `game_dll_string_pool_compat`
- `game_dll_user_message_registry`
- `game_dll_visibility_trace_policy`

Collapse direction:

- Do not collapse this into one giant bridge file. `sv_game.c` is already the
  cautionary tale.
- Prefer internal sub-areas:
  - ABI/lifecycle;
  - entities/map parse;
  - message sessions and user-message registry;
  - game-facing resources/payloads;
  - world query and movement callbacks;
  - output/string pool/changelevel.
- Keep `game_dll_enginefuncs` as the ABI inventory anchor.
- `game_dll_message_bridge`, `game_dll_message_session`, and
  `game_dll_user_message_registry` are the strongest merge candidates, but
  only if tests prove a single Game API messaging unit is clearer.
- `game_dll_entity_lifecycle` and `game_dll_entity_parse` are related, but
  they should not merge until entity lifecycle and map parse fixtures share
  enough setup to justify it.

Test collapse:

- `game_dll_bridge_domain.cpp` is the aggregate anchor.
- Keep ABI table tests focused. They protect callback order.
- Collapse only tests that are pure domain behavior and not easier to debug as
  small callback-specific files.

### Flat Server Layer

Current shape:

- 20 source files, 2702 lines;
- 20 headers, 1275 lines.

Files:

- `server_limits`, `server_lifecycle_limits`, `server_group_filter`,
  `server_map_validation`, `server_visibility_constraints`;
- `server_filter`, `server_event_log`, `server_command_lifecycle`,
  `server_operator_command_policy`;
- `server_movement_constraints`, `server_physics_routing_policy`,
  `server_pmove_bridge_policy`, `server_world_link_policy`,
  `server_world_trace_policy`;
- `save_restore_format`, `save_restore_values`, `save_restore_runtime`;
- `source_query`, `netapi_info`.

Collapse direction:

- Move obvious files into `shared/`, `runtime/`, `world/`, and `save/`.
- Decide whether `source_query` and `netapi_info` belong in `client/` as
  Session query responses or in a future network/query module. Prefer
  `client/` for now because the query response path is client-facing.
- Add module aggregate tests only where they clarify behavior.
- Avoid creating a flat `server_common.hpp` dumping ground.

## Test Consolidation Rules

Tests are allowed to become less numerous, but not less diagnostic.

Use this order:

1. Add a module aggregate test.
2. Move duplicated setup to a `*_test_common.hpp` helper.
3. Verify the aggregate covers the edge cases from one or two tiny tests.
4. Remove or merge those tiny tests and update `src/wscript`.
5. Run focused targets and full validation.

Keep narrow tests when they protect:

- ABI table order;
- protocol byte layout;
- save format compatibility;
- exact compatibility quirks that are hard to diagnose from a large aggregate
  failure.

Good aggregate names:

- `server_shared_constraints.cpp`
- `server_runtime_shell.cpp`
- `server_savegame.cpp`
- `server_simulation.cpp`
- `server_replication.cpp`
- `server_content.cpp`
- `game_api_bridge.cpp`
- `server_sessions.cpp`

These names are intentionally engine-flavored. They describe concepts rather
than the historical legacy file that happened to contain the first call site.

## Header Cleanup Rules

- Use one module vocabulary header only when multiple files genuinely share
  the same plain structs or enums.
- Prefer `engine/server/<module>/types.hpp` or
  `engine/server/<module>/<concept>.hpp` over a large global
  `server_types.hpp`.
- Keep compatibility snapshots plain and copyable.
- Keep adapter-only conversion structs out of modern public headers.
- Do not include `server.h` from modern headers.

## Aggressive Phase Techniques

Use a deeper scan at the start of each cleanup phase, but keep the edit itself
mechanical and reviewable.

### Start With A Move Map

Before moving files, write a small move map in the phase notes:

| From | To | Reason | Tests |
| --- | --- | --- | --- |
| `server_limits` | `shared/` | Cross-domain compatibility constants. | `test_engine_server_limits` |
| `save_restore_runtime` | `save/` | Savegame fixture behavior. | `test_engine_save_restore_runtime` |

This prevents cleanup from becoming archaeology by impulse. If a file has no
clear reason and no focused test, do not move it in the first pass.

### Batch Mechanical Work

For each module pass:

1. Move source/header files with `git mv`.
2. Update include paths mechanically.
3. Update READMEs and Waf paths.
4. Run the focused targets for moved files.
5. Run full validation only after the focused targets are green.

The important trick is to separate "move it" from "redesign it." A pure move
phase should not also rewrite behavior.

### Use Search Matrices

Use `rg` in small matrices instead of hunting manually:

- file references: `rg -n "server_limits|server_group_filter" src engine tests`
- include references: `rg -n "#include .*server_limits" .`
- target references: `rg -n "test_engine_server_limits" .`
- legacy ownership references: `rg -n "SV_|sv\\.|svs\\.|svgame|edict_t|sizebuf_t" <files>`

For collapse phases, scan pairs:

- include overlap;
- duplicated test setup;
- duplicated enum or struct definitions;
- repeated adapter conversion logic;
- functions that are always called together.

### Keep A Test Matrix

Every move/collapse phase should name three test sets:

- focused targets for moved helpers;
- aggregate/domain targets;
- full validation command.

If a test is removed or merged, search for the target name first. Do not remove
a target that appears in scripts, task evidence, or validation shortcuts unless
the replacement is recorded.

### Prefer Domain Test Support Over Mega Tests

Efficiency does not mean one giant test. Better:

- `tests/engine/server_content_test_common.hpp`;
- `tests/engine/server_replication_test_common.hpp`;
- `tests/engine/game_api_test_common.hpp`;
- aggregate tests that cover flows;
- narrow tests that remain for byte layout, ABI order, save format, and other
  sharp edges.

### Use Checkpoints As Friction

After a chunky cleanup phase, ask:

- Did file names get shorter or just different?
- Did the module boundary become clearer?
- Did validation time or target noise improve?
- Did any live legacy side effect become less visible?
- Did we merge anything that is now harder to debug?

If the answer is bad, stop collapsing and document why.

## Suggested Next Phases

This cleanup lane should be ambitious enough to reduce ceremony. Do not create
one phase per moved helper.

1. Roadmap and per-submodule collapse audit.
2. Flat server module rehome: move shared, runtime, savegame, simulation, and
   query/session helpers in one coherent pass.
3. Submodule collapse pass: inspect Session, Content, Replication, and Game API
   modules and merge only the parts that are clearer together than apart.
4. Test suite consolidation pass: introduce module test support and merge
   selected tiny tests behind aggregate coverage.
5. Adapter and common-glue cleanup: reduce repeated mechanical conversion
   without hiding legacy side effects.
6. Module cleanup checkpoint with full validation and smoke timing.

## Recommendation

Start with physical grouping of the remaining flat files, then do conceptual
collapses inside existing modules. That ordering gives us immediate visual
simplification without making the already-tested domains harder to debug.

The biggest stylistic change should be restraint: fewer suffixes, fewer
one-function files, and more plain game-engine nouns. But the compatibility
boundary remains non-negotiable. The old C side still owns the dangerous live
state until fixture coverage says otherwise.
