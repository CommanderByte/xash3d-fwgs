# Deep Dive: Legacy BSP Loader (v29/v30/BSP2) — On-Disk Format & Load Path

*Recon brief produced 2026-07-04 by a read-only survey agent ahead of Chunk 5
(map_loader). Scope: everything needed to load a BSP into an in-memory world
model usable for **PVS + hull traces**, with byte-exact parity. Render-only
data is identified and deferred. Line numbers are against the working tree on
that date; behaviour references, not design constraints.*

Primary sources:

- `common/bspfile.h` — on-disk format
- `engine/common/mod_bmodel.c` — loader (~4760 lines)
- `common/com_model.h` — in-memory structs
- `engine/common/mod_local.h` — `world_static_t`, prototypes
- `public/xash3d_mathlib.h` — `mplane_t`
- `public/crclib.c` / `crclib.h` — CRC32
- `engine/common/pm_trace.c` — hull dimension tables
- `engine/server/sv_game.c` — server PVS consumers

**Global endianness assumption:** all on-disk integers/floats are
**little-endian**. Byte-swapping only happens under `#if XASH_BIG_ENDIAN` via
`le_struct_*` swap defs (`mod_bmodel.c:207-344`). On a little-endian target,
read structs directly. There are **no `#pragma pack` / packed attributes** on
any BSP struct — they rely on natural alignment. `vec3_t` is `float[3]`
(`common/xash3d_types.h:32`).

---

## 1. Versions, Magic Numbers, Header Structures

### 1.1 Version constants (`bspfile.h:31-43`)

```c
#define Q1BSP_VERSION 29   // quake1 regular version (beta is 28)
#define HLBSP_VERSION 30   // half-life regular version
#define QBSP2_VERSION (('B')|('S'<<8)|('P'<<16)|('2'<<24))   // 'BSP2' = 0x32505342
#define IDEXTRAHEADER (('X')|('A'<<8)|('S'<<16)|('H'<<24))   // 'XASH' — BSP30ext extra header id
#define EXTRA_VERSION 4    // ver 1=XashXT, 2/3=P2:savior (historical)
#define IDBSPXHEADER  (('B')|('S'<<8)|('P'<<16)|('X'<<24))   // 'BSPX'
#define DELUXEMAP_VERSION 1
#define IDDELUXEMAPHEADER (('Q')|('L'<<8)|('I'<<16)|('T'<<24)) // 'QLIT' — external .lit/.dlit header
```

- **Supported by loader:** 29 (Q1BSP), 30 (HLBSP), `'BSP2'` (QBSP2). There is
  **no "BSP31"** distinct version; "bsp31 legacy" appears only as a texinfo
  flag name (`TEX_EXTRA_LIGHTMAP`, `bspfile.h:134`). Extended HLBSP maps are
  still version 30 but carry the `XASH`/`EXTRA_VERSION 4` extra header
  ("BSP30ext").
- **Dispatch** by first 4 bytes (the `version` field) in `model.c:347-362`:
  `Q1BSP_VERSION`/`HLBSP_VERSION`/`QBSP2_VERSION` → `Mod_LoadBrushModel`.
  (Unlike Quake's `IDBSPHEADER`, there is no separate magic — the version int
  *is* the discriminator.)

### 1.2 Header directory structs (`bspfile.h:152-183`)

```c
typedef struct { int32_t fileofs; int32_t filelen; } dlump_t;                    // 8 bytes
typedef struct { int32_t version; dlump_t lumps[HEADER_LUMPS]; } dheader_t;      // 4 + 15*8 = 124 bytes
typedef struct { int32_t id; int32_t version; dlump_t lumps[EXTRA_LUMPS]; } dextrahdr_t; // 8 + 12*8 = 104 bytes
typedef struct { char lumpname[24]; int32_t fileofs; int32_t filelen; } dbspx_lump_t;    // 32 bytes
typedef struct { int32_t id; int32_t numlumps; dbspx_lump_t lumps[]; } dbspx_hdr_t;      // 8-byte header + flex array
```

The extra header (`dextrahdr_t`) sits **immediately after `dheader_t`** at
file offset `sizeof(dheader_t)` = 124 (`mod_bmodel.c:785`, `:4245`). The BSPX
header is found by scanning to end-of-lumps and 4-byte aligning (see §9.17,
`Mod_FindBSPX`).

---

## 2. Lump Index Tables

### 2.1 Standard lumps (`bspfile.h:92-110`) — `HEADER_LUMPS = 15`

| Idx | Name | Idx | Name |
|----|------|----|------|
| 0 | LUMP_ENTITIES | 8 | LUMP_LIGHTING |
| 1 | LUMP_PLANES | 9 | LUMP_CLIPNODES |
| 2 | LUMP_TEXTURES | 10 | LUMP_LEAFS |
| 3 | LUMP_VERTEXES | 11 | LUMP_MARKSURFACES |
| 4 | LUMP_VISIBILITY | 12 | LUMP_EDGES |
| 5 | LUMP_NODES | 13 | LUMP_SURFEDGES |
| 6 | LUMP_TEXINFO | 14 | LUMP_MODELS |
| 7 | LUMP_FACES | — | HEADER_LUMPS = 15 |

Matches Quake/GoldSrc ordering exactly.

### 2.2 Extra lumps (BSP30ext, `dextrahdr_t`) (`bspfile.h:113-128`) — `EXTRA_LUMPS = 12`

`LUMP_LIGHTVECS=0` (deluxemap), `LUMP_FACEINFO=1`, `LUMP_CUBEMAPS=2`,
`LUMP_VERTNORMALS=3`, `LUMP_LEAF_LIGHTING=4`, `LUMP_WORLDLIGHTS=5`,
`LUMP_COLLISION=6`, `LUMP_AINODEGRAPH=7`, `LUMP_SHADOWMAP=8`,
`LUMP_VERTEX_LIGHT=9`, `LUMP_UNUSED0=10`, `LUMP_UNUSED1=11`.
**Only 3 are actually loaded** (`extlumps[]`, `mod_bmodel.c:512-543`):
`LUMP_LIGHTVECS`→deluxdata, `LUMP_FACEINFO`→faceinfo,
`LUMP_SHADOWMAP`→shadowdata. All three are **render-only** for map_loader
purposes.

