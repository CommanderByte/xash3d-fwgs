# Deep Dive: Legacy Hull-Trace / Point / PVS Query Layer

*Recon brief produced 2026-07-04 by a read-only survey agent ahead of Chunk 5
(map_loader). Scope: the legacy spatial-query algorithms mapped precisely
enough for a float-for-float C++23 reimplementation, plus fixture options.
Line numbers are against the working tree on that date; behaviour reference,
not a design constraint.*

> **Refreshed 2026-07-06 (as-built cross-reference).** Implemented as Chunk 5
> `map_loader`, float-/bit-exact to legacy (Q-18). As-built mapping: the kernel
> (§1) → `src/map_loader/trace.cpp` behind `include/xash3dpp/map_loader/
> trace.hpp` (`recursive_hull_check` / `hull_point_contents` / `trace_hull` /
> `finalize_trace` / `hull_for_bsp` / `BoxHull`), **deliberately edict-free** —
> no physent list, no `usehull` global (the hard Chunk 6 prerequisite); the
> contents/point queries (§2) fold into the same kernel; the PVS surface (§3) →
> `src/map_loader/pvs.cpp` behind `pvs.hpp` (`point_leaf`,
> `leaf_compressed_pvs`, `pvs_for_point`, `box_leafnums`, `box_visible`,
> `fat_pvs`, `decompress_pvs`, `check_vis_bit`). Parity is verified
> (trace-kernel audit PARITY-CONFIRMED; 18,156 traces / 0 mismatches vs the
> verbatim legacy kernel). The **PHS build path** (`Mod_CalcPHS` +
> `Mod_CompressPVS`), which §3 mentions only via the `Mod_FatPVS` phs parameter
> and `Mod_HeadnodeVisible`, is written up in the new **§7** below and shipped
> in `phs.hpp` / `phs.cpp` per Q-19. This dive stays the *behaviour* reference;
> the boundary spec §2/§5 is the shipped contract.

## 0. Headline findings (read first)

1. **There is exactly ONE canonical hull-trace kernel:
   `PM_RecursiveHullCheck` + `PM_HullPointContents` in
   `engine/common/pm_trace.c`.** There is no `SV_RecursiveHullCheck`. The
   server (`engine/server/sv_world.c`) calls
   `PM_RecursiveHullCheck( hull, …, (pmtrace_t *)trace )` directly
   (sv_world.c:911, 921) by punning its `trace_t*` to `pmtrace_t*`. Server
   physics (`SV_Move`), the pmove seam (`pfnPM_Move`), and client prediction
   all funnel through the same kernel.
2. **The pfnPM_Move seam is compiled into the mod DLL, not the engine** (see
   `xash3dpp/docs/design/pm-determinism-decision.md`). The engine only
   supplies trace callbacks via `playermove_t` function pointers; both
   `engine/server/sv_pmove.c:467-492` and
   `engine/client/dll_int/cl_pmove.c:758-783` bind those pointers to thin
   wrappers around `PM_PlayerTraceExt` / `PM_TestPlayerPosition` /
   `PM_TraceModel` / `PM_PointContents` — all in `pm_trace.c`. **`pm_trace.c`
   is the file to port for Chunk 5.**
3. **The core kernel is near-pure and needs no BSP file to test.** It depends
   only on a `hull_t` (planes + clipnodes), the single global
   `world.version`, `PlaneDiff`, `DIST_EPSILON`, and vector macros. The
   engine's own box hull (`box_clipnodes16` + 6 planes) is a ready-made
   synthetic hull. **Golden vectors can be hand-derived / self-generated
   against an in-memory synthetic `hull_t` — no `.bsp` asset required.**
4. **Determinism decision is ACCEPTED** (Q-18 PM_FP_MODEL): keep `float`, pin
   `/fp:precise` + `-ffp-contract=off`, match `pm_trace.c` epsilons exactly,
   gate with golden trace fixtures.

---

## 1. Core hull trace

### 1.1 `PM_HullPointContents` — pm_trace.c:113-137

Node walk returning a `CONTENTS_*` value (always negative). Iterative;
branches on `world.version` (a global `uint32_t`) to pick 16- vs 32-bit
clipnodes.

```text
int PM_HullPointContents( hull_t *hull, int num, const vec3_t p ):
    if !hull || !hull->planes: return CONTENTS_NONE        # 0; "fantom bmodels"
    if world.version == QBSP2_VERSION:                     # 'BSP2' fourcc
        while num >= 0:
            plane = &hull->planes[ hull->clipnodes32[num].planenum ]
            num   = hull->clipnodes32[num].children[ PlaneDiff(p, plane) < 0 ]
    else:                                                  # classic HL/Q1 BSP
        while num >= 0:
            plane = &hull->planes[ hull->clipnodes16[num].planenum ]
            num   = hull->clipnodes16[num].children[ PlaneDiff(p, plane) < 0 ]
    return num                                             # negative == contents
```

