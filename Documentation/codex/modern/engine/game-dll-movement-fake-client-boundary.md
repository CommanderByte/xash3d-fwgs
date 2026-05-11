# Game DLL Movement And Fake-Client Boundary

Phase 99 models game-DLL-facing movement and fake-client callback decisions
without moving physics, player command execution, or live client mutation out
of the legacy server.

## Legacy Baseline

- `pfnMoveToOrigin()` returns when the goal pointer is null or the edict is
  invalid. `SV_MoveToOrigin()` then ignores entities without `FL_FLY`,
  `FL_SWIM`, or `FL_ONGROUND`. `MOVE_NORMAL` steps toward `ideal_yaw`; other
  move types build a goal vector and use vertical distance only for fly/swim
  entities.
- `pfnChangeYaw()` and `pfnChangePitch()` skip invalid edicts and otherwise
  use `SV_AngleMod()`. The helper normalizes the current angle with legacy
  fixed-point `anglemod`, chooses the shortest turn across the wrap boundary,
  clamps by yaw/pitch speed, and normalizes the result again.
- `pfnWalkMove()` skips invalid or immobile entities before checking the mode.
  Valid movable entities convert yaw to a horizontal move vector. Mode `0`
  calls `SV_MoveStep(..., true)`, mode `1` calls `SV_MoveTest(..., true)`,
  mode `2` calls `SV_MoveStep(..., false)`, and unknown modes are fatal only
  after the entity passed the early movement gates.
- `pfnSetOrigin()` skips invalid edicts, then copies the supplied origin and
  relinks with `SV_LinkEdict(e, false)`. The legacy callback does not validate
  a null origin pointer.
- `pfnSetClientMaxspeed()` accepts clients that are not fully spawned, clamps
  the requested value to `[-svgame.movevars.maxspeed, maxspeed]`, writes the
  rounded `"maxspd"` physics info value, and mirrors the clamped value onto
  `edict->v.maxspeed`.
- `SV_FakeConnect()` chooses `"Bot"` for empty names, initializes the legacy
  userinfo defaults, frees old frame storage, clears resource lists, zeros the
  client, marks it spawned, assigns an edict and user ID, sets
  `FCL_FAKECLIENT`, and marks the edict `FL_CLIENT|FL_FAKECLIENT`.
- `pfnRunPlayerMove()` accepts only spawned fake clients. It temporarily swaps
  `sv.current_client`, computes `timebase` as
  `(sv.time + sv.frametime) - msec / 1000.0`, builds a `usercmd_t` from the
  callback arguments, runs `SV_RunCmd()` with a random seed, stores
  `lastcmd`, and restores the old current client. The legacy callback does not
  validate a null `viewangles` pointer.

## Modern Boundary

`src/engine/server/game_dll/game_dll_movement_policy.cpp` owns pure plans for:

- `pfnMoveToOrigin()` admission, immobile-entity rejection, normal versus
  strafe route selection, and vertical-goal use;
- yaw/pitch angle normalization, shortest-path turn selection, and speed
  clamping;
- `pfnWalkMove()` early gates, mode routing, horizontal move-vector
  calculation, and fatal unknown-mode admission;
- `pfnSetOrigin()` invalid-edict gating;
- client maxspeed clamping and `"maxspd"` formatting;
- fake-client run-player-move admission, timebase calculation, and command
  argument snapshotting;
- empty fake-client name fallback to `"Bot"`.

The legacy server still owns:

- `SV_MoveToOrigin()`, `SV_StepDirection()`, `SV_NewChaseDir()`,
  `SV_FlyDirection()`, `SV_MoveStep()`, `SV_MoveTest()`, and world collision;
- `SV_LinkEdict()`, edict storage, entity flags, and live origin mutation;
- `SV_ClientFromEdict()`, `SV_FindEmptySlot()`, `SV_UserinfoChanged()`,
  resource-list cleanup, client flags, user IDs, and edict fake-client flags;
- `SV_RunCmd()`, `usercmd_t`, random seed generation, `sv.current_client`,
  `lastcmd`, and `playermove_t` state.

No route-through is added in Phase 99. The helper is a compatibility fixture
for future movement and fake-client bridge slices after physics and client
runtime ownership have clearer modern homes.
