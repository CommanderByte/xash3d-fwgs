# Server World Link And Touch Boundary

Phase 131 audits the entity-area link and trigger-touch portions of
`engine/server/sv_world.c`. This area owns live spatial storage, edict links,
trigger callbacks, water brushes, group filters, and collision traversal, so
the safe modernization seam is only the plain area-node routing policy.

## Legacy Owners

| Area | Legacy ownership reason |
| --- | --- |
| Area tree storage | `sv_areanodes[]`, `sv_numareanodes`, `ClearLink()`, `InsertLinkBefore()`, and `RemoveLink()` own linked-list mutation and fixed storage. |
| World clearing | `SV_ClearWorld()` initializes box hulls, lightstyles, touch semaphore state, and rebuilds the area tree from the live worldmodel bounds. |
| Link/unlink | `SV_LinkEdict()` calls the game DLL `pfnSetAbsBox()`, handles follow entities, updates PVS leaf caches, chooses trigger/portal/solid lists, and may trigger touch callbacks. |
| Trigger touch | `SV_TouchLinks()` iterates trigger lists, supports physics extension overrides, applies group filters, checks bounds, tests brush trigger hulls, respects `sv.playersonly`, and calls `pfnTouch()`. |
| Water links | `SV_WaterLinks()` iterates solid-edict links for special contents brushes, applies group filters, tests hull contents, and ranks water/current contents. |
| Collision traversal | `SV_ClipToLinks()`, `SV_ClipToPortals()`, and `SV_ClipToWorldBrush()` iterate live edicts and call trace/collision helpers. Full collision ownership belongs to later world/physics phases. |

Existing helpers already cover some inputs:

- `server_group_filter`: entity pair and active-mask group filtering.
- `server_visibility_constraints`: leaf capacity, overflow markers, and
  cached visibility leaf behavior.

## Modern Helper

Phase 131 adds:

- `src/include/engine/server/server_world_link_policy.hpp`
- `src/engine/server/server_world_link_policy.cpp`
- `engine/server/server_world_link_policy_adapter.*`

The helper owns only plain area-node routing decisions:

- choose area split axis from X/Y world extents;
- compute split distance as the midpoint;
- decide whether an entity bounds can descend to child 0, child 1, or must stop
  at the current node while linking;
- build a strict child traversal mask for trigger, water, and collision walks.

## Compatibility Notes

- Split-axis selection preserves the legacy tie behavior: equal X/Y sizes pick
  axis `1`.
- Link insertion descends only when bounds are strictly on one side:
  `min > dist` selects child `0`, `max < dist` selects child `1`, and touching
  or crossing the plane stops at the current node.
- Recursive traversal uses the old strict checks:
  `max > dist` descends child `0`, `min < dist` descends child `1`. A point
  exactly on the split plane descends neither side.

Those edge cases look odd, but they are part of the legacy spatial behavior and
are now covered by focused tests.

## Fixture Needs

Do not route more of this area until fixtures exist for:

- `edict_t` link list membership and unlink/relink ordering;
- trigger brush hull tests, including rotated `MODEL_HAS_ORIGIN` brushes;
- physics extension `SV_TriggerTouch()` override behavior;
- `pfnTouch()` callback ordering and `sv.playersonly` suppression;
- water brush content ranking with overlapping brushes;
- area tree collision traversal with solid, portal, and world-brush lists.

These require either synthetic edicts plus model/hull fixtures or a generated
mini-map fixture. Plain policy tests are not enough for the callbacks and
linked-list mutation.

## Validation

Phase 131 validation:

- `.\waf.bat build --targets=test_engine_server_world_link_policy` passed.
- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 127/127 tests.
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.485 seconds and stopped with reason `command`.