### 2.3 BSPX lumps loaded (`bspxlumps[]`, `mod_bmodel.c:545-563`)

`"RGBLIGHTING"`→rgblightdata, `"LIGHTINGDIR"`→deluxdata. Both **render-only**.

---

## 3. On-Disk Structures (exact layouts + byte sizes)

Byte sizes computed for a little-endian, naturally-aligned target. **Two
variants exist for several lumps:** the classic 16-bit form and a 32-bit
`*32_t` form used by BSP2 (and by BSP30ext clipnodes). The loader picks
per-map (see §5/§9).

```c
// dmodel_t — LUMP_MODELS — 64 bytes    (bspfile.h:185-194)
typedef struct {
    vec3_t  mins;                     // off 0
    vec3_t  maxs;                     // off 12
    vec3_t  origin;                   // off 24  (for sounds/lights)
    int32_t headnode[MAX_MAP_HULLS];  // off 36  (MAX_MAP_HULLS=4 → 16 bytes)
    int32_t visleafs;                 // off 52  (NOT including solid leaf 0)
    int32_t firstface;                // off 56
    int32_t numfaces;                 // off 60
} dmodel_t;

// dmiptexlump_t — start of LUMP_TEXTURES — header 4 bytes + 4*nummiptex  (bspfile.h:196-200)
typedef struct {
    int32_t nummiptex;
    int32_t dataofs[4];   // declared [4] but really [nummiptex]; -1 = no data for that miptex
} dmiptexlump_t;

// dvertex_t — LUMP_VERTEXES — 12 bytes  (bspfile.h:202-205)
typedef struct { vec3_t point; } dvertex_t;

// dplane_t — LUMP_PLANES — 20 bytes  (bspfile.h:207-212)
typedef struct {
    vec3_t  normal;   // off 0
    float   dist;     // off 12
    int32_t type;     // off 16  (PLANE_X..PLANE_ANYZ)
} dplane_t;

// dnode_t — LUMP_NODES (16-bit) — 24 bytes  (bspfile.h:214-222)
typedef struct {
    int32_t  planenum;      // off 0
    int16_t  children[2];   // off 4   negative = -(leafs+1), not nodes
    int16_t  mins[3];       // off 8   sphere culling
    int16_t  maxs[3];       // off 14
    uint16_t firstface;     // off 20
    uint16_t numfaces;      // off 22  (both sides)
} dnode_t;

// dnode32_t — LUMP_NODES (BSP2) — 44 bytes  (bspfile.h:224-232)
typedef struct {
    int32_t planenum;       // off 0
    int32_t children[2];    // off 4   negative = -(leafs+1)
    float   mins[3];        // off 12
    float   maxs[3];        // off 24
    int32_t firstface;      // off 36
    int32_t numfaces;       // off 40
} dnode32_t;

// dleaf_t — LUMP_LEAFS (16-bit) — 28 bytes  (bspfile.h:236-248)
typedef struct {
    int32_t  contents;                    // off 0
    int32_t  visofs;                      // off 4   -1 = no vis info
    int16_t  mins[3];                     // off 8   frustum culling
    int16_t  maxs[3];                     // off 14
    uint16_t firstmarksurface;            // off 20
    uint16_t nummarksurfaces;             // off 22
    uint8_t  ambient_level[NUM_AMBIENTS]; // off 24  (NUM_AMBIENTS=4)
} dleaf_t;

// dleaf32_t — LUMP_LEAFS (BSP2) — 44 bytes  (bspfile.h:250-262)
typedef struct {
    int32_t contents;                     // off 0
    int32_t visofs;                       // off 4
    float   mins[3];                      // off 8
    float   maxs[3];                      // off 20
    int32_t firstmarksurface;             // off 32
    int32_t nummarksurfaces;              // off 36
    uint8_t ambient_level[NUM_AMBIENTS];  // off 40  (4 bytes) → 44 total
} dleaf32_t;

// dclipnode_t — LUMP_CLIPNODES (16-bit) — 8 bytes  (bspfile.h:264-268)
typedef struct {
    int32_t planenum;      // off 0
    int16_t children[2];   // off 4   negative = contents
} dclipnode_t;

// dclipnode32_t — LUMP_CLIPNODES (BSP2 / BSP30ext) — 12 bytes  (bspfile.h:270-274)
typedef struct {
    int32_t planenum;      // off 0
    int32_t children[2];   // off 4   negative = contents
} dclipnode32_t;

// dtexinfo_t — LUMP_TEXINFO — 40 bytes  (bspfile.h:276-282)
typedef struct {
    float   vecs[2][4];    // off 0   texmatrix [s/t][xyz offset], 32 bytes
    int32_t miptex;        // off 32
    int16_t flags;         // off 36  (TEX_* flags)
    int16_t faceinfo;      // off 38  -1 = none, else index into dfaceinfo_t
} dtexinfo_t;

// dfaceinfo_t — LUMP_FACEINFO (extra) — 22 bytes  (bspfile.h:284-290)
typedef struct {
    char     landname[16]; // off 0
    uint16_t texture_step; // off 16  default 16
    uint16_t max_extent;   // off 18  default 16
    int16_t  groupid;      // off 20  -1 = no group  → 22 total
} dfaceinfo_t;

typedef uint16_t dmarkface_t;    // LUMP_MARKSURFACES 16-bit — 2 bytes  (bspfile.h:292)
typedef int32_t  dmarkface32_t;  // LUMP_MARKSURFACES BSP2   — 4 bytes  (bspfile.h:293)
typedef int32_t  dsurfedge_t;    // LUMP_SURFEDGES           — 4 bytes  (bspfile.h:295)

// dedge_t — LUMP_EDGES (16-bit) — 4 bytes  (bspfile.h:299-302)
typedef struct { uint16_t v[2]; } dedge_t;
// dedge32_t — LUMP_EDGES (BSP2) — 8 bytes  (bspfile.h:304-307)
typedef struct { int32_t v[2]; } dedge32_t;

// dface_t — LUMP_FACES (16-bit) — 20 bytes  (bspfile.h:309-321)
typedef struct {
    uint16_t planenum;          // off 0
    int16_t  side;              // off 2
    int32_t  firstedge;         // off 4   supports >64k edges
    int16_t  numedges;          // off 8
    int16_t  texinfo;           // off 10
    uint8_t  styles[LM_STYLES]; // off 12  (LM_STYLES=4)
    int32_t  lightofs;          // off 16  byte offset into lightdata; -1 = none
} dface_t;

// dface32_t — LUMP_FACES (BSP2) — 28 bytes  (bspfile.h:323-335)
typedef struct {
    int32_t planenum;           // off 0
    int32_t side;               // off 4
    int32_t firstedge;          // off 8
    int32_t numedges;           // off 12
    int32_t texinfo;            // off 16
    uint8_t styles[LM_STYLES];  // off 20  (4 bytes)
    int32_t lightofs;           // off 24  → 28 total
} dface32_t;
```

