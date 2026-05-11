# Pre-159 Simplification Checkpoint

This checkpoint pauses before the client/render audit to address a real risk:
the compatibility-first migration is working, but the modern server surface can
start to feel like "one tiny policy object, one tiny adapter, one tiny test,
forever." That was useful while proving behavior. It should not become the
final architecture.

## Current Shape

Current scan after Phase 158:

| Area | Files | Source | Headers | Adapter-named files | Lines |
| --- | ---: | ---: | ---: | ---: | ---: |
| `src/engine/server` | 67 | 66 | 0 | 0 | 9025 |
| `src/include/engine/server` | 67 | 0 | 66 | 0 | 4500 |
| `engine/server` | 124 | 67 | 57 | 108 | 26293 |
| `tests/engine` | 88 | 86 | 2 | 4 | 13619 |

This is not bad by itself. It means the migration has created a lot of
behavioral probes and a lot of compatibility coverage. The problem is that
many modern files are now smaller than the ceremony needed to name, build, and
adapt them.

## What Is Still Useful

Keep this pattern where risk is high:

- world, physics, PMove, save/restore, and game DLL bridge code that touches
  live globals, edicts, buffers, callbacks, or filesystem mutation;
- one focused test per risky decision while the legacy owner is still active;
- thin C adapters where a legacy C file routes exactly one decision through
  modern code.

In those zones the verbosity is buying us safety. A little ceremony is cheaper
than subtle compatibility drift.

## What Should Change

For lower-risk or already-proven domains, stop defaulting to a new file per
decision. Prefer one cohesive module per concept:

- `resources/`: catalog, identity, consistency, upload, download, hot resource,
  reslist;
- `messaging/`: envelopes, fixed service messages, static/sound/text payloads,
  voice relay, multicast, frame datagram, event playback;
- `client/`: admission, challenge, rejection, userinfo, command routing,
  timeout, remote admin;
- `game_dll/`: ABI metadata, lifecycle, entities, messages, movement,
  resources, visibility/trace, output;
- future `world/`: area links, trace setup, visibility limits, movement
  constraints, PMove bridge fixtures;
- future `save/`: format, values, runtime fixture manifest/patch plans.

The target is not fewer lines at all costs. The target is fewer concepts
scattered across dozens of one-off names.

## Practical Rules

Use these rules starting with Phase 159:

1. A new modern file should introduce a real domain concept, not only a new
   noun for a branch condition.
2. A one-function policy file is acceptable only as a temporary extraction
   when the legacy runtime is risky.
3. Once a domain has three or more related helper files, the next checkpoint
   should ask whether they belong in one module or subdirectory.
4. C adapters should be grouped by legacy owner or modern domain once the
   route-through is stable. Do not merge adapters just for file-count vanity.
5. Tests should remain focused, but phase validation can prefer aggregate
   domain tests once narrow tests have proven the pieces.
6. Avoid `Plan` suffixes for objects that are just decoded values. Keep
   `Plan` for decisions that actually describe intended actions.
7. Prefer shorter names inside clear namespaces. For example, a future
   `engine::server::save::ArchiveManifest` is better than endlessly growing
   `SaveRestoreRuntimeArchiveManifestPlan` names.

## Immediate Simplification Candidates

These are good candidates for near-term consolidation, not emergency cleanup:

| Candidate | Why |
| --- | --- |
| `save_restore_format`, `save_restore_values`, `save_restore_runtime` | These now form a clear save fixture domain. They can move under a future `server/save/` folder with shorter names. |
| `server_world_link_policy`, `server_world_trace_policy`, movement/visibility constraints | These are future `server/world/` material, but wait until hull/trace fixtures are stronger. |
| `server_message_*` helpers | The `messaging/` folder is conceptually right, but many tiny payload writers could share envelope/writer utilities or aggregate tests. |
| `server_*_adapter` files under `engine/server` | Some adapters are 10-25 lines. They can later collapse into grouped legacy adapters such as `server_world_adapter`, `server_message_adapter`, or `game_dll_bridge_adapter`. |
| README inventories | The current server README lists every helper and is becoming noisy. It should move toward domain summaries plus links to focused docs. |

## What Not To Simplify Yet

Do not collapse these prematurely:

- game DLL ABI table publication and callback order;
- PMove setup/finish and physent population;
- save/restore stream mutation and game DLL field callbacks;
- world hull traversal and trace mutation;
- client connection admission and live netchan/session mutation.

These still need boring, explicit boundaries until fixtures prove more of the
runtime.

## Recommendation

Do Phase 159 as planned, but keep it audit-oriented. Do not start the client
lane by creating another wave of one-file micro-policies unless the behavior is
high risk.

Make Phase 160 a real consolidation checkpoint:

- measure file/adapters again;
- identify domains with enough tests to consolidate;
- choose one small simplification pilot;
- prefer grouping and naming cleanup over more extraction for its own sake.

This keeps our cautious migration method, but gives it an exit ramp from
enterprise-shaped scaffolding.
