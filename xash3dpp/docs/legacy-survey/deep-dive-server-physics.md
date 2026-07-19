# Deep dive: server physics — `sv_phys.c`, `sv_move.c`, `sv_pmove.c`

*Chunk 6 recon, 2026-07-04. Narrow-and-exact behavioural reference for the
xash3dpp server rewrite. All paths under `engine/server/` unless noted;
line numbers verified against the current tree.*

> **Refreshed 2026-07-06 (as-built cross-ref).** Shipped in
> `src/server/physics/`: `physics.cpp` (MOVETYPE dispatch + pushers),
> `pmove.cpp` + `init_client_move.cpp` + `pm_trace.cpp` + `run_cmd.cpp` (the
> pmove bridge over the single frozen `playermove_t` + its ~30 callbacks), and
> `movevars.cpp`. The Quake-lineage constants/bugs (ClipVelocity ±1.0 snap,
> `215.0f` chase-dir typo, friction reset-to-1.0, `pushed[256]` cap) are
> behavioural contract — preserved as-built; see the frozen-ABI prohibition in
> `docs/modernization-opportunities/server-modernization.md`. Studio-hitbox
> trace (OQ-2) stays a Chunk 7 `TODO`.

## 1. Responsibility

- **sv_phys.c** (2165 lines) — per-frame server physics driver:
  `SV_Physics()` frame loop, MOVETYPE_* dispatch (`SV_Physics_Entity`),
  pushmove (`SV_PushMove`/`SV_PushRotate`/`SV_Physics_Pusher`),
  toss/bounce/step simulation, gravity/velocity clamping, water
  level/transition, plus the physics-interface bootstrap
  (`SV_InitPhysicsAPI`, `gPhysicsAPI` export table) and its small pfn
  implementations.
- **sv_move.c** (550 lines) — Quake-derived monster locomotion:
  `SV_CheckBottom` ledge test, `SV_MoveStep`/`SV_MoveTest` discrete step
  moves, `SV_StepDirection`/`SV_NewChaseDir`/`SV_MoveToOrigin` chase AI
  steering, `SV_WaterMove` (drown/lava/slime damage + water friction),
  `SV_VecToYaw`.
- **sv_pmove.c** (1015 lines) — server side of player movement: builds
  `playermove_t` from the client edict (`SV_SetupPMove`,
  physent/moveent/visent collection), invokes the **game DLL's**
  `pfnPM_Move`, copies results back (`SV_FinishPMove`), dispatches touch
  impacts, lag compensation rewind/restore (`SV_SetupMoveInterpolant`/
  `SV_RestoreMoveInterpolant`), and the `SV_RunCmd` usercmd pipeline.

## 2. Physics dispatch

**Frame flow** — `SV_Physics()` (sv_phys.c:1812–1854), called from
`SV_RunGameFrame` (sv_main.c:618 inside the fixed-`sv_fps` residual loop
with `sv.frametime = 1/(sv_fps-0.01)`, :629 for variable frametime; also
once from sv_init.c:619 at activation):

1. `SV_CheckAllEnts()` — 5-second-interval sanity sweep, gated on
   `sv_check_errors` (:68–113).
2. `svgame.globals->time = sv.time`; `pfnStartFrame()` game-DLL callback
   (:1819–1822).
3. Loop `i = 0 .. svgame.numEntities-1`: **skips indices 1..maxclients
   (players)** but includes worldspawn (i=0); calls
   `SV_Physics_Entity(ent)` (:1825–1836). Players are simulated only via
   `SV_RunCmd` (cmd-driven, out of band with this loop).
4. `force_retouch--` if nonzero (:1838).
5. `svgame.physFuncs.SV_EndFrame()` hook if present (:1841).
6. `SV_RunLightStyles()`; `sv.framecount++` (:1845–1848). A disabled
   `#if 0` block for shrinking numEntities notes "memory corruption"
   (:1850–1853).

**Per-entity dispatch** — `SV_Physics_Entity` (:1722–1782):

- **Hook**: `svgame.physFuncs.SV_PhysicsEntity(ent)` — if game DLL returns
  nonzero, engine physics is skipped entirely (:1725).
- `SV_UpdateBaseVelocity` (conveyor: groundentity FL_CONVEYOR →
  basevelocity = movedir*speed, additive if FL_BASEVELOCITY already set;
  :162–184).
- If FL_BASEVELOCITY not set but basevelocity nonzero:
  `velocity += basevelocity * (1 + frametime*0.5)` then clear basevelocity
  (momentum apply, :1730–1735). FL_BASEVELOCITY always cleared after
  (:1737).
- `force_retouch != 0` → `SV_LinkEdict(ent, true)` even for stationary
  ents (:1739–1743).
- Dispatch table (:1745–1776): NONE→`SV_Physics_None` (think only);
  NOCLIP→`SV_Physics_Noclip` (think, CheckWater, origin+=vel*dt,
  angles+=avel*dt, link **without** trigger touch);
  FOLLOW→`SV_Physics_Follow` (origin = aiment.origin + **v_angle** used as
  offset, angles = aiment.angles; invalid aiment ⇒ demote to
  MOVETYPE_NONE, :1226–1245); COMPOUND→`SV_Physics_Compound`
  (matrix-glued child; `oldorigin`/`avelocity` abused as
  parent-pos/angles cache, `ltime` as init flag; only follows
  PUSH/PUSHSTEP parents; solid forced SOLID_NOT unless SOLID_TRIGGER;
  quake inverse-pitch compensation unless ENGINE_COMPENSATE_QUAKE_BUG,
  :1254–1325); STEP & **PUSHSTEP**→`SV_Physics_Step`;
  FLY/TOSS/BOUNCE/FLYMISSILE/BOUNCEMISSILE→`SV_Physics_Toss`;
  PUSH→`SV_Physics_Pusher`; **WALK→`Host_Error` "bad movetype"**
  (:1773–1775, players never come through here).