`LM_STYLES = 4` (`bspfile.h:62`); `NUM_AMBIENTS = 4` (`bspfile.h:140-147`,
enum WATER/SKY/SLIME/LAVA).

**miptex payload** (`mip_t`, in `common/wadfile.h:79-85`, **not** bspfile.h —
pointed into by `dmiptexlump_t.dataofs[i]`):

```c
typedef struct mip_s {
    char         name[16];   // off 0
    unsigned int width;      // off 16
    unsigned int height;     // off 20
    unsigned int offsets[4]; // off 24  four mip levels; 40 bytes total
} mip_t;
```

When `offsets[0] > 0` the texel data is embedded in-BSP (GoldSrc); when
`<= 0` the texture lives in an external WAD (name-only entry). Payload size =
`sizeof(mip_t) + (w*h*85)>>6` (the `>>6` = sum of 1 + 1/4 + 1/16 + 1/64 mip
levels), optionally `+ 2 + 768` for an embedded custom palette
(`Mod_CalculateMipTexSize`, `mod_bmodel.c:664-671`;
`MIPTEX_CUSTOM_PALETTE_SIZE_BYTES = sizeof(int16_t)+768`, line 32).

### Texture flags (`bspfile.h:130-135`)

`TEX_SPECIAL=BIT(0)`, `TEX_WORLD_LUXELS=BIT(1)`, `TEX_AXIAL_LUXELS=BIT(2)`,
`TEX_EXTRA_LIGHTMAP=BIT(3)`, `TEX_SCROLL=BIT(6)`.

---

## 4. MAX_MAP_* Limits & Extended-Limit Support

### 4.1 Hard limits (`bspfile.h:50, 67-89`)

```text
MAX_MAP_HULLS            4
MAX_MAP_CLIPNODES_HLBSP  32767        // 16-bit clipnode child cap (0x7FFF)
MAX_MAP_CLIPNODES_BSP2   524288
MAX_MAP_MODELS           2048
MAX_MAP_ENTSTRING        0x200000     // 2 MB
MAX_MAP_PLANES           131072
MAX_MAP_NODES            262144
MAX_MAP_CLIPNODES        MAX_MAP_CLIPNODES_BSP2 (524288)
MAX_MAP_LEAFS            131072       // "CRITICAL to run ad_sepulcher"
MAX_MAP_VERTS            524288
MAX_MAP_FACES            262144
MAX_MAP_MARKSURFACES     524288
MAX_MAP_ENTITIES         8192         // network limit
MAX_MAP_TEXINFO          MAX_MAP_FACES (262144)
MAX_MAP_EDGES            0x100000     // 1048576
MAX_MAP_SURFEDGES        0x200000     // 2097152
MAX_MAP_TEXTURES         2048
MAX_MAP_MIPTEX           0x2000000    // 32 MB
MAX_MAP_LIGHTING         0x2000000    // 32 MB (may contain deluxemaps)
MAX_MAP_VISIBILITY       0x1000000    // 16 MB
MAX_MAP_FACEINFO         8192
```

Comment note (`bspfile.h:70`): most limits are used **only for the `mapstats`
display**, not enforced as fatal at load — enforcement is per-lump via
`mincount`/`maxcount`/`CHECK_OVERFLOW` in `srclumps[]`/`extlumps[]`
(`mod_bmodel.c:350-543`). Lumps flagged `CHECK_OVERFLOW` (nodes, texinfo,
faces, leafs, models) abort on overflow; others only warn.

### 4.2 Three large-map mechanisms

1. **QBSP2 ('BSP2')** — every lump uses its `*32_t` variant. Chosen by
   `version == QBSP2_VERSION`; sets model flag `MODEL_QBSP2 = BIT(28)`
   (`com_model.h:591`, set at `mod_bmodel.c:4278`). In `Mod_LoadLump`, BSP2
   always uses `entrysize32` when available (`mod_bmodel.c:835-841`).
2. **BSP30ext** — HLBSP v30 + `XASH`/`EXTRA_VERSION 4` extra header. Enables
   extended **clipnodes only**: the loader guesses 32-bit clipnodes if the
   16-bit interpretation doesn't divide evenly OR count ≥
   `MAX_MAP_CLIPNODES_HLBSP` (`mod_bmodel.c:826-833`). Flag
   `bmod->isbsp30ext`.
3. **16-bit clipnode child promotion** — even in plain HLBSP, negative 16-bit
   children need care; see the "aguirRe broken clipnodes" fix in §9.3.

---

