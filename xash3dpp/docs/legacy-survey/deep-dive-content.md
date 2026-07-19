# Deep Dive: Legacy Content Pipeline — studio/sprite/alias model formats + imagelib codecs + the studio bone/pose math

*Recon brief produced 2026-07-06 by a read-only survey as part of the as-built
documentation refresh. **Scope: the non-brush content pipeline** — the model
handle cache, the studio/sprite/alias on-disk formats, the image codec family,
and the studio bone/pose/hull math. The **brush** path (BSP load, hull build,
map CRC) is `map_loader`'s and has its own briefs
([deep-dive-bsp-loader.md](deep-dive-bsp-loader.md),
[deep-dive-trace-pvs.md](deep-dive-trace-pvs.md)) — not duplicated here. Wide
survey coverage: [engine-common-and-platform.md](engine-common-and-platform.md)
and [public-common-sdk.md](public-common-sdk.md). Line numbers are against the
legacy tree; behaviour references, not design constraints. Everything here is
**legacy** — read it for the on-disk layouts, the decode quirks, and the
bit-exact bone math, not as a rewrite blueprint (the rewrite chose a
`ModelCache` pimpl + opaque `ModelHandle` + stateless `IImageCodec` codecs +
`StudioView`/`BoneSetupInput` value types — see §7 As-built mapping).*

Primary sources:

- `engine/common/model.c` (~640 lines) — the `mod_known[]` handle cache,
  find-or-load, the magic-dispatch loader, the level-transition purge
- `engine/common/mod_studio.c` (~1200 lines) — studio header load, the
  renderer-free server-collision queries (bone position, attachment, hitbox
  hull), the seqgroup demand-loader, the swappable bone-solver hook
- `engine/common/mod_sprite.c` / `mod_alias.c` — the sprite (`IDSP`) and Quake
  alias (`IDPO`) loaders
- `public/xash3d_mathlib.c` — `R_StudioCalcBones` (the merged RLE decompressor),
  `AngleQuaternion` / `QuaternionSlerp` / matrix concat
- `engine/studio.h` — the frozen `studiohdr_t` + `mstudio*` ABI (version 10)
- `common/com_model.h` — `model_t`, `msprite_t`, `aliashdr_t`, `modtype_t`,
  `cache_user_t`
- `engine/common/imagelib/` — `img_main.c` / `img_utils.c` + `img_bmp/tga/dds/
  ktx2/png/wad.c` (the codec family), `img_quant.c` (NeuQuant)

______________________________________________________________________

## 0. Headline findings (read first)

1. **Everything hangs off three global singletons.** The model cache is
   `mod_known[MAX_MODELS]` (`model.c:27`); studio queries rewrite a
   `mod_studiohdr` current-model pointer (`mod_studio.c:46`); every image codec
   decodes into one file-scope `imglib_t image` struct (`img_main.c:23`). These
   three make the whole subsystem non-reentrant — and they are exactly what the
   rewrite dissolved (§7).
2. **The in-memory result structs are frozen ABI; the *process* is not.** Game
   DLLs walk `studiohdr_t` by raw offset via `pfnGetModelPtr`'s `void*`; the
   client draws `msprite_t`/`aliashdr_t`; the studio DLL caches `cache_user_t`.
   Field layout may not move. The load path, scratch, and dispatch around them
   are free to modernise.
3. **Slot 0 is the world, and it is load-bearing.** `Mod_LoadWorld` pins the
   BSP to `mod_known[0]` with `ASSERT(pworld == mod_known)`; the purge frees
   slot 0 explicitly and `Mod_FreeUnused` starts at slot 1. Inline `*N` brush
   submodels share the world data and must never be individually freed.
4. **The bone math is bit-exact and shared headless.** `R_StudioCalcBones`
   solves bones with no renderer present (server collision) by linking the pure
   math from `xash3d_mathlib`. The RLE walk + the exact-float lerp gate are an
   observable contract — the rewrite golden-verified it (Q-18).
5. **imagelib is a GoldSrc byte-for-byte parity surface.** Palette
   classification (Quake vs HL on the first 765 bytes), texgamma/overbright,
   luma fullbright, index-255 transparency, and a dozen texture-name-prefix
   quirks must reproduce the original engine's pixels exactly.

______________________________________________________________________

## 1. The model handle cache (`model.c`)

```c
mod_known[MAX_MODELS]   // slot array; slot 0 = world
mod_numknown            // high-water mark, NOT a live count
mod_crcinfo[MAX_MODELS] // parallel CRC flags + initialCRC
com_studiocache         // shared studio seqgroup cache pool
```

`MAX_MODELS` is build-dependent: **4096** default, **1024** / **512** under
`XASH_LOW_MEMORY 1/2`.

