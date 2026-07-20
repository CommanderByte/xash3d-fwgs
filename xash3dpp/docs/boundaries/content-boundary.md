# Content Pipeline Boundary Spec

> Refreshed 2026-07-06 (as-built pass). The original 2026-07-06 recon text is
> preserved below; an `As-built reconciliation` section, a refreshed
> `Extension axes (Q-21)`, and a `Threading` pointer were appended, and the
> **Status** line was corrected against the shipped code. Prior recon language
> that read "no implementation yet" is superseded where noted.

> **Chunk**: 7 (content pipeline — model & image loaders)\
> **Subsystem**: `content`\
> **Status**: **Superseded 2026-07-06:** ~~recon — no implementation yet~~ →
> **Complete (13 TUs)** per `status_table.py` — model cache + 3 model loaders
> (studio/sprite/alias) + 7 image codecs + the studio bone solver (OQ-5,
> bit-exact vs the Q-18 goldens) + the pose/attachment/hull query pfns. The
> `SV_ClipMoveToEntity` studio-hitbox trace-loop **landed 2026-07-19** (the
> smoke gate opened the same day) — Chunk 7 is ✅ DONE and the plan row
> agrees (the status drift recorded below was reconciled by the 2026-07-19
> consolidation audit).\
> **Depends on**: `filesystem`, `utilities`, `memory`, `map_loader` *(all done)*\
> **Legacy reference**: `engine/common/model.c`, `mod_studio.c`, `mod_sprite.c`,
> `mod_alias.c` (brush path → `mod_bmodel.c`, already `map_loader`);
> `engine/common/imagelib/*` (`img_main.c`, `img_utils.c`, `img_wad.c` [ex
> `img_mip.c`], `img_bmp/tga/dds/ktx2/png.c`, `img_quant.c`)\
> **Date**: 2026-07-06

______________________________________________________________________

## Responsibility

The content pipeline turns on-disk **model** and **image** files into the
frozen in-memory structures the rest of the engine reads, and normalises every
texture to RGBA **byte-identically to GoldSrc** for visual compatibility. It is
two cooperating layers behind one chunk:

1. **The model handle cache + non-brush loaders.** An engine-global registry
   (`mod_known[]`) that finds-or-loads a model by name, dispatches on the file
   magic to the studio / sprite / alias parsers (or hands the brush path to
   `map_loader`), and manages the level-transition purge lifecycle. It also
   supplies the **server-side studio collision surface** — bone positions,
   hitbox hulls, attachments, sequence/animtime data — that the Chunk 6 server
   already scaffolded stubs against (`pfnGetModelPtr`, `GetBonePosition`,
   `GetAttachment`, `Mod_HullForStudio`).

2. **The image codec library (`imagelib`).** Load/save/convert for BMP, TGA,
   DDS, KTX2, PNG and the GoldSrc/Quake lump family (MIP/MDL/SPR/LMP/FNT/PAL,
   plus single-lump WAD3 pack/unpack for player sprays), with the palette →
   RGBA, DXT-alpha-scan, gamma/overbright, luma, resample and quantise
   transforms that make decode match the original engine.

**What it does *not* do.** It does not load the world BSP or build BSP geometry
(→ `map_loader`, done); it does not upload textures or geometry to the GPU, run
studio animation for drawing, or build lightmaps/glpolys (→ `renderer`, Chunk
13); it does not own the per-side precache index→model maps (`sv.models[]` /
`cl.models[]` stay in `server` / `client`); and WAD-as-a-search-path is already
owned by `filesystem` (`WadBackend`, auto-mount) — content only decodes the raw
miptex bytes filesystem serves.

______________________________________________________________________

## External ABI contracts

Content parses files **into** frozen structs and hands raw pointers into them
across the game/client/renderer DLL boundary. It implements a handful of
`GAME_EXPORT` studio-cache callbacks and *calls out* to renderer/physics/game
post-process hooks (it never links toward them).

### Frozen structures content produces (read by DLLs — layout may not move)

