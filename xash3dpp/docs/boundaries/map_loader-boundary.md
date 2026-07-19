# map_loader — Boundary Spec (Chunk 5)

> Refreshed 2026-07-06 (as-built pass). Re-scanned `src/map_loader/**`
> (10 TUs: `map_loader/world/pvs/phs/trace.cpp` + `bsp/{bsp_loader,bsp_lumps,`
> `bsp_hulls,bsp_flags,map_crc}.cpp`) and the six public headers. The spec
> below is confirmed against the shipped code; deltas since the 2026-07-04
> authoring are collected in the new **§9 As-built reconciliation**, and the
> previously-absent **Extension axes (Q-21)** section is added at the end.
> No source changed.

*Status: implemented 2026-07-04 (commits C1..C9, branch `codex/modular-restart-docs`).
Legacy reference: `engine/common/mod_bmodel.c`, `engine/common/pm_trace.c`,
`common/bspfile.h`, `public/crclib.c`.
Deep dives: `docs/legacy-survey/deep-dive-bsp-loader.md`,
`docs/legacy-survey/deep-dive-trace-pvs.md`.
Verification: format-watchdog CLEAR; loader parity audit DIVERGENCES-FOUND →
all findings fixed (`d292c8c0`, `.ent` patch in `ec8b5828`); trace-kernel
parity audit PARITY-CONFIRMED; kernel cross-checked bit-for-bit against the
verbatim legacy kernel (18,156 traces, 0 mismatches).*

## 1. Scope / Responsibility

Chunk 5 deliverable: BSP v29/v30/BSP2/BSP30ext loading → immutable
`WorldData` → PVS queries + clip-hull trace kernel, float-exact to legacy
(Q-18), with the trace interface **decoupled from edict pointers** (the hard
prerequisite for the Chunk 6 server ABI work).

## 2. Exposed surface (Interface)

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
(`plane_diff`, `box_on_plane_side`), `fat_vis.hpp` (fat-PVS/PHS query
backing — shipped with the Q-19 PHS module; inventory row added 2026-07-19).

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

## 3. Invariants and Quirks

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

**PHS module (Chunk 6 S3, Q-19).** `Mod_CalcPHS`'s OpenMP build
parallelism is not ported (single-threaded fold, byte-identical output;
parallelising internally at load is an allowed follow-up per the
server-boundary OQ-9 posture); `PhsTable::compressed_row()` bounds-checks
the row index (legacy indexes `phsofs` unchecked; out-of-range decompresses
as all-visible per the module hardening convention); the `vis_stats`
developer counters are not ported (pure logging). Both fat-vis paths share
one walk (`private/map_loader/fat_vis.hpp`); `Mod_HeadnodeVisible`'s
recursion is an explicit stack preserving front-first traversal order.

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

## 8. Constant classification (QO)

Only one literal is a tunable capacity and it already lives in `limits.hpp`:
`map_qpath_max` (level/landmark name buffers, legacy `MAX_QPATH`). Everything
else is **frozen**, not a capacity or a cvar, so it stays in-code next to what
defines it:

- **Disk-format facts** — `k_max_map_*` element caps and the on-disk record
  field widths (the `name[16]` / `landname[16]` / `modelname[16]` /
  `name[17]` MIPTEX/model-name arrays flagged by `limits_scan` as
  "unclassified magic"): wire/disk-format constants pinned by the BSP layout.
  They live in `private/map_loader/bsp/disk_format.hpp` and at the parse
  sites, never `limits.hpp` (same rule as the networking wire constants).
- **ABI-frozen values** — `k_contents_*` (BSP leaf/clipnode + game-DLL ABI,
  `int` per QG) in `contents.hpp`.
- **Algorithm constants (Q-18)** — the trace/PVS kernel numbers
  (`k_dist_epsilon = 1/32`, `k_fatpvs_radius`/`k_fatphs_radius = 8.0f`, the
  zero-RLE 255 run cap, `k_max_box_leafs`) and the map-CRC constants
  (SP `0x58415348`, the CRC-32 polynomial over lumps 1..14) are float/bit
  exact to legacy and stay beside the algorithms that require them.