**`needload` is a 4-state enum, not a bool** (`mod_local.h:57`):
`NL_UNREFERENCED=0` (free slot) / `NL_NEEDS_LOADED` / `NL_PRESENT` /
`NL_FREE_UNUSED`. It is stored in the frozen `model_t.needload` (`qboolean`
storage). A slot is free iff `NL_UNREFERENCED`.

**Find-or-load** (`Mod_ForName` = `Mod_FindName` + `Mod_LoadModel`):

- `Mod_FindName` — linear scan by name; never returns NULL; allocates a slot or
  reuses a `NL_UNREFERENCED` one; sets `needload`.
- `Mod_LoadModel` — the **dispatcher**: `FS_LoadFile`, CRC32 the raw bytes,
  `switch` on the file magic to the studio/sprite/alias loader (or brush →
  `map_loader`), then run the DLL post-process hook, then null `g_poseverts`.

**Level-transition purge (not access-LRU):** `Mod_LoadWorld` →
`Mod_PurgeStudioCache` empties `com_studiocache`, frees inline submodels, flags
every other model `NL_FREE_UNUSED`; the new map's `Mod_FindName` calls bump
survivors back to `NL_PRESENT`; `Mod_FreeUnused` reaps the rest.

**CRC cheat-detection:** a second load with a changed CRC → `Host_Error`.
`Mod_ValidateCRC` / `Mod_NeedCRC` expose it to `sv_client`.

Quirk list:

- World pinned to slot 0 (`ASSERT(pworld == mod_known)`); purge frees slot 0.
- "bmodels and sprites don't cache normally" — inline `*N` submodels share world
  data, force-`NL_PRESENT`, never mempool-freed individually.
- Dedicated-server sprites are **half-loaded** (`numframes` zeroed, texture load
  skipped).
- `MAX_MODELS` overflow → `Host_Error` (hard cap).

______________________________________________________________________

## 2. Studio format (`engine/studio.h`, `mod_studio.c`) — version 10

Magic `IDST` (`"IDST"` LE = `0x54534449`), `STUDIO_VERSION == 10`. The
`studiohdr_t` (244 B through `transitionindex`) is a header of `int count` +
`int index` (byte-offset) pairs into sub-chunk arrays inside the same file
image. Sub-chunk strides (frozen):

| Struct | Bytes | Key fields (offset) |
|--------|-------|---------------------|
| `mstudiobone_t` | 112 | `parent`@32, `bonecontroller[6]`@40, `value[6]`@64 (0-2 pos, 3-5 rot rad), `scale[6]`@88 |
| `mstudiobonecontroller_t` | 24 | `bone`@0, `type`@4, `start`@8, `end`@12, `index`@20 |
| `mstudioanim_t` | 12 | `uint16 offset[6]` — byte offsets to each channel's RLE stream (0 = absent) |
| `mstudioanimvalue_t` | 2 | union `{num{valid,total}, int16 value}` — the RLE token |
| `mstudioseqdesc_t` | 176 | `numframes`@56, `motiontype`@68, `motionbone`@72, `numblends`@120, `animindex`@124, `seqgroup`@156 |
| `mstudioattachment_t` | 88 | `flags`@32, `bone`@36, `org`(vec3)@40 |
| `mstudiobbox_t` | 32 | `bone`@0, `group`@4, `bbmin`(vec3)@8, `bbmax`(vec3)@20 |

Bone-controller motion flags (`type`): `STUDIO_X/Y/Z` = 0x0001/0002/0004,
`STUDIO_XR/YR/ZR` = 0x0008/0010/0020, `STUDIO_TYPES` mask 0x7FFF, `STUDIO_RLOOP`
0x8000 (0..360 wrap). Controller index **4** is the reserved `STUDIO_MOUTH`
slot (ignored by the collision solver).

Quirks:

- `mod_studiohdr` singleton ⇒ not reentrant; every bone/attachment/hull call
  rewrites it.
- **Quake pitch-inversion bug**: `angles2[PITCH] = -angles2[PITCH]` unless
  `ENGINE_COMPENSATE_QUAKE_BUG` — an observable behavioural invariant applied
  render/attachment-side, not by the loader.
- `XASH_LOW_MEMORY` drops studio texel data post-upload, **mutating
  `studiohdr.length`**.
- External `…T.mdl` textures are merged onto a fresh copy when
  `numtextures == 0`.
- Big-endian re-swaps demand-loaded seqgroups by re-deriving the header via
  `PARM_GET_STUDIO_HDR`.
- Former sound fields are now `unused[]`; `studiohdr2index` cannot move.

______________________________________________________________________

