# Deep dive: world interaction & snapshots — `sv_world.c`, `sv_frame.c`

*Chunk 6 recon, 2026-07-04. Narrow-and-exact behavioural reference for the
xash3dpp server rewrite. Line numbers verified against the current tree.*

## 1. Responsibility

- **`engine/server/sv_world.c` (1665 lines)** — server-side world
  interaction: areanode spatial index (link/unlink edicts, trigger
  touching), hull selection (bmodel/bbox/studio), swept AABB traces
  composited over world + linked entities + portals, point contents
  (incl. water entities), lightstyle storage and per-entity light
  sampling.
- **`engine/server/sv_frame.c` (982 lines)** — per-client snapshot
  pipeline: builds the visible entity set via game-DLL hooks, stores it in
  a circular `entity_state_t` ring, emits
  `svc_packetentities`/`svc_deltapacketentities` delta-compressed against
  the client's acked frame, plus clientdata/weapondata deltas, events,
  pings, and drives send-rate/choke logic (`SV_SendClientMessages`).

## 2. Entity linking & areanodes

**Areanode structure** (`areanode_t` decl in `engine/common/mod_local.h`;
constants `engine/common/world.h:32-34`):

- `AREA_NODES=32`, `AREA_DEPTH=4`, `MAX_TOTAL_ENT_LEAFS=128` (world.h:32 —
  **defined but unused** in this fork; vestigial Quake constant).