- **Side test:** `PlaneDiff(p, plane) < 0` → boolean 0/1 → indexes
  `children[]`. `children[0]` = front/on-plane side (dist ≥ 0),
  `children[1]` = back side. **On-plane (exactly 0) goes to `children[0]`
  (front).**
- **`PlaneDiff` (public/xash3d_mathlib.h:140), axial fast path:**
  `PlaneDiff(point,plane) = ((plane->type < 3 ? point[plane->type] :
  DotProduct(point, plane->normal)) - plane->dist)`.
  `type` ∈ {0,1,2} (PLANE_X/Y/Z) skips the dot product; `type == 3`
  (PLANE_NONAXIAL) uses `DotProduct`. **A faithful port MUST replicate this
  branch** — for axial planes the two are algebraically equal but can differ
  in the last ULP; keep it identical.
<!-- pyml disable-next-line no-reversed-links -->
- `PlaneDiff` uses single-precision `DotProduct` (`xash3d_mathlib.h:96`: `(x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]`), NOT `DotProductPrecise`.

### 1.2 `PM_RecursiveHullCheck` — pm_trace.c:200-323 (THE kernel)

Signature: `qboolean PM_RecursiveHullCheck( hull_t *hull, int num, float p1f,
float p2f, vec3_t p1, vec3_t p2, pmtrace_t *trace )`. `p1f`/`p2f` are
fractions (0..1) of `p1`/`p2` along the original ray; `p1`/`p2` are points in
the hull's LOCAL frame. Return: `true` = subsegment fully empty/open (no
impact); `false` = impact recorded or ran into solid. Callers ignore the bool
and read `trace`.

Exact constant: **`DIST_EPSILON = (1.0f / 32.0f) = 0.03125f`** —
com_model.h:572.

Faithful pseudocode (the two `goto loc0` are loop edges; the two mid-body
`PM_RecursiveHullCheck(...)` calls are true recursion):

```text
PM_RecursiveHullCheck(hull, num, p1f, p2f, p1, p2, trace):
loc0:
  # --- (A) leaf / contents ---
  if num < 0:
      if num != CONTENTS_SOLID:                 # -2
          trace->allsolid = false
          if num == CONTENTS_EMPTY: trace->inopen  = true    # -1
          else:                     trace->inwater = true    # water/slime/lava/etc.
      else:
          trace->startsolid = true
      return true                                # empty

  # --- (B) degenerate / empty hull ---
  if hull->firstclipnode >= hull->lastclipnode:
      trace->allsolid = false
      trace->inopen   = true
      return true

  if num < hull->firstclipnode or num > hull->lastclipnode:
      Host_Error("PM_RecursiveHullCheck: bad node number %i", num)

  # --- (C) load node (world.version selects 16/32-bit clipnodes) ---
  children[0], children[1] = clipnodes[num].children[0], [1]
  plane = &hull->planes[ clipnodes[num].planenum ]

  t1 = PlaneDiff(p1, plane)
  t2 = PlaneDiff(p2, plane)

  # --- (D) both sides same → descend without split ---
  if t1 >= 0.0f and t2 >= 0.0f: num = children[0]; goto loc0     # both front
  if t1 <  0.0f and t2 <  0.0f: num = children[1]; goto loc0     # both back

  # --- (E) segment crosses; put crosspoint DIST_EPSILON on the NEAR side ---
  side = (t1 < 0.0f)                             # 1 if p1 is behind plane
  if side: frac = (t1 + DIST_EPSILON) / (t1 - t2)
  else:    frac = (t1 - DIST_EPSILON) / (t1 - t2)
  if frac < 0.0f: frac = 0.0f
  if frac > 1.0f: frac = 1.0f

  midf = p1f + (p2f - p1f) * frac
  mid  = VectorLerp(p1, frac, p2)               # mid[i] = p1[i] + frac*(p2[i]-p1[i])

  # --- (F) trace the NEAR side first ---
  if not PM_RecursiveHullCheck(hull, children[side], p1f, midf, p1, mid, trace):
      return false                               # hit found nearer; stop

  # --- (G) is the FAR side open at the crosspoint? ---
  if PM_HullPointContents(hull, children[side^1], mid) != CONTENTS_SOLID:
      return PM_RecursiveHullCheck(hull, children[side^1], midf, p2f, mid, p2, trace)

  # --- (H) far side solid at mid → this is the impact ---
  if trace->allsolid:
      return false                               # never got out of solid

  if not side:                                   # hit front face
      trace->plane.normal = plane->normal
      trace->plane.dist   = plane->dist
  else:                                          # hit back face → flip
      trace->plane.normal = -plane->normal       # VectorNegate
      trace->plane.dist   = -plane->dist

  # --- (I) back up until mid is out of solid (whole hull, from firstclipnode) ---
  while PM_HullPointContents(hull, hull->firstclipnode, mid) == CONTENTS_SOLID:
      frac -= 0.1f
      if frac < 0.0f:
          trace->fraction = midf
          trace->endpos   = mid
          Con_Reportf(S_WARN "trace backed up past 0.0\n")
          return false
      midf = p1f + (p2f - p1f) * frac
      mid  = VectorLerp(p1, frac, p2)

  # --- (J) record impact ---
  trace->fraction = midf
  trace->endpos   = mid                          # NOTE: LOCAL frame; caller usually overwrites
  return false
```

