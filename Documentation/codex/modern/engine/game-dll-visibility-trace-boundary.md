# Game DLL Visibility And Trace Boundary

Phase 98 models the pure decisions around game-DLL-facing trace and visibility
callbacks while leaving collision, BSP, PVS/PAS, and leaf ownership in the
legacy world/server code.

## Legacy Baseline

- `pfnTraceLine()` calls `SV_Move()` with zero hull extents. If the hit entity
  is not valid, it replaces the hit with the world edict before converting to
  `TraceResult`.
- `pfnTraceToss()` returns immediately for invalid toss entities.
- `pfnTraceHull()` clamps hull numbers outside `0..3` back to hull `0`.
- `pfnTraceMonsterHull()` returns `0` for invalid entities, passes the entity
  mins/maxs to `SV_Move()`, enables monsterclip when the entity has
  `FL_MONSTERCLIP`, and returns true only when the trace is all-solid or has a
  fraction other than `1.0`.
- `pfnTraceModel()` also clamps hull numbers. Invalid entities skip the trace.
  `SOLID_CUSTOM` always uses custom clipping; brush models temporarily force
  `MOVETYPE_PUSH` and `SOLID_BSP`; other entities use normal entity clipping.
- `pfnTraceTexture()` returns null for invalid entities and otherwise delegates
  to `SV_TraceTexture()`.
- `pfnSetFatPVS()` and `pfnSetFatPAS()` force full visibility when the world
  lacks visdata, `sv_novis` is active, no origin is supplied, or client
  visibility is disabled. They merge visibility when `SVF_MERGE_VISIBILITY` is
  set.
- `pfnCheckVisibility()` returns `0` for invalid entities and `1` when the
  visibility set is null. Custom entities owned by clients are checked through
  their owner. Negative `headnode` values use the entity leaf list directly;
  non-negative headnodes check cached leafs first, then recurse through the
  BSP headnode and return `2` when visibility is proven through that slower
  path.
- `pfnFindClientInPVS()` returns the world edict for invalid viewers, refreshes
  the cached client PVS after `0.1` seconds or negative time deltas, and merges
  portal-camera PVS only for monsters.
- `pfnCanSkipPlayer()` is true only for real clients with
  `FCL_LOCAL_WEAPONS`.

## Modern Boundary

`src/engine/server/game_dll/game_dll_visibility_trace_policy.cpp` owns pure plans for:

- invalid hit-entity fallback to world;
- invalid trace-call admission;
- hull number clamping;
- monsterclip admission and monster-hull hit result conversion;
- trace-model route choice between custom, temporary brush, and normal entity
  clipping;
- trace-texture null fallback;
- fat PVS/PAS fullvis and merge flags;
- visibility-check action selection and leaf/headnode result codes;
- client-PVS cache refresh admission and monster portal-merge admission;
- local-weapons skip-player decisions.

The legacy server still owns:

- `SV_Move()`, `SV_MoveToss()`, `SV_ClipMoveToEntity()`,
  `SV_CustomClipMoveToEntity()`, and `SV_TraceTexture()`;
- `Mod_FatPVS()`, `Mod_GetPVSForPoint()`, `Mod_PointInLeaf()`,
  `Mod_HeadnodeVisible()`, and BSP leaf traversal;
- `edict_t` leaf arrays, `trace_t`, `TraceResult`, and global trace fields;
- `SV_ClientFromEdict()`, portal-camera PVS merging, and client visibility
  state.

Phase 98 therefore adds compatibility fixtures only. Route-through should wait
for a broader world/trace phase that can move collision and visibility state
without changing game DLL behavior.