| Struct / enum | Header | Read by | Notes |
|---|---|---|---|
| `model_t` | `common/com_model.h:327` | renderer + physics post-process; `SV_/CL_ModelHandle` | `STATIC_CHECK_SIZEOF` pins `mnode_t/mextrasurf_t/decal_t/mfaceinfo_t`; `needload` field stores the 0..3 `NL_*` enum despite its `qboolean` type |
| `studiohdr_t` + all `mstudio*` | `engine/studio.h:145` | game DLL via `pfnGetModelPtr` (raw `void*`, walked by offset) | `STUDIO_VERSION == 10`; former sound fields `unused[]` and `studiohdr2index` cannot move |
| `msprite_t`, `mspriteframe_t`, `mspritegroup_t`, `mspriteframedesc_t` | `common/com_model.h:456` | client draw | in-memory sprite; two on-disk formats (`dsprite_q1_t`/`dsprite_hl_t`) |
| `aliashdr_t`, `maliasframedesc_t`, `trivertex_t` | `common/com_model.h:507` | client draw | Quake MDL; effectively dead for HL (see quirks) |
| `cache_user_t` | `common/com_model.h:321` | studio DLL `Cache_Check` | studio seqgroup cache slot |
| `rgbdata_t`, `pixformat_t`, `imgFlags_t`, `bpc_desc_t PFDesc[]` | `common/com_image.h` | **renderer ABI** (`ref_api.h` by pointer) **and mainui SDK** | ownership of `buffer`/`palette` crosses the boundary → not a flat POD; ABI status is **OQ-1** |

### `GAME_EXPORT` callbacks content must expose (studio DLL interface)

`Mod_Calloc`, `Mod_CacheCheck`, `Mod_LoadCacheFile` (`model.c:556-636`) and
`Mod_StudioExtradata` (`mod_studio.c:241`) — bundled into the studio blending
interface (`gStudioAPI`, server `sv_game.c` / client `cl_game.c`).

### DLL hooks content drives (calls out to; injected, never linked toward)

| Hook | Provider | When |
|---|---|---|
| `ref.dllFuncs.Mod_ProcessRenderData(mod, create, buf, len)` | renderer (Chunk 13) | client post-load of every model |
| `svgame.physFuncs.Mod_ProcessUserData(mod, create, buf)` | game DLL | **dedicated-server** post-load |
| `ref.dllFuncs.Mod_StudioLoadTextures` / `Mod_SpriteLoadTextures` | renderer | studio/sprite skin upload (client only) |
| `pBlendAPI->SV_StudioSetupBones` | game DLL `Server_GetBlendingInterface` | swappable bone solver; builtin `gBlendAPI` fallback |
| `GL_LoadTextureFromBuffer(name, rgbdata_t*, …)` | renderer | texture upload consumes `rgbdata_t` |

______________________________________________________________________

## Interface (what the rest of the engine calls)

### Model registry (legacy `Mod_*`, `model.c` / `mod_local.h:146`)

| Legacy function | Purpose |
|---|---|
| `Mod_Init` / `Mod_Shutdown` | alloc `com_studiocache` pool, register cvars/cmds, init studio hull; teardown |
| `Mod_ForName(name, crash, trackCRC)` | find-or-load convenience (`FindName` + `LoadModel`) |
| `Mod_FindName(name, trackCRC)` | find-or-allocate a cache slot; never returns NULL; sets `needload` |
| `Mod_LoadModel(mod, crash)` | **dispatcher** — read file, switch on magic, run loader, DLL post-process, CRC |
| `Mod_LoadWorld(name, preload)` | purge studio cache, load map into **slot 0** |
| `Mod_FreeUnused` / `Mod_FreeAll` / `Mod_ClearUserData` | level-transition reaping / full free / drop render+phys data |
| `Mod_StudioExtradata(mod)` / `pfnMod_Extradata(type, mod)` | typed `void*` extradata accessor (per-type) |
| `Mod_ValidateCRC(name, crc)` / `Mod_NeedCRC(name, need)` | client-consistency / cheat-detection surface (consumed by `server`) |
| `Mod_Calloc` / `Mod_CacheCheck` / `Mod_LoadCacheFile` | studio seqgroup cache (SDK `Cache_*`) |

Handle→pointer lookup (`SV_ModelHandle` / `CL_ModelHandle`) and the precache
index maps stay in `server`/`client`; the "typed model handle lookup"
deliverable is **OQ-2**.

### Studio server-collision queries (`mod_studio.c`, renderer-free)

`Mod_HullForStudio` (hitbox trace hull), `Mod_GetBonePosition`,
`Mod_StudioGetAttachment`, `Mod_HitgroupForStudioHull`, `R_StudioGetAnim`
(demand-loads seqgroup `…NN.mdl`), `Mod_GetStudioBounds`,
`Mod_StudioBodyVariations`. These unblock the Chunk 7-tagged server stubs.

### Image codec library (`imagelib`; engine API `common.h:483`, renderer ABI `ref_api.h:493`)

