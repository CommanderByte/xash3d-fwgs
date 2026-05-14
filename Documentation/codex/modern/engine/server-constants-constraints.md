# Server Constants And Constraints

Phase 101 creates a modern server-limits contract without replacing legacy C
macros. The goal is to give later route-through phases a stable vocabulary and
tests before touching live server storage, protocol, or gameplay behavior.

## Modern Contract

`src/include/engine/server/server_limits.hpp` mirrors selected server-only
constants as typed C++ values and classifies each constraint by role:

- `AbiLayout`: affects struct size, array capacity, or public/private storage
  layout. Mirror and test first; do not replace the legacy macro casually.
- `NetworkProtocol`: affects packet, challenge, or wire-facing behavior.
  Route only with protocol tests.
- `SaveFormat`: affects savegame layout or version compatibility.
  Route only with save-format fixtures.
- `Gameplay`: affects game simulation or game DLL observable behavior.
  Route only with behavioral tests and runtime smoke.
- `PrivateImplementation`: affects internal server scheduling or flags.
  Usually routeable once tests cover the relevant owner.

The contract also exposes small helpers for:

- `ServerEntityLeafCapacity(extendedLeafs)`, mirroring `MAX_ENT_LEAFS(ext)`;
- `ServerUpdateMask(updateBackup)`, mirroring `SV_UPDATE_MASK` arithmetic;
- descriptor lookup and role naming for audit/test output.

## Mirrored Values

| Legacy source | Modern name | Role | Route-through note |
| --- | --- | --- | --- |
| `SVF_SKIPLOCALHOST` | `kServerHostFlagSkipLocalhost` | Gameplay | Routeable with multicast/visibility tests. |
| `SVF_MERGE_VISIBILITY` | `kServerHostFlagMergeVisibility` | Gameplay | Routeable with visibility tests. |
| `MAP_IS_EXIST` | `kServerMapExists` | Gameplay | Routeable in map validation helpers. |
| `MAP_HAS_LANDMARK` | `kServerMapHasLandmark` | Gameplay | Routeable in changelevel helpers. |
| `MAP_INVALID_VERSION` | `kServerMapInvalidVersion` | Gameplay | Routeable after map-validation users agree on bit positions. |
| `SV_SPAWN_TIME` | `kServerSpawnTimeSeconds` | Gameplay | Routeable in lifecycle tests. |
| `GROUP_OP_AND` / `GROUP_OP_NAND` | `kServerGroupOpAnd` / `kServerGroupOpNand` | Gameplay | Routeable only with entity group filtering tests. |
| `MAX_PUSHED_ENTS` | `kServerMaxPushedEntities` | ABI/layout | Mirror only until `svgame.pushed` ownership moves. |
| `MAX_VIEWENTS` | `kServerMaxViewEntities` | ABI/layout | Mirror only until `sv_client_t::viewentity` ownership moves. |
| `MAX_LOCALINFO_STRING` | `kServerMaxLocalInfoString` | ABI/layout | Mirror only while `svs.localinfo` remains a C array. |
| `MAX_ENT_LEAFS_32` / `MAX_ENT_LEAFS_16` | `kServerMaxEntLeafs32` / `kServerMaxEntLeafs16` | ABI/layout | Mirror only while `edict_t` owns fixed leaf arrays. |
| `FCL_*` client flags | `kServerClientFlag*` | Private implementation | Routeable one owner at a time after flag-specific tests. |
| `CHALLENGE_WINDOW_SECONDS` | `kServerChallengeWindowSeconds` | Network protocol | Phase 102 candidate. |
| `MOVE_NORMAL` / `MOVE_STRAFE` in `sv_move.c` | `kServerMoveNormal` / `kServerMoveStrafe` | Gameplay | Phase 104 candidate. |
| `MOVE_EPSILON` | `kServerMoveEpsilon` | Gameplay | Phase 104 candidate. |
| Server `MAX_CLIP_PLANES` | `kServerMaxClipPlanes` | Gameplay | Phase 104 candidate; distinct from GL and pm_shared names. |

## Layout-Sensitive Values

These values are intentionally **not** route-through targets in Phase 101:

- `MAX_PUSHED_ENTS`, because it sizes `svgame.pushed`;
- `MAX_VIEWENTS`, because it sizes `sv_client_t::viewentity`;
- `MAX_LOCALINFO_STRING`, because it sizes `svs.localinfo`;
- `MAX_ENT_LEAFS_32` and `MAX_ENT_LEAFS_16`, because they size `edict_t`
  leaf arrays and therefore affect edict storage.

They can be referenced by modern tests and helper decisions, but the legacy
macros remain the layout source until a later ownership phase deliberately
changes the relevant storage.

## Test Strategy

`tests/engine/server_limits.cpp` uses a tiny legacy-value snapshot instead of
including `server.h`. The live server header pulls in model/runtime internals,
which is too heavy for a pure target-neutral test. The snapshot is intentionally
small and names the legacy source files it mirrors:

- `engine/server/server.h`;
- `engine/edict.h`;
- `engine/server/sv_client.c`;
- `engine/server/sv_move.c`;
- `engine/server/sv_phys.c`.

This gives us drift detection for the modern contract while keeping the test
decoupled from live server globals. If a future phase changes a legacy
constant, the snapshot and modern value should be updated together with an
explicit compatibility note.