Exact float constants inside: `DIST_EPSILON = 0.03125f` (step E), frac clamp
`[0.0f, 1.0f]` (E), back-up decrement `frac -= 0.1f` and fail threshold
`frac < 0.0f` (I). **No other magic numbers.** No
`ON_EPSILON`/`FRAC_EPSILON`/`BACKFACE_EPSILON` appears in this function.

Semantics to preserve exactly:

- **`side = (t1 < 0.0f)`**: strict `<`. When `t1 == 0` exactly, `side = 0`
  (front).
- **Near-side epsilon sign** flips with `side` (E): behind→`+DIST_EPSILON`,
  front→`-DIST_EPSILON` — pulls the crosspoint back toward `p1` so the
  reported impact never penetrates.
- **`allsolid` starts `true`** (set by `PM_InitPMTrace`, pm_local.h:75-81)
  and is cleared only in (A) on a non-solid leaf. So `allsolid` stays true
  iff every leaf along the ray was solid. (H)'s `if trace->allsolid: return
  false` suppresses recording an impact plane when the ray started buried.
- **`startsolid`** set in (A) whenever any `CONTENTS_SOLID` leaf is entered.
- **`fraction` written ONLY in (I)-fail and (J).** A fully-empty ray keeps
  the init value `1.0f`.
- Back-up loop (I) re-evaluates contents from `hull->firstclipnode` (whole
  hull), not from `children[side^1]` — a different node than (G).

### 1.3 Trace setup, hull selection, offset math, output fields

**Init helpers** (pm_local.h): `PM_InitPMTrace(trace, end)` (:75-81) and
`PM_InitTrace(trace, end)` (:67-73): `memset 0`; `endpos = end`;
`allsolid = true`; `fraction = 1.0f`.

**`PM_PlayerTraceExt`** — pm_trace.c:325-533 (main entry; `PM_TraceLine`,
`PM_TraceLineEx`, client/server `pfnPlayerTrace*` call it):

- Accumulator setup (:338-341): `memset(&trace_total,0)`;
  `trace_total.endpos = end`; `trace_total.fraction = 1.0f`;
  `trace_total.ent = -1`.
- Per-physent `PM_InitPMTrace(&trace_bbox, end)` (:458) →
  `PM_RecursiveHullCheck(hull, hull->firstclipnode, 0, 1, start_l, end_l,
  &trace_bbox)` (:476).
- Post-process per ent (:504-529): `if allsolid: startsolid = true` (:504);
  `if startsolid: fraction = 0.0f` (:507); `if !startsolid:` **recompute
  world-space endpos** `VectorLerp(start, fraction, end, endpos)` (:512) and
  for non-rotated **recompute** `plane.dist = DotProduct(endpos,
  plane.normal)` (:521) (rotated: `Matrix4x4_TransformPositivePlane`, :517).
  `if fraction < trace_total.fraction: trace_total = trace_bbox;
  trace_total.ent = i` (:525-528) — nearest hit wins.
- **Fixture note:** world-space `endpos` and (non-rotated) `plane.dist`
  reported to callers are RECOMPUTED here from `fraction`, NOT the values
  written inside the kernel. Kernel-level goldens assert the hull-local
  values; wrapper-level goldens assert the recomputed ones.

**Hull selection — `PM_HullForBsp`** — pm_trace.c:146-176. Maps
`pmove->usehull` → model BSP hull index, then computes the offset:

```text
switch usehull:
  case 1: hull = &pe->model->hulls[3]   # ducked  → BSP head hull
  case 2: hull = &pe->model->hulls[0]   # point   → BSP point hull
  case 3: hull = &pe->model->hulls[2]   # large   → BSP large hull
  default(0): hull = &pe->model->hulls[1] # standing → BSP human hull