| Function | Purpose |
|---|---|
| `FS_LoadImage(name, buf, size)` | dispatch by extension → `rgbdata_t`; probe cubemap sides; `#name` loads from `buf` |
| `FS_SaveImage(name, pix)` | write by extension; expand cubemap/skybox to 6 side files |
| `FS_CopyImage` / `FS_FreeImage` | deep copy / free (`buffer` + `palette` + struct) |
| `Image_Process(&pix, w, h, flags, _)` | in-place post-load: luma/remap/force-RGBA/light-gamma/flip/resample/quantise |
| `Image_Init` / `Image_Setup` / `Image_Shutdown` | pool + load/save table install per host type |
| `Image_AddCmdFlags` / `SetForceFlags` / `ClearForceFlags` / `CustomPalette` | renderer advertises decode hints (`IL_DDS_HARDWARE`, …) |
| `Image_SetMDLPointer` / `Image_CheckPaletteQ1` / `Image_PaletteHueReplace` | studio-skin side-channel; palette install; player top/bottom-colour remap |

Loader table (`load_game[]`, `img_utils.c:96`): dds, bmp, tga, png, wad, mip,
mdl, spr, lmp, fnt, pal, ktx2. Saver table (`save_game[]`): tga, bmp, png, wad.
Vestigial decls with no definition (`Image_Load/Save/AddRGBAImageToPack/
SetPixelFormat/…`, `imagelib.h:132-143`) are dead — drop in the rewrite.

______________________________________________________________________

## Dependencies (what this module calls)

| Dependency | Why |
|---|---|
| **filesystem** (done) | `FS_LoadFile` for model / image / external `…T.mdl` texture / seqgroup files; raw miptex bytes from the already-mounted `WadBackend`; `FS_Search` (cubemap sides); `FS_Open/Write` (save paths) |
| **memory** (done) | per-model private `mempool`; the shared `com_studiocache` pool; imagelib's `host.imagepool` — all allocation routes here (Q-2, no raw `malloc`) |
| **utilities** (done) | byte-swap (`le_struct_*`), CRC32 (cheat check + PNG chunks), string/path helpers, math for bounds/reflectivity |
| **map_loader** (done) | brush dispatch: `Mod_LoadModel` → `Mod_LoadBrushModel` → `Mod_LoadBmodelLumps`; world pinned to slot 0 via `world.loading`; inline `*N` submodels registered by the BSP loader |
| **xash3d_mathlib** (shared, static) | `R_StudioCalcBones` / `R_StudioSlerpBones` — why bones solve with no renderer present (**satellite/OQ-5**) |
| miniz | PNG inflate/deflate + CRC32 (imagelib-only external dep — the Q-11 separation lever) |
| renderer / physics / game DLL | post-process, texture upload, bone solver — **injected seams**, content never links toward them |

______________________________________________________________________

## Owned state

Content is the most singleton-heavy legacy surface after the server; nearly all
of this is P-3 / threading debt to eliminate (see Extension axes).

**Model cache (`model.c`)** — `mod_known[MAX_MODELS]` (slot 0 = world),
`mod_numknown` (high-water, not live count), parallel `mod_crcinfo[MAX_MODELS]`
(CRC flags + `initialCRC`), the `com_studiocache` pool, per-model `mempool`.
`MAX_MODELS` is build-dependent: 4096 default, 1024 / 512 under
`XASH_LOW_MEMORY 1/2`. Cvars: `r_studiocache`, `r_wadtextures`, `r_showhull`,
`r_allow_wad3_luma`.

**Studio trace state (`mod_studio.c`, all file-static)** — `mod_studiohdr`
(**the current-model scratch pointer rewritten by every bone/attachment/hull
call — a hard non-reentrant singleton**), `cache_studio[16]` trace ring +
`cache_*` cursors, `studio_hull`/`studio_planes`/`studio_bones` scratch,
`pBlendAPI` (game-DLL bone hook), `gBlendAPI`/`gStudioAPI` interface tables.

**Alias loading scratch (`mod_alias.c`)** — `g_poseverts[MAXALIASFRAMES]` +
`g_posenum`; `aliashdr.pposeverts` dangles at it until `Mod_LoadModel` nulls it
post-mesh-build (`model.c:392`).

**imagelib scratch (`img_main.c` / `img_utils.c`)** — the global `imglib_t
image` (current 2D image + cubemap accumulation + indexed state + parms;
makes the whole codec layer non-reentrant), palette tables `palette_q1[768]` /
`palette_hl[768]` / `d_8to24table` (+ init guards), `PFDesc[]`, format tables,
`g_mdltexdata` (MDL side-channel). Gamma tables live in `client/gamma.c`, not
imagelib.

______________________________________________________________________

## Quirks and invariants

**Model cache**

- `needload` is a 4-state enum (`NL_UNREFERENCED=0 / NEEDS_LOADED / PRESENT /
  FREE_UNUSED`), not a bool; a slot is free iff `NL_UNREFERENCED`.
- **World is pinned to slot 0** (`ASSERT(pworld == mod_known)`); purge frees
  slot 0 explicitly, `Mod_FreeUnused` starts at slot 1.