No behavioural knob here is a cvar; there is nothing to migrate into
`limits.hpp` or the cvar registry beyond `map_qpath_max`.

## 9. As-built reconciliation (2026-07-06)

The 2026-07-04 spec is accurate; the following clarify where the shipped code
has moved past the "Chunk 6 future" framing used in §2 and §5.

- **PHS is SHIPPED here, not deferred.** The §2 *Chunk 6 contract* paragraph
  and §5 still read PHS as future/"Chunk 6 S3" work. As built, the
  BSP-derived query-data half now lives in `map_loader` per Q-19: `phs.hpp` /
  `phs.cpp` ship `PhsTable`, `compress_pvs`, `build_phs` (single-threaded
  `Mod_CalcPHS` port), `fat_phs` (the phs path of `Mod_FatPVS`) and
  `headnode_visible` (`Mod_HeadnodeVisible`). **Superseded 2026-07-06:** the
  "PHS … (→ server, Chunk 6)" deferral in §5's *Deferred* list applies only to
  the **decision** logic that stays server-side — `pfnCheckVisibility`, entity
  leaf caching, and the *when-to-build* trigger (the server calls `build_phs`
  during MP spawn; the table is immutable after, Q-6). Both fat-vis paths
  share `private/map_loader/fat_vis.hpp` as the §5 note already states.

- **The FSM enforces Main by assertion, not by convention.** §6 says the
  frame-ownership contract is "enforced by convention until the server chunk
  lands `assert_thread_role`." As built, `map_loader.cpp` already carries
  **7 `assert_thread_role(Main)` sites** — `init`, `shutdown`, `load_level`,
  `load_game`, `run_frame_step`, `set_level_executor`, `load_world`. (Gaps:
  `new_game`, `change_level`, `clear_world` are not yet guarded — see the
  threading doc.) Queries stay lock-free RO; the world-swap non-overlap with
  readers is still a frame-ownership (compute/commit) contract, not a lock.

- **The server bring-up seam exists.** `map_loader.hpp` now exposes
  `ILevelChangeExecutor` (`exec_load_level` / `exec_load_game` /
  `exec_change_level`) and `set_level_executor`: the server registers one so
  the FSM delegates spawn → entities → activate; **absent → the inline
  `load_world` fallback** keeps `map_loader` running standalone (its own tests,
  the client background map). This is the Chunk 6 contract of §2 already
  shaped in code.

- **The edict-free trace contract is confirmed shipped.** `trace.hpp` is
  DELIBERATELY EDICT-FREE (no physent list, no entity indices, no `usehull`
  global): the server composes per-entity traces from
  `hull_for_bsp`/`BoxHull` + `trace_hull` + `finalize_trace` and owns
  nearest-fraction merging, hit-entity recording and rotated-entity transforms.
  Matches §2's *Chunk 6 contract* exactly.

