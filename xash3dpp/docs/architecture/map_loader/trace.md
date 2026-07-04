# Trace Kernel and Hull Selection

> **Defined in**: `map_loader/trace.hpp`, `src/map_loader/trace.cpp`,
> `private/map_loader/trace_math.hpp`\
> **Namespace**: `xash::map_loader`\
> **Legacy reference**: `engine/common/pm_trace.c`
> ([deep dive §1](../../legacy-survey/deep-dive-trace-pvs.md))

## Overview

The ONE canonical collision kernel: legacy server physics (`SV_Move`), the
`pfnPM_Move` seam and client prediction all funnel through
`PM_RecursiveHullCheck`, and this file is its float-for-float port. **Q-18
applies in full**: strict FP compilation, ULP-exact expressions, and golden
trace fixtures as the standing determinism gate.

Deliberately **edict-free** (the hard Chunk 6 prerequisite): no physent
list, entity indices or `usehull` global. The server composes per-entity
traces from the pieces below and owns nearest-fraction merging, hit-entity
recording, rotated-entity transforms and `PM_*` filter flags.

## The kernel

- **`hull_point_contents(hull, num, p)`** — iterative clipnode walk;
  `children[plane_diff < 0]` (strict `<`: on-plane ⇒ FRONT child — opposite
  of `point_leaf`); empty `planes` span ⇒ `k_contents_none` (legacy "fantom
  bmodels" guard).
- **`recursive_hull_check(hull, num, p1f, p2f, p1, p2, trace)`** —
  statement-for-statement port: leaf contents handling (SOLID ⇒ startsolid;
  else clear allsolid, EMPTY ⇒ inopen else inwater), degenerate-hull
  early-open, the `goto loc0` tail loop as `for(;;)/continue`,
  `side = (t1 < 0.0f)`, the `DIST_EPSILON` (1/32) near-side nudge with
  side-dependent sign, two independent clamps, `VectorLerp` expansion
  `v1 + frac*(v2-v1)` per component, near-side recursion first, far-side
  contents check at `mid`, allsolid impact suppression, plane negation on
  back-side hits, the `frac -= 0.1f` backup loop re-checking from
  `firstclipnode`, and the hull-LOCAL `endpos` write. A bad node number logs
  and aborts the trace where legacy `Host_Error`s (documented deviation).
- **`trace_hull`** — `PM_InitPMTrace` (endpos = end, allsolid = true,
  fraction = 1) + kernel from `firstclipnode` over [0, 1].
- **`finalize_trace(tr, start_world, end_world)`** — the extracted
  per-entity post-processing: allsolid ⇒ startsolid ⇒ fraction 0; clean
  traces get the WORLD-frame endpos lerp and the
  `plane.dist = dot(endpos, normal)` recompute.

`plane_diff` (private `trace_math.hpp`) keeps the legacy axial fast path —
`type < 3` reads the coordinate instead of the dot product; algebraically
equal for unit axial normals but ULP-distinct, and tie-breaks sit exactly on
those boundaries.

## Hull selection

- **`world_hull(w, submodel, bsp_hull)`** — `TraceHull` view over a wired
  `HullDescriptor`: hull 0 walks `hull0_nodes()`, hulls 1–3 walk
  `clipnodes()`; an absent hull yields empty spans (legacy
  `planes == NULL`: CONTENTS_NONE point answer, fully-open traces).
- **`hull_for_bsp(w, submodel, usehull, player_bounds, origin)`** — the
  legacy switch (usehull 1/2/3/default → BSP hull 3/0/2/1) plus the
  centering offset `clip_mins − player_mins + origin`; the caller moves the
  ray into the local frame.
- **`BoxHull`** — `PM_InitBoxHull`/`PM_HullForBox` as a value type: the
  fixed six-node chain + six axial planes (`type = i>>1`, interleaved
  maxs/mins distances). Non-copyable/non-movable — its cached `TraceHull`
  spans reference its own member arrays (QJ self-referential rule). Callers
  pass Minkowski-expanded bounds (`ent.mins − player_maxs`,
  `ent.maxs − player_mins`) for entity sweeps.

## Verification posture (Q-18 gate)

Golden vectors in `tests/map_loader/trace/test_hull_trace.cpp` are
hand-derived with inline derivations and compared on **bit patterns**
(`std::bit_cast`). Before commit they were cross-checked against the legacy
kernel compiled VERBATIM in a throwaway harness — 18,156 traces across
box/wedge/water/DAG/inconsistent hulls, zero mismatches — and the code
passed a line-by-line adversarial parity audit (PARITY-CONFIRMED). Any
change to `trace.cpp`/`trace_math.hpp` must keep these tests bit-green.

## Threading model

Pure reads over caller-supplied views; safe from any thread (Q-6). One
`BoxHull` per callsite/thread (mutable `set_bounds`).

## See also

- [hulls.md](./hulls.md) — how the hulls get wired
- [Boundary spec §2 "Chunk 6 contract"](../../boundaries/map_loader-boundary.md)
