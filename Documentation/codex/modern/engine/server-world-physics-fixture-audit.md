# Server World Trace And Physics Fixture Audit

Phase 132 audits the collision and physics code around `sv_world.c`,
`sv_phys.c`, and `sv_move.c`. The result is intentionally narrow: live hull
traversal, trace globals, edict mutation, and game DLL callbacks remain
legacy-owned, while the pure `MOVETYPE_*` routing decisions used by server
physics now have a tested modern helper.

## Legacy Owners

| Area | Legacy owner | Why it stays legacy-owned |
| --- | --- | --- |
| Hull selection and exact tracing | `SV_HullForEntity()`, `SV_HullForStudioModel()`, `SV_ClipMoveToEntity()` in `sv_world.c` | Depends on `edict_t`, `model_t`, brush/studio/custom hulls, entity angles, physics extension callbacks, `PM_RecursiveHullCheck()`, and trace globals. |
| Entity clip admission | `SV_ClipToEntity()` in `sv_world.c` | Mixes pure filters with group masks, game DLL `pfnShouldCollide()`, model lookup, bounds tests, owner relations, portal CSG, and exact hull traces. |
| Area traversal for traces | `SV_ClipToLinks()`, `SV_ClipToPortals()`, `SV_ClipToWorldBrush()`, `SV_Move()`, and `SV_MoveNoEnts()` | Walks live area-node lists, touches solid/portal/world-brush lists, combines trace results, and writes `svgame.globals->trace_ent`. Phase 131 only extracted child traversal policy. |
| Physics think and impact | `SV_RunThink()`, `SV_PlayerRunThink()`, and `SV_Impact()` in `sv_phys.c` | Calls game DLL think/touch callbacks, frees edicts, writes global trace state, and applies group filtering. |
| Fly, toss, step, and pusher loops | `SV_FlyMove()`, `SV_PushEntity()`, `SV_PushMove()`, `SV_PushRotate()`, `SV_Physics_Toss()`, `SV_Physics_Step()`, and `SV_Physics_Pusher()` | Mutates origins, velocities, flags, ground entities, pushed-entity restore stacks, area links, and callback ordering around traces. |
| Monster movement | `SV_CheckBottom()`, `SV_MoveStep()`, `SV_MoveTest()`, `SV_NewChaseDir()`, and `SV_MoveToOrigin()` in `sv_move.c` | Consumes `SV_Move()`, `SV_MoveNoEnts()`, point contents, random chase ordering, relinking, and ground/partial-ground flags. Phase 104 only extracted monster move-type classification. |
| Physics extension bridge | `server_physics_api_t` callbacks in `sv_phys.c` | Exposes `SV_Move()`, `SV_MoveNoEnts()`, point contents, model handles, memory allocation, renderer-backed debug drawing, fog, files, and native objects to external physics code. |

## Modern Helper

Phase 132 adds:

- `src/include/engine/server/server_physics_routing_policy.hpp`
- `src/engine/server/server_physics_routing_policy.cpp`
- `engine/server/server_physics_routing_policy_adapter.*`
- `tests/engine/server_physics_routing_policy.cpp`

The helper owns only plain routing decisions:

- map legacy `MOVETYPE_*` values to the server physics handler that
  `SV_Physics_Entity()` should call;
- classify the `MOVETYPE_WALK` case as an invalid server-physics dispatch that
  still reaches the old `Host_Error()` path;
- preserve the old "unsupported movetype does nothing" behavior;
- decide whether a pusher considers an entity movetype for push testing;
- decide whether a pushed movetype uses precise blocking comparison.

This is a useful seam because it is observable, easy to test, and close to the
physics loop without moving trace math or callback ownership.

## Existing Coverage

Phase 132 relies on existing helpers instead of duplicating their coverage:

- `server_movement_constraints`: monster movement modes and `SV_FlyMove()`
  clip-plane capacity constants.
- `server_group_filter`: group-mask predicates used by world clipping and
  impact callbacks.
- `server_world_link_policy`: area-node split and traversal policy used by
  trigger, water, and collision walks.
- `game_dll_visibility_trace_policy`: game-DLL trace callback admission,
  hull clamping, fallback, and result-code policy.

Together these cover the pure decision islands. They do not make live hull
tracing, BSP traversal, physics callbacks, or edict mutation safe to move yet.

## Fixture Requirements

Before routing exact traces or physics loops through C++ ownership, we need
fixtures in several layers.

### Trace fixtures

- Synthetic `edict_t` snapshots for `solid`, `movetype`, flags, owner,
  groupinfo, `rendermode`, `absmin/absmax`, `mins/maxs`, origin, angles,
  model index, and custom-solid state.
- Area-node/list fixtures that can prove solid, portal, and world-brush
  traversal order against `SV_ClipToLinks()`, `SV_ClipToPortals()`, and
  `SV_ClipToWorldBrush()`.
- Hull fixtures for box, brush, studio hitbox, custom-solid, rotated BSP,
  portal, and monsterclip cases.
- Golden `SV_Move()` inputs with start/end, mins/maxs, type,
  ignore-transparent flag, monsterclip flag, pass entity, and expected trace
  fraction, startsolid, allsolid, endpos, plane, and trace entity.
- Callback mocks for `pfnShouldCollide()`, physics `ClipMoveToEntity()`,
  physics `pfnGetEntityHull()`, and trace-global writes.

### Physics fixtures

- Entity motion fixtures for toss, bounce, fly, flymissile, step, push,
  pushstep, noclip, follow, compound, and invalid walk dispatch.
- Pusher fixtures with a pushed-entity restore stack, blocked callback order,
  rotation, linear motion, non-solid pusher behavior, and "standing on pusher"
  cases.
- Velocity fixtures for NaN cleanup, max velocity clamping, gravity, friction,
  basevelocity, conveyors, static friction, and bounce backoff.
- Water fixtures for contents transitions, drowning/lava/slime timing,
  water-entry/exit sounds, and buoyancy.
- Monster movement fixtures for bottom checks, partial-ground compatibility,
  swim/fly chase vertical adjustment, direct versus chase-direction routes,
  and `SV_MoveNoEnts()` world-only movement.
- Game DLL callback fixtures for think, touch, blocked, free-on-think,
  free-on-touch, `force_retouch`, and physics-extension override behavior.

### Fixture source options

The best next fixture path is a synthetic harness first, then a tiny golden map
or generated BSP only when exact hull traversal needs real model data. A live
Half-Life install is useful for smoke tests, but it should not be the only
source of deterministic unit fixtures.

## Deferred Boundaries

Do not route these yet:

- `SV_ClipMoveToEntity()`, `SV_CustomClipMoveToEntity()`, `SV_PortalCSG()`,
  `SV_ClipToEntity()`, `SV_Move()`, or `SV_MoveNoEnts()`;
- `SV_FlyMove()`, `SV_PushMove()`, `SV_PushRotate()`, `SV_Physics_Toss()`,
  `SV_Physics_Step()`, or `SV_Physics_Pusher()`;
- `SV_CheckBottom()`, `SV_MoveStep()`, `SV_MoveTest()`, or
  `SV_NewChaseDir()`;
- the external physics API table or any callback object lifetime.

The current migration should continue with small plain-value helpers at the
edges, but live collision and physics loops need fixture evidence before they
can safely leave the legacy files.

## Validation

Phase 132 validation:

- `.\waf.bat build --targets=test_engine_server_physics_routing_policy`
  passed.
- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 128/128 tests.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.475 seconds and stopped with reason `command`.
