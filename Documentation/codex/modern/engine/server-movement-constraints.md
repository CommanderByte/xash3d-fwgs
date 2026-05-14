# Server Movement Constraint Constants

Phase 104 makes server movement constants visible in the modern server helper
layer without moving live physics ownership.

## Legacy Baseline

The server has several similarly named movement constants in different
domains:

- `engine/server/sv_move.c` had private monster movement mode values:
  `MOVE_NORMAL == 0` and `MOVE_STRAFE == 1`. These are inputs to
  `SV_MoveToOrigin()` and describe whether monster movement follows ideal yaw
  or a direct vector.
- `engine/common/world.h` also defines `MOVE_NORMAL == 0`, but that value is
  a trace collision mode for `SV_Move()`, not a monster movement mode.
- `common/const.h` defines `WALKMOVE_NORMAL == 0` for game-DLL walkmove
  callbacks. It is adjacent but not the same contract.
- `engine/server/sv_phys.c` defines `MAX_CLIP_PLANES == 5` for
  `SV_FlyMove()` stack storage and clipping loops.
- `pm_shared/pm_defs.h` also defines `MAX_CLIP_PLANES == 5` for player
  movement. The numeric value matches today, but server physics should not
  assume player-movement ownership.
- `engine/server/sv_phys.c` defines `MOVE_EPSILON == 0.01f`; this is distinct
  from `common/com_model.h` `DIST_EPSILON == 1.0f / 32.0f`.

## Modern Boundary

The modern helper owns:

- explicitly named server monster movement type classification;
- server fly-move clip-plane capacity helper logic;
- a snapshot of the movement constants for tests and future debug output.

Legacy code still owns:

- `SV_MoveStep()`, `SV_MoveTest()`, and `SV_MoveToOrigin()` movement execution;
- `SV_FlyMove()` clipping, stack arrays, and loop bodies;
- `SV_Move()` world collision and hull traces;
- player movement and `pm_shared` constants.

## Route-Through

`engine/server/server_movement_constraints_adapter.*` routes the
`SV_MoveToOrigin()` monster movement mode test through the modern helper.

`sv_phys.c` keeps its local `MAX_CLIP_PLANES` macro because it is used for a
stack array dimension and loop bounds inside `SV_FlyMove()`. Moving that safely
belongs with a future physics-loop migration, not this constants pass.

## Test Coverage

`tests/engine/server_movement_constraints.cpp` covers:

- legacy server monster movement mode values;
- monster movement type classification helpers;
- fly-move clip-plane capacity and iteration limits;
- movement-constraint snapshot values;
- the fact that adjacent constants with matching numbers are not automatically
  the same domain contract.