- `SV_CreateAreaNode` (sv_world.c:422-458): recursive uniform subdivision
  from `sv.worldmodel->mins/maxs`. At `depth == AREA_DEPTH` (4) → leaf
  node, `axis=-1`. Split axis = longer of X/Y
  (`size[0] > size[1] ? 0 : 1` — never Z), `dist = 0.5*(maxs+mins)[axis]`.
  Each node has **three** lists (Xash extension over Quake's two):
  `trigger_edicts`, `solid_edicts`, `portal_edicts` (sv_world.c:431-433).
- Storage: `areanode_t sv_areanodes[AREA_NODES]` global (sv_world.c:412,
  extern in server.h:378), `static int sv_numareanodes`. Reset in
  `SV_ClearWorld` (sv_world.c:466-484), which also inits box hull and
  lightstyles (value=256.0f).

**SV_LinkEdict** (sv_world.c:640-706):

1. Unlink if linked; skip world (`ent == svgame.edicts`) and freed ents.
2. `svgame.dllFuncs.pfnSetAbsBox( ent )` (sv_world.c:650) —
   **absmin/absmax expansion is game-DLL responsibility** (HLSDK
   `SetObjectCollisionBox`: ±1 unit expansion, FL_ITEM → ±15 horizontal /
   -1/+1 vertical rules live there, not in the engine). Engine has no
   fallback.
3. `MOVETYPE_FOLLOW` with valid `aiment`: copies aiment's
   `leafnums32`/`num_leafs`/`headnode` wholesale (sv_world.c:652-657).
4. Else: `SV_FindTouchedLeafs` (sv_world.c:593-633) walks world nodes;
   **stores `leaf->cluster` (not leaf index)** into
   `leafnums32[]`/`leafnums16[]` union depending on `MODEL_QBSP2` flag.
   Cap: `MAX_ENT_LEAFS(ext)` = **24 (int32, QBSP2) / 48 (int16)**
   (engine/edict.h:19-20, server.h:63; comment "Originally was 16"). On
   overflow it sets `num_leafs = cap+1` and continues counting; also
   records `headnode` = first node index where box straddles both sides
   (`sides == 3 && *headnode == -1`, sv_world.c:625-626). After the walk,
   if `num_leafs > cap`: `memset(leafnums32, -1)`, `num_leafs = 0`,
   `ent->headnode = headnode` → headnode-based vis fallback
   (sv_world.c:668-673).
5. `SOLID_NOT && ent->v.skin >= CONTENTS_EMPTY` → not linked at all
   (water/ladder bmodels have skin < 0 so they DO link into solid list)
   (sv_world.c:677).
6. Descend areanodes until box straddles split plane; insert into
   `trigger_edicts` if `SOLID_TRIGGER`, `portal_edicts` if `SOLID_PORTAL`,
   else `solid_edicts` (sv_world.c:694-698).
7. If `touch_triggers && !iTouchLinkSemaphore`: sets semaphore,
   `SV_TouchLinks` over whole tree, clears semaphore (sv_world.c:700-705).
   `iTouchLinkSemaphore` (sv_world.c:411) prevents recursive touch
   cascades.

**SV_TouchLinks** (sv_world.c:506-585): walks `trigger_edicts`;
overridable via physics-interface hook `svgame.physFuncs.SV_TriggerTouch`
(sv_world.c:520-524). Default filter: skip self / non-`SOLID_TRIGGER`;
groupinfo AND/NAND filter (svs.groupop); `BoundsIntersect` absbox test;
for brush-model triggers does an exact `PM_HullPointContents` test of ent
origin against the trigger's BSP hull, with rotation support if
`MODEL_HAS_ORIGIN && angles != 0` via `Matrix4x4_VectorITransform`
(sv_world.c:546-567). Touch callback suppressed while `sv.playersonly`
(sv_world.c:571). Ordering = areanode list order; recursion: children[0]
(upper half) first.

**SV_UnlinkEdict** (sv_world.c:491-499): no-op if `!ent->area.prev`;
unlinks, NULLs both pointers.

**Relink triggers:** `SV_LinkEdict` is called from
`SV_SetMinMaxSize`/`pfnSetOrigin`/`SV_SetModel` (sv_game.c), every physics
move (sv_phys.c — 26 refs), monster moves (sv_move.c — 20 refs), pmove
finalization (sv_pmove.c), save/restore (sv_save.c), client spawn
(sv_client.c).

## 3. Trace composition

**Box hull** (sv_world.c:43-70,152-167): static `box_hull` +
`box_planes[6]`; clipnodes point at shared `box_clipnodes16/32[6]`
(mod_bmodel.c:593-594, `BOX_CLIPNODES_INITIALIZER`); QBSP2 world selects
32-bit clipnodes (`world.version == QBSP2_VERSION`, sv_world.c:161-164).
Plane i: `type = i>>1`, normal = +1 on axis, even index = maxs, odd =
mins.

**SV_HullForBsp** (sv_world.c:176-236): overridable via
`svgame.physFuncs.SV_HullForBsp`. Hull select by `size = maxs-mins`:

- Quake maps (`world.flags & FWORLD_SKYSPHERE`): `size[0] < 3 ||
  SOLID_PORTAL` → hull 0; `<= 32` → hull 1; else hull 2. Offset =
  `hull->clip_mins - mins`.
- HL maps: `size[0] <= 8 || SOLID_PORTAL` → hull 0, offset = `clip_mins`
  verbatim (**not** `clip_mins - mins` — point-hull quirk); `size[0] <=
  36`: `size[2] <= 36` → hull 3 (crouch) else hull 1; else hull 2; offset
  = `clip_mins - mins`.
- Offset += `ent->v.origin` (sv_world.c:233). Errors hard (`Host_Error`)
  if model missing/non-brush (sv_world.c:191-192). Dead joke macro
  `RANDOM_HULL_NULLIZATION` (`COM_RandomLong(0,0)`) at sv_world.c:196-199.

**SV_HullForEntity** (sv_world.c:248-273): `SOLID_BSP`/`SOLID_PORTAL` →
`SV_HullForBsp` (SOLID_BSP without MOVETYPE_PUSH/PUSHSTEP = `Host_Error`,
sv_world.c:255-258). Else Minkowski box: `hullmins = ent->v.mins - maxs`,
`hullmaxs = ent->v.maxs - mins`, offset = origin.

**SV_HullForStudioModel** (sv_world.c:281-355): decides complex (hitbox)
hull when trace `size` is null (point trace) and `!FTRACE_SIMPLEBOX` in
`svgame.globals->trace_flags`; for players (`FL_CLIENT|FL_FAKECLIENT`)
gated by `sv_clienttrace` cvar (0 → bbox; else `scale =
sv_clienttrace.value * 0.5`, size forced to 1,1,1). Models flagged
`STUDIO_TRACE_HITBOX` always use hitboxes. Player path synthesizes
controller bytes 0x7F and blending from `SV_StudioPlayerBlend` (pitch*3
mapped through seqdesc blend range, sv_world.c:78-100) before
`Mod_HullForStudio` (mod_studio.c; returns hull array + numhitboxes).
Fallback → bbox `SV_HullForEntity` with `numhitboxes=1`.

**SV_ClipMoveToEntity** (sv_world.c:836-958): `PM_InitTrace`; studio →
hull array, else single hull. `rotated = (SOLID_BSP||SOLID_PORTAL) &&
angles != 0`. **Pusher extension** (not a cvar — gated by `host.features &
ENGINE_PHYSICS_PUSHER_EXT`, sv_world.c:867; there is **no**
`sv_allow_rotate_pushables` in this codebase): `transform_bbox = true`
when pitch or roll is an exact multiple of 90 (`check_angles` macro,
world.h:105: `(int)x == ±90/±180/±270`) and mins non-null; then the full
matrix uses `ent->v.origin` and the trace bbox is world-transformed via
`World_TransformAABB` with per-axis sign-dependent offset re-application
(sv_world.c:887-901 — quirk: `start_l[j] >= 0 ? -offset : +offset`).
Non-transform path: matrix from `offset`; unrotated path: simple
`start - offset`. Hitbox arrays: loop over hulls, keep best fraction (also
accept if `allsolid||startsolid`), `startsolid` is sticky-merged
(sv_world.c:923-930), `trace->hitgroup =
Mod_HitgroupForStudioHull(last_hitgroup)`. Endpos = lerp; if rotated,
plane transformed by `Matrix4x4_TransformPositivePlane`, else `plane.dist
= DotProduct(endpos, normal)` (sv_world.c:944-953). `trace->ent = ent` if
`fraction < 1 || startsolid` (sv_world.c:956-957).

**SV_Move** (sv_world.c:1314-1361): 1) clip against worldspawn
(`SV_EdictNum(0)`) first; 2) if `fraction != 0`: re-scale — clip.end =
world-clipped endpos, `clip.trace.fraction = 1.0`, entity pass, then
`clip.trace.fraction *= trace_fraction` (fraction compose quirk);
`clip.type = type & 0xFF`, `clip.ignoretrans = type >> 8` (high byte of
type is the "ignore transparent" flag); monsterclip honored only when
`!ENGINE_QUAKE_COMPATIBLE` (sv_world.c:1336); `MOVE_MISSILE` → mins2/maxs2
= ±15 (sv_world.c:1339-1343); `World_MoveBounds` (±1 expansion,
world.h:39-56); `SV_ClipToLinks` then `SV_ClipToPortals` over
`sv_areanodes`; sets `svgame.globals->trace_ent`. Always
`SV_CopyTraceToGlobal` (sv_game.c). Passedict defaults to world if NULL.

**SV_MoveNoEnts** (sv_world.c:1368-1405): same but entity pass replaced by
`SV_ClipToWorldBrush` (only `SOLID_BSP` + `FL_WORLDBRUSH` ents,
sv_world.c:1286) + `SV_ClipToPortals`.

**SV_ClipToEntity filter order** (sv_world.c:1106-1201), exhaustive:

1. groupinfo AND/NAND vs passedict (svs.groupop) → skip.
2. `touch == passedict || SOLID_NOT` → skip.
3. `SOLID_TRIGGER` in solid list → `Host_Error("trigger in clipping
   list")` (sv_world.c:1124).
4. `svgame.dllFuncs2.pfnShouldCollide` game hook (NEW_DLL_FUNCTIONS) →
   skip if 0.
5. `SOLID_BSP||SOLID_CUSTOM` with `FL_MONSTERCLIP` and `!clip->monsterclip`
   → skip; else non-BSP with `MOVE_NOMONSTERS` skipped **unless
   `MOVETYPE_PUSHSTEP`** (pushables still hit, sv_world.c:1143).
6. `ignoretrans`: brush with `rendermode != kRenderNormal` and no
   `FL_WORLDBRUSH` → skip (sv_world.c:1149-1153).