- **`string_view`→C-string over-read: ABSENT.** The tree-wide over-read
  pattern (a non-terminated `string_view` handed to `strnicmp`/`strncmp`,
  present in utilities/filesystem/cmd_cvar) does **not** occur here. The two
  candidate spots both feed **NUL-terminated `std::string::c_str()`** to the
  C-string comparators: texture-name matching (`bsp_flags.cpp` —
  `strncmp(name,"sky",3)` etc. and `strnicmp(name,"laser",5)`, `name` from
  `texture_names_[…].c_str()`) and entity-key parsing (`bsp_hulls.cpp` /
  `bsp_lumps.cpp` — `stricmp(keyname.c_str(), …)`). Negative data point for the
  cross-cutting sweep.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`. map_loader is the
rewrite's cleanest published-snapshot surface: `WorldData` is
**immutable-after-load** (Safe-RO, Q-6) and every spatial query is a pure
function of `const WorldData&` + caller buffers. That makes the P-2 and P-4
doors **open by construction** — the work is *preservation*, not new seams.

| Goal / primitive | Applies? | Required seam or door |
|------------------|----------|-----------------------|
| **P-2** published-snapshot reads | **Yes — headline** | `WorldData` is immutable after `load_world_data` returns, so it **is** the published snapshot — no double-buffer needed because it is never mutated in place. A G-3 debug thread or G-1 MCP world-query reads `const WorldData&` (+ `const PhsTable&`) concurrently with the sim, zero synchronization. The only publish event is the atomic world swap at `MapLoader::load_world`/`clear_world`, which is Main-only and must not overlap readers — the frame-ownership (compute/commit) contract the host/server already owns. **Door-keep:** keep every query a pure function of `(const WorldData&, caller buffers)`; never add a mutable cache to `WorldData`. |
| **P-4** typed introspection | **Yes — headline** | The PVS/PHS/trace free functions plus `WorldData`'s const span accessors **are** the typed world-query surface (leaf / cluster / contents / CRC / vis / trace). A G-1 MCP "where is point X / what can leaf Y see / trace this ray" and a G-4 overlay read them directly — no `extern` into a `model_t`. **Door-keep:** extend by ADDING typed queries here (the `EntityView` precedent), never reach around. |
| **off-main read** (G-3) | **Yes** | Trace/PVS/PHS are the substrate a debug thread reads while the sim runs; all are concurrent-read-safe. The one rule: a borrowed `world()` pointer must not be held across a load/clear (documented §6). |
| **P-3** context-first, no new file-scope state | **Yes** | Queries take `const WorldData&`; the FSM takes injected `Filesystem*` (Q-4). Zero mutable file-scope state — the legacy `g_visdata` / `pm_boxhull` / `world.version` globals were all removed (function-locals + `BoxHull` value type + normalized-at-load width). |
| **P-5** narrowest-state signatures | **Yes** | The trace kernel takes a `TraceHull` *view*, not a whole model; queries take the world + caller buffers. |
| **P-1** main-thread inbox + worker pool | N/A here | map_loader owns no inbox; load/activation is a Main-thread transition driven by host/server. `Mod_CalcPHS`'s dropped OpenMP is an allowed *internal* load-time parallelism door (server-boundary OQ-9), not a P-1 seam. |
| **P-6** services are satellites | Single target (Q-11) | bsp/pvs/trace/phs are one `xash3dpp_map_loader` target (sub-feature bundling scores < 2 separate-criteria). |
| **G-1** in-engine MCP service | Consumer via P-4 | An MCP "where is point X / what can leaf Y see / trace this ray" tool composes the same typed world queries the P-4 row names; nothing map_loader-side beyond keeping them pure and typed. |
| **G-2** game ABI v2 | Door-keep | The edict-free trace API is the confinement point: a v2 ABI can hand a typed hull/trace view to game DLLs without touching the kernel. Clipnodes stay 32-bit in memory; a v2 shim may produce a narrowed copy. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Vis/trace overlays read the same query surface (leaf/cluster/contents/vis/trace); the P-4 door rule is the entire G-4 obligation. |
| **G-5** scripting runtime | Nothing owed now | §G-5's script surface v0 names no map_loader affordance; a tooling VM would call the same pure queries. Q-18 constraint applies unchanged to any binding (results are bit-frozen). |
| **P-7** pool-owned RAII lifecycle | **N/A — value types** | `MapLoader` is a pimpl (`make_unique<Impl>`, the sanctioned carve-out); `WorldData`/`BoxHull` are plain RAII value/member-owned types. No pool-owned `create_<thing>` object exists; the idiom gains a site only if a pool-backed world arena ever appears. |
| **P-8** annotation discipline | **Yes — satisfied (denominatored)** | 2026-07-19 audit: every mutating entry now asserts Main (12 sites — the former `new_game`/`change_level`/`clear_world`/observer gaps closed, HB-3) and `annotation-coverage` marker classes are clean with denominators; the one recorded `compliance-allow` (worker-notify at `map_loader.cpp`) stands. |
| **Q-18** determinism | Constraint (not a door) | Trace/PVS/CRC math is float-/bit-exact to legacy; any off-main reader gets the **same bits**. No modernization may change results (see modernization doc). |

**Net verdict — no new seam owed.** map_loader is already the ideal P-2/P-4
off-main-read surface; the charter is to keep `WorldData` immutable, keep
queries pure, and keep the world-swap Main-only. The single latent risk is the
node-tree traversal cycle-guard (§5 residual, legacy-equivalent) that a hostile
*off-main* reader could also trip — tracked as the Chunk 6 visited-budget
follow-up.
