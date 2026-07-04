# map_loader — Boundary Spec (Chunk 5)

*Status: implemented 2026-07-04 (commits C1..C9, branch `codex/modular-restart-docs`).
Legacy reference: `engine/common/mod_bmodel.c`, `engine/common/pm_trace.c`,
`common/bspfile.h`, `public/crclib.c`.
Deep dives: `docs/legacy-survey/deep-dive-bsp-loader.md`,
`docs/legacy-survey/deep-dive-trace-pvs.md`.
Verification: format-watchdog CLEAR; loader parity audit DIVERGENCES-FOUND →
all findings fixed (`d292c8c0`, `.ent` patch in `ec8b5828`); trace-kernel
parity audit PARITY-CONFIRMED; kernel cross-checked bit-for-bit against the
verbatim legacy kernel (18,156 traces, 0 mismatches).*

## 1. Scope

Chunk 5 deliverable: BSP v29/v30/BSP2/BSP30ext loading → immutable
`WorldData` → PVS queries + clip-hull trace kernel, float-exact to legacy
(Q-18), with the trace interface **decoupled from edict pointers** (the hard
prerequisite for the Chunk 6 server ABI work).

## 2. Exposed surface

Public headers under `include/xash3dpp/map_loader/`:

| Header | Surface |
|---|---|
| `world.hpp` | `BspVersion`, normalized record types (`Plane`, `ClipNode32`, `Node`, `Leaf`, `SubModel`, `HullDescriptor`, `Surface`, `TexInfo`), `HullBounds`/`k_default_hull_bounds`, `WorldLoadOptions` (incl. `entity_patch`), move-only `WorldData` with const span accessors, `load_world_data()` (span + Filesystem overloads, `std::expected`) |
| `contents.hpp` | `k_contents_*` (ABI values, `int` per QG) |
| `pvs.hpp` | `decompress_pvs`, `point_leaf`, `leaf_compressed_pvs`, `pvs_for_point`, `box_leafnums`, `box_visible`, `fat_pvs`, `check_vis_bit`, `k_max_box_leafs`, `k_fatpvs_radius` |
| `trace.hpp` | `k_dist_epsilon`, `TraceHull`, `TraceResult`, `hull_point_contents`, `recursive_hull_check`, `trace_hull`, `finalize_trace`, `world_hull`, `hull_for_bsp`, `BoxHull` |
| `map_loader.hpp` | `MapLoader` FSM (OQ-2) + world ownership: `load_world`/`clear_world`/`world()`; `MapLoaderInitParams{Filesystem*}` |

Private shared headers under `include/xash3dpp/private/map_loader/`:
`bsp/disk_format.hpp` (on-disk records, size-pinned), `bsp/bsp_loader.hpp`
(validation ladder + pipeline stages), `bsp/map_crc.hpp`, `trace_math.hpp`
(`plane_diff`, `box_on_plane_side`).

### Chunk 6 contract

The server composes per-entity traces itself: `hull_for_bsp(world, submodel,
usehull, player_bounds, origin)` or `BoxHull::set_bounds(minkowski-expanded)`
→ move the ray to the local frame with `HullSelection::offset` →
`trace_hull` → `finalize_trace`. Nearest-fraction merging, hit-entity
recording, rotated-entity matrix transforms, `PM_*` filter flags and the
physent list are server-side. `WorldData::checksum()` (with
`multiplayer_crc`) feeds `sv.worldmapCRC`. `pfnGetHullBounds` overrides flow
in through `WorldLoadOptions::hull_bounds`. PHS (Mod_CalcPHS + the phs path
of Mod_FatPVS), `pfnCheckVisibility`/`Mod_HeadnodeVisible` and entity leaf
caching are Chunk 6 — the PHS lands here as a map_loader `phs` query
module per Q-19 (PHS_PLACEMENT).

## 2a. Dependencies

Links `xash3dpp_memory` (pool for the FSM), `xash3dpp_filesystem`
(`load_file`/`file_time` for the path overload + `.ent` probe),
`xash3dpp_utilities` (Vec3 math, string/Tokenizer), `xash3dpp_core`
(logging, ErrorCode). Injected at init per Q-4:
`MapLoaderInitParams{Filesystem*}`. No platform/networking dependency.