- **"bmodels and sprites don't cache normally"** — inline `*N` submodels share
  the world's data, are force-`NL_PRESENT`, and must never be mempool-freed
  individually; dedicated-server sprites are half-loaded (`numframes` zeroed,
  texture load skipped).
- **Purge is level-transition-driven, not access-LRU**: `Mod_LoadWorld` →
  `Mod_PurgeStudioCache` empties `com_studiocache`, frees inline submodels,
  flags every other model `NL_FREE_UNUSED`; the new map's `Mod_FindName` calls
  bump survivors back to `PRESENT` before `Mod_FreeUnused` reaps the rest.
- **CRC cheat-detection**: `Mod_LoadModel` CRC32s the raw file; a second load
  with a changed CRC → `Host_Error`. `Mod_ValidateCRC`/`Mod_NeedCRC` expose it
  to `sv_client` for client-consistency (**OQ-6** placement).
- `MAX_MODELS` overflow → `Host_Error` (hard cap).

**Studio**

- `mod_studiohdr` singleton ⇒ not reentrant; a threaded rewrite must thread it.
- **Quake pitch-inversion bug**: `angles2[PITCH] = -angles2[PITCH]` unless
  `ENGINE_COMPENSATE_QUAKE_BUG` — an observable behavioural invariant.
- Bone solver is swappable (game DLL `Server_GetBlendingInterface`); builtin
  fallback. Shared bone math links from `xash3d_mathlib` so it runs headless.
- `XASH_LOW_MEMORY` drops studio texel data post-upload, mutating
  `studiohdr.length`; external `…T.mdl` textures are merged onto a fresh copy
  when `numtextures == 0`; big-endian re-swaps demand-loaded seqgroups by
  re-deriving the header through `PARM_GET_STUDIO_HDR`.
- Player-skin hue remap (`SUIT/PLATE/SHIRT/PANTS` ranges, `com_model.h:560`) is
  executed **render-side**, not by the loader — content carries the constants
  only.

**Alias** — effectively dead for HL (studio owns `.mdl`); must still parse an
`IDPO`-magic file; server needs nothing from it (`Mod_AliasExtradata` is a dead
prototype). Treat as parse-minimal, render-only.

**imagelib (the texture-normalisation parity surface — the complexity note's
hardest part; must match GoldSrc byte-for-byte)**

- Texture-name prefixes: `{` → masked/gradient decal (`IMAGE_ONEBIT_ALPHA`);
  `!` / `water*` → grab water fog colour+density into `fogParams`; `sky` +
  `w==2h` → `IMAGE_QUAKESKY` kept 8-bit; `~` / `+N~` → wad3 luma; `#` →
  internal-buffer load; `#logo` / `#XASH_SYSTEMFONT_001` → spray / menu-font
  tricks.
- Index-255 transparency across Q1/HL/MIP/WAD/LMP paths.
- **Quake-vs-HL palette** classified on the first 765 bytes; a Q1-paletted HL
  mip is left `LUMP_NORMAL` (no texgamma) to avoid over-darkening.
- Texgamma/overbright applied per palette colour; `IMAGE_LIGHTGAMMA` in the
  process pipeline; luma fullbright threshold index `≥ 224`; `black_pixel`
  searched per-palette (luma flag dropped if none found).
- DDS/KTX2 kept **compressed** for GPU decode (only alpha blocks scanned for
  `IMAGE_HAS_ALPHA`); need `IL_DDS_HARDWARE`. NeuQuant quantiser reserves index
  255 black. Cubemap loader fabricates black sides and forces a constant
  `w*h*4` per-side size "because render.dll expects it".
- BMP tolerates filesize mismatch (Sweet Half-Life `splash.bmp`); TGA origin /
  `IL_DONTFLIP_TGA`; PNG rejects Adam7 interlacing.

______________________________________________________________________

## Satellite components

Evaluated against `decisions-architecture.md §Q-11` (count of "separate"
criteria met out of 5; ≥ 2 → separate target).

| Candidate feature | Score (0-5) | Verdict |
|-------------------|-------------|---------|
| **imagelib** (image codec library) | 3 — (a) independent formats/state machine, (b) extra external dep **miniz** the model core does not need, (c) useful without the model cache (renderer textures, screenshots, sprays) | **separate** target `xash3dpp_imagelib` under the content area |
| **Model cache + studio/sprite/alias loaders** (the model core) | n/a — the parent concern | **same** target `xash3dpp_content` |
| **NeuQuant quantiser** (`img_quant.c`, RGBA→indexed for sprays) | 0-1 — only imagelib's save path uses it | **same** target — `quantize.cpp` inside `xash3dpp_imagelib` |
| **WAD/MIP miptex codec** (`img_wad.c` lump decode + single-lump WAD3 pack/unpack) | 1 — shares imagelib's format protocol; WAD-*as-archive* is already `filesystem`'s `WadBackend` | **same** target — inside `xash3dpp_imagelib` |
| **Studio bone / animation math** (`R_StudioCalcBones`/`SlerpBones`) | 2 — shared by server-collision **and** renderer-draw, pure math, useful to both without the loader | **same** target for now; the pure-math kernel is a `utilities`/`xash3d_mathlib` promotion candidate — **OQ-5** |