## 5. Load Sequence — `Mod_LoadBmodelLumps` (`mod_bmodel.c:4242-4384`)

Top-level entry: `model.c` `Mod_LoadModel` → `Mod_LoadBrushModel`
(`mod_bmodel.c:4506`) → `Mod_LoadBmodelLumps`. `Mod_LoadWorld`
(`model.c:501`) sets `world.loading=true` so the first brush model becomes
`worldmodel`.

**Phase A — raw lump directory resolution** (`Mod_LoadLump`,
`mod_bmodel.c:760-952`): fills a scratch `dbspmodel_t`
(`mod_bmodel.c:44-141`) with pointers into the file buffer + element counts.
Validates: nonzero `fileofs`, `filelen % entrysize == 0`,
`mincount`/`maxcount`. Does **not** copy data.

**Phase B — heap builders, in this EXACT order** (`mod_bmodel.c:4326-4340`):

| # | Loader | In lump(s) | Out (model_t) | Classification & consumer |
|---|--------|-----------|---------------|---------------------------|
| 1 | `Mod_LoadEntities` (2347) | ENTITIES (+ external `.ent`) | `mod->entities` (raw NUL-terminated string) | **Required** (entity queries). Parses `wad`,`message`,`compiler`,`generator`,`_litwater*` keys for world; wadlist. |
| 2 | `Mod_LoadPlanes` (2475) | PLANES | `mod->planes[]` `mplane_t` | **Required (TRACE+PVS)** — nodes/clipnodes/hulls reference planes. Computes `signbits`. |
| 3 | `Mod_LoadSubmodels` (2259) | MODELS | `mod->submodels[]` `dmodel_t` | **Required** — headnodes per hull, visleafs, face ranges. Spreads mins/maxs by 1 unit. |
| 4 | `Mod_LoadVertexes` (2509) | VERTEXES | `mod->vertexes[]` | **Trace-support** (face bevels, surface extents). Also computes `world.mins/maxs/size`. |
| 5 | `Mod_LoadEdges` (2545) | EDGES | `mod->edges16[]`/`edges32[]` | **Render-ish** (surface-extent/bevel calc; not hull trace). |
| 6 | `Mod_LoadSurfEdges` (2582) | SURFEDGES | `mod->surfedges[]` | **Render-ish** (same as edges). |
| 7 | `Mod_LoadTextures` (3167) | TEXTURES | `mod->textures[]` | **RENDER-ONLY** payload, but *names* feed surface content flags (§9.7/§9.8). Safe to skip texel upload on server. |
| 8 | `Mod_LoadVisibility` (3921) | VISIBILITY | `mod->visdata` (raw compressed) | **Required (PVS)** — memcpy of compressed vis; only for world. |
| 9 | `Mod_LoadTexInfo` (3318) | TEXINFO (+FACEINFO) | `mod->texinfo[]` | **Render-mostly**; texinfo→texture name read to set surface content/flags. |
| 10 | `Mod_LoadSurfaces` (3365) | FACES | `mod->surfaces[]` | **RENDER-ONLY** for drawing, but sets `SURF_*` flags; surfaces referenced by leaf marksurfaces. |
| 11 | `Mod_LoadLighting` (3974) | LIGHTING (+LIGHTVECS/SHADOWMAP/RGBLIGHTING) | `mod->lightdata` | **RENDER-ONLY** — defer entirely. |
| 12 | `Mod_LoadMarkSurfaces` (2594) | MARKSURFACES | `mod->marksurfaces[]` | **Trace/PVS-support** (leaf→surface; underwater marking + water-alpha PVS test). Validates indices. |
| 13 | `Mod_LoadLeafs` (3626) | LEAFS | `mod->leafs[]` | **Required (PVS)** — contents, cluster, compressed_vis, marksurfaces, ambient levels. |
| 14 | `Mod_LoadNodes` (3515) | NODES | `mod->nodes[]` | **Required (TRACE+PVS)** — plane, children, bbox. Calls `Mod_SetParent`. |
| 15 | `Mod_LoadClipnodes` (3875) | CLIPNODES | `bmod->clipnodes_out[]` (temp `dclipnode32_t`) | **Required (TRACE)** — always widened to 32-bit temp array; §9.4. |

**Phase C — post-init:**

- `Mod_MakeHull0` (`mod_bmodel.c:1907`) — builds hull[0] clipnodes from the
  drawing nodes.
- `Mod_SetupSubmodels` (`mod_bmodel.c:2152`) — per-submodel hull wiring +
  `"*N"` inline model duplication; frees `clipnodes_out`.
- If world + multiplayer server (`SV_Active() && svs.maxclients>1`):
  `Mod_CalcPHS` (`mod_bmodel.c:4354`).

**Ordering dependencies to preserve for parity:** planes before
nodes/clipnodes; textures+texinfo before surfaces (content flags); surfaces
before leafs (leaf marks underwater surfaces, 3705-3711); marksurfaces before
leafs; leafs+nodes before `Mod_MakeHull0`/`Mod_SetupSubmodels`; clipnodes
before setup. Note **leafs (13) are loaded before nodes (14)** — nodes' child
pointers reference `mod->leafs` which must already exist.

### 5.1 Entity lump (`Mod_LoadEntities`, `mod_bmodel.c:2347-2468`)

Stored as a **raw NUL-terminated string** copied to `mod->entities`
(mempool, size `entdatasize+1`). For world, an external `maps/<name>.ent`
patch overrides if newer (`:2356-2382`). Then for world only, a light
key-value parse extracts `wad` (semicolon list → `world.wadlist` via
`Q_splitstr`, `:2311-2340`), `message`, `compiler`/`_compiler`,
`generator`/`_generator`, `_litwater*`. **No general entity dictionary is
built** — the game DLL parses `mod->entities` later.

### 5.2 Hull construction (CRITICAL for trace parity)