## 2b. Owned state

- `MapLoader::Impl`: FSM state/next, level/landmark name buffers
  (`limits::map_qpath_max`), 4-slot observer table, memory pool, borrowed
  `Filesystem*`, and the active `std::optional<WorldData>`.
- `WorldData`: all loaded arrays as `std::vector` members (QL — BSP lumps
  exceed the 64 KB array cap), plus the raw entity/wadlist/message strings
  and visdata. Immutable after load; exposed only through const spans/views.
- Queries own nothing: caller buffers + function locals only (the legacy
  `g_visdata`/`pm_boxhull` shared statics have no equivalent; `BoxHull` is a
  per-callsite value type).

Sub-feature bundling (Q-11): bsp/pvs/trace subfolders score <2 "separate"
criteria (shared WorldData, no extra deps, no independent state machine, not
useful standalone) → single `xash3dpp_map_loader` target, satellite-style
folder layout only (networking/delta precedent).

## 3. Invariants

- **`WorldData` is immutable after `load_world_data` returns** (Q-6): every
  accessor is const; all queries take `const WorldData&`/non-owning views
  and are concurrent-read-safe.
- All on-disk variance is resolved at load: record widths normalized
  (always-32 clipnodes), Blue-Shift lump swap applied at resolution,
  broken-compiler fix-ups applied, indices validated. Queries never branch
  on the BSP version.
- Tie-break asymmetry (legacy-exact, pinned by tests): `point_leaf` sends an
  on-plane point to the **back** child (`PlaneDiff <= 0`); the hull walkers
  send it to the **front** child (`< 0`).
- The trace kernel preserves legacy float semantics ULP-for-ULP: axial
  `PlaneDiff` fast path, `DIST_EPSILON = 1/32` near-side nudge, `VectorLerp`
  expansion, `frac -= 0.1f` backup loop (Q-18; golden vectors + verbatim
  kernel cross-check gate this).
- Map CRC is wire-frozen: SP constant `0x58415348`; MP = **un-inverted**
  CRC-32 over lumps 1..14 as stored on disk (entities excluded, raw
  directory, EOF-clamped).

## 4. Load pipeline (legacy order preserved)

`parse_header` (version dispatch, BSP30ext id-only probe, Blue-Shift
detection, extended clipnode guess) → per-lump `resolve_lump` validation
(srclumps table: mincount/maxcount/CHECK_OVERFLOW, 16/32 entry-size
resolution, fileofs-0 silent absence) → stages: entities (+worldspawn
wad/message scan, `.ent` patch), planes (signbits), submodels (bounds
spread), textures (names only), visibility (raw), texinfo (miptex clamp),
surfaces (SURF_* flags, corrupt-face guard), marksurfaces (darkfuture
fix-up), leafs (clusters, leaf-0-solid, SURF_UNDERWATER marking, water-alpha
probe), nodes (validated, no parents), clipnodes (widen-32 + aguirRe wrap),
MakeHull0, SetupSubmodels (hull wiring, ZHLT skips, BSP30ext remap, "*N"
origins + c2a1 hack, MODEL_* flags), checksum, finalize.

Quirks replicated (each has a synthetic regression fixture): Blue-Shift
entities/planes swap; BSP30ext id-only detection with mismatched extra
version; extended-clipnode guess both triggers; aguirRe u16 child wrap
(incl. the mildly-OOB → insane-negative-contents case); ZHLT empty hulls;
optimizer `-1` headnodes kept raw on non-ext maps; darkfuture negative
marksurface remap; leaf-0-solid fatal; unclamped leaf visofs; miptex
`dataofs == -1` → `*default`; empty miptex names → `miptex_N`; lowercased
texture names; the water/laser case-sensitivity asymmetry; `{scroll`
combined flag; c2a1 submodel-11 origin hack (unconditional, as legacy);
hull0 `last = headnode + count` off-by-one; `.ent` patch age gate.

## 5. Known Deviations (intentional; parity-audit reviewed)