offset = hull->clip_mins - host.player_mins[usehull]        # VectorSubtract
offset = offset + pe->origin                                # VectorAdd
```

Ray moved into the hull's local frame (non-rotated, e.g. :454-455):
`start_l = start - offset`, `end_l = end - offset`.

**`host.player_mins/maxs[usehull]`** seeded at `Pmove_Init`
(pm_trace.c:47-54) from static defaults `pm_hullmins`/`pm_hullmaxs`
(:30-45). **Indexed by `usehull` (0..3) — a DIFFERENT numbering than the
model's BSP hull index:**

| usehull | meaning | player_mins | player_maxs | selected BSP hull |
|--------:|---------|-------------|-------------|------------------:|
| 0 | standing | (-16,-16,-36) | (16,16,36) | hulls[1] |
| 1 | ducking | (-16,-16,-18) | (16,16,18) | hulls[3] |
| 2 | point | (0,0,0) | (0,0,0) | hulls[0] |
| 3 | large | (-32,-32,-32) | (32,32,32) | hulls[2] |

At load (`Mod_SetupHull`, mod_bmodel.c:1972-1999) each model hull's
`clip_mins/clip_maxs` are copied so hull index ↔ dimensions line up. For
matched cases `hull->clip_mins - host.player_mins[usehull] == 0`, so
`offset == pe->origin` — but a port must still compute the subtraction
because mods can resize `host.player_mins` at runtime.

**Box hull path — `PM_HullForBox`** — pm_trace.c:90-105 + `PM_InitBoxHull`
:64-80. Turns an AABB into a 6-plane BSP. Planes 0..5 have `type = i>>1`
(PLANE_X,X,Y,Y,Z,Z — all axial fast path), `normal[i>>1] = 1.0`,
`signbits = 0`; `dist` set from maxs[0],mins[0],maxs[1],mins[1],maxs[2],
mins[2] (:92-97). Clipnodes come from shared const `box_clipnodes16/32`
(§5.3 of the BSP deep-dive; chain below). Moving box vs point-model entity
inflates: `mins = pe->mins - host.player_maxs[usehull]`,
`maxs = pe->maxs - host.player_mins[usehull]` (Minkowski; :406-409).

Box clipnode chain (`BOX_CLIPNODES_INITIALIZER`, mod_bmodel.c:567-592):

```text
node0: planenum 0, children {CONTENTS_EMPTY(-1), 1}
node1: planenum 1, children {2, CONTENTS_EMPTY}
node2: planenum 2, children {CONTENTS_EMPTY, 3}
node3: planenum 3, children {4, CONTENTS_EMPTY}
node4: planenum 4, children {CONTENTS_EMPTY, 5}
node5: planenum 5, children {CONTENTS_SOLID(-2), CONTENTS_EMPTY}
```

**`PM_TraceModel`** — pm_trace.c:743-789. Forces `usehull = 2` around
`PM_HullForBsp` (:755-759) → traces against the **point hull `hulls[0]`**.
**Divergence vs `PM_PlayerTraceExt`:** it does NOT recompute
`plane.dist = DotProduct(endpos,normal)`; it keeps the raw clipnode
`plane.dist` (transforms only if rotated, :780-784) and lerps `endpos`
(:786).

**`PM_TestPlayerPosition`** — pm_trace.c:535-657. First runs a full
`PM_PlayerTraceExt(origin→origin)` (:544) to fill `*ptrace`, then loops
physents calling `PM_HullPointContents(hull, firstclipnode, pos_l) ==
CONTENTS_SOLID` (:643); returns first solid ent index or -1.

**Output fields (`pmtrace_t`, common/pmove.h:34-45):** kernel writes
`allsolid, startsolid, inopen, inwater, fraction, endpos, plane.normal,
plane.dist`. Wrapper writes world-space `endpos`, `plane.dist` recompute
(PlayerTraceExt only), `ent`, `hitgroup` (studio only), `deltavelocity`
(`PM_StuckTouch` only). The kernel never touches
`ent`/`hitgroup`/`deltavelocity` — why the `(pmtrace_t*)trace_t*` pun in
sv_world.c is safe (leading fields align through `plane.dist`).

### 1.4 Studio-model special path — DEFERRABLE

`PM_HullForStudio` (:185-193) → `Mod_HullForStudio(...)` builds per-hitbox
mini-hulls; `PM_AllowHitBoxTrace` (:24) gates it. Invoked only when
`pe->studiomodel` set and `PM_STUDIO_BOX`/`PM_STUDIO_IGNORE` flags allow
(:385-403, :573-577). Multi-hull loop (:482-502) picks the nearest hitbox,
sets `hitgroup`. Needs studio model data + animation state — orthogonal to
BSP hull math; callers already branch `hullcount == 1` vs `> 1`.

### 1.5 `pm_surface.c` surface tracing — DEFERRABLE (different algorithm/epsilon)

`PM_TraceTexture` (pm_trace.c:845-858 → pm_surface.c) and
`PM_TraceSurface`/`PM_RecursiveSurfCheck`/`PM_TestLine_r`
(pm_surface.c:109-320) are a DIFFERENT walk over draw `mnode_t` (not clip
`hull_t`), for fence-texture content sampling / `msurface_t*` returns. **They
use a locally redefined `FRAC_EPSILON = (1.0f/32.0f)`** (pm_surface.c:21-22 —
`#undef` shadows the global `com_model.h:573` value `1/1024`) and a different
both-sides test (`t1 >= -FRAC_EPSILON && t2 >= -FRAC_EPSILON` /
`t1 < FRAC_EPSILON && t2 < FRAC_EPSILON`, :124-134), clamp via
`bound(0.0f, frac, 1.0f)`. Depends on `mextrasurf_t->bevel`, texinfo, and
(fence alpha) renderer texture buffers. Server twin: `SV_TraceSurface`/
`SV_TraceTexture` (sv_world.c:1415-1459) reuse `PM_RecursiveSurfCheck`.