- After dispatch: FL_KILLME → `SV_FreeEdict` only when
  `sv.state == ss_active` (baseline-corruption guard, :1778–1781).

**Think ordering**: `SV_RunThink` (:228–251) runs **before movement** in
Follow/Compound/Noclip/Toss (Toss runs `SV_CheckWater` first at :1445,
then think), but **after movement** in `SV_Physics_Step` (:1704). Think
fires if `0 < nextthink <= sv.time + frametime`; nextthink clamped up to
sv.time; nextthink zeroed before `pfnThink`; FL_KILLME after think →
free. Pushers use exact-time think instead (below).

**Pusher** — `SV_Physics_Pusher` (:1152–1216):

- `movetime = min(frametime, nextthink - ltime)` clamped ≥0 (:1163–1168) —
  pushers move on **ltime (local time)**, not sv.time.
- avelocity && velocity → `SV_PushRotate` first; if unblocked, ltime
  rolled back and `SV_PushMove` run for the same window, keeping max
  ltime (:1172–1187). Only avel → PushRotate; only vel → PushMove.
- Blocker ⇒ `pfnBlocked(pusher, blocker)` (:1202).
- Angles outside ±3600 wrapped by `fmod(angles, 3600)` (:1204–1208) — note
  3600, not 360.
- Think when `thinktime > oldltime` and (`FL_ALWAYSTHINK` or
  `thinktime <= ltime`); nextthink=0, time=sv.time, `pfnThink`
  (:1210–1215).

