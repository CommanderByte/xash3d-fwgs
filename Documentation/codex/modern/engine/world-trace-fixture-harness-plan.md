# World Trace Fixture Harness Plan

Phase: 144

## Purpose

`sv_world.c` owns exact world linking, trigger touch, point contents, entity
clipping, portal clipping, BSP hull traversal, and `SV_Move()` trace mutation.
Those paths are too entangled with `edict_t`, `model_t`, `areanode_t`,
`sv_areanodes`, `svgame`, physics callbacks, and BSP hull functions to move
directly.

This phase defines a fixture strategy first. The goal is to make future tests
describe world/trace behavior with plain data before any more runtime movement.

## Legacy Runtime Areas

| Area | Legacy owner | Fixture target |
| --- | --- | --- |
| Area tree creation | `SV_CreateAreaNode()` / `SV_ClearWorld()` | World bounds, split axis, split distance, child boxes. |
| Entity linking | `SV_LinkEdict()` / `SV_UnlinkEdict()` | Edict abs bounds, solid kind, trigger/portal/solid list selection, child selection. |
| Trigger touch | `SV_TouchLinks()` | Moving edict, trigger candidate, bounds intersection, group filter, touch callback presence. |
| Water/contents links | `SV_WaterLinks()` / `SV_PointContents()` | Query point, grouped brushes, contents aggregation. |
| Clip admission | `SV_ClipToEntity()` / `SV_ClipToLinks()` | Swept bounds, passedict, candidate bounds, solid kind, group filter, owner/filter flags. |
| Exact entity clip | `SV_ClipMoveToEntity()` / `SV_CustomClipMoveToEntity()` | Deferred until hull/model fixtures exist. |
| Portal clipping | `SV_PortalCSG()` / `SV_ClipToPortals()` | Deferred until portal plane fixtures exist. |
| Full move | `SV_Move()` / `SV_MoveNoEnts()` / `SV_MoveToss()` | Deferred until world hull and entity-clip fixtures exist. |

## Fixture Inputs

The reusable plain-data fixture shape should cover:

- `WorldFixtureBounds`: mins/maxs for world bounds, edict bounds, swept move
  bounds, and query volumes.
- `WorldFixtureAreaNode`: split axis and split distance derived from world
  bounds through the modern world-link helper.
- `WorldFixtureEdict`: abs bounds, group info, free/solid/trigger flags, and
  touch callback presence.
- `WorldFixtureMove`: swept bounds, passed-entity validity, and passed-entity
  group info.

This is enough to test:

- area-node split policy;
- link child vs parent selection;
- traversal mask selection for touch and clip recursion;
- trigger-touch admission before brush hull precision;
- clip-candidate admission before exact hull tracing;
- group-filter interaction with touch and clip admission.

## Harness Placement

Use modern C++ tests first.

The first fixture skeleton lives in:

- `tests/engine/world_trace_fixture_common.hpp`
- `tests/engine/world_trace_fixtures.cpp`

The skeleton intentionally avoids `server.h`, `edict_t`, `model_t`,
`areanode_t`, `trace_t`, and live globals. It only includes modern target-
neutral helpers:

- `engine/server/server_world_link_policy.hpp`
- `engine/server/server_group_filter.hpp`

Legacy C tests should come later only when the behavior must compare directly
against `sv_world.c` runtime side effects. A shared fixture harness can be
introduced later if both modern C++ and legacy C tests need the same generated
inputs.

## Deferred Runtime Ownership

Keep these legacy-owned for now:

- BSP hull traversal and `PM_RecursiveHullCheck()`;
- exact `trace_t` and `pmtrace_t` mutation;
- `sv_areanodes` storage and linked-list ownership;
- `SV_Move()`, `SV_MoveNoEnts()`, `SV_MoveToss()`;
- `SV_ClipMoveToEntity()` and `SV_CustomClipMoveToEntity()`;
- portal plane clipping and `SV_PortalCSG()`;
- physics callback overrides such as `SV_TriggerTouch()` and
  `ClipMoveToEntity()`;
- `SV_CopyTraceToGlobal()` and `svgame.globals->trace_ent` mutation.

## Next Good Fixture Step

The next implementation step should not try to reproduce full `SV_Move()`.
Instead, add a pure clip-admission helper around the existing fixture facts:

1. classify candidate kind: world, solid, trigger, portal, custom, brush;
2. apply passedict/group/owner/filter rules;
3. decide which exact clip path would be called;
4. leave exact hull collision and trace combination to legacy code.

That gives us meaningful coverage without pretending a synthetic fixture can
validate BSP hull math.

## Validation

Focused validation target:

```text
.\waf.bat build --targets=test_engine_world_trace_fixtures,test_engine_server_world_link_policy,test_engine_server_group_filter
```