### 1.6 `world.c` / `sv_world.c` — canonicity & divergences

- **`engine/common/world.c` contains only `World_TransformAABB`**
  (:29-69). Other `World_*` helpers are inline in `engine/common/world.h`:
  `World_MoveBounds` (:39-56), `World_CombineTraces` (:58-73),
  `RankForContents` (:82-101).
- **`engine/server/sv_world.c` reuses the pm_trace.c kernel.**
  `SV_ClipMoveToEntity` (:836-958) selects hull
  (`SV_HullForEntity`/`SV_HullForBsp`/`SV_HullForStudioModel`), transforms
  rotated entities, then calls `PM_RecursiveHullCheck(...,(pmtrace_t*)trace)`
  (:911/:921). `SV_Move` (:1314-1361) → `SV_ClipMoveToEntity` +
  `SV_ClipToLinks`/`SV_ClipToPortals`. `SV_PointContents`/
  `SV_TruePointContents` (:790-819) reuse `PM_HullPointContents`.
- **No epsilon or loop divergence in the kernel.** Divergences live in the
  WRAPPERS: `SV_HullForBsp` picks hull by entity SIZE (:176-236, with a
  Quake-map `FWORLD_SKYSPHERE` branch); `PM_HullForBsp` picks by `usehull`.
  Plus the `plane.dist` recompute divergence (§1.3): `PM_PlayerTraceExt`/
  `SV_ClipMoveToEntity` (sv_world.c:952) recompute, `PM_TraceModel` doesn't.
- **Portal CSG** (`SV_PortalCSG`, sv_world.c:969-1072) is server-only with
  its own epsilons (`4.0/32.0 = 0.125` portal-near, `+24` side-plane
  expansion, `portalradius = model->radius*0.5`). Out of scope for
  map_loader.

---

## 2. Contents / point queries

- **`PM_PointContents`** — pm_trace.c:685-735. Base contents from world
  `PM_HullPointContents(&physents[0].model->hulls[0], 0, p)` (:697; note
  headnode `0`, not `firstclipnode`). Then for each `SOLID_NOT` brush
  physent with special contents, offset the point (rotated:
  `Matrix4x4_VectorITransform`; else `p - pe->origin`) and if contents `!=
  CONTENTS_EMPTY`, promote via `RankForContents(pe->skin) >
  RankForContents(contents)` (:730).
- **`PM_TruePointContents`** — :665-677. Just world
  `PM_HullPointContents(hulls[0], firstclipnode, p)`; no water-brush merge.
- **`PM_PointContentsPmove`** — :860-870. Wraps `PM_PointContents`, then
  collapses currents to water: `if CONTENTS_CURRENT_DOWN (-14) <= cont <=
  CONTENTS_CURRENT_0 (-9): cont = CONTENTS_WATER (-3)`. (Server twin
  `SV_PointContents`, sv_world.c:812-819, identical clamp.)
- **`RankForContents`** — world.h:82-101. Priority ladder (higher wins):
  EMPTY 0, WATER 1, TRANSLUCENT 2, CURRENT_0..DOWN 3..8, SLIME 9, LAVA 10,
  SKY 11, SOLID 12, default(user/positive) 13.
- **Ladders / water:** ladders are `CONTENTS_LADDER (-16)` `SOLID_NOT`
  physents in `pmove->moveents[]`; the ENGINE does not special-case them in
  pm_trace.c — the mod's `PM_Move` (in the DLL) interprets. Water =
  `CONTENTS_WATER/SLIME/LAVA` via `pe->skin` on water-brush physents. The
  map_loader layer only needs correct per-brush contents. `CONTENTS_NONE
  (0)` (world.h:23) is the "no custom contents" skip sentinel (:362).

**`CONTENTS_*` — common/const.h:586-603:** EMPTY −1, SOLID −2, WATER −3,
SLIME −4, LAVA −5, SKY −6, ORIGIN −7, CLIP −8, CURRENT_0 −9, CURRENT_90 −10,
CURRENT_180 −11, CURRENT_270 −12, CURRENT_UP −13, CURRENT_DOWN −14,
TRANSLUCENT −15, LADDER −16; CONTENTS_NONE 0 (engine/common/world.h:23).

---

## 3. PVS query surface

BSP-PVS primitives live in `engine/common/mod_bmodel.c` (→ map_loader); the
"is entity in PVS" *decision* logic lives in `engine/server/sv_game.c`
(→ server chunk). Signatures in `engine/common/mod_local.h:175-182`.