**Hull dimension tables** live in `engine/common/pm_trace.c:30-45`, copied
into `host.player_mins/maxs` by `Pmove_Init` (:47-54). Exact values:

```c
static const vec3_t pm_hullmins[4] = { {-16,-16,-36}, {-16,-16,-18}, {0,0,0}, {-32,-32,-32} };
static const vec3_t pm_hullmaxs[4] = { { 16, 16, 36}, { 16, 16, 18}, {0,0,0}, { 32, 32, 32} };
```

Engine defaults; the game DLL can override per-index via
`pfnGetHullBounds(i, mins, maxs)` (`cl_pmove.c:746`, `sv_pmove.c:455`) — for
byte-exact SERVER parity a rewrite must apply the game's values, but the
built-in defaults are the GoldSrc-standard fallback.

**`Mod_SetupHull` index mapping** (`mod_bmodel.c:1972-1993`) — non-obvious
remap:

- BSP hull **1** ← `player_mins/maxs[0]` = (-16,-16,-36)/(16,16,36) "human hull"
- BSP hull **2** ← `player_mins/maxs[3]` = (-32,-32,-32)/(32,32,32) "large hull"
- BSP hull **3** ← `player_mins/maxs[1]` = (-16,-16,-18)/(16,16,18) "head hull"
- BSP hull **0** ← point (0,0,0), built by `Mod_MakeHull0`.

`hull_t` (`com_model.h:305-317`): `{ clipnodes16|clipnodes32 (union);
mplane_t *planes; int firstclipnode; int lastclipnode; vec3_t clip_mins;
vec3_t clip_maxs; }`.

`Mod_MakeHull0` (`mod_bmodel.c:1907-1965`): allocates `mod->numnodes`
clipnodes (16- or 32-bit per `MODEL_QBSP2`), copies
`planenum = node->plane - mod->planes`, and children = `child->contents` (if
leaf) else node index. hull0 `firstclipnode=0`, `lastclipnode=numnodes-1`,
`planes=mod->planes`.

`Mod_SetupHull` two paths:

- **Non-bsp30ext (normal HLBSP/Q1/BSP2):** `hull->planes=mod->planes`,
  `firstclipnode=headnode`, `lastclipnode=numclipnodes-1`; the actual
  `hull->clipnodes16/32` array is allocated **once**, for
  `mod==world && hullnum==1`, copied from `bmod->clipnodes_out` (the 32-bit
  temp) narrowing to 16-bit for non-BSP2. All other hulls/submodels **share**
  `world->hulls[1].clipnodes` (`mod_bmodel.c:2019-2052`).
- **bsp30ext:** per-submodel clipnodes are remapped to fit 16-bit indices via
  `CountDClipNodes_r` + `RemapClipNodes_r` (`:1838-1898, 2057-2075`), because
  total clipnodes may exceed 16 bits but per-submodel index space is still
  16-bit.

`Mod_SetupSubmodels` (`mod_bmodel.c:2152-2245`): per submodel sets hull0
first/lastclipnode from `bm->headnode[0]` then re-counts the real clipnode
span (`CountClipNodes16_r`/`32_r`), builds hulls 1-3 via `Mod_SetupHull`,
copies bounds/visleafs, and for i≥1 duplicates the model as `"*N"` via
`Mod_FindName` (:2228-2240). Origin-brush detection: `Mod_FindModelOrigin`
scans entities for `model "*N"` + `origin` key (:2201).

---

## 6. In-Memory Structures (what TRACE & PVS actually consume)

### 6.1 `model_t` (`com_model.h:327-403`) — PVS/trace-relevant fields

```text
vec3_t mins,maxs; float radius;
int firstmodelsurface, nummodelsurfaces;
int numsubmodels; dmodel_t *submodels;
int numplanes;    mplane_t *planes;       // TRACE + PVS
int numleafs;     mleaf_t  *leafs;        // PVS  (count excludes solid leaf 0)
int numvertexes;  mvertex_t *vertexes;    // trace-support (bevels/extents)
int numedges;     union{ medge16_t*edges16; medge32_t*edges32; };
int numnodes;     mnode_t  *nodes;        // TRACE + PVS
int numtexinfo;   mtexinfo_t *texinfo;
int numsurfaces;  msurface_t *surfaces;
int numsurfedges; int *surfedges;
int numclipnodes; union{ mclipnode16_t*clipnodes16; mclipnode32_t*clipnodes32; };  // TRACE
int nummarksurfaces; msurface_t **marksurfaces;   // PVS/trace-support
hull_t hulls[MAX_MAP_HULLS];              // TRACE (hulls[0..3])
int numtextures;  texture_t **textures;   // render
byte *visdata;                            // PVS (compressed)
color24 *lightdata; char *entities;       // render / entity
int flags;                                // MODEL_QBSP2 etc.
```

### 6.2 `mnode_t` (`com_model.h:144-181`) — `STATIC_CHECK_SIZEOF(mnode_t, 52, 72)`

```c
typedef struct mnode_s {
    int    contents;      // 0 for nodes, <0 for leafs (shared discriminator)
    int    visframe;
    float  minmaxs[6];    // bbox
    struct mnode_s *parent;
    mplane_t *plane;
#if !XASH_64BIT
    union {
        struct mnode_s *children_[2];
        struct {                         // 32-bit packs child ptr + surface counts into bitfields
            int child_0_leaf   : 1;  int child_0_off : 23;  int firstsurface_1 : 8;
            int child_1_leaf   : 1;  int child_1_off : 23;  int numsurfaces_1  : 8;
        };
    };
    unsigned short firstsurface_0, numsurfaces_0;
#else
    struct mnode_s *children_[2];
    unsigned short firstsurface_0, numsurfaces_0, firstsurface_1, numsurfaces_1;
#endif
} mnode_t;
```