**Error model.** Legacy `Host_Error` (process kill) → error codes
(`BspUnsupportedVersion`/`BspCorruptLump`/`BspBadWorld`), logged at tag
`map_loader` (Q-5). Legacy tolerates per-lump validation errors for the
WORLD (the "a1ba: why world excluded here?" branch) and crashes later; we
fail the load for world and bmodels alike. A bad trace node number logs and
aborts the trace instead of killing the process.

**Input hardening (corrupt files only; valid maps byte-identical).** Lump
ranges bounds-checked against the file image; node/clipnode/surface/
marksurface indices validated at load (the kernel then trusts them — its
documented precondition); miptex directory bounds-checked; the corrupt-face
guard uses widened addition (legacy overflows signed ints on a crafted
`firstedge`); PVS reads that legacy performs out of bounds (unclamped
visofs, exhausted RLE stream, water-alpha `cluster == -1` bit test) get
defined results (missing-vis ⇒ full visibility; exhausted stream ⇒ zero
fill; negative cluster ⇒ false). **Residual risk (legacy-equivalent, not a
regression):** node-tree TRAVERSAL is not cycle-guarded — a maliciously
self-referencing node tree can loop `point_leaf`/`fat_pvs` or overflow
`box_leafnums`' recursion, exactly as legacy; clipnode count/remap got the
iterative hardening because it runs at load time. Tracked as a Chunk 6
hardening follow-up (visited-budget on traversal).

**Representation.** Clipnodes stay 32-bit in memory permanently (legacy
narrows back to 16-bit inside `model_t`; no xash3dpp consumer needs that —
a Chunk 6 ABI shim may produce a narrowed copy). The `world.version` global
is gone. BSP30ext per-hull remapped arrays are appended into the one shared
`clipnodes()` vector at a base offset (children and first/last shifted
uniformly — traversal identical). Node parent links (`Mod_SetParent`) are
not built (renderer/efrag-only consumers). `model_t` presentation fields
(`radius`, `numframes`) are not computed. The legacy
`Mod_LoadClipnodes` source-width re-derivation (`bsp30ext && count >=
32767`) is not replicated — we follow the entry size the lump-level guess
resolved; legacy misreads the pathological `filelen%8 != 0 && count < 32767`
corner its own guess accepts.

**Parsing.** Entity tokenization uses `utilities::Tokenizer`
(COM_ParseFileSafe port); token length caps at
`limits::tokenizer_token_max` (512) vs legacy `MAX_TOKEN` (2048). The
worldspawn scan retains `wad` (raw, unsplit — WAD mounting is content-side)
and `message` only; `compiler`/`generator`/`_litwater*` diagnostics keys are
dropped. The Quake-compatibility water-name toggle is not modeled (GoldSrc
rules always).

**Deferred (not lost — tracked for later chunks).** Texture texel payloads,
lighting/deluxe/BSPX light lumps, edges/vertexes/surfedges payloads (the
surfedge record COUNT still feeds the corrupt-face guard), surface
extents/bevels and full `msurface_t` (→ content/renderer, Chunk 7);
`Mod_CalcPHS`, fat-PHS, `pfnCheckVisibility`, entity leaf caching, rotated
finalize, physent iteration, `PM_*` filter flags (→ server, Chunk 6); studio
hitbox hulls (`Mod_HullForStudio`), `pm_surface.c` texture-at-trace (NB: it
locally shadows `FRAC_EPSILON` to 1/32), portal CSG (→ later chunks);
`LoadGame`/`ChangeLevel` FSM branches are transition-only stubs (→ Chunks
8/6). Recursions replaced with iterative walks in count/remap (a crafted
cyclic hull errors instead of overflowing the stack).

## 6. Threading

See `docs/threading-analysis/map_loader-threading.md`. Summary: loading and
activation are main-thread; after activation all query entry points are
concurrent-read-safe over `const WorldData&` (Q-6). `MapLoader::load_world`
must not run concurrently with queries against the previous world — frame
ownership is the server/host contract (compute/commit separation).

## 7. Limits

`limits::map_qpath_max` (64, legacy MAX_QPATH). Format caps (`k_max_map_*`)
are file-format facts and live in `private/map_loader/bsp/disk_format.hpp`,
not `limits.hpp` (same rule as the networking wire constants).