| Function | File:line | Signature | Role |
|---|---|---|---|
| `Mod_PointInLeaf` | mod_bmodel.c:1122 | `mleaf_t *(const vec3_t, mnode_t*, model_t*)` | Walk `node_child(node, PlaneDiff <= 0)` until `contents < 0`. **Tie-break `<=0` → BACK child — opposite of the hull walk's `<0`.** |
| `Mod_GetPVSForPoint` | mod_bmodel.c:1145 | `byte *(const vec3_t)` | Decompress point-leaf PVS into `g_visdata`; **NULL if leaf invalid or `cluster < 0`** (callers treat NULL as fullvis). |
| `Mod_FatPVS` | mod_bmodel.c:1211 | `int (org, radius, visbuffer, visbytes, merge, fullvis, phs)` | OR of leaf vis/PHS within `radius`, via `Mod_FatPVS_RecursiveBSPNode` (:1168). Full-vis (0xFF) when `fullvis`, no visdata, bad leaf, or PHS absent. Callers: `pfnSetFatPVS/PAS` (sv_game.c:4266/4289), `cl_render.c:23`. |
| `Mod_BoxLeafnums` | mod_bmodel.c:1296 | `int (mins, maxs, int *list, listsize, int *topnode)` | Gather clusters an AABB touches (`Mod_BoxLeafnums_r`, `BOX_ON_PLANE_SIDE`). |
| `Mod_BoxVisible` | mod_bmodel.c:1325 | `qboolean (mins, maxs, const byte *visbits)` | **The "is AABB visible in PVS" answer**: BoxLeafnums → any `CHECKVISBIT` → true. True if `visbits` NULL. |
| `Mod_HeadnodeVisible` | sv_game.c:4299 | `qboolean (model_t*, mnode_t*, visbits, int *lastleaf)` | "Any visible leaf under headnode" — slow path for many-leaf entities. **Server.** |
| `pfnCheckVisibility` | sv_game.c:4329 | `int (const edict_t*, byte *pset)` | The server's entity-in-PVS decision (leaf cache, headnode fallback). **Server.** |

Supporting: `CHECKVISBIT(vis,b)` — mod_local.h:27
(`(b) >= 0 ? vis[b>>3] & (1<<(b&7)) : false`). `MAX_BOX_LEAFS = 256`
(com_model.h:575). `MAX_ENT_LEAFS_32 = 24` / `MAX_ENT_LEAFS_16 = 48`
(edict.h:19-20). Entity→leaf linking (`SV_FindTouchedLeafs`,
sv_world.c:593-633; `SV_LinkEdict`, :640-706) is server-chunk territory.

**Split:** `Mod_PointInLeaf`, `Mod_GetPVSForPoint`, `Mod_FatPVS`,
`Mod_BoxLeafnums`, `Mod_BoxVisible`, `Mod_DecompressPVS` → **map_loader**.
`pfnCheckVisibility`, `Mod_HeadnodeVisible`, entity leaf caching,
`SV_LinkEdict` → **server chunk**. (There is no `Mod_CheckBoxVisible`; the
AABB answer is `Mod_BoxVisible`.)

---

## 4. Constants inventory

| Constant | Value | File:line | Used by |
|---|---|---|---|
| `DIST_EPSILON` | `1.0f/32.0f` = **0.03125f** | com_model.h:572 | kernel crosspoint nudge |
| back-up decrement | `frac -= 0.1f` (literal) | pm_trace.c:305 | kernel back-up loop |
| back-up fail threshold | `frac < 0.0f` | pm_trace.c:307 | kernel back-up loop |
| frac clamp | `[0.0f, 1.0f]` | pm_trace.c:269-270 | kernel |
| `FRAC_EPSILON` (global) | `1.0f/1024.0f` | com_model.h:573 | **NOT** hull trace (shadowed in pm_surface.c) |
| `FRAC_EPSILON` (surface-local) | `1.0f/32.0f` | pm_surface.c:22 | surface walk only |
| `BACKFACE_EPSILON` | `0.01f` | com_model.h:574 | render, not trace |
| `ON_EPSILON` | `0.1f` | xash3d_mathlib.h:72 | pmove reconciliation, not kernel |
| `STOP_EPSILON` | `0.1f` | xash3d_mathlib.h:71 | velocity clip, not kernel |
| `EQUAL_EPSILON` | `0.001f` | xash3d_mathlib.h:70 | `Q_equal`, not kernel |
| `MAX_CLIP_PLANES` | `5` | pm_defs.h:21 | `SV_FlyMove`, not kernel |
| `MAX_MAP_HULLS` | `4` | bspfile.h:50 | hull tables |
| `MAX_PHYSENTS` / `MAX_MOVEENTS` | `600` / `64` | pm_defs.h:19-20 | playermove arrays |
| `MAX_BOX_LEAFS` | `256` | com_model.h:575 | `Mod_BoxVisible` |
| `MAX_ENT_LEAFS_16/_32` | `48` / `24` | edict.h:20/:19 | PVS entity leaf cache |
| `AREA_NODES` / `AREA_DEPTH` | `32` / `4` | world.h:33/:34 | server area tree |
| Plane types | PLANE_X 0, PLANE_Y 1, PLANE_Z 2, PLANE_NONAXIAL 3 | xash3d_mathlib.h:65-68 | `PlaneDiff` fast path |
| BSP versions | 29, 30, `'BSP2'` | bspfile.h:31-33 | clipnode width branch |
| Hull dims | see §1.3 table | pm_trace.c:30-45 | `pm_hullmins/maxs` |
| `CONTENTS_*` | −1..−16, NONE 0 | const.h:586-603, world.h:23 | contents checks |
| `PM_*` trace flags | NORMAL 0, STUDIO_IGNORE 0x01, STUDIO_BOX 0x02, GLASS_IGNORE 0x04, WORLD_ONLY 0x08, CUSTOM_IGNORE 0x10 | pm_defs.h:23-28 | wrapper filters |
| `PM_TRACELINE_*` | PHYSENTSONLY 0, ANYVISIBLE 1 | pm_defs.h:31-32 | `PM_TraceLine` |
| Portal CSG | `0.125`, `+24`, `radius*0.5f` | sv_world.c:999-1024 | server-only |