**SV_PushMove** (:899–1006): skips if `svgame.globals->changelevel` or
zero velocity (ltime still advanced). Moves pusher linearly, links, then
scans **all** entities 1..numEntities: skips via `SV_CanPushed`
(NONE/PUSH/FOLLOW/NOCLIP/COMPOUND immune, :855–868); entity already
in-solid vs pusher (tested with pusher temporarily SOLID_NOT) is skipped;
moved if standing on pusher (`groundentity == pusher` + FL_ONGROUND) or
bbox intersects swept bounds and `SV_TestEntityPosition` says inside final
pos. Non-WALK movers get FL_ONGROUND cleared (:970–971). Moved via
`SV_PushEntity` with pusher SOLID_NOT. If still stuck **and** blocked:
`SV_CanBlock` filter (point-sized ents can't block; SOLID_NOT/TRIGGER get
their bounds zeroed as "deadbody" fix and can't block, :877–891), then
**ltime rolled back** and all pushed ents restored in reverse order from
`svgame.pushed[]` (:990–1001). Saved/restored state is origin+angles
(+fixangle in the rotate variant).

**SV_PushRotate** (:1014–1144): same skeleton with matrix transform
(`start_l`/`end_l` from angles/origin); STEP/PUSHSTEP movers are
transformed about their **absbox center**, others about origin
(:1096–1098); non-WALK: FL_ONGROUND cleared only if `lmove[2] != 0`, and
downward lmove zeroed when pusher has no dmg ("let's the free falling",
:1104–1110); blocked non-WALK ents get FL_ONGROUND cleared (:1118–1119);
fixangle saved/restored on rollback (:1092, 1137).

**SV_PushEntity** (:793–846): trace type MOVE_MISSILE for FLYMISSILE,
MOVE_NOMONSTERS for SOLID_TRIGGER/SOLID_NOT, else MOVE_NORMAL. On any
movement: if `apush[YAW]` and ent is client → `avelocity[1] += apush[1]`,
`fixangle = 2` (:816–820); yaw rotation applied only if
`SV_AllowPushRotate` (non-brush always; brush needs
ENGINE_PHYSICS_PUSHER_EXT **and** MODEL_HAS_ORIGIN, :768–784). Blocked
determination: WALK/STEP/PUSHSTEP →
`!VectorCompareEpsilon(origin, end, ON_EPSILON)` (ON_EPSILON=0.1,
public/xash3d_mathlib.h:72); all other movetypes → `*blocked = true`
unconditionally (:829–839). Always runs `SV_Impact` with trace.ent
afterwards.

**Gravity** — `SV_AddGravity` (:737–752): `vel.z -= ent_gravity *
sv_gravity * frametime; vel.z += basevelocity.z * frametime;
basevelocity.z = 0` — comment literally says "add gravity incorrectly".
`ent->v.gravity` is a *multiplier* (0 ⇒ 1.0).

**Toss** (:1438–1564): CheckWater→think→ground invalidation (vel.z>0, or
groundentity invalid/FL_MONSTER/FL_CLIENT clears FL_ONGROUND :1452–1456);
at rest early-out clears avelocity (:1459–1465); gravity for all but
FLY/FLYMISSILE/BOUNCEMISSILE; angular friction only for TOSS/BOUNCE (uses
`ent->v.friction`); basevelocity added for the move then subtracted
(:1496–1501, with the comment noting it's not properly accounted across
the bounce); `SV_PushEntity` for the move; allsolid → zero both
velocities; backoff 2.0−friction (BOUNCE), 2.0 (BOUNCEMISSILE), 1.0 else;
landing on normal.z>0.7: `vel.z < sv_gravity*frametime` ⇒ ground + vel.z=0
(:1538–1544); `dot(vel+basevel) < 900` or non-bounce ⇒ full stop
(:1546–1552); else re-push with move scaled `(1-fraction)*frametime*0.9`
(:1555–1557). Ends with `SV_CheckWaterTransition` (splash sounds via
SoundList `PlayerWaterEnter/Exit`, entry halves vel.z, :1361–1429).

**Step** (:1584–1707): `SV_WaterMove` then `SV_CheckVelocity`; FL_FLOAT
buoyancy `SV_Submerged(ent) * ent->v.skin * frametime` added after gravity
(:1601–1607; `skin` = buoyancy ratio, `SV_Submerged` recursive 5-deep
bisection :403–451); airborne gravity unless FL_FLY, or FL_SWIM with
waterlevel>0, or inwater (:1609–1619). If moving: clears FL_ONGROUND;
ground/mover friction when `wasonground||wasonmover` and (alive or
`SV_CheckBottom`): 2D speed, `friction = sv_friction * ent->v.friction`,
then **`ent->v.friction = 1.0f` reset** (g-cont "???" comment, :1636),
mover friction ×0.5 (:1637), `control = max(speed, sv_stopspeed)`,
`newspeed = speed − dt*control*friction` (:1625–1646). Then basevel add →
`SV_FlyMove` → basevel subtract; ground re-detect by 4 corner **point
traces** at `mins[2]-1` setting FL_ONGROUND/groundentity/friction=1 on
`startsolid` (:1659–1684); `SV_LinkEdict(ent, true)`. If stationary and
`force_retouch`: self-trace + `SV_Impact` ("hentacle impact code",
:1690–1701). Think **after** move, then `SV_CheckWaterTransition`.

**SV_FlyMove** (:592–729): classic Quake multi-plane clip,
`MAX_CLIP_PLANES 5` but loop runs `bumpcount < MAX_CLIP_PLANES-1` = 4
bumps; allsolid → vel cleared, return 4; normal.z>0.7 ⇒ blocked|=1 and
ground set only if hit ent is SOLID_BSP/SOLID_SLIDEBOX/MOVETYPE_PUSHSTEP/
FL_CLIENT (:646–651); normal.z==0 ⇒ blocked|=2 + steptrace out;
`SV_Impact` per bump; 2-plane crease slide via cross product;
velocity-vs-primal dot ≤0 ⇒ dead stop (:718); `allFraction == 0` after all
bumps ⇒ vel cleared (:725–726).

**SV_ClipVelocity** (:549–571): overbounce backoff; **per-component
snap-to-zero when |out[i]| < 1.0** (:566–567) — note this is 1.0, not
Quake's STOP_EPSILON 0.1.

**Water interaction** — `SV_CheckWater` (:458–517): 3-level probe
(absmin+1, mid, mid+view_ofs); point entities (absmin.z==absmax.z) jump
straight to waterlevel 3; Quake2 CONTENTS_CURRENT_* adds
`150 * waterlevel/3` along `current_table[]` to **basevelocity**
(:506–513); returns `waterlevel > 1`.

**physFuncs override points in this file**: `SV_PlayerThink` (:267),
`SV_PhysicsEntity` (:1725), `SV_EndFrame` (:1841),
`DrawNormalTriangles`/`DrawDebugTriangles` (:1916–1931),
`DrawOrthoTriangles` (:1952), `SV_CheckFeatures` (:2147).

## 3. Monster movement (sv_move.c)

Game-DLL surface (via sv_game.c enginefuncs):

- `pfnWalkMove` (sv_game.c:1903–1928) — requires FL_FLY|FL_SWIM|
  FL_ONGROUND; yaw→XY move; WALKMOVE_NORMAL→`SV_MoveStep(relink=true)`,
  WALKMOVE_WORLDONLY→`SV_MoveTest(relink=true)`,
  WALKMOVE_CHECKONLY→`SV_MoveStep(relink=false)`, unknown mode→
  `Host_Error`. Constants in common/const.h:64–66.
- `pfnMoveToOrigin` (sv_game.c:1443–1449) → `SV_MoveToOrigin`.
- `pfnEntIsOnFloor` (sv_game.c:1854–1860) → `SV_CheckBottom(e,
  MOVE_NORMAL)`.
- `pfnDropToFloor` lives in **sv_game.c:1868–1895** (not sv_move.c):
  traces origin −256 down, allsolid→−1, no hit→0, else snap origin +
  FL_ONGROUND + groundentity → 1.
- `pfnVecToYaw` (sv_game.c:1432) → `SV_VecToYaw` (sv_move.c:210–226; yaw
  truncated to **int** degrees, :222).

Internals:

- `SV_CheckBottom(ent, iMode)` (sv_move.c:34–104): easy accept if all 4
  bbox corners at `mins.z−1` are CONTENTS_SOLID; real check traces from
  `mins.z (+sv_stepsize unless ENGINE_QUAKE_COMPATIBLE, :68–69)` down
  `2*sv_stepsize` at center; fail if fraction==1; each corner must land
  within `sv_stepsize` of center depth (:97–100). WALKMOVE_WORLDONLY uses
  `SV_MoveNoEnts`, else `SV_Move` with MOVE_NOMONSTERS + monsterclip flag.
- `SV_MoveStep(ent, move, relink)` (:230–340): FL_SWIM|FL_FLY path — two
  attempts, first with vertical enemy tracking (`dz > 40 ⇒ −8`,
  `dz < 30 ⇒ +8`, :254–257); a clear FL_SWIM move ending in CONTENTS_EMPTY
  is rejected (won't leave water, :268). Ground path — step up
  `sv_stepsize`, trace down `2*stepsize`; `startsolid` retries from
  un-raised origin (:295–301); full-fraction (over a ledge) succeeds only
  with FL_PARTIALGROUND (move applied blindly, FL_ONGROUND cleared,
  :306–312); otherwise land at endpos, then `SV_CheckBottom` — failure
  with FL_PARTIALGROUND accepts staying, else origin reverted (:319–329);
  success clears FL_PARTIALGROUND and sets groundentity (:332–334).
- `SV_MoveTest` (:342–406): identical ground path using `SV_MoveNoEnts` +
  `SV_CheckBottom(WALKMOVE_WORLDONLY)`.
- `SV_StepDirection` (:408–422): yaw→vector, MoveStep(relink=false) then
  always `SV_LinkEdict(ent, true)`.
- `SV_NewChaseDir` (:434–518): Quake 45°-quantized direction picker;
  **classic Quake bug preserved: diagonal `215.0f` instead of 225**
  (:463); d[1]/d[2] index usage from Quake; random axis-swap; turnaround
  avoided until last; total failure sets FL_PARTIALGROUND if
  `!SV_CheckBottom` (:514–517).
- `SV_MoveToOrigin` (:520–549): gated on FL_FLY|FL_SWIM|FL_ONGROUND;
  iMoveType==MOVE_NORMAL(0) → StepDirection(ideal_yaw) else NewChaseDir;
  otherwise (MOVE_STRAFE) goal-relative normalized dir, z zeroed for
  ground monsters. Note sv_move.c redefines `MOVE_NORMAL 0`/
  `MOVE_STRAFE 1` locally (:22–23) — these are *walk-mode* semantics,
  numerically colliding with the trace-type MOVE_NORMAL.
- `SV_WaterMove` (:106–201, called from `SV_Physics_Step`): NOCLIP just
  refreshes air_finished; dead monsters skipped; drowning
  `drownlevel = deadflag==DEAD_NO ? 3 : 1`, dmg += 2 with the **quake1
  original `if (dmg < 15) dmg = 10`** quirk (:134–137); lava dmgtime 0.2s
  (1.0s with radsuit), slime 1.0s (immune with radsuit); FL_INWATER
  enter/exit sounds (SoundList EntityWaterEnter/Exit); water friction
  `vel *= 1 + waterlevel * −0.8 * frametime` unless FL_WATERJUMP
  (:197–200).

## 4. Player move bridge (sv_pmove.c)

**Ownership**: the `playermove_t` object is a **static in the engine**
(`gpMove`, sv_game.c:5223, `svgame.pmove = &gpMove` :5231). The engine
fills the pmove **function table** with engine implementations (PM_* trace
code lives in `engine/common/pm_trace.c`, `pm_surface.c` — `Pmove_Init`,
`PM_PlayerTraceExt`, `PM_HullForBsp`, etc.). The actual movement
simulation is the **game DLL's**: `pfnPM_Init`/`pfnPM_Move`/
`pfnPM_FindTextureType` are DLL_FUNCTIONS exports (engine/eiface.h:
461–463) — game DLLs statically link pm_shared. Repo-root `pm_shared/`
holds only the frozen headers. Hull dims come from game DLL
`pfnGetHullBounds` (eiface.h:479) into `host.player_mins/maxs[4]`
(sv_pmove.c:453–462).

**SV_InitClientMove** (:442–498): `Pmove_Init()`; `pmove->server = true`,
`movevars = &svgame.movevars`, `runfuncs = false`; enumerates
MAX_MAP_HULLS hulls; wires 30 function pointers (Particle→
reliable-datagram svc_particle :324–343; PlaySound → `SV_StartSound` with
SND_FILTER_CLIENT :390–398; PlaybackEventFull → forced `FEV_NOTHOST`
"GoldSrc always sets" :400–414; the trace family delegates to PM_* with
`svgame.pmove`); finally `pfnPM_Init(svgame.pmove)`.

**SV_RunCmd(cl, ucmd, random_seed)** (:887–1014) — called from
`SV_ParseClientMove` (sv_client.c:3389/3396/3403) and from
`pfnRunPlayerMove` for fakeclients (sv_game.c:3823–3855, which synthesizes
timebase `(sv.time+frametime)−msec/1000`):

1. Kicked (`state <= cs_zombie`) → return.
2. **Speedhack guard**: `cl->ignorecmdtime > host.realtime` ⇒ warn once,
   count warns, optional kick after `sv_speedhack_kick` (default 10)
   warns, accumulate cmdtime, drop cmd (:904–919).
3. **Split moves**: `cmd.msec > 50` → recurse twice with msec/2 each,
   `impulse` zeroed on the second half (:925–934).
4. Non-fakeclient: `SV_SetupMoveInterpolant(cl)` — lag-compensation
   rewind (below).
5. `pfnCmdStart(edict, ucmd, random_seed)` — the prediction seed handed to
   the game DLL (:939).
6. `frametime = msec/1000`; `cl->timebase += frametime`;
   `cl->cmdtime += frametime` (:941–943).
7. `PM_CheckMovingGround` (:500–519): physFuncs.
   `SV_UpdatePlayerBaseVelocity` hook or `SV_UpdateBaseVelocity`; momentum
   apply `vel += basevel * (1 + dt*0.5)` when FL_BASEVELOCITY unset;
   clears the flag.
8. Angles: `pmove->oldangles = v_angle`; `if (!fixangle) v_angle =
   ucmd->viewangles` (:947–948). `clbasevelocity` cleared, then re-set
   from basevelocity after think (:950, 962–963).
9. `v.button = buttons`, `v.light_level = lightlevel`, `impulse` only if
   nonzero (:953–955).
10. `globals->time = timebase`; `pfnPlayerPreThink`;
    `SV_PlayerRunThink(clent, frametime, timebase)` (sv_phys.c:263–290 —
    physFuncs.SV_PlayerThink override; player variant runs against passed
    time, skips FL_DORMANT, and **clears** FL_KILLME instead of freeing).
11. `SV_SetupPMove` (below); `pfnPM_Move(svgame.pmove, true)` (:969);
    `SV_FinishPMove` (below).
12. **Touch dispatch** (if `solid != SOLID_NOT && !sv.playersonly`,
    :974–1000): physFuncs.`PM_PlayerTouch` override, else
    `SV_LinkEdict(clent, true)` (trigger touch), save velocity, then for
    each `pmove->touchindex[i]`: velocity temporarily set to
    `pmtrace->deltavelocity`, `PM_ConvertTrace`, `SV_Impact(touch, clent,
    &trace)` (touched entity is e1 → its Touch fires first), velocity
    restored.
13. `numtouch = 0`; `globals->time/frametime` restored;
    `pfnPlayerPostThink` (weapon think happens inside game DLL here);
    `pfnCmdEnd` (:1002–1008); non-fakeclient `SV_RestoreMoveInterpolant`.

**SV_SetupPMove** (:521–597): `globals->frametime = msec*0.001`;
`player_index = entnum−1`; `multiplayer = maxclients>1`; **`pmove->time =
timebase * 1000`** (float ms); copies origin/v_angle(→angles &
oldangles)/velocity/basevelocity/view_ofs/movedir, duck state, step-sound
state, fall velocity, swim time, punchangle, effects/flags/gravity/
friction/oldbuttons; `waterjumptime = v.teleport_time` (:552); `dead =
health<=0`; `spectator = 0` (comment: spectator physics runs client-side);
`usehull = FL_DUCKING ? 1 : 0`; **multiplayer forces `onground = -1`**
(:557); `maxspeed = svgame.movevars.maxspeed` (comment questions GoldSrc),
`clientmaxspeed = v.maxspeed`; iuser/fuser/vuser 1–4; `cmd = *ucmd`;
**`runfuncs = true`**; physinfo string copied. Physent collection: world
always physents[0]/visents[0]; gather box = origin ±256 (:584–588);
`SV_AddLinksToPmove(sv_areanodes, …)` then `SV_AddLaddersToPmove`.

**SV_AddLinksToPmove** (:190–275) selection rules, in order: groupinfo
AND/NAND filter; skip if `owner == player` or SOLID_TRIGGER; **visents**
gets everything surviving so far (cap MAX_PHYSENTS=600); then for
physents: skip SOLID_NOT with `skin == CONTENTS_NONE || modelindex == 0`;
skip FL_MONSTERCLIP+SOLID_BSP brushes; skip self; skip dead bodies
(client/fakeclient with health≤0, or `deadflag == DEAD_DEAD`) unless
MOVETYPE_PUSH ("nehahra collision flags"); skip zero-size; bounds test
uses **unlag-interpolated mins/maxs for real clients**
(`SV_GetTrueMinMax`); cap MAX_PHYSENTS. `SV_CopyEdictToPhysEnt` (:42–134):
fails (entity dropped) when modelindex has no model; clients get name
"player"/"bot" and `pe->player = pe->info` and **unlag-true origin**
(`SV_GetTrueOrigin`); SOLID_NOT/SOLID_BSP → `pe->model`, zero mins/maxs;
SOLID_BBOX → studiomodel only if STUDIO_TRACE_HITBOX flag; SOLID_CUSTOM →
model if brush, studiomodel if studio; default → studiomodel if studio;
copies rendermode/skin/frame/sequence/controller[4]/blending[2]/movetype/
takedamage/team/playerclass→classnumber, `blooddecal = 0` ("unused in
GoldSrc"), all user fields.

**SV_AddLaddersToPmove** (:282–322): SOLID_NOT + `skin == CONTENTS_LADDER` +
brush model only → `moveents` (cap MAX_MOVEENTS=64, hard `return` at
cap).

**SV_FinishPMove** (:599–664): copies back origin/view_ofs/velocity/
basevelocity/punchangle/movedir, `teleport_time = waterjumptime`,
step/fall/swim state, `oldbuttons = pmove->cmd.buttons`, water fields,
`maxspeed = clientmaxspeed`, duck state, movetype/friction/deadflag/
effects/**flags wholesale**, user fields. Groundentity: `onground == -1` ⇒
clear FL_ONGROUND; `0 <= onground < numphysent` ⇒ set + `groundentity =
physents[onground].info` edict (:640–648). Angles (only when
`!fixangle`): `v_angle = pmove->angles`; `angles[PITCH] =
−v_angle[PITCH]/3`; roll/yaw copied ("show 1/3 the pitch angle",
:652–658). `SV_SetMinMaxSize(clent, host.player_mins/maxs[pmove->usehull],
false)` (:660). **`runfuncs = false`** afterwards ("all next calls ignore
footstep sounds", :663).

**Lag compensation** (`sv_unlag`, `sv_maxunlag` 0.5, `sv_unlagpush` 0.0):
`SV_ShouldUnlagForPlayer` (:136–154) — multiplayer +
`pfnAllowLagCompensation()` + sv_unlag + FCL_LAG_COMPENSATION + spawned.
`SV_SetupMoveInterpolant` (:693–844): snapshot all other clients'
origin/absbox into `svgame.interp[]`; `finalpush = realtime −
min(latency,1.5,sv_maxunlag) − lerp_msec + sv_unlagpush` (lerp_msec
clamped ≤0.1 and ≥next_messageinterval); walks frame history backwards
flagging `nointerp` on death/EF_NOINTERP/64-unit teleports (:681–691);
lerps between two frames and **physically moves other clients' edicts +
relinks** (:837–842). `SV_RestoreMoveInterpolant` (:846–880) puts them
back only if their current origin still matches the rewound `curpos`.
Static `has_update` guards restore-without-setup (:23, 852–855).

**SV_ParseClientMove** cmd-loop context (sv_client.c:3305–3415):
delta-decodes `numcmds+numbackup` cmds, CRC check, pause/frozen zeroes
movement, `net_drop` replay — `> numbackup` replays `cl->lastcmd`,
remainder replays backup cmds, main cmds executed newest-index-down
(`SV_RunCmd(cl, &cmds[i], incoming_sequence − i)` — that sequence delta is
the prediction random seed); half-msec ping adjust after.

## 5. External / intra surface

- `SV_Physics` ← sv_main.c:618, 629 (`SV_RunGameFrame`) and sv_init.c:619
  (activation warm-up).
- `SV_RunCmd` ← sv_client.c:3389/3396/3403 (`SV_ParseClientMove`) and
  sv_game.c:3851 (`pfnRunPlayerMove`, fakeclients only).
- From sv_phys.c, non-static: `SV_CheckVelocity`, `SV_UpdateBaseVelocity`
  (used by sv_pmove.c:508), `SV_PlayerRunThink` (sv_pmove.c:959),
  `SV_Impact` (sv_pmove.c:994), `SV_Physics`, `SV_DrawDebugTriangles`/
  `SV_DrawOrthoTriangles` (renderer debug), `SV_InitPhysicsAPI` (called at
  game DLL load).
- From sv_move.c: `SV_CheckBottom` (sv_phys.c:1625, sv_game.c:1859),
  `SV_WaterMove` (sv_phys.c:1594), `SV_VecToYaw` (sv_game.c:1434, also
  delta encoding), `SV_MoveStep`/`SV_MoveTest` (sv_game.c pfnWalkMove),
  `SV_MoveToOrigin` (sv_game.c pfnMoveToOrigin).
- From sv_pmove.c: `SV_InitClientMove` (game DLL init), `SV_RunCmd`,
  `SV_ClipPMoveToEntity` (non-static; called from engine/common/pm_trace.c
  for SOLID_CUSTOM physents — delegates to physFuncs.ClipPMoveToEntity
  else `tr->allsolid = false` :26–40).
- Game-DLL pfn callbacks living **elsewhere** (sv_game.c) but
  physics-relevant: `pfnWalkMove`, `pfnMoveToOrigin`, `pfnDropToFloor`,
  `pfnEntIsOnFloor`, `pfnSetOrigin`, `pfnChangeYaw`/`pfnChangePitch` (use
  `SV_AngleMod`), `pfnRunPlayerMove`, `pfnSetClientMaxspeed` (bounds to
  ±movevars.maxspeed + physinfo "maxspd", sv_game.c:3800–3815).
- Physics interface export: game DLL exports
  `Server_GetPhysicsInterface(version, &gPhysicsAPI, &svgame.physFuncs)`
  (sv_phys.c:2136–2164); on version mismatch physFuncs zeroed;
  `SV_CheckFeatures` feeds `Host_ValidateEngineFeatures`.

## 6. Owned state

- sv_phys.c: `current_table[6]` (:46), static `nextcheck` in
  `SV_CheckAllEnts` (:70), static `pPhysIface` (:2138), `gPhysicsAPI`
  table (:2086–2127). Uses `svgame.pushed[MAX_PUSHED_ENTS=256]`
  (server.h:59, 326 — "should be enough for any game situation"; **no
  overflow check** in PushMove/PushRotate).
- sv_pmove.c: static `has_update` (:23). Uses `svgame.pmove` (→ static
  `gpMove` in sv_game.c:5223), `svgame.interp[MAX_CLIENTS]`
  (server.h:325), `svgame.movevars`, `host.player_mins/maxs[4]`,
  `sv.current_client`.
- sv_move.c: no file statics. **No `c_yes`/`c_no` counters anywhere** —
  Quake's SV_movestep stats were dropped in this engine.
- Cvars consumed (defined sv_main.c): `sv_gravity` 800, `sv_stopspeed`
  100, `sv_friction` 4, `sv_stepsize` 18, `sv_maxvelocity` 2000,
  `sv_maxspeed` 320, `sv_check_errors` 0, `sv_unlag` 1, `sv_maxunlag` 0.5,
  `sv_unlagpush` 0.0, `sv_speedhack_kick` 10 (sv_main.c:27–30, 68, 86–98,
  139).

## 7. Dependencies

- **World/trace (sv_world.c)**: `SV_Move`, `SV_MoveNoEnts`,
  `SV_PointContents`, `SV_TruePointContents`, `SV_LinkEdict`,
  `sv_areanodes`, `SV_TraceSurface`, `SV_BoxInPVS`;
  `svs.groupmask`/`svs.groupop` group filtering set before nearly every
  PointContents.
- **pm engine helpers (engine/common/pm_trace.c, pm_surface.c)**:
  `Pmove_Init`, `PM_InitBoxHull`, `PM_TestPlayerPosition`,
  `PM_PlayerTraceExt`, `PM_TraceLine(Ex)`, `PM_HullForBsp`,
  `PM_TraceModel`, `PM_TraceTexture`, `PM_TraceSurfacePmove`,
  `PM_PointContentsPmove`, `PM_TruePointContents`, `PM_HullPointContents`,
  `PM_StuckTouch`, `PM_ConvertTrace`.
- **Game DLL (svgame.dllFuncs)**: `pfnThink`, `pfnTouch`, `pfnBlocked`,
  `pfnStartFrame`, `pfnPlayerPreThink`, `pfnPlayerPostThink`,
  `pfnCmdStart`, `pfnCmdEnd`, `pfnPM_Init`, `pfnPM_Move`,
  `pfnGetHullBounds`, `pfnAllowLagCompensation`.
- **Game DLL optional (svgame.physFuncs)**: full list in §2/§4 above.
- **Misc engine**: `SV_StartSound` + `SoundList_GetRandom` (water
  splashes), `SV_PlaybackEventFull`, `Matrix4x4_*` (PushRotate/Compound),
  `COM_RandomLong/Float`, `Platform_DoubleTime`, `SV_SetMinMaxSize`
  (sv_game.c), `SV_EdictNum`/`SV_IsValidEdict`/`SV_FreeEdict`, MSG_*
  (particle effect), `Host_Error`.

## 8. Quirks & invariants (rewrite-spec checklist)

1. `sv_maxvelocity` clamp is **whole-vector** against `maxvel² * 1.73`
   ("half-diagonal") then uniform rescale — not per-component
   (sv_phys.c:144–154). NaN velocity/origin components silently zeroed
   (:129–141).
2. `SV_ClipVelocity` snaps components in **(−1, 1)** to 0
   (sv_phys.c:566–567) — 10x Quake's STOP_EPSILON.
3. `SV_FlyMove` runs at most `MAX_CLIP_PLANES−1` = **4** bumps (:611)
   though planes array holds 5; ground only granted if hit ent is
   SOLID_BSP/SOLID_SLIDEBOX/MOVETYPE_PUSHSTEP/FL_CLIENT (:646–651);
   `allFraction == 0` total-block velocity wipe (:725).
4. `SV_AddGravity` folds `basevelocity.z` into velocity and zeroes it,
   comment "add gravity incorrectly" (:745–748). Frame-level momentum
   apply uses factor `1 + frametime*0.5` (:1733, sv_pmove.c:514).
5. Pusher angles wrapped at **±3600 via fmod 3600** (:1204–1208); pusher
   think keyed to `ltime` with FL_ALWAYSTHINK bypass (:1210);
   rotate+translate pushers run PushRotate then PushMove over the same
   window with ltime rollback bookkeeping (:1176–1187).
6. `svgame.pushed` capacity 256 with **no bounds check** while pushing
   (server.h:59; sv_phys.c:920–977).
7. `SV_CanBlock` **mutates** SOLID_NOT/TRIGGER blockers: zeroes their
   mins/maxs ("clear bounds for deadbody", :882–888).
8. `SV_PushEntity`: client yaw-push → `avelocity[1] += apush`,
   `fixangle = 2` (client-side blend) (:816–820); pushables never
   yaw-rotated unless ENGINE_PHYSICS_PUSHER_EXT + MODEL_HAS_ORIGIN brush
   (:768–784); non-monster movetypes report blocked unconditionally,
   monsters use ON_EPSILON=0.1 origin compare (:833–839).
9. `SV_PushRotate`: STEP/PUSHSTEP transformed about absbox center (:1096);
   downward push zeroed when pusher dmg==0 (:1108–1109); FL_ONGROUND
   cleared only when lmove.z != 0 for non-WALK (:1107).
10. `SV_TestEntityPosition` re-sizes client hulls (duck/stand) before
    testing to "avoid falling through tracktrain" (:198–204).
11. Toss rest condition: `vel.z < sv_gravity*frametime` on a >0.7 slope
    (:1538); bounce survives only if `|vel+basevel|² ≥ 900` (30 ups)
    (:1546); post-bounce slide scaled `(1−frac)*dt*0.9` (:1555);
    groundentity that is monster/client invalidates FL_ONGROUND every
    frame (:1455).
12. Step friction: `ent->v.friction` consumed then **reset to 1.0** each
    frame (:1636); mover-riders get friction x0.5 (:1637); dead monsters
    skip friction unless `SV_CheckBottom` passes (`health > 0 ||` check,
    :1625); ground re-acquired via 4 corner point-traces checking
    `startsolid` (:1664–1684).
13. FL_FLOAT buoyancy = `SV_Submerged * v.skin * dt` where skin doubles as
    density; `SV_Submerged` bisects water surface max 5 iterations
    (:403–416).
14. Quake2 water currents feed **basevelocity** at `150 * waterlevel / 3`
    (:507–513) — "probably never used in Half-Life".
15. Water enter/exit halves vel.z on entry and plays sounds only via
    SoundList (`SV_CheckWaterTransition`, :1386–1389); fresh-spawn
    watertype==0 path initializes waterlevel=1 regardless of contents
    (:1373–1379).
16. `SV_CheckBottom`: step-height offset added to the start **unless
    ENGINE_QUAKE_COMPATIBLE** (sv_move.c:68–69); corner tolerance =
    `sv_stepsize` from mid, probe depth `2*sv_stepsize`.
17. `SV_MoveStep` flyers: enemy-relative altitude servo (dz>40 ⇒ −8,
    dz<30 ⇒ +8) only on first attempt (:252–258); swimmers refuse to
    surface into CONTENTS_EMPTY (:268); FL_PARTIALGROUND lets monsters
    walk off ledges/skip bottom check (:306–312, 321–325).
18. `SV_NewChaseDir` keeps Quake's **`215.0f` typo** (should be 225)
    (:463) and the d[1]/d[2] indexing.
19. `SV_VecToYaw` truncates atan2 result to int degrees (:222).
20. `SV_WaterMove` keeps Quake's nonsense drown logic
    `dmg += 2; if (dmg < 15) dmg = 10` (:134–137) and the
    `waterlevel > drownlevel` swim inversion; water drag
    `1 − 0.8*waterlevel*dt` multiplier (:199).
21. `SV_RunThink` clamps past thinktimes to now; **players'** run-think
    clears FL_KILLME instead of freeing (sv_phys.c:286–287); FL_DORMANT
    blocks player think only (:270).
22. `SV_Physics` loop includes worldspawn (i=0) and skips 1..maxclients;
    entities freed for FL_KILLME **only when ss_active** (:1780).
23. MOVETYPE_WALK in the entity loop is a hard `Host_Error` (:1774).
24. Speedhack detection via `ignorecmdtime`/`sv_speedhack_kick`
    (sv_pmove.c:904–919); cmd splitting threshold **msec > 50**, halves,
    impulse suppressed on second half (:925–934).
25. `pmove->onground` forced −1 in multiplayer setup (:557); `pmove->time`
    is timebase in **ms as float** (:531); `maxspeed` = movevars.maxspeed
    vs `clientmaxspeed` = pev->maxspeed (:560–561), and copy-back writes
    `clientmaxspeed` into `v.maxspeed` (:615).
26. `waterjumptime` ↔ `v.teleport_time` aliasing (:552, 603).
27. Post-move angle rule: `angles.pitch = −v_angle.pitch/3`, skipped
    entirely under `fixangle` (:652–658).
28. Touch replay uses `pmtrace->deltavelocity` as temporary velocity per
    impact, restoring the real one after (:985–998); suppressed by
    `sv.playersonly` (:974).
29. physent gather radius fixed **±256** around player (:584–588); ladder
    list requires SOLID_NOT + skin==CONTENTS_LADDER + mod_brush
    (:295–301); MAX_PHYSENTS 600 / MAX_MOVEENTS 64 caps
    (pm_defs.h:19–20).
30. Unlag: latency capped 1.5s and `sv_maxunlag` (negative reset to 0);
    lerp_msec clamped to [next_messageinterval, 0.1]; teleport threshold
    64u/axis; frame too old (>1.0s) aborts (sv_pmove.c:725–784); restore
    only when nobody moved the edict meanwhile (:874–878).
31. No `sv_fix_*` cvars exist in this engine (grep negative) — the compat
    switches here are `host.features` flags: ENGINE_QUAKE_COMPATIBLE
    (CheckBottom), ENGINE_PHYSICS_PUSHER_EXT (push rotate),
    ENGINE_COMPENSATE_QUAKE_BUG (Compound pitch flip, sv_phys.c:1301,
    1315).
32. `SV_CheckAllEnts` self-heals `pContainingEntity` and trashed private
    data (:97–108).
33. Fixed-timestep loop: with `sv_fps` set, `sv.frametime =
    1/(sv_fps − 0.01)` ("FP issues" fudge, sv_main.c:611).

## 9. ABI touchpoints

- **`playermove_t`** (pm_shared/pm_defs.h:79–215) — frozen GoldSrc layout:
  state block (player_index…clientmaxspeed), user fields,
  `physents[600]`, `moveents[64]`, `visents[600]`, `cmd`,
  `touchindex[600]`, `physinfo[MAX_PHYSINFO_STRING]`, `movevars*`,
  `player_mins/maxs[4]`, then ~30 function pointers ending in
  Xash-specific `PM_TraceSurface` (:214 — the one extension past GoldSrc).
  `physent_t` (:37–77) also frozen. Struct is huge and statically
  allocated (`gpMove`).
- **Physics interface**: `SV_PHYSICS_INTERFACE_VERSION` **6**
  (engine/physint.h:21); `server_physics_api_t` (engine→DLL, :67–118) and
  `physics_interface_t` (DLL→engine, :121–173), both with "ONLY ADD NEW
  FUNCTIONS TO THE END, VERSION FROZEN AT 6" markers (:77, :134);
  handshake export name `Server_GetPhysicsInterface` (sv_phys.c:2140).
  FWGS-extended tail members (pfnGetNativeObject, pfnVoiceData, etc.).
- **DLL_FUNCTIONS** physics entries: `pfnPM_Move`/`pfnPM_Init`/
  `pfnPM_FindTextureType` (eiface.h:461–463), `pfnGetHullBounds` (:479),
  plus Think/Touch/Blocked/CmdStart/CmdEnd/PlayerPreThink/PlayerPostThink.
- **entvars fields with special physics meaning**: `ltime` (pusher local
  clock; Compound init flag), `teleport_time` (= pmove waterjumptime),
  `v_angle` (FOLLOW: origin *offset*; player: view angles), `oldorigin` +
  `avelocity` (COMPOUND: parent pos/angles cache), `skin` (buoyancy
  density on FL_FLOAT; CONTENTS_LADDER/CONTENTS_NONE marker on brushes),
  `dmg` (pusher crush damage gates free-fall zeroing; also drown damage
  accumulator), `friction` (bounce backoff & step friction, reset to 1
  per frame), `gravity` (multiplier, 0⇒1), `movedir`+`speed`
  (FL_CONVEYOR basevelocity), `basevelocity`/FL_BASEVELOCITY handshake,
  `fixangle` (2 = pusher yaw blend), `groundentity`/FL_ONGROUND, `flags`
  copied **wholesale** through pmove, `iuser1-4/fuser1-4/vuser1-4`
  round-tripped through both physent_t and playermove_t,
  `playerclass`→physent `classnumber`, `waterlevel/watertype`, `aiment`
  (FOLLOW/COMPOUND parent), `ideal_yaw`/`yaw_speed`/`idealpitch`/
  `pitch_speed` (ChangeYaw/Pitch), FL_PARTIALGROUND/FL_MONSTERCLIP/
  FL_CONVEYOR/FL_FLOAT/FL_FLY/FL_SWIM/FL_WATERJUMP/FL_ALWAYSTHINK
  semantics as above.
- Hull selection: player hulls from game DLL `pfnGetHullBounds` cached in
  `host.player_mins/maxs[4]` and mirrored into `pmove->player_mins/maxs`;
  `usehull` 0=stand 1=duck (2=point by convention); server-side re-size on
  duck via `SV_SetMinMaxSize` (sv_phys.c:198–204, sv_pmove.c:660).