______________________________________________________________________

______________________________________________________________________

## As-built reconciliation (2026-07-06)

The recon spec above (structures, interface, quirks, satellites) reconciled
against the shipped 13-TU implementation. **The spec is accurate as intent** —
this section records where the as-built code lands and the one status drift.

**Status drift (flag).** `status_table.py` reports content **Complete**;
`implementation-plan.md` (row line 32; §"Chunk 7" line 193) still reads
**Partial / ◑ IN PROGRESS**. The plan is authoritative on chunk numbering but
lags here on the *build* state: everything in this boundary is coded and
tested except the `SV_ClipMoveToEntity` studio-hitbox trace-loop (server-side,
gated on the hl.dll smoke — see below). Recommend the plan row be reconciled to
"Complete (loaders + codecs + bone solver); server hitbox-trace finish gated on
Chunk 7 smoke".

**Two targets, 13 TUs** (matches the Q-11 satellite verdict):

- `xash3dpp_content` (4): `model/model_cache.cpp`, `model/studio.cpp`,
  `model/bone_solver.cpp`, `model/formats.cpp`.
- `xash3dpp_imagelib` (9): `imagelib/imagelib.cpp`, `imagelib/palette.cpp`, and
  the 7 codecs `codec_wad/tga/bmp/dds/ktx2/mip/png.cpp`. `miniz` links **PRIVATE**
  (PNG only; the Q-11 separation lever).

**Model cache (O-1) — as-built.** `ModelCache` is a pimpl class over a
`std::vector<Slot>` (not a fixed `mod_known[MAX_MODELS]` array); slot 0 is the
world (`register_world`), 1..N regular models, freed slots reused before
growth, `content_max_models` the hard cap (returns the null handle instead of
`Host_Error`). The 4-state `needload` FSM is `enum class NeedLoad`; the
`OQ-8` purge/free split is `purge_for_level_change()` (flags non-world,
non-inline slots `FreeUnused`) + `free_unused()`, with `find_or_alloc`
rescuing re-referenced models to `Present`. Handles are the **OQ-2 opaque
`ModelHandle {index, generation}`** — `free_model` bumps the generation so a
stale handle fails `resolve()` (use-after-free safe). CRC cheat-detection
(OQ-6) lives on the slot (`CrcFlags` + `crc()`); `load_from_bytes` runs the
reload-CRC guard (`LoadError::CrcMismatch`). `studio_extradata` is the single
G-2 raw-`void*` edge; `model_infos()` is the P-4 introspection snapshot.

**Loaders (O-3) — as-built.** `load_from_bytes` dispatches on the file magic:
`IDST`→`parse_studio`, `IDSP`→`parse_sprite`, `IDPO`→`parse_alias`; the BSP
magics (29/30/`BSP2`) return `LoadError::UnsupportedFeature` — brush models
come in via `register_world` / `map_loader`, **not** the byte path (OQ-3, the
seam content delegates to). Alias is parse-minimal (validate + own bytes;
OQ-7). The OQ-4 post-process hook (`IModelPostProcess`) is injected, optional
(null headless), and a `false` return frees the model.

**Studio bone solver (OQ-5) — as-built, bit-exact.** The pure trig/matrix
kernel was promoted to `utilities` (golden-verified against the Q-18 goldens);
`content/bone_solver.cpp` holds the struct-aware layer — `calc_bone_adj`,
the merged-RLE `calc_bones` (transcribed line-for-line from the legacy
decoder), `calc_rotations`, and the `setup_bones` driver. `IBoneSolver` /
`BuiltinBoneSolver` is the swappable game-DLL seam (`Server_GetBlendingInterface`
analog); it is injected at the *server* studio consumers, not at model-load
time. `StudioView` + its six sub-views (`BoneView`, `BoneControllerView`,
`AnimView`, `SeqDescView`, `AttachmentView`, `HitboxView`) are the typed,
bounds-safe G-2/P-4 read
surface over the frozen `studiohdr_t` byte image (out-of-range reads return 0,
untrusted-file-safe).