Box hull plane setup: `type = i>>1`, `normal[i>>1] = 1.0f`, `signbits = 0`,
`dist` from maxs/mins interleaved (pm_trace.c:73-97).

---

## 5. Fixture reconnaissance

### 5.1 BSP assets and BSP-writing tools — NONE in repo

- **Zero `.bsp` files** anywhere in the tree. No maps checked in.
- **No BSP writer / qbsp / csg utility.** Only the `mod_bmodel.c` reader and
  `bspfile.h` exist. A real `.bsp` cannot be generated from in-repo tooling.

### 5.2 Existing test infrastructure (prior art)

- **xash3dpp tests** (`xash3dpp/tests/**`): lightweight custom harness
  `test_helpers.hpp` (`CHECK*/REQUIRE/RUN_TEST`, `g_pass/g_fail`),
  CTest-integrated. Golden-vector tests fit directly.
- **Legacy C test dir** `public/tests/` (atlas/efp/filebase/parsefile/
  swapstruct) — no trace/BSP tests.
- **Caution:** the fixture-harness files referenced in
  `Documentation/codex/modern/engine/*.md` (e.g.
  `world_trace_fixture_common.hpp`) are ASPIRATIONAL — they exist only as
  text in those planning docs, not as code.

### 5.3 Can `pm_trace.c` kernels be compiled standalone? — YES

Dependency surface of the two kernel functions:

- **Types:** `hull_t` (com_model.h:305-317), `mplane_t`
  (xash3d_mathlib.h:169-176), `mclipnode16_t`/`mclipnode32_t`
  (com_model.h:56-66), `pmtrace_t` (pmove.h:34-45).
- **Globals:** exactly one — `world.version` (uint32_t). Trivial to stub.
- **Macros/inlines:** `PlaneDiff`, `VectorLerp`, `VectorCopy`,
  `VectorNegate`, `DotProduct` (header-only), `DIST_EPSILON`/`CONTENTS_*`.
- **External funcs:** only `Host_Error` and `Con_Reportf` — stub.

**Conclusion:** no real `.bsp` needed. Build a synthetic `hull_t` in memory
exactly like the engine's box hull (§1.3 chain + 6 axial planes) for the
first golden vectors (front/back/edge/corner hits, startsolid, allsolid,
DIST_EPSILON pullback, `frac -= 0.1f` back-up). For richer trees (interior
splits, non-axial planes exercising the `type==3` DotProduct path,
CONTENTS_WATER leaves for inopen/inwater), hand-author additional
`mplane_t[]` + `mclipnode16_t[]` tables.

**Recommended fixture path:** (1) port the kernels verbatim; (2) drive them
from synthetic in-memory hulls in `test_helpers.hpp`-style tests; (3)
hand-derive expected `{fraction, startsolid, allsolid, inopen, inwater,
plane.normal, plane.dist, endpos_local}` per ray; (4) separately test the
`PM_HullForBsp` offset math and the wrapper world-space endpos/`plane.dist`
recompute. Hand-derivation is tractable for axial boxes (exact in float);
non-axial cases cross-check against a compiled-legacy harness.

---

## 6. Uncertainties / flags

1. **`endpos` frame ambiguity.** Kernel writes hull-LOCAL endpos; wrappers
   overwrite with world-space lerp for the non-startsolid case, but the
   startsolid case keeps whatever was there (fraction forced 0). Decide which
   layer each golden asserts.
2. **`plane.dist` divergence** between `PM_TraceModel` (raw clipnode dist)
   and `PM_PlayerTraceExt`/`SV_ClipMoveToEntity` (dot recompute).
