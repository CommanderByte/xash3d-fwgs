# PVS Queries

> **Defined in**: `map_loader/pvs.hpp`, `src/map_loader/pvs.cpp`\
> **Namespace**: `xash::map_loader`\
> **Legacy reference**: `mod_bmodel.c:1059-1341` (`Mod_DecompressPVS`,
> `Mod_PointInLeaf`, `Mod_GetPVSForPoint`, `Mod_FatPVS`, `Mod_Box*`)
> ([deep dive §3](../../legacy-survey/deep-dive-trace-pvs.md))

## Overview

Visibility answers over `const WorldData&`: which cluster a point is in,
whether an AABB can be seen given a vis row, and the radius-merged "fat" PVS
the server sends per client. Everything is a pure free function — the legacy
`g_visdata` shared static became caller buffers and locals.

## Key operations

- **`decompress_pvs(in, visbytes, out)`** — classic zero-RLE (nonzero byte =
  literal; zero byte + run length). Empty input ⇒ 0xFF fill (the legacy NULL
  = "all visible" convention). Runs clamp to the output; an exhausted stream
  zero-fills the remainder (hardening — legacy reads past the buffer).
- **`point_leaf(w, p)`** — draw-node walk from node 0; **on-plane points go
  to the BACK child** (`plane_diff <= 0`) — the deliberate asymmetry vs the
  hull walkers' strict `< 0`, pinned by paired tests.
- **`leaf_compressed_pvs(w, leaf)`** — the raw compressed run:
  `visdata[visofs..end]` (legacy pointer semantics — decompression reads the
  identical byte stream); `visofs == -1`, out-of-range offsets or bad leaf
  indices ⇒ empty span ⇒ full visibility downstream.
- **`pvs_for_point(w, p, out)`** — decompresses the point-leaf's row;
  returns false for clusterless leafs (legacy returned NULL and callers
  treated it as fullvis). Pre: `out.size() >= w.visbytes()`.
- **`box_leafnums(w, mins, maxs, list, topnode)`** — collects the CLUSTER
  numbers (legacy stores `leaf->cluster`, not indices) of non-solid leafs
  the box touches, recursing via `box_on_plane_side` (the exact
  signbits-indexed corner tables from `BoxOnPlaneSide`); stops when the list
  fills; `topnode` gets the first straddling node.
- **`box_visible(w, mins, maxs, visbits)`** — any touched cluster set in
  `visbits`; empty visbits ⇒ true. Uses a `k_max_box_leafs` (256,
  limits-routed) stack list.
- **`fat_pvs(w, org, radius, visbuffer, merge, fullvis)`** — ORs the rows of
  every leaf whose plane distance band is within `radius`
  (`k_fatpvs_radius = 8`); fullvis / no-visdata / clusterless-origin fall
  back to 0xFF; `merge` accumulates. Returns
  `min(w.visbytes(), visbuffer.size())` (the legacy `Q_min`). The PHS path
  of legacy `Mod_FatPVS` is **not** here — PHS precomputation belongs to the
  server (Chunk 6).

## Threading model

Pure reads over immutable data + caller buffers — safe from any thread after
activation (Q-6). `fat_pvs` allocates a per-call scratch row (cold enough;
noted in the threading analysis).

## Edge cases and invariants

- `check_vis_bit(vis, cluster)` is false for negative clusters (legacy
  CHECKVISBIT gate) — this is what makes raw/unclamped `visofs` safe.
- Node-tree traversal is not cycle-guarded (legacy-equivalent residual risk,
  boundary spec §5; Chunk 6 hardening follow-up).

## See also

- [world-data.md](./world-data.md) — visdata/visbytes/cluster derivation
- [trace.md](./trace.md) — the opposite tie-break