## 3. The studio bone / pose / hull math (`xash3d_mathlib.c` + `mod_studio.c`)

The renderer-free server-collision surface. **This is the bit-exact class
(Q-18).**

**`Mod_StudioCalcBoneAdj`** — fill `adj[]` (indexed by controller slot) from the
entity's raw controller bytes: `STUDIO_RLOOP` controllers use
`ctl * (360/256) + start`; others `((1-v)*start + v*end)` with `v = clamp(ctl/255,
0, 1)`; rotation controllers scale by `M_PI_F/180` (the **float** `M_PI_F`,
distinct from the double `DEG2RAD`). Mouth slot skipped.

**`R_StudioCalcBones`** — the merged position+rotation RLE decompressor. For
each channel (max = q ? 6 : 3): if no anim channel, `value = bone.value +
adj`; else walk the `mstudioanimvalue_t` spans (`total`/`valid` counts) to the
one containing `frame`, then either copy (`s == 0` or past `valid`) or lerp the
two adjacent tokens by `s`. The exact-float gate on whether to lerp or copy is
observable. Position = `value[0..2] * scale`; rotation → `AngleQuaternion`, then
`QuaternionSlerp` between blends.

**`Mod_StudioCalcRotations`** — decode every used bone of one blend, then zero
the driven linear-motion axes of the motion bone (`motiontype & STUDIO_X/Y/Z`).

**`SV_StudioSetupBones`** — the driver: pick the sequence (clamp OOB → 0),
decode rotations for the blend(s), concat each bone onto its parent's world
matrix. The **swappable** game-DLL solver comes from
`Server_GetBlendingInterface` (`pBlendAPI`); the builtin `gBlendAPI` is the
fallback.

**`Mod_GetBonePosition`** / **`Mod_StudioGetAttachment`** — setup bones for the
pose, read the bone's world origin/angles (attachment concats its local `org`
first). **No pitch flip in the loader** — the caller applies it.

**`Mod_HullForStudio`** / **`Mod_SetStudioHullPlane`** — setup bones, then for
each `mstudiobbox_t` emit **six oriented planes** (the bone matrix axis columns;
even faces `bbmax +` the Minkowski trace-box expansion, odd faces `bbmin -`)
plus the hit group. A 16-entry trace-cache ring (`cache_studio[16]`) memoises
recent hulls. This is the surface the server's `SV_ClipMoveToEntity` per-hitbox
trace loop consumes.

______________________________________________________________________

## 4. Sprite format (`mod_sprite.c`, `com_model.h`)

Magic `IDSP`. Versions **1** (Quake), **2** (Half-Life), **32** (truecolor).
On-disk `dsprite_q1_t` vs `dsprite_hl_t` differ; in-memory `msprite_t` +
`mspriteframe_t` / `mspritegroup_t` / `mspriteframedesc_t`. Header carries
`type` (angle/camera align), `texFormat` (drawtype; 0 on Quake), `numframes`,
`bounds[2]` (max w/h), `boundingradius`. Frame textures are the renderer's job;
the loader parses the header and owns the bytes. Dedicated-server half-load
zeroes `numframes`.

______________________________________________________________________

## 5. Alias format (`mod_alias.c`) — effectively dead for HL

Magic `IDPO`, Quake MDL v6. Studio owns `.mdl` for HL, so alias is
**parse-minimal**: validate the header, own the bytes; the mesh/skins are
render-only. The server needs nothing (`Mod_AliasExtradata` is a dead
prototype). Loader scratch: `g_poseverts[MAXALIASFRAMES]` + `g_posenum`;
`aliashdr.pposeverts` dangles at it until `Mod_LoadModel` nulls it
post-mesh-build (`model.c:392`).

______________________________________________________________________

## 6. imagelib codecs (`img_*.c`) — the byte-exact texture surface

One global `imglib_t image` decode scratch; a `load_game[]` dispatch table
(dds, bmp, tga, png, wad, mip, mdl, spr, lmp, fnt, pal, ktx2) keyed by
extension; a `save_game[]` table (tga, bmp, png, wad). Output is always
extracted to 32-bit RGBA.

**Palette machinery** (`img_utils.c`): `palette_q1[768]` / `palette_hl[768]`
(differ only at entries 244-246, HL particle green), `d_8to24table`,
`Image_ComparePalette` (Quake-vs-HL classified on the first 765 bytes),
texgamma/overbright per palette colour (`BuildGammaTable`, default gamma 2.5 /
texgamma 2.0), luma fullbright threshold index **≥ 224**, `Image_FindBestBlack`
per palette (luma flag dropped if none found).

**Texture-name-prefix quirks** (keyed off the name or the embedded `mip.name`):

