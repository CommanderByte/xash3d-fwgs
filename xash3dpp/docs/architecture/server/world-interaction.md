# World Interaction

> **Defined in**: `private/server/world_links.hpp`, `world_trace.hpp`,
> `world_hooks.hpp`, `lightstyles.hpp` / `src/server/world/*.cpp`\
> **Namespace**: `xash::server`

## Overview

World interaction is the spatial-query layer: it composes the **edict-free**
`map_loader` trace kernel over the edict store to answer the questions physics,
snapshots, and the game DLL ask — "what is here?", "what does this box hit if it
moves from A to B?", "what triggers does this entity touch?", "how bright is this
point?". The map/world itself is owned by `map_loader` (Q-6); this module holds
borrowed pointers into it plus its own **areanode** spatial index. All entvars
access goes through `EntityView` (Q-20); edict identity stays `edict_t*`.

Two per-call environment structs carry the state the walks need (lifecycle owns
them): `LinkEnv` (world + worldspawn + `playersonly`) for linking, and `MoveEnv`
(world + `IModelResolver` + area root + hooks + group mask + the quake/pusher
feature toggles) for tracing and contents.

______________________________________________________________________

## Areanodes and linking

**Header**: `world_links.hpp` · **Source**: `world/links.cpp`

`WorldLinks` builds a fixed depth-4 BSP-of-space over the world bounds
(`server_area_nodes` = 32 slots). Each `AreaNode` splits on the **longer of X/Y**
(never Z) and holds **three** intrusive edict lists (an Xash extension over
Quake's two): triggers, solids, portals. Membership is the intrusive
`edict_t::area` link (an edict *header* field — the linking contract this module
owns; entvars stay behind `EntityView`). `edict_from_area` is the container-of
back-cast.

### Key operations

- `clear_world(mins, maxs)` — reset and rebuild the tree over the world bounds
  (the areanode half of `SV_ClearWorld`).
- `link_edict(ent, touch_triggers, env)` — insert into the right list by solid
  type; when `touch_triggers`, additionally walk the trigger lists dispatching
  `pfnTouch` (recursion-guarded by the legacy `iTouchLinkSemaphore`). Also
  refreshes the leaf cache via `find_touched_leafs`.
- `unlink_edict(ent)` (static) — no-op when not linked; NULLs both link pointers.
- `set_hooks(IWorldLinkHooks*)` / `set_group_op(GroupOp)` — install the game
  callbacks and the group-mask filter policy.

`IWorldLinkHooks` is the game-DLL seam the linker fires: `set_abs_box`
(`pfnSetAbsBox` — absmin/absmax expansion is the **game's** job, the engine has
no fallback), `dispatch_touch` (`pfnTouch`), and `brush_trigger_intersects` (the
exact BSP-hull refinement after the AABB pass). `GameWorldHooks`
(`world_hooks.cpp`) is the lifecycle-owned implementation that routes these to
the real DLL and the trace kernel; it is bound to the game + `MoveEnv` and
cleared on deactivate so a stale world is never refined against.

______________________________________________________________________

## Trace composition

**Header**: `world_trace.hpp` · **Source**: `world/clip.cpp`, `world/hulls.cpp`

The heart of the module: sweep a box through the world and every relevant
entity, returning the nearest impact as an `SvTrace` (kernel `TraceResult` + hit
`edict_t*` + studio hitgroup).

### Hull selection (`world/hulls.cpp`)

- `hull_for_bsp_entity(env, ent, mins, maxs)` — size-based hull select (Quake vs
  HL rules, the hull-0 verbatim-`clip_mins` point quirk) + origin offset.
  Returns `nullopt` on the legacy `Host_Error` conditions (non-brush model),
  which the bridge maps to the host error policy.
- `hull_for_entity(env, ent, mins, maxs, box_storage)` — BSP/portal solids route
  to `hull_for_bsp_entity` (with the `MOVETYPE_PUSH`/`PUSHSTEP` guard);
  everything else builds a Minkowski box into caller-owned storage.

### Clipping (`world/clip.cpp`)

- `clip_move_to_entity(env, ent, start, mins, maxs, end)` — hull select (studio →
  bbox fallback until Chunk 7), rotated-brush frames (including the
  `ENGINE_PHYSICS_PUSHER_EXT` `transform_bbox` path with its sign-dependent
  offset quirk), kernel invocation, endpos lerp + plane recompute, hit-entity
  stamp. Backs the game-facing `pfnTraceHull`.
- `move(env, start, mins, maxs, end, type, passedict, monsterclip)` — `SV_Move`:
  **world clip first**, then (when fraction != 0) the entity pass over the
  areanode solid+portal lists against the world-clipped segment, with the
  **fraction-compose quirk** (entity fraction × world fraction — entity fractions
  are relative to the world-clipped segment). `type` low byte is `MOVE_*`, high
  byte is ignore-transparent. The clip-links walk's filter order is observable
  (owner symmetry, monsterclip, ignoretrans, trigger-in-solid = fatal, allsolid
  aborts).
- `move_no_ents(...)` — `SV_MoveNoEnts`: the entity pass is replaced by
  `FL_WORLDBRUSH` `SOLID_BSP` ents only.

`IClipHooks` supplies the game/physics seams consulted mid-walk:
`should_collide` (`pfnShouldCollide` — absent hook collides), `sphere_cull`
(`SV_CheckSphereIntersection` — needs studio extradata, passes when absent), and
`custom_clip` (`SOLID_CUSTOM` → physics-interface clip; missing-hook default is
a no-hit trace with allsolid cleared).

`world_transform_aabb` / `transform_positive_plane` (exposed for tests) are the
rotation-only corner sweep and plane transform the rotated-brush path uses.

______________________________________________________________________

## Point contents

**Source**: `world/contents.cpp`

- `true_point_contents(env, p)` — `SV_TruePointContents`: world hull-0 contents
  merged with `SOLID_NOT` water bmodels from the areanode solid lists (highest
  `rank_for_contents` wins; rotational water supported).
- `point_contents(env, p)` — `SV_PointContents`: the above with `CURRENT_*`
  folded to `CONTENTS_WATER`.
- `rank_for_contents(contents)` — the `world.h` priority table
  (water < slime < lava < …).
- `brush_trigger_intersects(env, trigger, ent)` — `SV_TouchLinks`' exact
  refinement: force the BSP hull at the **toucher's** size (rotated triggers via
  `MODEL_HAS_ORIGIN`) and test whether the toucher's origin sits in the trigger's
  solid hull. Backs `GameWorldHooks::brush_trigger_intersects`.

