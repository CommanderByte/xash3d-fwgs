# BSP Loading Pipeline

> **Defined in**: `private/map_loader/bsp/{disk_format,bsp_loader}.hpp`,
> `src/map_loader/bsp/{bsp_loader,bsp_lumps,bsp_flags}.cpp`\
> **Namespace**: `xash::map_loader::bsp`\
> **Legacy reference**: `engine/common/mod_bmodel.c`, `common/bspfile.h`
> ([deep dive](../../legacy-survey/deep-dive-bsp-loader.md))

## Overview

The loader turns a whole-file BSP image (`std::span<const std::byte>`) into
an immutable [`WorldData`](./world-data.md) in one pass, preserving the
legacy heap-builder stage order for parity auditability. All format variance
is resolved here so queries never branch on the BSP flavour.

## Header parsing and quirk detection — `parse_header`

Produces a `HeaderInfo{version, bsp30ext, blueshift_swap, clipnodes32,
header}`:

- **Version dispatch**: 29 (Quake1), 30 (HalfLife), `'BSP2'` fourcc; anything
  else → `BspUnsupportedVersion`.
- **BSP30ext probe** (v30 only): reads ONLY the 4-byte id at offset 124 —
  the extra header's own version field gates extra-LUMP loading in legacy,
  not this flag. A mismatched extra version still counts as BSP30ext
  (pinned by test).
- **Blue-Shift swap** (v30, non-ext): entities lump lacks a quoted
  `"classname"` but the planes lump has one → the two directory entries are
  swapped; `resolve_lump` applies the swap transparently.
- **Extended clipnode guess** (BSP30ext): `filelen % 8 != 0 ||
  filelen / 12 >= 32767` → 12-byte records.

## Per-lump validation — `resolve_lump`

Mirrors legacy `Mod_LoadLump` against the vendored `k_src_lumps[]` table
(mincount/maxcount/entrysize/entrysize32/CHECK_OVERFLOW per lump):

1. `fileofs == 0` → silently absent (even for required lumps — legacy order
   of checks preserved).
2. Entry size resolved: BSP2 → 32-bit variant everywhere; BSP30ext clipnode
   guess; else classic.
3. `filelen <= 0` → error only when `mincount > 0 && entrysize != 1`.
4. Bounds check vs the file image (**hardening** — legacy trusts
   fileofs/filelen).
5. `filelen % entrysize`, `count < mincount` → `BspCorruptLump`;
   `count > maxcount` → error iff CHECK_OVERFLOW, warn otherwise.

Returns a `LumpView{bytes, count, entrysize, present}` over the file image —
no copying.

## Stage order (`WorldDataFill`, driven by `load_world_data`)

`begin` → `entities` → `planes` → `submodels` → `textures` → `visibility` →
`texinfo` → `surfaces` → `marksurfaces` → `leafs` → `nodes` → `clipnodes` →
`make_hull0` → `setup_submodels` → `checksum` → `finalize`. The relative
order of retained legacy stages is preserved (vertex/edge/lighting stages
are render-side and skipped). Stage functions are the only code with write
access to `WorldData` (single `friend struct WorldDataFill`).

Notable stage behaviours:

- **entities**: raw text copy (NUL via `std::string`); a supplied
  `WorldLoadOptions::entity_patch` replaces the lump wholesale (the
  `maps/<name>.ent` mechanism — see [fsm.md](./fsm.md)). Worldspawn-only key
  scan via `utilities::Tokenizer` (COM_ParseFileSafe port) captures `wad`
  (raw) and `message`; malformed text → `BspBadWorld`.
- **surfaces** (flag subset): SURF_* derivation from lowercased miptex names
  (`sky*`, water names incl. the case-sensitive-`water` /
  case-insensitive-`laser` asymmetry and the `*default` exception,
  `scroll*`, `{scroll`, `{`, TEX_SCROLL/TEX_SPECIAL) plus the corrupt-face
  guard against the surfedge record count (widened addition).
- **leafs**: cluster = index−1 clamped at `visclusters`; `visofs` kept raw
  and unclamped (legacy pointer-arithmetic parity); leaf 0 must be
  CONTENTS_SOLID for worlds; non-empty leafs mark their marksurfaces
  `SURF_UNDERWATER`; the water-alpha probe sets `k_fworld_wateralpha`.
- **checksum**: see the wire-frozen CRC rules in the
  [boundary spec §3](../../boundaries/map_loader-boundary.md).

## Error handling

`std::expected<WorldData, core::ErrorCode>` with `BspUnsupportedVersion` /
`BspCorruptLump` / `BspBadWorld`; every failure logs at tag `map_loader`
(Q-5). Legacy `Host_Error` process-kills became error returns; world loads
fail on lump errors (legacy's "a1ba" branch tolerated them) — both
documented Known Deviations.

## Threading model

Cold path, main thread only. The produced `WorldData` is immutable —
see [threading analysis](../../threading-analysis/map_loader-threading.md).

## See also

- [world-data.md](./world-data.md) — the output model
- [hulls.md](./hulls.md) — clipnode/hull stages in detail
- [Boundary spec §4/§5](../../boundaries/map_loader-boundary.md) — quirk
  catalogue + Known Deviations