7. `BoundsIntersect` box reject.
8. Non-SLIDEBOX: `SV_CheckSphereIntersection` — player-only sequence-bbox
   sphere test (sv_world.c:109-141; radiusSquared is actually a **sum of
   max-abs components, not squared** — bug-compat).
9. Xash extension: passedict `SOLID_TRIGGER` never clips clients (old HL
   "give stuck item" bug workaround, sv_world.c:1164-1171).
10. Non-null passedict size vs zero-size touch → skip ("points never
    interact", sv_world.c:1174).
11. `clip->trace.allsolid` → **abort whole node walk** (return false →
    SV_ClipToLinks stops, sv_world.c:1178,1222-1223).
12. Owner symmetry skip: `touch->v.owner == passedict` or
    `passedict->v.owner == touch` (sv_world.c:1180-1186).
13. `SOLID_PORTAL` → `SV_PortalCSG` pre-pass (sv_world.c:969-1072: 6-plane
    CSG box around the portal, front/near epsilon `4.0/32.0`, side planes
    at `model->radius*0.5` grown by literal `+24`; elongates trace to
    portal-hole edges, may reassign `trace->ent = portal` when hitting
    near plane).
14. Dispatch: `SOLID_CUSTOM` → `SV_CustomClipMoveToEntity`
    (physics-interface `ClipMoveToEntity`; missing hook →
    `allsolid=false` no-hit); `FL_MONSTER` → clip with **mins2/maxs2**
    (expanded); else mins/maxs.
15. `World_CombineTraces` (world.h:58-73): accept if
    `allsolid||startsolid||fraction < best`; `startsolid` sticky.

There is no `SV_CheckTransform`; group/instance filtering is entirely the
`groupinfo`/`svs.groupop` mechanism (set via `pfnSetGroupMask` in
sv_game.c).

**Point contents:** `SV_TruePointContents` (sv_world.c:790-804) = world
hull0 `PM_HullPointContents` + `SV_WaterLinks` (sv_world.c:715-782) which
scans **solid_edicts** for `SOLID_NOT` brush ents (water volumes),
groupinfo vs `svs.groupmask`, exact hull test with rotated-water support,
and keeps highest `RankForContents` (world.h:82-101 priority table: EMPTY
< WATER < TRANSLUCENT < CURRENT_0..DOWN < SLIME < LAVA < SKY < SOLID <
user). `SV_PointContents` (sv_world.c:812-819) folds all CURRENT_* into
`CONTENTS_WATER`.

**Lighting (confirmed here, not elsewhere):** `SV_SetLightStyle`
(sv_world.c:1612-1630) stores pattern + broadcasts `svc_lightstyle` on
`sv.reliable_datagram`; `SV_LightForEntity` (sv_world.c:1639-1664) —
EF_FULLBRIGHT/no lightdata → 255, players return `v.light_level`
(client-computed), else `SV_RecursiveLightPoint` (sv_world.c:1516-1603)
down ±`world.size[2]` (EF_INVLIGHT traces upward), samples lightmap with
`sv.lightstyles[].value` scaling, returns `VectorAvg`.

**Trace kernel:** `PM_RecursiveHullCheck`/`PM_HullPointContents` in
`engine/common/pm_trace.c`; **DIST_EPSILON usage lives there
(pm_trace.c:263-267), not in sv_world.c.**
`SV_TraceSurface/SV_TraceTexture` (sv_world.c:1415-1459) →
`PM_RecursiveSurfCheck` for texture name lookup.

## 4. PVS/PHS multicast

**`SV_Multicast`** — static in `engine/server/sv_game.c:354-462` (all
message fan-out flows through sv_game.c). Sends `sv.multicast` buffer then
clears it.

Dest semantics (`common/const.h:574-583`):

| dest | mask | reliable | notes |
|---|---|---|---|
| MSG_INIT(3) | none | — | during `ss_loading` copied to `sv.signon`; in-game falls through to MSG_ALL |
| MSG_ALL(2) | none | yes | |
| MSG_BROADCAST(0) | none | no | |
| MSG_PAS(5)/MSG_PAS_R(7) | fat **PHS** at origin (`Mod_FatPVS(..., FATPHS_RADIUS, fatphs, ..., phs=true)`; singleplayer forces fullvis — "GoldSource not using PHS for singleplayer", sv_game.c:392-394) | R variant | returns 0 if origin NULL |
| MSG_PVS(4)/MSG_PVS_R(6) | `Mod_GetPVSForPoint(origin)` (single-leaf PVS, can be NULL → everyone) | R variant | |
| MSG_ONE(1)/MSG_ONE_UNRELIABLE(8) | — | ONE=reliable | ent must be valid client index |
| MSG_SPEC(9) | — | yes | writes into `sv.spec_datagram`, HLTV proxies only |
| other | `Host_Error("bad dest")` | | |

Per-client filters (sv_game.c:422-451): skip free/zombie; non-spawned
skipped unless (reliable && !usermessage); specproxy requires
`FCL_HLTV_PROXY`; skip fakeclients; `filter` param skips
`sv.current_client` when `FCL_PREDICT_MOVEMENT` (prediction step-sound
suppression, sv_game.c:436-439); groupinfo AND/NAND;
`SV_CheckClientVisiblity` (sv_game.c:302-338): NULL mask → true ("GoldSrc
rules"); vieworg from `pViewEntity` if set else client edict (Invasion
workaround); leaf-cluster bit check; then **all portal cameras** in
`cl->viewentity[0..num_viewents)` checked too. Delivery: specproxy →
`sv.spec_datagram`; reliable → `cl->netchan.message`; else `cl->datagram`.

**Fat PVS/PHS buffers:** `static byte fatphs[(MAX_MAP_LEAFS+7)/8]` at
**sv_game.c:31**; `static byte fatpvs[...]` local to `pfnSetFatPVS` at
**sv_game.c:4257**. Scratch decompress row `g_visdata` +
`world.compressed_phs`/`world.phsofs`/`world.visbytes`/`world.fatbytes`
live in `engine/common/mod_bmodel.c` / `world_static_t`
(mod_local.h:107-108). `FATPVS_RADIUS = FATPHS_RADIUS = 8.0f`
(mod_local.h:31-32).

**Mod_FatPVS** (mod_bmodel.c:1211-1241): descend BSP; nodes within ±radius
of plane recurse both sides; each reached leaf with `cluster >= 0`
OR-merges its decompressed PVS row (or PHS row via
`world.compressed_phs[world.phsofs[cluster+1]]`) into the buffer
(`Q_memor`). `fullvis`/no visdata/solid leaf → memset 0xFF. `merge` skips
the initial zeroing (used for portal-camera accumulation via
`SVF_MERGE_VISIBILITY`). PHS requested but absent → 0xFF.

**Mod_CalcPHS** (mod_bmodel.c:3730-3860, called at world load for MP from
mod_bmodel.c:4355): rows = `numleafs+1` (1-based), rowbytes = visbytes
aligned to 4. Decompress all leaf PVS rows; PHS row i = PVS row i OR'd
with PVS row of every leaf j visible in row i (bit index +1 mapping,
`index=(j*8)+k+1`, skip if `>= count`); OpenMP-parallel; rows then RLE
recompressed (`Mod_CompressPVS`) into `world.compressed_phs` with
`world.phsofs[]` offsets. Note: this is **PVS-of-PVS ("hearable = visible
from anywhere visible")**, same as GoldSrc/Quake `CalcPHS`; comment at
3845-3859 documents byte-parity with GoldSrc fat PHS/PVS dumps.

**Vis entry points used by snapshot:** game DLL `pfnSetupVisibility`
(HLSDK) calls back into engine `pfnSetFatPVS`/`pfnSetFatPAS`
(sv_game.c:4255-4292) — both honor `sv_novis` cvar, missing visdata, and
`SVF_MERGE_VISIBILITY` merge flag; `pfnCheckVisibility`
(sv_game.c:4329-4389) does the per-entity leaf/headnode test.

## 5. Snapshot pipeline (sv_frame.c)

**Cadence:** `Host_ServerFrame` (sv_main.c:678-719) → `SV_Physics` (fixed
`1/sv_fps` steps accumulated in `sv.time_residual`; note
`1.0/(sv_fps - 0.01)` FP fudge sv_main.c:611) → `SV_SendClientMessages()`
(sv_main.c:712) **every host frame** (even if not simulated, to flush
reliables — but see the early-return quirk when zero physics frames ran).

**SV_SendClientMessages** (sv_frame.c:820-906): `SV_UpdateToReliableMessages`
first; per client: skip zombies/fakeclients; `FCL_SKIP_NET_MESSAGE`
one-shot skip (set by `SV_SkipUpdates` before changelevel,
sv_frame.c:915-930); local addresses always send unless `host_limitlocal`;
for spawned clients send when `cl->next_messagetime - (host.realtime +
sv.frametime) <= 0` **or** `> 2.0` ("something got hosed");
reliable-overflow → drop client; `sv_failuretime` since last received
packet → stop sending; `Netchan_CanPacket` bandwidth choke →
`cl->chokecount++` and skip; on send: `next_messagetime = realtime +
sv.frametime + next_messageinterval` (updaterate, default 0.05 = 20 fps,
clamped by `sv_minupdaterate/sv_maxupdaterate` via `SV_CheckUpdateRate`,
sv_client.c:451,1900); spawned → `SV_SendClientDatagram`, else empty
`Netchan_TransmitBits` keepalive.

**SV_SendClientDatagram** (sv_frame.c:685-728): local
`msg_buf[MAX_DATAGRAM=16384]` (net_ws.h:34). Order: `svc_time` + `sv.time`
float; `SV_WriteClientdataToMessage`; `SV_WriteEntitiesToClient`; then
appends `cl->datagram` (per-client unreliable: sounds/tempents/multicast
copies) if it fits (else warn, 5s rate-limited), clears it; overflow of
msg → clear and error; `Netchan_TransmitBits`.

**SV_WriteClientdataToMessage** (sv_frame.c:526-605): frame =
`cl->frames[cl->netchan.outgoing_sequence & SV_UPDATE_MASK]`
(`SV_UPDATE_BACKUP` = 16 SP / 64 MP, netchan.h:75-76; mask = backup-1);
stamps `frame->senttime`, `ping_time=-1` (latency computed on ack in
sv_client.c:3646 as `realtime - senttime - next_messageinterval`). Emits
in order: `svc_choke` if chokecount (then reset); fixangle 1 →
`svc_setangle` (3 angles), fixangle 2 → `svc_addangle` (16-bit yaw
avelocity, then zeroed); fixangle reset to 0.
`pfnUpdateClientData(clent, FCL_LOCAL_WEAPONS, &frame->clientdata)` game
hook fills clientdata. `svc_clientdata` header; **HLTV proxies get header
only** (sv_frame.c:570). Delta source: `cl->delta_sequence == -1` → null
clientdata + 0-bit; else 1-bit + delta_sequence byte, from
`frames[delta_sequence & SV_UPDATE_MASK].clientdata`.
`MSG_WriteClientData` (net_encode.c:1752). If `FCL_LOCAL_WEAPONS &&
pfnGetWeaponData(clent, frame->weapondata)`: `MAX_LOCAL_WEAPONS=64`
(common/entity_state.h:179) `MSG_WriteWeaponData` deltas (each internally
has a change bit + 6-bit index). Trailing 0 bit ends clientdata blob.
`delta_sequence` is set from client's `clc_delta` byte (sv_client.c:3679),
reset to -1 each non-delta request (sv_client.c:3656).

**SV_WriteEntitiesToClient** (sv_frame.c:613-671): static `sv_ents_t
frame_ents` (entities[`MAX_VISIBLE_PACKET`=2048 (protocol.h:98-99; 256/128
on LOW_MEMORY)], `sended[MAX_EDICTS_BYTES]` dedup bitmask). Clears
`SVF_MERGE_VISIBILITY`; calls `SV_AddEntitiesToPacket(cl->pViewEntity,
cl->edict, ..., from_client=true)`; logs "Too many entities in visible
packet list" only when count changes (sv_frame.c:633-638); **qsorts by
entity number** (portal recursion breaks ordering; qsort comparator has
watcom self-compare guard, sv_frame.c:36-50); ring-overflow guard: if
`svs.next_client_entities + N >= 0x7FFFFFFE` → reset to 0 +
`SV_FinalMessage` forced reconnect ("delta is outdated",
sv_frame.c:647-653); copies states into circular
`svs.packet_entities[next_client_entities % num_client_entities]`
(`num_client_entities = maxclients * SV_UPDATE_BACKUP *
NUM_PACKET_ENTITIES`, NUM_PACKET_ENTITIES=256, netchan.h:79,
sv_init.c:826); records `frame->first_entity/num_entities`; then
`SV_EmitPacketEntities`, `SV_EmitEvents`, `SV_EmitPings` (pings only on
HLTV 2s timer or client holding IN_SCORE, sv_client.c:1293-1306).

**SV_AddEntitiesToPacket** (sv_frame.c:58-167): if `from_client`:
sets/clears `SVF_SKIPLOCALHOST` in `sv.hostflags` per `FCL_LOCAL_WEAPONS`
(weapon prediction), resets `cl->num_viewents`.
`pfnSetupVisibility(pViewEnt, pClient, &pvs, &phs)` game hook (which calls
engine fat PVS/PAS); NULL pvs → fullvis. Loop e = 1..numEntities (world
excluded): skip if already in `sended` bitmask (portal dedup); players
(1..maxclients) must be `cs_spawned` and not HLTV proxies; per-entity mask
= `clientphs` if `EF_REQUEST_PHS` effect else pvs (sv_frame.c:120-122);
**`pfnAddToFullPack(state, e, ent, pClient, sv.hostflags, player, pset)`
game hook does BOTH the vis test (via pfnCheckVisibility) and the
entity_state_t fill** — there is no engine-side `SV_FillEntityState`; the
engine only supplies baselines/delta. On accept: SETVISBIT; if ent's
aiment has `EF_MERGE_VISIBILITY` → register aiment as portal camera in
`cl->viewentity[]` (cap `MAX_VIEWENTS=128`, server.h:60); accept into list
if `num_entities < MAX_VISIBLE_PACKET-1` else count `c_notsend` (silent
overflow). After each ent: if `!fullvis` and `from_client` and ent has
`EF_MERGE_VISIBILITY` → set `SVF_MERGE_VISIBILITY`, recurse with ent as
view (portal pass merges fat PVS), clear flag.

**SV_EmitPacketEntities** (sv_frame.c:235-363): from-frame =
`cl->frames[delta_sequence & SV_UPDATE_MASK]` if `delta_sequence != -1`;
**staleness check**: `from->first_entity <= next_client_entities -
num_client_entities` → warn "delta request from out of date entities",
fall back to full `svc_packetentities` (sv_frame.c:251-259). Header:
`svc_deltapacketentities` + `num_entities-1` in
`MAX_VISIBLE_PACKET_BITS`(11) + delta_sequence byte, or
`svc_packetentities` + count. Two-pointer merge old/new by entity number:

- equal → `MSG_WriteDeltaEntity(old, new, msg, force=false, player,
  sv.time, 0)` (emits nothing if unchanged);
- new only → baseline selection: default `svs.baselines[newnum]`; if
  `sv_instancedbaseline && sv.num_instanced && newnum >
  sv.last_valid_baseline` → match instanced baseline by classname, offset
  = `-i-1`; else `SV_FindBestBaseline` (sv_frame.c:184-226: scans up to
  `MAX_CUSTOM_BASELINES-1=63` previous states **in this same frame's
  list** for the delta costing fewer bits via `Delta_TestBaseline`,
  returns positive offset; also used with `frame=NULL` against
  `svs.static_entities` for `svc_spawnstatic`, sv_game.c:566); write with
  force=true;
- old only → removal: `force = ed->free || FL_KILLME` → fRemoveType 2
  (full remove) else 1 (leave-PVS), `MSG_WriteDeltaEntity(old, NULL, ...)`.

Terminator: `LAST_EDICT (8191)` in `MAX_ENTITY_BITS` (13)
(sv_frame.c:362). Player flag: `SV_IsPlayerIndex(newent->number)` selects
the `DT_ENTITY_STATE_PLAYER_T` delta table.

**SV_EmitEvents** (sv_frame.c:371-485): drains `cl->events` queue
(`MAX_EVENT_QUEUE=64`, world.h:117; queued by `SV_PlaybackEventFull`
sv_game.c:4060-4230 with PAS masking, FEV_* flag handling, FEV_UPDATE
slot-merge; reliable events bypass queue → `svc_event_reliable` direct to
netchan). Emit cap: `ev_count` clamped to `MAX_EVENT_QUEUE/2 - 1 = 31`;
resolves each event's entity to a packet index in the current frame
(found → packet_index + zeroed ducking/conditional
origin/angles/velocity cleared; not found → packet_index = num_entities,
entindex passed in args). Wire: `svc_event`, 5-bit count, per event:
10-bit event index (`MAX_EVENT_BITS`), 1 bit has-packet-index, 13-bit
packet index, 1 bit has-args → `MSG_WriteDeltaEvent` vs null args, 1 bit
has-fire-time → word `fire_time*100`. Queue slots cleared as sent.

**SV_EmitPings** (sv_frame.c:493-518): `svc_pings`, per spawned client: 1
bit marker + 5-bit slot (`MAX_CLIENT_BITS`) + 12-bit ping + 7-bit loss
("25 bits per client"), 0-bit terminator.

**SV_UpdateToReliableMessages** (sv_frame.c:747-813): resend userinfo
(`FCL_RESEND_USERINFO`, 1s repeat, needs `strlen(userinfo)+6` bytes free)
via `SV_FullClientUpdate` into `sv.reliable_datagram`;
`FCL_RESEND_MOVEVARS` → `SV_FullUpdateMovevars` direct to netchan;
overflow-clears `sv.datagram`/`sv.spec_datagram`; then fans
`sv.reliable_datagram` to every connected non-fake client's netchan (or
`Netchan_CreateFragments` if too big), `sv.datagram` to `cl->datagram`
(drop with warning if full), `sv.spec_datagram` only to HLTV proxies;
clears all three.

**SV_InactivateClients** (sv_frame.c:939-981): changelevel prep — drops
fakeclients, demotes spawned → `cs_connected`, clears
customizations/physinfo; **keeps netchan message buffers when
`svgame.globals->changelevel`** (CryOfFear HideHud/PlayMp3 carry-over
quirk, sv_frame.c:973-976).

## 6. External/intra surface

- `SV_Move`: sv_phys.c (26 call sites — pushers, toss, step), sv_move.c
  (20 — monster nav), sv_pmove.c, sv_game.c (`pfnTraceLine/TraceHull/
  TraceToss/TraceMonsterHull`), sv_client.c (spawn testing), sv_save.c.
  `SV_MoveNoEnts` ← `pfnTraceLine` flag path. `SV_MoveToss` ←
  `pfnTraceToss`.
- `SV_LinkEdict` ← sv_game.c (SetOrigin/SetSize/SetModel), sv_phys.c after
  every position change, sv_move.c, sv_pmove.c, sv_client.c, sv_save.c.
- `SV_PointContents` ← `pfnPointContents`, physics water checks
  (`SV_CheckWater` in sv_phys.c), pmove setup.
- `SV_Multicast` (static, sv_game.c) ← `pfnMessageEnd` (user messages,
  sv_game.c:2729), `SV_StartSound`/ambient (2087, 2112), `SV_StartMusic`
  (603), fade-volume style broadcasts (3533).
- `SV_SendClientMessages` ← `Host_ServerFrame` (sv_main.c:712) only.
  `SV_SkipUpdates` ← changelevel path. `SV_InactivateClients` ←
  `SV_DeactivateServer`.
- `SV_FindBestBaseline` exported for `SV_CreateStaticEntity`
  (sv_game.c:566).
- `SV_SetLightStyle` ← `pfnLightStyle`; `SV_LightForEntity` ←
  `pfnGetEntityIllum`; `SV_TraceTexture` ← `pfnTraceTexture`.

## 7. Owned state

sv_world.c: `box_hull` + `box_planes[6]` (statics, :43-44);
`iTouchLinkSemaphore` (static, :411); `sv_areanodes[32]` (global, :412) +
`sv_numareanodes` (static, :413). Lightstyles live in
`sv.lightstyles[MAX_LIGHTSTYLES=256]` (server_t, server.h:146).

sv_frame.c: `c_fullsend`/`c_notsend` debug counters (statics, :28-29);
`static sv_ents_t frame_ents` inside `SV_WriteEntitiesToClient` (:617 —
~2048 entity_state_t, single-threaded reuse). Snapshot ring:
`svs.packet_entities`/`num_client_entities`/`next_client_entities`,
`svs.baselines[max_edicts]`, `svs.static_entities[MAX_STATIC_ENTITIES=
3096]` (server_static_t, server.h:356-361; alloc sv_init.c:826-828,
sv_game.c:5341). Per-client: `cl->frames[SV_UPDATE_BACKUP]` (alloc
sv_client.c:422), `cl->events`, `cl->datagram`.

Fat vis buffers are **NOT** in sv_world/sv_frame: `fatphs` sv_game.c:31,
`fatpvs` sv_game.c:4257 (function-static), decompress scratch `g_visdata`
+ PHS tables in mod_bmodel.c/`world` struct.

## 8. Dependencies

- **model/CM (engine/common):** `Mod_PointInLeaf`, `Mod_GetPVSForPoint`,
  `Mod_FatPVS`, `Mod_DecompressPVS/Mod_CompressPVS`, `Mod_CalcPHS`,
  `Mod_BoxVisible`, `node_child/node_numsurfaces/node_firstsurface`
  accessors (QBSP2 dual-width), `Mod_SampleSizeForFace` (mod_bmodel.c);
  `Mod_HullForStudio`, `Mod_HitgroupForStudioHull`, `Mod_StudioExtradata`
  (mod_studio.c); `box_clipnodes16/32` (mod_bmodel.c:593).
- **trace kernel:** `PM_InitTrace`, `PM_RecursiveHullCheck`,
  `PM_HullPointContents`, `PM_RecursiveSurfCheck` (pm_trace.c) —
  `trace_t` cast to `pmtrace_t` directly (sv_world.c:911 —
  layout-compat assumption).
- **world.c/world.h helpers:** `World_TransformAABB` (only function in
  world.c); inline `World_MoveBounds` (±1 box), `World_CombineTraces`
  (merge rule), `RankForContents`, `check_angles` macro; event queue types
  (`event_info_t/event_state_t`).
- **math:** `Matrix4x4_CreateFromEntity/VectorITransform/
  TransformPositivePlane/Invert_Simple`, `BoundsIntersect`,
  `SphereIntersect`, `BOX_ON_PLANE_SIDE`.
- **netbuffer/delta:** `MSG_*` (net_buffer.c), `MSG_WriteDeltaEntity/
  MSG_ReadDeltaEntity/MSG_WriteClientData/MSG_WriteWeaponData/
  MSG_WriteDeltaEvent/Delta_TestBaseline` (net_encode.c), Netchan
  (`Netchan_TransmitBits/CanPacket/CreateFragments`).
- **game DLL callbacks:** `pfnSetAbsBox`, `pfnTouch`, `pfnAddToFullPack`,
  `pfnSetupVisibility`, `pfnUpdateClientData`, `pfnGetWeaponData`,
  `pfnCreateBaseline`, `pfnCreateInstancedBaselines` (DLL_FUNCTIONS,
  engine/eiface.h); `pfnShouldCollide` (NEW_DLL_FUNCTIONS); physics
  interface hooks `SV_HullForBsp`, `SV_TriggerTouch`, `ClipMoveToEntity`,
  `pfnPrepWorldFrame` (physint.h, Xash extensions).
- cvars: `sv_clienttrace`, `sv_gravity`, `sv_novis`,
  `sv_instancedbaseline`, `sv_failuretime`,
  `sv_minupdaterate/sv_maxupdaterate`, `host_limitlocal`, `sv_fps`.

## 9. Quirks & invariants (exhaustive)

1. Areanode split never uses Z axis (sv_world.c:443-445). Depth constant 4
   → max 31 of 32 nodes used (2^5-1); `AREA_NODES=32` (world.h:33-34).
2. `MAX_ENT_LEAFS` asymmetric: 24 ints (QBSP2) vs 48 shorts (classic)
   sharing one union (edict.h:19-38); stores **clusters**, not leaf
   indices. Overflow → `num_leafs = cap+1` sentinel then reset to 0 +
   headnode fallback with `leafnums = -1` fill (sv_world.c:604-608,
   668-673). `MAX_TOTAL_ENT_LEAFS=128` defined (world.h:32) but never
   referenced — do not implement.
3. Headnode = first node where absbox straddles (`sides==3`), captured
   only once (`*headnode == -1` guard, sv_world.c:625-626).
   `pfnCheckVisibility` (sv_game.c:4329-4389): `headnode >= 0` path scans
   leafnums until `-1`, then `Mod_HeadnodeVisible` recursive walk; on
   success **caches the found cluster into `leafnums[num_leafs]` and
   increments `num_leafs` modulo cap** (sv_game.c:4380-4385) —
   self-mutating vis cache, returns 2 vs 1. Beams (`FL_CUSTOMENTITY`)
   upcast vis test to their owner if owner is a client
   (sv_game.c:4340-4341).
4. `MOVETYPE_FOLLOW` copies aiment leaf set verbatim, including
   QBSP2-width `leafnums32` memcpy regardless of world type
   (sv_world.c:652-657).
5. `SOLID_NOT` with `skin < CONTENTS_EMPTY` (water volumes use negative
   contents in skin) still links so `SV_WaterLinks` can find them in
   **solid** lists (sv_world.c:677, 724-729).
6. Trigger touch: brush triggers get exact hull point test (Quake
   behaviour), non-brush triggers are pure AABB; rotation only honored
   when `MODEL_HAS_ORIGIN && angles != 0` (sv_world.c:546-567).
   `sv.playersonly` suppresses pfnTouch, not the scan (sv_world.c:571).
   `iTouchLinkSemaphore` makes nested SV_LinkEdict calls from inside Touch
   callbacks skip trigger touching (sv_world.c:700-705).
7. Quake-map hull select branch keyed off `FWORLD_SKYSPHERE` with
   thresholds 3/32 vs HL's 8/36/36, and point-hull offset asymmetry
   (`VectorCopy(clip_mins)` vs subtract, sv_world.c:200-231).
8. `sv_clienttrace 0` disables player hitbox traces; scale = `value *
   0.5`; hitbox path forces size (1,1,1) (sv_world.c:302-315). Player
   hitbox pose uses hardcoded controllers 0x7F and blend from pitch*3
   (sv_world.c:336-341).
9. `FTRACE_SIMPLEBOX` in `svgame.globals->trace_flags` suppresses complex
   hulls for one trace; reset to 0 in `SV_ConvertTrace` after every
   game-visible trace (sv_game.c:292).
10. Rotated-bbox pusher path: gated by `ENGINE_PHYSICS_PUSHER_EXT` host
    feature (game-dll opt-in), **no cvar named `sv_allow_rotate_pushables`
    exists**; only exact 90/180/270-degree pitch/roll (`check_angles`
    truncates to int, world.h:105) with non-null mins; comment "keep
    untransformed bbox less than 45 degrees or train on subtransit.bsp
    will stop working" (sv_world.c:867-874); sign-dependent offset
    application (sv_world.c:892-900).
11. `SphereIntersect` "radiusSquared" is Σ max(|bbmin|,|bbmax|) per axis —
    not squared; bug-compat (sv_world.c:135-138).
12. Fraction composition: world trace first; entity pass runs only if
    world fraction != 0, with clip.end = world endpos and final
    `fraction *= world_fraction` (sv_world.c:1320-1354) — entity fractions
    are relative to the world-clipped segment.
13. `type >> 8` = ignoretrans flag smuggled in high byte of the move type
    (sv_world.c:1329-1330); `type & 0xFF` compared against
    MOVE_NOMONSTERS/MOVE_MISSILE.
14. monsterclip disabled entirely under `ENGINE_QUAKE_COMPATIBLE`
    (sv_world.c:1336); MOVE_NOMONSTERS still hits `MOVETYPE_PUSHSTEP`
    pushables (sv_world.c:1143).
15. Trigger found in solid list is a hard `Host_Error` (sv_world.c:1124).
16. `allsolid` short-circuits the whole areanode walk (returns without
    recursing, sv_world.c:1178, 1222).
17. Portal CSG epsilons: near/far planes ε = `4.0/32.0`; side planes `+24`
    literal; portal radius = `model->radius * 0.5` (sv_world.c:999-1024).
    Portal traces clear `startsolid/allsolid` unconditionally once inside
    the CSG region (sv_world.c:1055).
18. `SV_MoveToss`: 200 iterations of dt=0.05, gravity `tossent->v.gravity
    * sv_gravity * 0.05`, restores entity state after
    (sv_world.c:1466-1501).
19. DIST_EPSILON (0.03125) lives in pm_trace.c:263-267, not sv_world.c.
20. `trace_t` ↔ `pmtrace_t` cast (sv_world.c:911,921) requires layout
    prefix compatibility — ABI invariant for the rewrite.
21. Water contents ranking: user contents (default case) outrank SOLID
    (world.h:82-101); CURRENT_* collapsed to WATER only in
    `SV_PointContents`, not `SV_TruePointContents` (sv_world.c:816-817).
22. Snapshot: entity 0 (world) never sent (loop from e=1, sv_frame.c:95);
    HLTV-proxy players excluded from packs (sv_frame.c:116-117);
    `MAX_VISIBLE_PACKET-1` acceptance cap (one slot reserved,
    sv_frame.c:142); overflow silent to client, console-error only on
    change (sv_frame.c:633-638).
23. `EF_REQUEST_PHS` per-entity mask switch (sv_frame.c:120-122);
    `EF_MERGE_VISIBILITY` drives both portal recursion (on the entity) and
    portal-camera registration (on the aiment) — max 128 cameras
    (sv_frame.c:132-139,160-165).
24. `SVF_SKIPLOCALHOST` (weapon-prediction skip flag) passed to
    AddToFullPack via `sv.hostflags`; only mutated on the from_client pass
    (sv_frame.c:80-89).
25. Ring staleness: delta refused if `from->first_entity <=
    next_client_entities - num_client_entities` (sv_frame.c:251); global
    reset at `0x7FFFFFFE` with forced server restart message
    (sv_frame.c:647-653).
26. Baseline search window: 64 (`MAX_CUSTOM_BASELINES`) previous states,
    only same `entityType`, cost via `Delta_TestBaseline` bit counting
    (string fields cost len*8, net_encode.c:1210-1212); offset
    wire-encoded as signed 7-bit; instanced-baseline offset = `-i-1` "to
    avoid zero offset" (sv_frame.c:333); instanced path only for `newnum >
    sv.last_valid_baseline` (entities created in-game after baseline
    snapshot).
27. Removal semantics: fRemoveType 1 = left PVS (keep client-side state),
    2 = `ed->free || FL_KILLME` full delete (sv_frame.c:349-356,
    net_encode.c:1925-1932).
28. Event emit clamp `MAX_EVENT_QUEUE/2 - 1 = 31` but 5-bit count field
    (max 31 — exactly fits); `args.ducking` zeroed and velocity cleared
    when entity is in-pack (sv_frame.c:414-423); fire_time serialized as
    `word(t*100)` (10ms resolution, sv_frame.c:475).
29. Clientdata delta reference is a byte (`delta_sequence`), mask
    `SV_UPDATE_MASK` (15 SP / 63 MP); weapondata loops all 64 slots each
    frame.
30. fixangle=2 zeroes `avelocity[YAW]` after send (sv_frame.c:557-558);
    fixangle always reset to 0 whether or not anything was sent
    (sv_frame.c:562).
31. Send-rate: `next_messagetime` uses `host.realtime + sv.frametime +
    interval`; the "> 2.0" hosed-clock recovery (sv_frame.c:857-858);
    dropped-client path still sets `FCL_SEND_NET_MESSAGE` and
    `cleartime = 0` to flush the disconnect (sv_frame.c:869-870).
32. `SV_InactivateClients` preserves netchan message during `changelevel`
    for mods that pre-send next-level messages (sv_frame.c:973-976);
    duplicated `!cl->edict` check at :950-954 (dead code).
33. Multicast: `MSG_INIT` during load goes to signon, else silently
    becomes reliable-ALL (sv_game.c:372-381); PAS uses **fat PVS machinery
    with PHS rows** (radius 8.0); singleplayer PAS forces fullvis
    (sv_game.c:392-393); non-spawned clients receive reliable engine
    messages but not user messages (sv_game.c:427).
34. `SV_UPDATE_BACKUP` is a runtime **variable** (sv_init.c:24, set 16/64
    at InitGame per maxclients) except LOW_MEMORY=2 builds where it's a
    macro.
35. Delta tables come from `delta.lst` (`DELTA_PATH`, net_encode.c:27):
    `entity_state_t` / `entity_state_player_t` / `custom_entity_state_t`
    (ENTITY_BEAM) / `clientdata_t` / `weapon_data_t` / `event_t`; static
    ents use DELTA_STATIC (all fields active, no custom encode,
    net_encode.c:1977-1982).

## 10. ABI/wire touchpoints

- **Frozen structs:** `entity_state_t`, `clientdata_t`, `weapon_data_t`,
  `local_state_t` — `common/entity_state.h` (MAX_LOCAL_WEAPONS=64 at
  :179); `event_args_t` (`common/event_args.h`); `usercmd_t`;
  `edict_t`/`entvars_t` (engine/edict.h, common/progdefs.h) — `link_t
  area`, `headnode`, `num_leafs`, leafnums union are engine-owned but
  ABI-visible to the game DLL; `TraceResult` conversion
  (sv_game.c:276-293).
- **svc opcodes emitted by these files** (engine/common/protocol.h):
  `svc_time`(7), `svc_setangle`(10), `svc_lightstyle`(12),
  `svc_clientdata`(15), `svc_pings`(17), `svc_spawnstatic`(20),
  `svc_event_reliable`(21), `svc_spawnbaseline`(22), `svc_addangle`(38),
  `svc_packetentities`(40), `svc_deltapacketentities`(41),
  `svc_choke`(42), `svc_event`(3); related: `svc_bspdecal`(36),
  `svc_stufftext`(9). PROTOCOL_VERSION 49.
- **Bit widths (wire-frozen):** entity number 13 bits (`MAX_ENTITY_BITS`,
  max_edicts 8192, terminator `LAST_EDICT=8191`); remove type 2 bits;
  baseline-offset flag 1 + signed 7 bits; entityType flag 1 + 2 bits;
  packet count 11 bits (`MAX_VISIBLE_PACKET_BITS`); event index 10 bits;
  event count 5 bits; client slot 5 bits; ping 12 + loss 7; delta
  sequence 8 bits; weapondata index 6 bits.
- **delta.lst dependency:** all field encodings data-driven;
  `Delta_CustomEncode` callbacks (player/entity conditional field
  activation) must be replicated for parity. (See
  `deep-dive-delta-encoder.md`.)
- **Game DLL contract:** `pfnAddToFullPack` (returns 1 to include; fills
  entire entity_state_t; receives hostflags SVF_SKIPLOCALHOST/
  SVF_MERGE_VISIBILITY, player flag, PVS/PHS pset), `pfnCreateBaseline`
  (+ engine-supplied player model index and hull mins/maxs,
  sv_init.c:503), `pfnCreateInstancedBaselines` + `pfnRegisterEncoders`,
  `pfnSetupVisibility`, `pfnUpdateClientData`, `pfnGetWeaponData`,
  `pfnSetAbsBox`, `pfnTouch`, `pfnShouldCollide`; engine exports consumed
  by them: `pfnSetFatPVS/pfnSetFatPAS/pfnCheckVisibility` (the vis kernel
  the rewrite must reproduce, sv_game.c:4255-4389).
- **PHS parity note:** Mod_CalcPHS output is byte-identical to GoldSrc fat
  PHS per in-code verification note (mod_bmodel.c:3845-3859) — a
  golden-fixture opportunity for the rewrite.