**Pose/hull query pfns — as-built.** `bone_world_position`
(`Mod_GetBonePosition`), `attachment_world_position`
(`Mod_StudioGetAttachment`), and the geometric core of the hitbox hull
(`studio_hitbox_hulls` → six oriented Minkowski-expanded planes per hitbox +
hitgroup) are shipped. The server side **landed 2026-07-19**:
`SV_ClipMoveToEntity`'s per-hitbox trace loop, the `EntityView` pose
accessors, `SV_HullForStudioModel` gating (size scaling, `sv_clienttrace`,
player-blend, CS shield-skip), the pmove mirror, and the legacy pooled
16-entry pose cache — server-boundary **OQ-2 resolved** (quirk adjudications
recorded there). Content's provider surface for it is
`IModelResolver::studio_hulls` over `studio_hitbox_hulls` + `StudioView`.

**imagelib (O-2) — as-built.** `ImageDecoder` is a pimpl instance; `decode()`
lowercases the extension and walks the read-only `IImageCodec* const registry[]`
(wad, tga, bmp, dds, ktx2, mip, png). Codecs are **stateless** (no global
`image`), parse through bounds-checked `std::span` reads (H-3/H-4), and return
an owned `Image` value (OQ-1). The `img_utils.c` MDL/SPR/LMP/FNT/PAL
image-lump codecs, `Image_Process` (resample/flip/quantise/NeuQuant), and the
`content → imagelib` CMake link are **deferred to the renderer (Chunk 13)** —
the one live stub (`imagelib.cpp:42`, tag `o-2`) is a stale scratch-comment,
not missing behaviour (the stateless codecs need no shared scratch).

**Delegation seams content dispatches to (does not re-specify).**
`filesystem` for file mounting/loads (injected `Filesystem&`); `map_loader`
for brush models (the `register_world` / BSP-magic reject seam); `memory` for
the content pool; `utilities` for CRC32 / byte-swap / the promoted bone math.
See those boundary specs for their contracts.

______________________________________________________________________

## Threading

Full analysis: `docs/threading-analysis/content-threading.md`. Summary —
content is **main-thread-only for mutation, reentrant for pure work**. The
`ModelCache` mutators (init/shutdown/find_or_alloc/register_world/free_model/
purge/free_unused/load_from_bytes/need_crc — 9 sites) and the `ImageDecoder`
lifecycle+decode (3 sites) assert `ThreadRole::Main`; the parsers, the 7
codecs, and the bone/hull kernels hold **no shared state** and are safe to run
off-Main (the P-1 door). The crux hazard is the shared mutable `ModelCache`
registry: the const read accessors (`resolve`/`find`/`validate_crc`/
`studio_extradata`) hand back live refs without a lock — safe under the
current Main-only contract, but the P-2 published-snapshot swap is the door an
off-main render/trace consumer needs.

______________________________________________________________________

## Role & parity

- **Role:** shared substrate with a float-exact obligation. The model/image
  loaders are role-neutral, but the **studio bone/hull math** feeds the
  shared-deterministic collision path — the server world trace and the Chunk-11
  shared `pm_trace` resolve studio hitboxes through it, so a ULP drift there is a
  cross-role divergence, not just a render nit.
- **Parity fence:** HB-2 — the studio bone transform / hitbox math is byte-exact
  no-touch (no FMA, no reassociation).
- **Consumers:** server world trace + client render + (Chunk 11) shared
  `pm_trace`, all through `IModelResolver` / studio hull queries.
