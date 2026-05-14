# World Or PMove Micro-Extraction Pilot

Phase 157 extracts one fixture-backed world trace decision into modern C++:
the `SV_Move` clip setup plan.

## Extracted Decision

`BuildServerWorldMoveClipPlan` owns only plain-value decisions:

- decode the low byte of the encoded move type
- decode ignore-transparent bits from the high byte
- enable monster clipping only when requested and not in Quake-compatible mode
- select missile bounds when the decoded move type matches `MOVE_MISSILE`

`SV_Move` now calls this helper through
`SV_WorldTrace_BuildMoveClipPlan`. The adapter converts between the C ABI
shape and the modern value object.

## Legacy-Owned Runtime

The extraction deliberately leaves runtime work in `sv_world.c`:

- initial world trace
- hull traversal
- link traversal
- portal clipping
- trace fraction adjustment
- trace entity/global updates
- vector copying and bound construction

`SV_MoveNoEnts` remains unchanged because its legacy behavior copies the input
mins/maxs rather than applying missile-specific bounds.

## Follow-Up

The next world/PMove moves should stay in this style: extract narrow decisions
only when fixtures already describe the behavior, then let legacy code keep
the live state until the surrounding ownership boundary is clearer.
