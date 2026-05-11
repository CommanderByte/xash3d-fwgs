# Server Group Filter Policy

Phase 107 extracts the repeated `GROUP_OP_AND` / `GROUP_OP_NAND` group-filter
predicate into a target-neutral helper while leaving live entity traversal and
side effects in legacy code.

## Legacy Behavior

`server.h` defines:

- `GROUP_OP_AND`: entities pass only when their group masks share at least one
  bit.
- `GROUP_OP_NAND`: entities pass only when their group masks share no bits.

Unknown group operations currently behave permissively because the old code only
checks the two known constants. The modern helper preserves that behavior.

There are two compatibility shapes:

| Shape | Legacy pattern | Modern helper |
| --- | --- | --- |
| Entity pair | Filter only when both entity `groupinfo` masks are non-zero. | `EntityPairPassesGroupFilter()` |
| Active mask | Filter when the candidate entity mask is non-zero, comparing it to `svs.groupmask` or another active mask that may be zero. | `EntityPassesGroupMask()` |

The distinction matters. Pair filtering treats a missing mask on either entity
as "no filter." Active-mask filtering can reject under `GROUP_OP_AND` when the
candidate has a mask and the active mask is zero.

## Route-Through Scope

Phase 107 routes only the repeated predicate through
`engine/server/server_group_filter_adapter.*`.

Routed callers:

- `sv_game.c`: multicast recipient group checks and server event playback group
  checks.
- `sv_world.c`: trigger touch, point-content mask filtering, and clip entity
  filtering.
- `sv_phys.c`: `SV_Impact()` touch callback admission.
- `sv_pmove.c`: player-move visible entity admission.

Legacy-owned concerns remain in place:

- edict validity and iteration;
- `svs.groupop` and `svs.groupmask` storage;
- collision, trace, PVS/PHS, and point-content ownership;
- multicast/event payload writes;
- save/restore `svs.groupmask` setup.

## Modern Contract

The pure helper lives in:

- `src/include/engine/server/server_group_filter.hpp`
- `src/engine/server/server_group_filter.cpp`

The tests cover:

- operation mapping for AND, NAND, and unknown operations;
- raw bit sharing;
- entity-pair inactive behavior when either mask is zero;
- active-mask behavior with a zero active mask;
- permissive unknown-operation behavior.

## Follow-Up

This helper is a small enabler for later event playback and world/trace phases.
It should not grow into a broad collision owner. When event playback starts,
use this policy as one input to a larger recipient/admission plan.