- **Annotation:** `model/bone_solver.cpp` carries the `// ROLE: shared-deterministic`
  banner.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`. Content is where the
**worker pool + `JobToken`** land (per-chunk hook, extension-goals §4), so the
context-first door is not theoretical here — it is the prerequisite for the
first parallel workload, and the legacy code is singleton-hostile.

> **As-built confirmation 2026-07-06.** The verdicts below are no longer
> forward-looking — the shipped code *keeps every door open*. The headline P-3
> claim is **realised**: there is **zero file-scope mutable state** in any
> content or imagelib TU. `mod_known` → the `ModelCache` pimpl instance;
> `mod_studiohdr` → the per-call `StudioView` value; the global `image` scratch
> → the (stateless) codecs return an owned `Image`; `g_poseverts` → local
> `setup_bones` scratch; `palette_q1/hl` → `constexpr std::array`; the codec
> table → a `static const IImageCodec* const registry[]` (read-only). The two
> live doors are the **P-1 reentrant-loader door** and the **P-4 typed
> registry door**; both are open by construction (see the refreshed rows). Full
> analysis in `docs/threading-analysis/content-threading.md`.

| Goal / primitive | Applies? | Required seam or door |
|------------------|----------|-----------------------|
| **P-3** context-first, no new file-scope state | **Yes — headline, ✅ met** | Loaders/decoders take injected cache + pools + filesystem as context; `mod_known` / `image` / `mod_studiohdr` / `g_poseverts` singletons are all **gone**. Verified: 0 file-scope mutable statics tree-wide (content-threading.md) |
| **P-1** main-thread inbox + worker pool | **Yes — chunk hook, door OPEN** | The pure work is **already reentrant**: `parse_studio` / `parse_sprite` / `parse_alias`, the 7 stateless codecs, and `calc_bones` / `setup_bones` / `studio_hitbox_hulls` hold no shared state, so a worker can own a parse/decode off-Main today. Only the **publish** step (`ModelCache::load_from_bytes`, which mutates the slot vector) asserts `ThreadRole::Main`. That parse-off-Main / publish-on-Main split **is** the `JobToken` shape — no code change owed, the pool is built when a consumer schedules it |
| **P-2** published-snapshot reads | Partial — **door identified** | `ModelCache::model_infos()` already returns an **owned** snapshot (safe to hold past a mutation). The raw `resolve()` / `find()` accessors hand back a live `Model*` into the mutable slot vector — fine under the current Main-only contract, but an off-main reader (G-3 render/trace) must consume a published snapshot, never a live `resolve()` ref. The double-buffer/generation-swap is the P-2 bring-up work (content-threading.md hazard row) |
| **P-4** typed introspection | **Yes — ✅ door OPEN** | `ModelCache::model_infos()` (`{name,type,needload,crc}` per slot) and `StudioView` (typed, bounds-safe read over the frozen `studiohdr_t`) are the shipped `EntityView` analog — no `extern mod_known`, no raw offset poke. This is the G-1/G-3/G-4 query surface |
| **P-5** narrowest-state signatures | **Yes — ✅ met** | Loaders are free functions over `std::span<const std::byte>` returning `Result<T>`; the bone kernel takes a `BoneSetupInput` + `StudioView`, not a runtime aggregate |
| **P-6** services are satellites | **Yes — ✅ met** | `xash3dpp_imagelib` is a **separate** CMake target (9 TUs) from `xash3dpp_content` (4 TUs), per the Q-11 verdict; `miniz` is linked PRIVATE to imagelib only |
| **P-7** pool-owned RAII lifecycle | **N/A — pimpl + values** | `ModelCache` is a pimpl value type (`make_unique<Impl>`, the sanctioned P-7 carve-out); models own their bytes as `std::vector` members; the codecs are stateless. No pool-owned `create_<thing>` object exists — if one ever appears (e.g. a pool-backed model arena) it takes the canonical shape |
| **P-8** annotation discipline | **Yes — satisfied (denominatored)** | content-threading.md's census (9 `ModelCache` mutator asserts + 3 `ImageDecoder` sites) verified exact by the 2026-07-19 audit; `annotation-coverage` marker classes clean after this pass (the two `InitParams` `@lifetime` tags + the `StudioView::name` `// SAFETY:` landed with it) |
| **G-1** in-engine MCP service | Consumer via P-4 | MCP model queries read `model_infos()` + `StudioView` — the shipped typed surface; no content-side service code |
| **G-2** game ABI v2 | **Door-keep, ✅ confined** | The raw `void*` studiohdr handed to game DLLs is produced only at the ABI edge (`ModelCache::studio_extradata` returns `s->bytes().data()`); everywhere else the engine reads through `StudioView`. A v2 ABI can hand a typed `StudioView` instead of the pointer without touching the loaders — the confinement point is one accessor |
| **G-3** dedicated debug thread | Consumer (see P-2) | An off-main reader consumes the published snapshot (`model_infos()`), never a live `resolve()` ref — the P-2 row's double-buffer bring-up is the entire G-3 obligation here |
| **G-4** expanded in-game debugging | Consumer via P-4 | Model-cache overlays read the same typed surface (`model_infos()` slots + `StudioView`); nothing content-side beyond keeping it typed |
| **G-5** scripting runtime | Nothing owed now | §G-5's script surface v0 names no content affordance; a tooling VM would consume the same P-4 typed queries. Revisit only if a script binding wants model queries directly |
| **Q-12** compat scope | **Yes — door** | A `content::ICompatPolicy` for the GoldSrc WAD / palette / decal / luma quirks (Quake-vs-HL palette classification, gradient decals), link-selected by `XASH_GOLDSRC_COMPAT`. The palette machinery (`palette.cpp`) already isolates the two built-in tables; the compat seam wraps the quirk *selection*, not the tables |

______________________________________________________________________

## Open questions

