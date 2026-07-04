# Clip Hulls — widening, MakeHull0, submodel wiring

> **Defined in**: `src/map_loader/bsp/bsp_hulls.cpp`\
> **Namespace**: `xash::map_loader::bsp`\
> **Legacy reference**: `mod_bmodel.c` — `Mod_LoadClipnodes`, `Mod_MakeHull0`,
> `Mod_SetupHull`, `Mod_SetupSubmodels`, `Count*ClipNodes_r`, `RemapClipNodes_r`

## Overview

GoldSrc collision runs on four clip hulls per brush model: hull 0 (point,
derived from the draw nodes) and hulls 1–3 (precomputed against the player
extents). This stage widens every on-disk clipnode variant to `ClipNode32`,
builds hull 0, and wires per-submodel `HullDescriptor`s.

## Widening (`WorldDataFill::clipnodes`)

- Source width follows the entry size resolved at the lump level (BSP2 or
  the BSP30ext guess ⇒ 12-byte records). The legacy re-derivation inside
  `Mod_LoadClipnodes` — which misreads one pathological corner its own guess
  accepts — is deliberately not replicated (Known Deviation).
- 16-bit children pass through the **aguirRe wrap**: `(uint16)child`, then
  `>= numclipnodes → child -= 65536`. Faithful including the
  mildly-out-of-range-index → insane-negative-contents behaviour; it also
  guarantees every surviving non-negative child is in range.
- 32-bit children and all planenums are range-validated at load (hardening)
  so the trace kernel can trust indices per its documented precondition.

## Hull 0 (`make_hull0`)

Duplicates the draw-node tree as clipnodes: `planenum` copied, child = node
index if a node, else the referenced leaf's CONTENTS value. Stored in
`WorldData::hull0_nodes()`.

Per submodel, the hull-0 descriptor keeps the legacy quirk exactly:
`firstclipnode = headnode[0]`, `lastclipnode = headnode[0] + subtree count`
— the counter is SEEDED with the headnode and the overflow cap tests the
running value (`seed + count == cap`), matching `Mod_SetupSubmodels`'
pre-seeded `lastclipnode`. Counting is iterative (explicit stack) with the
legacy caps (32767 / 524288).

## Hulls 1–3 (`setup_submodels`)

Per hull, `clip_mins/maxs` come from the injectable
`WorldLoadOptions::hull_bounds` table with the legacy index remap — BSP hull
**1 ← usehull 0** (human), **2 ← usehull 3** (large), **3 ← usehull 1**
(head/duck). Null bounds ⇒ "no hull specified" (`present = false`).

- **Classic path**: `firstclipnode = headnode` (a compiler-written `-1`
  passes through raw — the kernel reads it as an immediate CONTENTS_EMPTY),
  `lastclipnode = numclipnodes − 1`, over the shared widened array.
  ZHLT empty hulls (`headnode >= numclipnodes`) stay absent.
- **BSP30ext path**: per-submodel index space is still 16-bit, so each
  hull's subtree is re-emitted in legacy **preorder** into a compact array
  appended to the shared `clipnodes()` vector at a base offset; children and
  first/last shift uniformly (traversal identical — Known Deviation vs
  legacy's separate per-hull allocations). Missed-hull rules are stricter
  here: `headnode == -1` or (`hull != 1 && headnode == 0`).

## `"*N"` origins and model flags

For submodels 1..N: `Mod_FindModelOrigin` equivalent scans the full entity
text (utilities::Tokenizer) for `"model" "*N"` and takes its `"origin"`
(only when the disk origin is null — legacy early-out); non-null origin sets
`k_model_has_origin`, plus the unconditional c2a1 submodel-11 hack. Surface
flags then derive `k_model_conveyor/transparent/liquid` — the legacy loop is
gated on `i != 0`, so the world model never gets them.

## Threading model

Load-time only (main thread); outputs are immutable `WorldData` members.

## Edge cases and invariants

- The hull-0 `last = headnode + count` off-by-one (one past the true last
  index) is legacy-exact and harmless: the kernel's range check only ever
  admits reachable indices.
- Iterative count/remap means a crafted cyclic clipnode graph fails with
  `BspCorruptLump` instead of overflowing the stack (Known Deviation; node
  TREE traversal at query time retains legacy behaviour — boundary spec §5).

## See also

- [trace.md](./trace.md) — the consumer (`world_hull`, `hull_for_bsp`)
- [Boundary spec §5](../../boundaries/map_loader-boundary.md)