- `{` → masked/gradient decal (`IMAGE_ONEBIT_ALPHA`), index-255 transparent.
- `!` / `water*` → grab water fog colour + density into `fogParams`.
- `sky` + `w == 2h` → `IMAGE_QUAKESKY`, kept 8-bit.
- `~` / `+N~` → wad3 luma (fullbright overlay).
- `#` → load from an in-memory buffer; `#logo` / `#XASH_SYSTEMFONT_001` →
  spray / menu-font tricks.
- A Q1-paletted HL mip is left `LUMP_NORMAL` (no texgamma) to avoid
  over-darkening.

**Per-codec quirks:**

- **BMP** tolerates a filesize mismatch (Sweet Half-Life `splash.bmp`).
- **TGA** honours the attributes-bit-0x20 vertical-flip; `IL_DONTFLIP_TGA`;
  image types 1/2/3 + RLE variants 9/10/11.
- **DDS / KTX2** kept **compressed** for GPU decode (only alpha blocks scanned
  for `IMAGE_HAS_ALPHA`); need `IL_DDS_HARDWARE`.
- **PNG** rejects Adam7 interlacing; inflate/CRC via miniz.
- **MIP / WAD** index-255 transparency across Q1/HL/MIP/WAD/LMP paths.
- **Cubemap** loader fabricates black sides and forces a constant `w*h*4`
  per-side size "because render.dll expects it".
- **NeuQuant** (`img_quant.c`) reserves index 255 black for sprays.

______________________________________________________________________

## 7. As-built mapping (legacy → `xash3dpp/content` + `xash3dpp_imagelib`)

| Legacy | As-built | Notes |
|--------|----------|-------|
| `mod_known[MAX_MODELS]` + `mod_numknown` | `ModelCache` pimpl over `std::vector<Slot>` | grows to `content_max_models`; null handle on overflow (not `Host_Error`) |
| slot index (raw int) | opaque `ModelHandle {index, generation}` (OQ-2) | generation-checked; use-after-free safe |
| `needload` `NL_*` in a `qboolean` | `enum class NeedLoad` | FSM is a class invariant |
| `Mod_ForName` / `Mod_FindName` | `find` / `find_or_alloc` / `register_world` | slot-0-world preserved |
| `Mod_LoadModel` magic `switch` | `load_from_bytes` → `parse_studio/sprite/alias`; BSP magic → `map_loader` seam | brush not on the byte path |
| `mod_crcinfo[]` + `Mod_ValidateCRC/NeedCRC` | `CrcFlags` on the slot + `need_crc` / `validate_crc` | reload guard = `LoadError::CrcMismatch` |
| `Mod_PurgeStudioCache` + `Mod_FreeUnused` | `purge_for_level_change` + `free_unused` (OQ-8) | world + inline `*N` never purged |
| `mod_studiohdr` singleton | `StudioView` (per-call value) + 6 typed sub-views | bounds-safe; the G-2/P-4 read surface |
| `pfnGetModelPtr` raw `void*` | `ModelCache::studio_extradata` (the one ABI edge) | v2 ABI can hand a `StudioView` |
| `R_StudioCalcBones` (in `xash3d_mathlib`) | `utilities` quaternion/matrix (bit-exact goldens) + `content::calc_bones` RLE layer | Q-18 no-touch |
| `SV_StudioSetupBones` + `pBlendAPI` | `content::setup_bones` + `IBoneSolver` / `BuiltinBoneSolver` | injected at server studio consumers |
| `Mod_GetBonePosition` / `…GetAttachment` / `Mod_HullForStudio` | `bone_world_position` / `attachment_world_position` / `studio_hitbox_hulls` | plane-production core done; server trace-loop gated on hl.dll smoke |
| `imglib_t image` global | codecs return an owned `Image` value | stateless / reentrant |
| `load_game[]` table | `IImageCodec* const registry[]` (7 codecs) | `handles(ext)` dispatch |
| `palette_q1/hl[768]` + init bools | `constexpr std::array<…,768>` | no lazy-init race |
| `Mod_ProcessRenderData` / `Mod_ProcessUserData` | injected `IModelPostProcess` (OQ-4) | null headless; false = free |
| `Mem_Malloc(host.imagepool, …)` | `memory::PoolHandle` + `std::vector<std::byte>` | Q-2 pooled |

**Deferred to the renderer (Chunk 13), not content:** the MDL/SPR/LMP/FNT/PAL
image-lump codecs, `Image_Process` (resample/flip/quantise/NeuQuant), the
internal `Image`↔`rgbdata_t` adapter (OQ-1), and the `content → imagelib` +
`content → map_loader` CMake links.