3. **`usehull` vs model-hull index numbering** coincide dimensionally only
   because `Mod_SetupHull` copies dims to match — beware when refactoring.
4. **The `(pmtrace_t*)trace_t*` pun** (sv_world.c) relies on layout
   coincidence through `plane.dist`; a C++ rewrite should use a shared struct
   or explicit adapter.
5. **`world.version` is a process-global** read on every node; a reentrant
   port should thread version/width through the model (legacy already has
   `MODEL_QBSP2` in `flags` — prefer that, or normalize width at load).
6. **Studio and portal-CSG paths excluded** from Chunk 5; real code paths in
   the same files but dependent on studio data / server portals.
7. Exact `FATPVS_RADIUS`/`FATPHS_RADIUS` values: `8.0f` each
   (mod_local.h:31-32).

---

## 7. PHS build path (Q-19) — added 2026-07-06

The §3 table covers the PVS *queries* and the two server-side consumers
(`Mod_FatPVS`'s phs parameter, `Mod_HeadnodeVisible`). It does not detail how
the **PHS itself is built** — that is the `Mod_CalcPHS` path, which Q-19
(`PHS_PLACEMENT`) resolves *into* `map_loader` (BSP-derived immutable query
data), not the server. This section fills that gap for the recon record; the
shipped code is `phs.hpp` / `phs.cpp`.

**Definition.** The PHS ("potentially hearable set") answers *"can anything
audible to leaf i be heard from leaf j"* — used for sound/event culling. It is
the transitive one-hop closure of the PVS: **row i = PVS row i OR the PVS row
of every cluster set in row i**.

**Legacy build — `Mod_CalcPHS` (mod_bmodel.c:3730).** Runs once per map load,
**only for multiplayer servers** (`SV_Active && maxclients > 1`,
mod_bmodel.c:4353-4354); SP and listen-server-with-one-player skip it. Shape:

```text
Mod_CalcPHS():
  rowbytes = (visclusters + 7) >> 3           # bits→bytes, one bit per cluster
  rowwords = align4(rowbytes) / 4             # rows are 32-bit aligned
  for i in 0 .. visclusters-1:
      decompress PVS row i  → scan[]           # Mod_DecompressPVS
      copy scan → hearing[]                     # start PHS row = PVS row
      for each cluster j with bit set in scan:  # word-at-a-time scan of set bits
          decompress PVS row j → in[]
          hearing[] |= in[]                      # OR in everything j can see
      # hearing[] is now PHS row i
      count set bits (developer stat only)
      compress hearing[] → world.compressed_phs (Mod_CompressPVS), record phsofs[i]
```

- **`Mod_CompressPVS` (mod_bmodel.c:1088)** is the same zero-RLE codec as the
  PVS: nonzero bytes copy through; a zero byte is followed by the length of the
  zero run (max 255 per pair). The compressed rows are concatenated into
  `world.compressed_phs`; `world.phsofs[i]` is the byte offset of row i.
- **OpenMP.** Legacy parallelises the outer `for i` loop with `#pragma omp
  parallel for` when built with OpenMP (each row is independent — a pure
  scatter-free fold). Output is identical with or without it.
- **Cost.** O(visclusters² / 8) bytes of PVS decompression — the reason it is
  MP-only and load-time-only.

**As-built (`phs.cpp`).** `build_phs(const WorldData&)` is a faithful
**single-threaded** fold (the OpenMP parallelism is dropped — byte-identical
output; re-parallelising internally at load is an allowed follow-up per the
server-boundary OQ-9 posture). It returns an immutable `PhsTable`
(`blob_` = `compressed_phs`, `offsets_` = `phsofs`) — empty when the map has no
visdata (legacy early-return). Rows are built `align4(visbytes)` wide with zero
padding, matching the legacy 32-bit row alignment. `compress_pvs` is the shared
codec. The *trigger* stays server-side: the server calls `build_phs` during MP
spawn; the table is immutable afterward (Q-6). Hardening vs legacy:
`PhsTable::compressed_row(i)` bounds-checks the row index (legacy indexes
`phsofs` unchecked → an out-of-range row decompresses as all-visible per the
module convention); the `vis_stats` developer counters are not ported.

**Consumers (recap, both shipped in `phs.hpp`).** `fat_phs` is the phs path of
`Mod_FatPVS` — ORs the PHS row of every leaf within `radius` of a point, with
the extra legacy "requested PHS but we have none → full visibility" rule
(mod_bmodel.c:1230-1234); it shares the leaf-gather walk with `fat_pvs`
(`private/map_loader/fat_vis.hpp`). `headnode_visible` is `Mod_HeadnodeVisible`
as an explicit front-first recursion preserving traversal order. The
`pfnCheckVisibility` *decision* logic and entity leaf caching remain
server-chunk work (§3 split unchanged).