**Child access is NOT a plain pointer on 32-bit BSP2 maps.** The exact
`node_child()` accessor (`com_model.h:594-619`): for `MODEL_QBSP2` on 32-bit,
`child_N_leaf` selects `mod->leafs` vs `mod->nodes` and `child_N_off` is the
index; otherwise `children_[side]` is a direct pointer.
`node_firstsurface`/`node_numsurfaces` recombine the split 16+16 fields
(`com_model.h:627-641`). The trace/PVS walkers all go through `node_child` —
a rewrite must preserve its *semantics* (leaf-vs-node selection), not
necessarily the bitfield layout (xash3dpp targets 64-bit).

Leaf/node polymorphism: code distinguishes leaf from node by `contents < 0`
(`Mod_PointInLeaf:1128`, `Mod_SetParent:1800`). Leafs are cast from
`mnode_t*` when `contents<0`.

### 6.3 `mleaf_t` (`com_model.h:203-220`)

```c
typedef struct mleaf_s {
    int contents;                 // <0 (PVS/trace)
    int visframe;
    float minmaxs[6];
    struct mnode_s *parent;
    byte *compressed_vis;         // PVS: pointer into mod->visdata (or NULL)
    struct efrag_s *efrags;
    msurface_t **firstmarksurface; int nummarksurfaces;
    int cluster;                  // PVS: 0-based (leaf i → cluster i-1); -1 if none/solid
    byte ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;
```

### 6.4 `mplane_t` (`xash3d_mathlib.h:169-176`)

```c
typedef struct mplane_s {
    vec3_t normal; float dist;
    byte type;      // PLANE_X/Y/Z (<3 → axial fast path) or PLANE_ANYx
    byte signbits;  // signx | signy<<1 | signz<<2  (set in Mod_LoadPlanes:2492)
    byte pad[2];
} mplane_t;
```

Trace/PVS side test: `BOX_ON_PLANE_SIDE` (`xash3d_mathlib.h:194-208`) —
axial fast path when `type<3`, else `BoxOnPlaneSide`. `PlaneDiff(p,plane)`
used in `Mod_PointInLeaf`.

### 6.5 In-memory clipnodes (`com_model.h:56-66`)

```c
typedef struct mclipnode32_s { int planenum; int   children[2]; } mclipnode32_t; // BSP2/bsp30ext
typedef struct mclipnode16_s { int planenum; short children[2]; } mclipnode16_t; // classic HLBSP
```

Selected by `MODEL_QBSP2`. `box_clipnodes16/32[6]` (`mod_bmodel.c:567-594`)
are the shared 6-plane box hull used for point/box entities (planenum 0..5,
children chain to `CONTENTS_EMPTY`/`CONTENTS_SOLID`).

### 6.6 `medge` (`com_model.h:69-78`)

`medge32_t{ unsigned int v[2]; }` (8B) and `medge16_t{ unsigned short v[2];
unsigned int cachededgeoffset; }` (16-bit form carries an extra render cache
field). Consumed only by surface extent/bevel math, not hull trace.

---

## 7. PVS Machinery (exact algorithms)

All in `mod_bmodel.c`. Note the function is named **`Mod_DecompressPVS`**,
not `Mod_DecompressVis`.

### 7.1 Vis RLE decompression — `Mod_DecompressPVS` (`mod_bmodel.c:1059-1086`)

```c
static void Mod_DecompressPVS( byte *out, const byte *in, size_t visbytes ) {
    byte *dst = out;
    if( !in ) { memset(out, 0xFF, visbytes); return; }   // NULL → all visible
    while( dst < out + visbytes ) {
        if( *in )                 // non-zero byte: copy literal
            *dst++ = *in++;
        else {                    // zero byte: next byte = run length of zero bytes
            size_t c = in[1];
            if( c > out + visbytes - dst ) c = out + visbytes - dst;  // clamp to end
            memset( dst, 0, c );
            in += 2; dst += c;
        }
    }
}
```

Classic Quake zero-RLE. `Mod_CompressPVS` (:1088-1114) is the inverse (max
run 255).

### 7.2 Buffer sizing (`mod_bmodel.c:349`, `3638-3640`)

- Static scratch: `static byte g_visdata[(MAX_MAP_LEAFS+7)/8]` =
  `(131072+7)/8` = **16384 bytes**.
- `world.visbytes = (visclusters + 7) >> 3` where
  `visclusters = submodels[0].visleafs`.
- `world.fatbytes = (visclusters + 31) >> 3` (32-bit-row aligned for fat PVS).
- Leaf clusters: `leaf[i].cluster = i - 1`, clamped to `-1` if
  `>= visclusters` (`:3686-3689`). Solid leaf 0 → cluster -1.

### 7.3 `Mod_PointInLeaf` (`mod_bmodel.c:1122-1135`)

```c
while(1){ if(node->contents < 0) return (mleaf_t*)node;
          node = node_child(node, PlaneDiff(p, node->plane) <= 0, mod); }
```

Side select: `PlaneDiff <= 0` → child index 1 (back), else 0 (front).
**Tie-break differs from the hull-trace walk** (which uses strict `< 0`).

### 7.4 `Mod_GetPVSForPoint` (`mod_bmodel.c:1145-1160`)

PointInLeaf on `worldmodel->nodes`; if `leaf->cluster >= 0` decompress into
`g_visdata` and return it, else NULL. Base PVS used by `SV_BoxInPVS`.

### 7.5 `Mod_FatPVS` / `Mod_FatPVS_RecursiveBSPNode` (`mod_bmodel.c:1168-1241`)

Accumulates (OR) the PVS/PHS of all leafs whose bbox is within `radius` of
`org`. Signature `Mod_FatPVS(org, radius, visbuffer, visbytes, merge,
fullvis, phs)`. If `phs` true, reads precomputed
`world.compressed_phs`/`world.phsofs`; else per-leaf `compressed_vis`.
Full-vis fallback (0xFF) when `fullvis`, no visdata, no leaf, or `cluster<0`.
Radii: `FATPVS_RADIUS=8.0f`, `FATPHS_RADIUS=8.0f` (`mod_local.h:31-32`).
**PHS is server-side machinery — deferrable to the server chunk.**

