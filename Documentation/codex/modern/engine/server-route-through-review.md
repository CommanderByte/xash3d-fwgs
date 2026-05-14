# Server Runtime Route-Through Review

Phase 106 reviews the constants mirrored or wrapped in Phases 101-105 and
decides whether any additional direct route-through should be made immediately.

The result: no new runtime route-through is added in Phase 106. The safest
small call sites from this lane were already routed in Phases 102-105. The
remaining constants either affect fixed storage/layout, touch many live server
owners at once, or need a dedicated behavior phase.

## Already Routed

| Area | Modern owner | Legacy route-through | Notes |
| --- | --- | --- | --- |
| Challenge window | `server_challenge_policy` | `sv_client.c` via `server_challenge_policy_adapter` | Only current/previous window math moved. Address hashing, salts, packets, and rejection output remain legacy-owned. |
| Lifecycle limits | `server_lifecycle_limits` | `sv_init.c` via `server_lifecycle_limits_adapter` | Maxclient clamping, update-backup selection, client entity count, game entity count, and spawn settling frame/time moved. Cvars, shutdown, allocation, activation, baselines, and resources remain legacy-owned. |
| Monster move type check | `server_movement_constraints` | `sv_move.c` via `server_movement_constraints_adapter` | Only `SV_MoveToOrigin()` mode classification moved. Movement execution and world traces remain legacy-owned. |
| Visibility capacity | `server_visibility_constraints` | `sv_world.c`, `sv_frame.c`, `sv_game.c` via `server_visibility_constraints_adapter` | Leaf capacity, overflow marker, cached leaf index, and portal viewentity capacity moved. BSP traversal, PVS/PAS, packet selection, and edict arrays remain legacy-owned. |

## Legacy-Owned Directly

These values should not be directly replaced in the legacy headers yet:

- `MAX_PUSHED_ENTS`: sizes `svgame.pushed`; migrate only with pusher storage
  ownership.
- `MAX_VIEWENTS`: sizes `sv_client_t::viewentity`; modern helpers may use the
  capacity, but the array layout stays legacy-owned.
- `MAX_LOCALINFO_STRING`: sizes `svs.localinfo`; route only with a localinfo
  storage/serialization phase.
- `MAX_ENT_LEAFS_32` and `MAX_ENT_LEAFS_16`: size fixed `edict_t` leaf arrays;
  helpers may decide capacity behavior, but the arrays remain legacy-owned.
- Server `MAX_CLIP_PLANES`: sizes the `SV_FlyMove()` stack array in
  `sv_phys.c`; leave direct ownership there until a physics-loop migration.
- `FCL_*`: widely shared private client flags; route through narrow policy
  helpers only, not by sweeping macro replacement.

## Deferred Route Candidates

These are legitimate future work, but not good Phase 106 additions:

- `GROUP_OP_AND` and `GROUP_OP_NAND`: appear in touch, collision, multicast,
  PVS, and player-move paths. This deserves a focused server group-filter
  policy phase with behavior fixtures.
- `MAP_IS_EXIST`, `MAP_HAS_LANDMARK`, and `MAP_INVALID_VERSION`: already touch
  changelevel/save paths. Route through a map-validation or changelevel policy
  phase, not a constants cleanup.
- `SVF_SKIPLOCALHOST` and `SVF_MERGE_VISIBILITY`: hostflags affect frame
  visibility, multicast filtering, and portal/PVS behavior. Route only when
  the frame visibility owner is ready.
- `MOVE_EPSILON`: mirrored for drift detection, but currently not used by live
  server physics code. Do not invent a runtime route-through for an unused
  value.

## Review Rule

For this lane, "route-through candidate" means "safe to route through a narrow
behavior helper after the owner is named and tested." It does not mean "replace
every legacy macro reference immediately."

The next useful server phases should be behavior-owner phases, not another
constants sweep. Good candidates are:

- server group filtering policy;
- map validation and landmark flag policy;
- client flag accessor/policy cleanup by owner;
- host visibility flag policy inside frame construction.