**Water-brush quirk** (parity-critical): water volumes are `SOLID_NOT` brushes
whose skin is a contents value *below* `CONTENTS_EMPTY` (skin < −1). They **do**
link into solid lists for contents composition; `SOLID_NOT` entities are skipped
only when `skin >= CONTENTS_EMPTY`. Coding this as `skin < 0` would diverge for
skin == −1.

______________________________________________________________________

## Lightstyles

**Header**: `lightstyles.hpp` · **Source**: `world/light.cpp`

`LightStyles` is the 256-entry animation table (`server_lightstyles`; FWGS
raised the protocol limit from 64). `reset()` sets every style to full value
(256) at time 0 (part of `SV_ClearWorld`). `set(style, pattern, time)` stores an
`'a'`-relative map (returns `false` on an out-of-range index — hardening over the
legacy unchecked index). `run_frame(frametime)` advances every style's clock and
resolves its animated value (consumed by `GetEntityIllum`). The
`svc_lightstyle` broadcast on `set` belongs to the message layer (S9) — the
caller broadcasts after `set()` when the server is active.

`light_for_entity(ed)` (`SV_LightForEntity`) returns −1 for invalid edicts and
255 for `EF_FULLBRIGHT`.

______________________________________________________________________

## Threading model

Main-thread only (OQ-9). `link_edict`, `move`, `set_abs_box`, and the lightstyle
mutators all self-assert `ThreadRole::Main`. The areanode tree, the box-hull
scratch, the touch-link semaphore, and the fat PVS/PHS buffers are single-thread
assumptions — correct precisely because every entry runs on Main. The game
callbacks the walks fire (`pfnSetAbsBox`, `pfnTouch`) inherit that context
transitively (they run inside the asserted engine call). See
[docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md).

## Error handling

Non-brush hull requests return `nullopt` (logged, mapped to the host error
policy at the bridge — a Known Deviation from legacy's direct `Host_Error`).
`link_edict` tolerates already-linked / unlinked edicts. Missing game hooks fall
back to their documented safe defaults (collide, pass, no-hit). No exceptions.

## Edge cases and invariants

- Areanode splits are X/Y only, depth 4; three lists per node.
- The `SV_Move` fraction-compose (entity × world) is load-bearing parity.
- `SOLID_NOT` water linking uses `skin < CONTENTS_EMPTY`, not `skin < 0`.
- Hull selection thresholds are 8/36/36 (HL) vs 3/32 (Quake maps) with the
  point-hull offset asymmetry.
- `light_for_entity` currently always hits the no-lightdata branch (→ 255,
  including for players) because `map_loader` does not load `LUMP_LIGHTING` yet —
  an `XASH3DPP-STUB(chunk6)` tracked follow-up, matching legacy behaviour on
  unlit maps. Real lightmap sampling waits on a map_loader lighting-lump
  extension (Chunk 7).

## See also

- [physics-and-pmove.md](./physics-and-pmove.md) — the pmove `PM_*` traces reuse
  the same kernel with physent-sourced hulls
- [abi-bridge.md](./abi-bridge.md) — `pfnTraceHull` / `pfnPointContents` /
  `pfnSetView` slots that call this surface
- `docs/legacy-survey/deep-dive-server-world-frame.md`
- `docs/legacy-survey/deep-dive-trace-pvs.md`