### 7.6 `Mod_BoxVisible` (`:1325-1341`) + `Mod_BoxLeafnums` (:1250-1315)

Gathers leaf clusters overlapping a box (recursive `Mod_BoxLeafnums_r` using
`BOX_ON_PLANE_SIDE`, stops at `CONTENTS_SOLID`), returns true if any is set
in `visbits`. `MAX_BOX_LEAFS = 256` (`com_model.h:575`).
`CHECKVISBIT(vis,b)` = `vis[b>>3] & (1<<(b&7))` (`mod_local.h:27`).

### 7.7 `Mod_HeadnodeVisible` (`sv_game.c:4299-4321`) — server-side, deferrable

Recursive node walk returning true if any visible (per `visbits`) non-solid
leaf is reachable; records `*lastleaf`. Used by `pfnCheckVisibility` (:4329).

### 7.8 PHS build — `Mod_CalcPHS` (`mod_bmodel.c:3730-3868`) — server-MP only, deferrable

Decompresses all PVS rows (row stride `ALIGN(world.visbytes,4)`, count
`numleafs+1`), then for each leaf ORs in the PVS of every visible leaf to
form the "potentially hearable set", recompresses into `world.compressed_phs`
with per-row offsets `world.phsofs[]`. OpenMP-parallelized. Comment (:3859)
confirms uncompressed PVS/PHS match GoldSrc byte-for-byte.

---

## 8. Map Identity / Networking Checksum

`CRC32_MapFile` (`mod_bmodel.c:4093-4165`), called at `sv_init.c:1036`
(`sv.worldmapCRC`) and validated on client at `cl_parse.c:895` against
`cl.checksum`.

Algorithm:

1. **Singleplayer (`multiplayer==false`): CRC is a fixed constant**
   `('H'<<24)|('S'<<16)|('A'<<8)|'X'` = `"XASH"` little-endian =
   `0x58415348` (`mod_bmodel.c:4107`). No file hashing.
2. **Multiplayer:** read 124-byte `dheader_t`, validate version ∈
   {29,30,'BSP2'}, then CRC32 **over lumps `LUMP_PLANES..LUMP_MODELS`
   (indices 1..14 inclusive)** — i.e. **`LUMP_ENTITIES` (index 0) is
   EXCLUDED** (`mod_bmodel.c:4139`). Each lump's `[fileofs, fileofs+filelen)`
   bytes are streamed in 1024-byte chunks.

CRC32 details (`crclib.c:23-139`, `crclib.h:29-40`): standard reflected
CRC-32 (zlib polynomial, `0xedb88320` reflected). `CRC32_Init` →
`0xFFFFFFFF`; `CRC32_Final` → `crc ^ 0xFFFFFFFF`. `CRC32_ProcessBuffer`
processes 8/4/1 bytes at a time with `LittleLong` on the wide reads (result
endian-independent). **Note:** `CRC32_MapFile` does **not** call
`CRC32_Final` — the stored CRC is the pre-final (un-inverted) accumulator.
Reproduce exactly: init `0xFFFFFFFF`, feed lumps 1..14, **no** XOR-invert.

---

## 9. Quirks & Hazards (parity-audit checkpoints)

Each is a real branch in the legacy loader; a byte-exact rewrite must match.

1. **Blue-Shift swapped lumps** (`mod_bmodel.c:4268-4273`, applied at
   `Mod_LoadLump:773-779`). HL:Blue Shift maps have `LUMP_ENTITIES`(0) and
   `LUMP_PLANES`(1) **swapped**. Detected (v30 only, non-bsp30ext) when the
   entities lump does NOT contain `"classname"` but the planes lump DOES
   (`Mod_LumpLooksLikeEntities:4064`). When set, `Mod_LoadLump` swaps lump
   indices 0↔1. `Mod_TestBmodelLumps` reads from the file for detection
   (`4451-4463`) and returns the corrected entities lump to caller (`4478`).
2. **BSP30ext extended clipnode guess** (`:826-833`): only for v30 +
   `LUMP_BSP30EXT` + `LUMP_CLIPNODES`, switch to 12-byte `dclipnode32_t` if
   `filelen % 8 != 0` OR `filelen/12 >= 32767`.
3. **aguirRe QBSP "broken" 16-bit clipnodes** (`Mod_LoadClipnodes`,
   `:3901-3908`): after widening 16-bit children to `unsigned short`, if a
   child index `>= numclipnodes`, subtract `65536` (reinterpret as negative
   contents). Classic broken-compiler workaround.
4. **Clipnodes always widened to 32-bit temp** (`:3880`):
   `bmod->clipnodes_out` is `dclipnode32_t[]` regardless of source width; the
   final per-hull array is narrowed back to 16-bit for non-BSP2 in
   `Mod_SetupHull`. Temp freed in `Mod_SetupSubmodels:2243`.
5. **Colored vs mono lighting detection** (`:3378-3382, 3457-3507,
   3974-4014`): sample count predicted from version, refined by measuring the
   byte-gap between consecutive `lightofs` values. Mono lightmaps upgradable
   via external `.lit` or BSPX `RGBLIGHTING` (must be exactly 3× lightdata).
   Sets `MODEL_COLORED_LIGHTING = BIT(4)`. **Render-only**, but affects
   `world.flags`/mapstats.
6. **`"wad"` key parsing from entities** (`:2437-2440`, handler
   `:2311-2340`): worldspawn `wad` value is `;`-split; each entry basename'd,
   `.wad`-checked, existence-checked, appended to `world.wadlist` (allocated
   in `host.mempool`, noted FIXME). Reverse-order WAD search in
   `Mod_LoadTextureFromWadList` (:616-662).