- **OQ-1 — `rgbdata_t` ABI status. ✅ DECIDED 2026-07-06: internal `Image`
  type + `rgbdata_t` adapter at the renderer seam** (the `IProtocolDriver`
  precedent). imagelib's public result is a fresh `Image` value type, so the
  codecs decode through `std::span` / `std::mdspan` and `enum class` formats
  internally (unlocks modernization H-4 / M-1 / M-2 / O-2); `rgbdata_t` is
  materialised only by a compat adapter at the (future, Chunk 13) renderer seam.
  Content does **not** freeze `rgbdata_t`.
- **OQ-2 — typed model handle shape. ✅ DECIDED 2026-07-06: opaque
  `ModelHandle` (index + generation) internally, raw pointer only at the ABI
  edge.** `ModelCache` lookups return/consume a generation-checked `ModelHandle`
  (stale-slot safe); the frozen `SV_ModelHandle(int) → model_t*` and
  `pfnGetModelPtr → void*` are produced only where the ABI demands a raw
  pointer. This is the O-1 `ModelCache` public surface.
- **OQ-3 — brush texture-finishing seam. ✅ DECIDED 2026-07-06: content fills
  `texture_t` (decode via imagelib); `map_loader` stays geometry-only; renderer
  uploads.** `map_loader` keeps producing geometry + texinfo indices with no
  texels (unchanged — no re-widening of the seam it deliberately narrowed);
  content walks `model_t.textures[]`, resolves embedded-miptex vs external-WAD
  (`r_wadtextures`) and decodes to RGBA via `xash3dpp_imagelib`; the renderer
  uploads the finished `texture_t` at Chunk 13. This activates the commented
  `content → imagelib` CMake link. (The other `map_loader`-deferred payloads
  split per-consumer: content owns texture decode; `LUMP_LIGHTING`, surface
  extents/`msurface_t`, glpolys stay renderer/server work.)
- **OQ-4 — DLL post-process seam shape. ✅ RESOLVED (implemented):** an injected
  `content::IModelPostProcess` (`InitParams.post_process`, optional/null on
  headless); `ModelCache::load_from_bytes` calls `on_model_loaded` after the
  payload attaches and **frees the model on a false return** (legacy
  `Mod_ProcessRenderData`/`Mod_ProcessUserData` returning 0). The
  dedicated-vs-client branch is the implementer's: the server registers a
  physics `on_model_loaded`, the client a render-data one — content calls out to
  whichever is injected, never linking toward the renderer.
- **OQ-5 — studio bone-math placement. ✅ RESOLVED 2026-07-06 (implemented):**
  the pure `R_StudioCalcBones` / `QuaternionSlerp` / matrix kernel was promoted to
  `utilities` (`quaternion.hpp` + `matrix.cpp` — bit-exact against the Q-18
  verbatim-legacy goldens, `tests/goldens/studio_math_goldens.inc`); the
  struct-walking merged RLE decoder + the `SV_StudioSetupBones` driver live in
  `content` (`bone_solver.cpp`); the swappable game-DLL solver
  (`Server_GetBlendingInterface`) is the `content::IBoneSolver` seam
  (`BuiltinBoneSolver` builtin). **Placement note:** `IBoneSolver` is NOT a
  `ModelCache::InitParams` dependency — bone setup runs at trace/query time over a
  `StudioView`, so the solver is injected at the server studio consumers, not at
  model-load time.
- **OQ-6 — CRC cheat-detection ownership. ✅ RESOLVED (implemented):** the
  per-model CRC + `CrcFlags` live on the `ModelCache` registry;
  `ModelCache::need_crc` / `validate_crc` are the server-facing surface, and
  `load_from_bytes` runs the reload CRC guard (`LoadError::CrcMismatch`).
- **OQ-7 — parity scope of legacy quirks.** *Partially decided:* **alias =
  parse-minimal** (render-only, validated but not meshed); sprite/studio header
  parse implemented. *Still open (with the studio-collision pass):*
  `XASH_LOW_MEMORY` texel truncation, external `…T.mdl` merge,
  dedicated-server sprite half-load, and the Quake pitch-inversion bug.
- **OQ-8 — model-registry lifecycle coupling. ✅ RESOLVED (implemented):**
  content owns the `purge_for_level_change` / `free_unused` FSM (with
  `find_or_alloc` rescuing re-referenced models); the world (slot 0) and inline
  `*N` submodels are never purged. The level-change **orchestrator** (host/
  server, via the map-load FSM) drives the purge/free sequence around the new
  world load; the precache maps stay in `server`/`client`.

______________________________________________________________________

*Recon deliverable. Scaffold landed (commit `0a387308`). The gating
OQ-1 / OQ-2 / OQ-3 are ✅ decided 2026-07-06 (above); OQ-4…OQ-8 are
implementation-time calls. Next: `/plan-implementation content` — stand up the
`ModelCache` (O-1) and the `IImageCodec` registry (O-2) first.*
