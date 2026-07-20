# Physics and Pmove

> **Defined in**: server harness under `private/server/{physics,pmove}.hpp` /
> `src/server/physics/*.cpp`; shared trace API in `physics/pm_trace.hpp` /
> `src/physics/pm_trace.cpp`\
> **Namespaces**: `xash::server` (role owner), `xash::physics` (shared kernel)

## Overview

This layer runs the per-frame simulation. Two paths share the frame loop: the
**entity physics** path (`sv_physics` — the `MOVETYPE_*` dispatch, pushers,
gravity, thinks) and the **player-move bridge** (`sv_pmove` — the state bridge
between a client's edict and the frozen `playermove_t` the game DLL's `PM_Move`
mutates). Players simulate through usercmds via the pmove bridge, **not** the
entity loop.

Q-20: the physics code reads/writes entvars through `EntityView`; the pmove
bridge is the sanctioned **raw** `edict->v.` access exception (its state copy is
field-for-field with legacy).

______________________________________________________________________

## The fixed-step frame loop

**Header**: `physics.hpp` · **Source**: `physics/physics.cpp`, `movevars.cpp`

`host_server_frame(rt, host_frametime)` (`Host_ServerFrame`) is the per-host-frame
tick. It wires the S8 pieces — movevars refresh → run game frame → prep world
frame — and leaves the client/networking steps (`ReadPackets`,
`SendClientMessages`, timeouts, master heartbeat) as S9 seams the S9 slice
splices in.

`sv_run_game_frame(rt, sv_fps)` (`SV_RunGameFrame`) is the fixed-step
accumulator. On listen builds it uses the `sv_fps` residual accumulator with the
`1/(sv_fps - 0.01)` FP fudge (**kept**); dedicated builds have no `sv_fps` cvar
and always run **one** physics step per host frame at `host.frametime`. It
returns `false` when **zero** physics frames ran — the early-return quirk that
skips sends and the heartbeat that host frame (unreachable on dedicated, where
one step always runs).

`sv_physics(rt)` (`SV_Physics`): `StartFrame` → per-entity movetype dispatch
(skipping the client slots) → `force_retouch` decrement → lightstyle animation
→ `framecount++`. Runs one fixed step at `rt.level.frametime`.

`sv_update_movevars(rt, initialize)` (`SV_UpdateMovevars`, `physics/movevars.cpp`)
mirrors the `sv_*` physics cvars into `rt.movevars` and clamps `sv_zmax`.
`initialize` is the spawn-time fill; the non-initialize path also
delta-broadcasts changes to clients (an S9 seam here). `sv_prep_world_frame`
clears `EF_MUZZLEFLASH | EF_NOINTERP` on every live entity before the next
frame. `sv_is_simulating` returns `true` unconditionally on dedicated (the
listen-server freeze/pause/background logic is an OQ-4 client-hook seam).

Two helpers are exposed for the pmove run chain: `update_base_velocity`
(`SV_UpdateBaseVelocity` — the conveyor-belt momentum handshake) and `sv_impact`
(`SV_Impact` — dispatch both `pfnTouch` directions, group-mask gated, `SOLID_NOT`
suppressed).

**Physics parity contract**: the Quake-lineage constants and bugs are
behavioural — whole-vector maxvelocity clamp, ClipVelocity snap-to-zero at ±1.0,
4-bump `FlyMove`, `SV_AddGravity`'s basevelocity fold, the pusher `ltime` clock +
±3600 angle wrap, the chase-dir `215.0f` typo, drown `dmg<15→10`, friction
consumed-then-reset-to-1.0. The `pushed[256]` stack may be bound-checked + logged
by the rewrite, but must not change behaviour below the cap.

______________________________________________________________________

## The pmove bridge

**Header**: `pmove.hpp` · **Source**: `physics/pmove.cpp`, `run_cmd.cpp`,
`init_client_move.cpp`

The bridge marshals a client's edict state into the single `rt.pmove`
`playermove_t` (physents[600] etc., ~270 KB — pool-owned to keep `ServerRuntime`
off the stack), runs the game's `PM_Move`, and copies the mutated state back.

- `sv_init_client_move(rt)` (`SV_InitClientMove`) — allocate/init `rt.pmove`
  (server flag, movevars, hull-bounds table), install the ~30-entry `PM_*`
  callback table the DLL's `PM_Move` invokes, register `rt.pmove` +
  `player_bounds` on the engine bridge (so the context-free callbacks can reach
  them), and call the DLL's `pfnPM_Init`. Called from `load_progs`.
- `sv_setup_pmove(rt, cl, ucmd, physinfo)` (`SV_SetupPMove`) — copy the client
  edict's movement state into `rt.pmove`, then gather physents (solids),
  moveents (ladders), and visents from the areanode tree within a 256-unit cube
  around the player. Each successful append also snapshots its model index into
  the aligned role-owned sidecar; only each list's `num*` prefix is valid.
  **Pre**: `rt.pmove` allocated.
- `sv_finish_pmove(rt, cl)` (`SV_FinishPMove`) — copy the mutated state back:
  position/velocity/water/duck, `onground` → `FL_ONGROUND` + groundentity, the
  show-1/3-pitch body angles, and the usehull hull resize.
- `pm_clear_phys_ents(rt)` — drop the gathered physent/moveent/visent counts
  (called on deactivate; no-op when `rt.pmove` is unallocated).