7. **Server can skip miptex texel upload** (`:644-645, 2913-2916,
   3171-3178`): on dedicated server, texel loading is `#if !XASH_DEDICATED`.
   **A server-only loader can skip miptex payloads** — but must still read
   miptex **names** because surface `SURF_*` content flags derive from them.
   `miptex` out-of-range is silently clamped to 0 (`Mod_LoadTexInfo:3349-3351`).
8. **Face content/flag special names** (`Mod_LoadSurfaces:3431-3451`,
   `Mod_GetFaceContents:1471-1489`, `Mod_LooksLikeWaterTexture:2637-2649`):
   `"sky*"`→`SURF_DRAWSKY`/`CONTENTS_SKY`; `!`/`*`/`water*`/`laser*`→water
   (`SURF_DRAWTURB`, `CONTENTS_WATER`/`SLIME`/`LAVA` by `!lava`/`!slime`
   prefix); `"scroll*"`/`TEX_SCROLL`→`SURF_CONVEYOR`;
   `"{scroll*"`→conveyor+transparent; leading `{`→`SURF_TRANSPARENT`;
   `TEX_SPECIAL`→`SURF_DRAWTILED`. Quake-compat mode changes water-name
   matching (`Host_IsQuakeCompatible`).
9. **Leaf `compressed_vis` double-assignment** (`Mod_LoadLeafs:3684-3702`):
   the world branch bounds-checks `visofs < visdatasize` before setting
   `compressed_vis`, but lines 3701-3702 then **unconditionally overwrite
   it**: `compressed_vis = (visofs==-1) ? NULL : visdata+visofs` for ALL
   leafs, with **no bounds clamp**. The earlier check only gates a warning.
   For parity: final `compressed_vis` = NULL iff `visofs==-1`, else
   `visdata + visofs`. (Safe in practice because PVS code checks
   `cluster>=0` first.)
10. **Leaf 0 must be `CONTENTS_SOLID`** for world, else `Host_Error`
    (`:3715-3716`).
11. **Water-alpha support probe** (`Mod_CheckWaterAlphaSupport:1412-1437`):
    decompresses each liquid leaf's PVS and checks if it can see an empty
    leaf; sets `FWORLD_WATERALPHA`. A PVS dependency inside the loader.
12. **`c2a1.bsp` hardcoded hack** (`:2206-2210`, under
    `HACKS_RELATED_HLMODS`): submodel 11 of `maps/c2a1.bsp` is force-flagged
    `MODEL_HAS_ORIGIN` (it has no origin brush but is centered).
13. **ZHLT empty hulls** (`Mod_SetupHull:2002-2003`):
    `headnode >= numclipnodes` → hull left empty (`return`). Also
    `headnode==-1` or (`hullnum!=1 && headnode==0`) in bsp30ext path → empty
    hull (`:2057-2058`). Some optimizers write `-1`/`CONTENTS_EMPTY` into
    headnode.
14. **Node face-index 24-bit cap on 32-bit builds** (`Mod_LoadNodes:3535-3547`):
    BSP2 `firstface`/`numfaces >= BIT(24)` → `Host_Error` (in-memory 32-bit
    `mnode_t` bitfield limitation; N/A on 64-bit layouts).
15. **Marksurface fix-ups** (`Mod_LoadMarkSurfaces:2617-2634`): buggy GoldSrc
    compilers wrote negative/invalid surface indices; when
    `numsurfaces <= INT16_MAX` and `(int16_t)in[i] < 0`, remap to surface 0
    with a warning; otherwise out-of-range → `Host_Error`.
16. **External light files** `.lit`/`.dlit` (`Mod_LoadLitfile:2078-2142`):
    header `QLIT` + version 1; size must match expected. **Render-only.**
17. **`Mod_FindEndOfBSPFile` / BSPX discovery** (`:4174-4233`): end-of-file =
    max(`fileofs+filelen`) over standard lumps (and extra lumps if bsp30ext),
    then `ALIGN(_,4)`; BSPX header expected there (`IDBSPXHEADER`).
18. **Deferred/skippable for a PVS+trace server model:** LIGHTING + all
    extra/BSPX light lumps, TEXTURES texel payloads (names still needed),
    EDGES/SURFEDGES/VERTEXES/FACES only needed for surface extents/bevels or
    drawing — pure hull traces (`hulls[]` + clipnodes + planes) and PVS
    (nodes + leafs + visdata) do **not** need them. However `Mod_LoadLeafs`
    marks underwater surfaces (needs surfaces+marksurfaces) and
    `Mod_CheckWaterAlphaSupport` needs vis — replicate for identical
    `world.flags`.

---

## 10. Uncertainties / Flags for the Implementer

- **`dmiptexlump_t.dataofs[4]`** is a fixed `[4]` in the struct but
  semantically `[nummiptex]`; the loader indexes `dataofs[i]` for
  `i < nummiptex` (`Mod_GetMipTexForTexture:604-613`).
- **Hull dimensions are game-DLL-overridable** via `pfnGetHullBounds`. The
  table in `pm_trace.c` is the engine default; for byte-exact trace parity
  against a specific mod, source the mod's values.
- **Big-endian swap defs** are compiled out on little-endian; ignore unless
  targeting big-endian.
- **`Mod_CalcPHS` uses OpenMP** and row alignment `ALIGN(visbytes,4)`;
  results deterministic. The *compressed* PHS byte stream depends on
  `Mod_CompressPVS` details (max run 255) — reproduce exactly if the PHS
  blob is ever serialized/compared.
- **`STATIC_CHECK_SIZEOF(mnode_t,52,72)`** enforces the legacy in-memory node
  size; the 32-bit bitfield layout is load-bearing for BSP2 `node_child`. A
  64-bit C++ rewrite may use plain indices/pointers so long as
  `node_child`/`node_firstsurface`/`node_numsurfaces` semantics are
  identical.
- **CRC stored value is NOT finalized** (no `^0xFFFFFFFF`) — §8. External
  tooling expecting a "standard" CRC-32 will disagree.