`sv_run_cmd(rt, cl, ucmd, random_seed)` (`SV_RunCmd`, `run_cmd.cpp`) drives one
usercmd through the full chain: the speed-hack clock → the `msec > 50`
split-recurse → `pfnCmdStart` → `PM_CheckMovingGround` → viewangle latch →
`pfnPlayerPreThink` → `SV_PlayerRunThink` → `SetupPMove` → `pfnPM_Move` →
`FinishPMove` → touch dispatch (deltavelocity → `SV_Impact`) →
`pfnPlayerPostThink` → `pfnCmdEnd`. It drives **both** the real-client path and
the `pfnRunPlayerMove` bot path (reachable through the bridge `runtime`
back-pointer). `random_seed` seeds the DLL's shared RNG for the command.

______________________________________________________________________

## The `PM_*` trace family

**Header**: `xash3dpp/physics/pm_trace.hpp` · **Source**:
`src/physics/pm_trace.cpp` · **Target**: `xash3dpp_physics`

The physent-list analogue of the world composition
([world-interaction.md](./world-interaction.md)): it reuses the **same**
edict-free `map_loader` kernel and the **same** rotated-brush transforms exposed
from `clip.cpp`, but sources its hulls from the gathered `physents[]`
(usehull-indexed player bounds) rather than the areanode edict store, and merges
the nearest fraction across the list, recording the winning physent index in
`pmtrace_t::ent`. `PmTraceEnv` mirrors `MoveEnv`, sourced from the physent list
(world + resolver + aligned model-index sidecars + player-hull table +
pusher-ext toggle). It has no arena or server-private dependency.

The family: `pm_player_trace_ext` (the main hull sweep), `pm_test_player_position`
(point-in-solid), `pm_trace_model` (single-entity BSP sweep forcing usehull 2),
`pm_trace_line{,_ex}` (usehull-swapping traceline over the physent/visent lists),
`pm_true_point_contents` / `pm_point_contents` / `pm_point_contents_pmove` (the
contents trio, with `CURRENT_*` fold), and `pm_stuck_touch` (dedup + append a
touch record). A `PmIgnore` filter callback the DLL may pass overrides the
`ignore_pe` index.

______________________________________________________________________

## Threading model

Main-thread only (OQ-9). `host_server_frame`, `sv_physics`, `sv_run_game_frame`,
`sv_update_movevars`, `sv_setup_pmove`, `sv_finish_pmove`, `sv_run_cmd`, and the
two usehull trace mutators all self-assert `ThreadRole::Main`. The single
`rt.pmove` working set, the physent gather buffers, and the `pushed[256]` stack
are single-thread scratch — safe because the whole frame runs on Main. The
`PM_*` callbacks the DLL invokes inherit the Main context transitively (they run
inside the asserted `sv_run_cmd`). The pmove callback uses the canonical
`EngineContext`-owned random stream; server physics owns no RNG static. The
remaining `init_client_move.cpp` `Info_ValueForKey` static buffer is a
Race-static-buf shape contained by OQ-9. See
[docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md).

## Error handling

No exceptions. The pmove callbacks degrade to clear-trace / no-op defaults when
their bridge pointers are null (pre-`load_progs` fixtures). `sv_run_game_frame`'s
`false` return is a control signal (zero frames ran), not an error.

## Edge cases and invariants

- Dedicated always runs one physics step per host frame; the `sv_fps`
  accumulator and its early-return quirk are listen-only.
- Players simulate via usercmds through `sv_run_cmd`, never the entity loop.
- The pmove bridge is the sanctioned raw-entvars access site (field-for-field
  with legacy).
- The MP rules force `onground = -1`; `waterjumptime` ↔ `teleport_time` aliasing
  and pitch = −v_angle/3 on copy-back are exact.
- **Deferred (documented, not broken):** P5 lag-compensation
  (`SV_SetupMoveInterpolant` / `SV_RestoreMoveInterpolant`) is a no-op in
  `run_cmd.cpp` — the milestone gathers un-interpolated positions; pmove
  *parity* ticks at S15/Chunk 11 (needs a real `PM_Move`). The group-(c)
  surface/texture trace family (`PM_TraceSurface`/`PM_TraceTexture`) and studio
  hitbox hulls are stubbed at the callback table (Chunk 7 / OQ-2), exactly as
  the world path falls back to the bbox hull. `PM_PlaySound` → `SV_StartSound`
  waits on Chunk 9. `SV_CheckCmdTimes` (the speed-hack clock producer) is an S9
  stub, so `ignorecmdtime` stays 0 and the guard is dormant — the fields are
  ported now so `sv_run_cmd` is byte-faithful. The `sv_move.c` locomotion family
  (`WalkMove`/`MoveToOrigin`/`CheckBottom`/`MoveToss`) was explicitly deferred at
  S8.

## See also

- [world-interaction.md](./world-interaction.md) — the shared trace kernel + the
  areanode tree the physent gather walks
- [abi-bridge.md](./abi-bridge.md) — the `playermove_t` callback table + the
  bridge `pmove`/`runtime` pointers
- [clients-and-messaging.md](./clients-and-messaging.md) — `execute_client_message`
  → `SV_ParseClientMove` feeds `sv_run_cmd`
- `docs/legacy-survey/deep-dive-server-physics.md`
