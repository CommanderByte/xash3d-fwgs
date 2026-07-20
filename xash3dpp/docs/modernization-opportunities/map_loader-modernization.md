# map_loader Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> **Refreshed 2026-07-20** (tree-wide modernization audit, HEAD `cc73c054`).
> This pass adds two High-priority subtraction findings surfaced by the
> audit's L11-subtraction lens — a vestigial memory pool that has never
> allocated (H-1) and the one `I*` interface in the tree with no owner at
> all, `IMapLoaderObserver` (H-2) — corrects a now-stale "no stats tier"
> claim (`MapLoaderStats` landed 2026-07-19), and records four Low-priority
> doc-staleness items in `map_loader-threading.md`. The three prior
> low-priority cosmetic items (bit_cast, `as_chars`, `std::ranges`) were
> re-verified against current source and are **unchanged** — kept below
> under their original labels. One prior candidate finding
> ("world publish has no synchronization primitive") was investigated and
> **refuted**; see the dedicated section below so it is not re-derived.
> C++ standard in use: C++**23** (from `xash3dpp/src/map_loader/CMakeLists.txt`,
> `target_compile_features … cxx_std_23`; the tree-wide `CMAKE_CXX_STANDARD 23`).
> Boundary spec: `docs/boundaries/map_loader-boundary.md`
> Threading: `docs/threading-analysis/map_loader-threading.md`
> Deep dives: `docs/legacy-survey/deep-dive-bsp-loader.md`,
> `docs/legacy-survey/deep-dive-trace-pvs.md`
> ABI-frozen symbols in this subsystem: `k_contents_*` (BSP leaf/clipnode +
> game-DLL ABI, `int` per QG); the `k_surf_*` / `k_model_*` / `k_fworld_*` flag
> bits (game DLLs read them through the SDK `msurface_t`/`model_t`/`world`
> fields); the BSP disk-format struct layouts (`d*_t` in `disk_format.hpp`,
> `static_assert`-pinned); and — as a **behavioural** ABI — the Q-18
> float-/bit-exact trace, PVS and map-CRC results (kernel-fenced: `trace.cpp`,
> `pvs.cpp`, `phs.cpp`, `bsp/map_crc.cpp`; see the determinism section below).
> None of the value types the queries take (`WorldData`, `TraceHull`,
> `TraceResult`, `PhsTable`) are ABI-frozen; they are `xash3dpp`-internal.
> Max tier observed this pass: High **H-2**, Medium none, Low **L-7**.

## Summary

map_loader is a large subsystem (10 TUs, six public headers) but was written
directly in modern C++23 and is already **highly modern** — there is no
legacy-C residue to convert. The BSP parser reads on-disk records through
`memcpy`-based `read_record<T>(std::span<const std::byte>, index)` /
`read_record_at<T>` helpers (no `reinterpret_cast` over lump arrays — the
alignment-safe pattern), validates with `static_assert`-pinned struct sizes and
a `std::endian::native == little` guard, propagates load failures with
`std::expected<WorldData, core::ErrorCode>` (no `Host_Error` process-kill), and
exposes the whole immutable world through `std::span` / `std::string_view` const
accessors. Queries take `const WorldData&` / non-owning `TraceHull` views and
caller-supplied `std::span` buffers; the legacy `g_visdata` / `pm_boxhull` /
`world.version` globals are gone (function-locals + a `BoxHull` value type +
width-normalized-at-load).

The result is that the usual modernization headline — raw casts over lump data,
C string handling, sentinel error returns — **does not apply**: those idioms
were never introduced. `compliance_scan.py map_loader` and `stub_scan.py
map_loader` are the mechanical checks; what remains after them is a short tail
of **cosmetic** cleanups plus one **load-bearing non-opportunity**: the Q-18
determinism constraint that forbids "modernizing" the trace/PVS/CRC math in any
way that changes the produced bits (the exact analog of the studio-math caution
in the utilities report).

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern that headlines
the utilities / filesystem / cmd_cvar reports is **absent** here — the two
candidate spots (texture-name matching, entity-key parsing) feed
NUL-terminated `std::string::c_str()` to the C-string comparators. See the
boundary §9 note.

The 2026-07-20 pass found the currency/shape story unchanged — this pass's
findings are all **shape and doc-hygiene**, not currency. Two are Chunk-3
scaffold residue promoted to High by the tree-wide subtraction lens (L11):
a memory pool that has been created and destroyed every `init`/`shutdown`
since the subsystem was scaffolded without ever allocating a byte (H-1), and
`IMapLoaderObserver`, the one `I*` interface in the whole tree that is
neither a scheduled door nor an exercised nullable seam — plus a boundary
doc elsewhere that wrongly claims Server implements it (H-2). Both are pure
subtractions with no consumer requirement.

______________________________________________________________________

## High-priority opportunities

### H-1: Delete `MapLoader::Impl::pool` — created and destroyed every cycle, never allocated from

- **File(s)**: `xash3dpp/src/map_loader/map_loader.cpp:30` (member),
  `:92-93` (`create_pool("map_loader")` in `init`), `:107-109`
  (`destroy_pool` in `shutdown`).
- **Current pattern**: `MapLoader::Impl` holds a `xash::memory::PoolHandle
  pool`. `init()` registers it under the name `"map_loader"` and fails init
  if registration fails; `shutdown()` tears it down. No `pool_new<T>` /
  `mem_alloc` / `mem_calloc` call site anywhere in `xash3dpp/src/map_loader`
  or `xash3dpp/include/xash3dpp/{map_loader,private/map_loader}` references
  it — `WorldData`'s members are plain `std::vector`/`std::string` using the
  default allocator. The file-header comment even says so today: "pool-backed
  allocations (no allocations yet in this stub)" — `map_loader.cpp:5`, a
  Chunk-3 scaffold note that outlived the scaffold.
- **Suggested replacement**: Delete the `pool` member and the
  `create_pool`/`destroy_pool` calls (5 lines total). If a future need for
  pool-backed world storage arises (e.g. a G-5 scripting bridge that wants
  `WorldData` buffers pool-owned for HB-7's aligned-allocation door), it can
  be re-added together with the allocation site that actually uses it —
  see the shape constraint this creates, below.
- **Boundary-safe**: Yes. Pure internal lifecycle cleanup; no ABI-frozen
  struct, no Q-18 kernel, no public signature change (`create_pool`'s
  failure path in `init()` returns `false` today only because the pool
  registration can fail — removing it also removes that one theoretical
  init-failure mode, which is a simplification, not a behaviour change any
  caller depends on).
- **Rationale**: Confirmed by the tree-wide audit's L11-subtraction lens as
  one of exactly three dead pools in the tree (the other two are content's
  `ModelCache::Impl::pool_` and imagelib's `ImageDecoder::Impl` — not this
  report's concern). Deletions do not need a named consumer; per the audit
  brief, a subtraction the L11 lens names as deletable is promoted to a
  High slot specifically so it does not sit in a Low/cosmetic bucket next to
  items nobody will ever prioritize. `MapLoader::stats()` (unaffected by
  this deletion) is documented separately below.
  - **Shape constraint (record, do not build now)**: if `WorldData` storage
    is ever moved to pool-owned allocation, it must go through the existing
    `create_<thing>` + `pool_new<T>` + both-`operator delete` idiom (P-7),
    and any oversized BSP-derived type must respect `pool_new<T>`'s
    `alignof(T) <= 8` ceiling (HB-7, still open, not a blocker today since
    no allocation is proposed).

### H-2: Decide `IMapLoaderObserver` — the one unowned interface in the tree — and correct the false implementation claim it produced

- **File(s)**: `xash3dpp/include/xash3dpp/map_loader/map_loader.hpp:51-57`
  (interface), `:143-144` (`attach_observer`/`detach_observer`
  declarations); `xash3dpp/src/map_loader/map_loader.cpp:50-52` (fixed
  4-slot observer table), `:68-78` (`notify_begin`/`notify_end`), `:252-259`
  (`attach_observer`/`detach_observer` definitions, Main-asserted).
- **Current pattern**: `IMapLoaderObserver` is fully implemented, tested
  (`assert_thread_role(Main)` on both mutators) and documented as observed
  by "client (loading plaque, demo bookkeeping)" and "server (savegame
  staging)" in the header's own class comment (`map_loader.hpp:22-24`) —
  but a repo-wide grep for `IMapLoaderObserver`, `attach_observer` and
  `detach_observer` outside `map_loader.cpp` and `tests/` returns **zero**
  hits. Neither client nor server attaches an observer. Server already gets
  equivalent load-begin/load-end visibility through
  `ILevelChangeExecutor`'s return values (the seam it does implement,
  `server.hpp:103`), which may be why the "server observes" half of the
  original design was never wired. Separately, and this is the more
  consequential half: `docs/boundaries/server-boundary.md:153` lists an
  "IMapLoaderObserver impl — map_loader FSM" row in Server's Interface
  table, and Server derives from and implements **only**
  `ILevelChangeExecutor` — that row is false, not stale-but-close.
- **Suggested replacement**: The tree-wide audit's subtraction lens (L11)
  classified every zero-implementation `I*` interface in the tree into
  three buckets — over-abstraction (delete), scheduled door (keep, record a
  chunk), or exercised nullable seam (keep as-is) — and found
  `IMapLoaderObserver` is the **only** one that fits none of the three: it
  has no in-code chunk marker (unlike `IClipHooks`'s Chunk-11 physFuncs tie,
  or `IModelPostProcess`/`IEventSource`'s Chunk-12/13 markers), and a
  boundary doc asserting a false implementation is worse than either
  silence or a recorded door. Two options, in the order the lens
  recommends:
  1. **(Preferred) Delete.** Remove the interface (`map_loader.hpp:51-57`),
     `attach_observer`/`detach_observer` (declaration + definitions), the
     4-slot observer array and `notify_begin`/`notify_end`
     (`map_loader.cpp:50-52`, `:68-78`, `:252-259`), and the two test-local
     fakes (`RecordingObserver` in `tests/map_loader/test_map_loader.cpp`,
     `Recorder` in `test_map_loader_world.cpp`). ~40 lines net. An
     observer interface with no observers after four chunks of map_loader
     work (Chunks 3-10 touched this subsystem) has no demonstrated demand;
     reconstruction from this recorded shape, if a real consumer appears,
     is cheap.
  2. **(Alternative) Keep and assign a chunk.** If Chunk 12 (client) is
     judged the natural owner for "loading plaque, demo bookkeeping" per
     the header's own comment, add a Q-21 "Extension axes" door row to
     `map_loader-boundary.md` naming Chunk 12 as the scheduled implementer,
     matching the treatment already given to the tree's other 8 legitimate
     scheduled-but-unimplemented doors.

  - **Either way, in the same commit**: `server-boundary.md:153`'s false
    "IMapLoaderObserver impl" row for Server must be corrected — deleted if
    option 1 is taken, or reworded to name Chunk 12 (not Server) as the
    implementer if option 2 is taken. That file is outside this report's
    scope (owned by the server-modernization pass), so it is recorded here
    as a cross-file obligation, not silently assumed fixed.
- **Boundary-safe**: Yes for deletion (no ABI-frozen surface, no Q-18
  kernel contact — `IMapLoaderObserver` is `xash3dpp`-internal). Yes for
  the keep-and-record alternative (pure documentation).
- **Rationale**: `[EXT:G-4]` — this is the one interface the audit's
  extension-door survey could not classify as either justified or
  scheduled; G-4 (expanded in-game debugging) is the natural owner if
  option 2 is taken, since map-load diagnostics is exactly its shape, but
  no chunk currently claims it. Per the anti-gold-plating rule, leaving it
  undecided is what produced the false doc row in the first place — a
  decision (delete or door-record) closes that gap either way.

## Medium-priority opportunities

None found this pass. (`MapLoaderStats`'s plain, non-atomic shape is
correctly deferred — see the corrected implementation-status entry below —
and is recorded as a shape constraint, not a Medium action item.)

______________________________________________________________________

## The Q-18 determinism constraint (do NOT "modernize" the math)

This is the single most important item and it is a **prohibition**, not an
opportunity. The trace kernel (`trace.cpp`), the PVS/PHS codecs and walks
(`pvs.cpp`, `phs.cpp`, `private/map_loader/fat_vis.hpp`) and the map CRC
(`bsp/map_crc.cpp`) are pinned float-/bit-exact to the legacy engine (Q-18
`PM_FP_MODEL`; `docs/design/pm-determinism-decision.md`; golden vectors +
verbatim-kernel cross-check gate them, 18,156 traces / 0 mismatches). The
following "clean-ups" would silently change results and must **not** be made:

- **No reassociation / FMA / `-ffp-contract` in the kernel.** `PlaneDiff`'s
  single-precision `DotProduct` (`n.x*p.x + n.y*p.y + n.z*p.z`), the axial fast
  path branch (`type < 3`), the `frac = (t1 ± DIST_EPSILON)/(t1 - t2)`
  crosspoint, the `frac -= 0.1f` back-up loop and the `VectorLerp` expansion are
  bit-load-bearing. Do not rewrite them with `std::transform_reduce`,
  `std::fma`, `std::inner_product`, or a "tidier" fused expression — any of
  those can reorder the adds or contract a multiply-add and move the last ULP.
- **No changing the on-plane tie-break.** `point_leaf` sends an on-plane point
  to the BACK child (`PlaneDiff <= 0`); the hull walkers send it FRONT
  (`< 0`). The asymmetry is deliberate and tested — do not "unify" it.
- **No touching the CRC fold.** The MP checksum is an **un-inverted** CRC-32
  over lumps 1..14 as stored on disk; the SP constant is `0x58415348`. These
  are wire-frozen (`sv.worldmapCRC`). Leave the polynomial and the
  no-final-XOR exactly as is.
- **No reordering the zero-RLE codec.** `decompress_pvs` / `compress_pvs`
  reproduce the legacy run-length stream byte-for-byte (incl. the 255-run cap
  and the exhausted-stream zero-fill hardening). A `std::ranges` rewrite is
  allowed only if it is proven to emit identical bytes.

Any future refactor that touches these files should re-run the golden fixtures
as the acceptance gate, exactly as the utilities studio-math kernels do.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| `std::expected` load-error propagation | **Implemented** | `load_world_data` → `std::expected<WorldData, ErrorCode>`; no `Host_Error` kill |
| `memcpy` record readers over `std::span<const std::byte>` | **Implemented** | `read_record<T>` / `read_record_at<T>`; alignment-safe, no array `reinterpret_cast` |
| `static_assert`-pinned disk layouts + LE guard | **Implemented** | every `d*_t` size-asserted; `std::endian::native` checked |
| Immutable `WorldData` with `std::span` / `std::string_view` accessors | **Implemented** | Safe-RO after load (Q-6) |
| Edict-free trace API (`TraceHull` / `TraceResult` value types) | **Implemented** | globals `pm_boxhull` / `world.version` eliminated |
| `enum class` flavours + typed flag constants | **Implemented** | `BspVersion`, `MapLoadState`; `k_surf_*` / `k_model_*` etc. |
| PHS as `map_loader` module (Q-19) | **Implemented** | `PhsTable` + `build_phs` / `fat_phs` / `headnode_visible` |
| `MapLoaderStats` always-on counters | **Implemented** (2026-07-19) | 4 plain `uint32_t` counters, `map_loader.hpp:101-107`, snapshot via `MapLoader::stats()` (`map_loader.cpp:271`); **corrects the prior "No stats tier" entry in "Out of scope / ABI-frozen" below**, which is now stale — see the correction there |
| `MapLoader::Impl::pool` (memory pool) | **Vestigial — recommend delete** | H-1: created/destroyed every init/shutdown cycle, zero allocations through it |
| `IMapLoaderObserver` | **Zero production impls/callers — decide** | H-2: delete or assign a chunk door; `server-boundary.md:153`'s claim that Server implements it is false |
| `bit_cast` record reader variant | **Not implemented** | L-1 — marginal over the memcpy readers; source is a sub-span, not a value |
| `as_chars` byte→text view helper | **Not implemented** | L-2 — two documented `reinterpret_cast<const char*>` sites |
| `std::ranges` needle scan / fixed-name copy | **Not implemented** | L-3 — cosmetic |
| Node-tree traversal cycle-guard | **Not implemented** | Tracked as a Chunk 6 hardening follow-up in boundary §5, not a modernization item |

______________________________________________________________________

## Low-priority opportunities

L-1 through L-3 are cosmetic — the current code is correct, `noexcept` and
alignment-safe; re-verified 2026-07-20 against current source and unchanged
since the 2026-07-06 pass. L-4 through L-7 are new this pass, all
doc-hygiene in `map_loader-threading.md`, none a code change.

### L-1: `std::bit_cast` record-reader variant (marginal)

- **File(s)**: `xash3dpp/include/xash3dpp/private/map_loader/bsp/bsp_loader.hpp`
  (`read_record<T>` / `read_record_at<T>`).
- **Current pattern**: `T out; std::memcpy(&out, bytes.data() + …, sizeof(T)); return out;`
- **Modernization**: `std::bit_cast<T>` is the C++23 idiom for byte-reinterpret,
  but it needs a value of a *same-sized source type*, whereas here the source is
  a slice of a larger `std::span<const std::byte>`. A `bit_cast` variant would
  only help at the fixed-size call sites that first copy into a
  `std::array<std::byte, sizeof(T)>` — which is exactly what `memcpy` already
  does in one step. **Verdict: leave as memcpy.** The helpers are the correct,
  documented, alignment-safe form; recording only so a future reader does not
  "discover" bit_cast and regress the ergonomics.

### L-2: `as_chars(std::span<const std::byte>)` view helper

- **File(s)**: `xash3dpp/src/map_loader/bsp/bsp_lumps.cpp` (entity lump +
  `.ent` patch assign, two `reinterpret_cast<const char*>` with SAFETY
  comments).
- **Modernization**: a one-line `[[nodiscard]] inline std::string_view
  as_chars(std::span<const std::byte> b) noexcept { return { reinterpret_cast<
  const char*>(b.data()), b.size() }; }` (or `std::bit_cast` of the pointer)
  would centralise the byte→char aliasing behind one audited seam and let the
  call sites read `w.entities_.assign(as_chars(lv->bytes))`. Pure readability;
  the aliasing itself is well-formed (`std::byte`/`char` may alias any object).

### L-3: `std::ranges` for the classname scan and the miptex-name copy

- **File(s)**: `xash3dpp/src/map_loader/bsp/bsp_loader.cpp` (the
  `reinterpret_cast<const std::byte*>` classname-needle search) and
  `bsp/bsp_flags.cpp:108` (`std::memcpy(name, mip.name, 16)`).
- **Modernization**: the needle search could be `std::ranges::search` over two
  byte spans; the fixed 16-byte miptex-name copy could be `std::ranges::copy`
  into a `std::array<char, 16>`. Both are cosmetic and neither is on a hot path
  (load-time only). Tree-wide `std::ranges` usage is still exactly 0 (verified
  fact, not a heuristic); this remains the only candidate site in map_loader
  and stays correctly deferred as non-essential.

### L-4: `map_loader-threading.md` Notes section still says "No stats tier yet", three lines below the paragraph that says otherwise

- **File(s)**: `xash3dpp/docs/threading-analysis/map_loader-threading.md:56-57`
  (correctly states `MapLoaderStats` now exists, added 2026-07-19) vs.
  `:61-63` (unedited leftover: "No stats tier yet: the subsystem currently
  exposes no always-on counters").
- **Current pattern**: The same doc's Notes section contradicts itself three
  paragraphs apart.
- **Suggested replacement**: Doc-only — delete or rewrite the stale `:61-63`
  sentence. No code change; recorded here because it concerns the exact
  fact (`MapLoaderStats`) corrected in the implementation-status table above.
- **Boundary-safe**: Yes (doc-only).

### L-5: Stale "Chunk 8 ... behind this seam" parentheticals — the save wiring they describe as future already shipped

- **File(s)**: `xash3dpp/include/xash3dpp/map_loader/map_loader.hpp:73`
  (`exec_load_game`), `:76` (`exec_change_level`); cross-reference
  `xash3dpp/src/server/server.cpp:173-188` where Server (the sole production
  `ILevelChangeExecutor` implementer) already delegates both to
  `save_exec_load_game`/`save_exec_change_level` in `save_bridge.cpp`.
- **Current pattern**: The interface doc comments still read "(Chunk 8 save
  body behind this seam)" / "(Chunk 8 save staging)" as if the save
  subsystem were future work; Chunk 8 shipped.
- **Suggested replacement**: Delete the parentheticals; describe what
  `exec_load_game`/`exec_change_level` **do** (delegate to the save
  subsystem) rather than a completed future-tense placeholder. Pure doc
  cleanup, no behaviour change.
- **Boundary-safe**: Yes (doc-only, `xash3dpp`-internal interface comment).

### L-6: Threading doc's "Model" section and hazard table still list superseded gaps its own "Notes" section says are closed

- **File(s)**: `xash3dpp/docs/threading-analysis/map_loader-threading.md:16-22`
  (top "Model" section text, and the hazard table's `clear_world`/
  `new_game`/`change_level` gap notes) vs. `:50-57` (the 2026-07-19 "Notes"
  correction stating all 12 mutating entry points now assert
  `ThreadRole::Main`, and that "the hazard-table gap notes above are
  superseded" — without removing them).
- **Current pattern**: A reader who stops at the Model section or the hazard
  table sees "7 sites" and open gaps; only a reader who continues to the
  Notes section learns the true, closed state (12 sites, HB-3 closed
  2026-07-19).
- **Suggested replacement**: Delete the superseded gap language from the
  Model section and hazard table; state the 12-site closed picture once, in
  one place, instead of leaving a corrected claim standing beside its
  correction.
- **Boundary-safe**: Yes (doc-only).

### L-7: `MapLoaderStats` is correctly plain/by-value for now — recorded as a shape constraint, not a gap

- **File(s)**: `xash3dpp/include/xash3dpp/map_loader/map_loader.hpp:101-107`
  (4 plain `uint32_t` counters); `xash3dpp/src/map_loader/map_loader.cpp:271`
  (`stats()` returns by value).
- **Current pattern**: Counters are incremented on Main with no atomics,
  copied out by value. A repo-wide grep found **zero** production callers of
  `MapLoader::stats()` today, so there is no live race — Main-only writes,
  no readers at all yet.
- **Suggested replacement**: No change now. The tree-wide audit's
  introspection-stats lens (L3) classifies this as "Class C — lifetime-clean,
  race-dirty": a by-value plain-struct snapshot has no dangling hazard (unlike
  the tree's `const&`-into-live-`Impl` accessors, which are the real class of
  defect elsewhere in the tree) but would still be a torn/UB read if a second
  thread ever wrote it concurrently. When the tree's overdue
  `diagnostics_dump` aggregator (triggered at ≥3 stats structs; the tree
  already has 14) is built, `MapLoaderStats` should convert to per-field
  atomics at that point, matching the 6 subsystems that already did
  (`cmd_cvar`, `networking`, `input`, `server`, `sound`, plus memory's
  `PoolStats`). Building that conversion today, with zero consumers, would
  be the speculative-build the anti-gold-plating rule forbids.
- **Boundary-safe**: NeedsVerification if/when actually converted (would
  touch the public `MapLoaderStats` layout, which is `xash3dpp`-internal,
  not ABI-frozen, so verification is about call-site fallout, not ABI).

______________________________________________________________________

## Investigated and refuted

- **"`WorldData` publish has no synchronization primitive despite doc
  language calling it 'atomic' — the exact gap HB-5 needs to close."**
  Refuted on inspection of the cited lines in full:
  `map_loader-boundary.md`'s P-2 row reads "The only publish event is the
  atomic world swap at `MapLoader::load_world`/`clear_world`, which is
  **Main-only and must not overlap readers**" — the qualifier is in the
  same sentence, not a separate contradicted claim. The threading doc is
  more explicit still (see the hazard table above): "Swap is NOT
  synchronized: callers must not hold or use a `world()` pointer across a
  load/clear." Both documents already say exactly what the finding claimed
  they failed to say. `MapLoader::Impl::world` is a plain
  `std::optional<WorldData>` mutated via `reset()`+`emplace()` under a
  Main-thread-only convention (`map_loader.cpp:273-317`, Main-asserted on
  both `load_world` and `clear_world`) — that is accurately described, not
  mis-described, by the existing docs. Do not re-file this; if HB-5's
  shared published-snapshot idiom is ever designed, `MapLoader::world()` is
  a plausible first adopter (see the L1-publish-primitive lens), but that is
  a shape constraint for HB-5, not a defect here.

______________________________________________________________________

## Out of scope / ABI-frozen

- **The `d*_t` disk structs keep the legacy spelling and raw field types.**
  They are format-frozen POD mirrors of `bspfile.h` (same rule as
  `include/xash3dpp/abi/`); the 1:1 name mapping is what keeps the parser
  greppable against the legacy source. Do not `enum class` the lump indices or
  rename the fields.
- **The trace/PVS/CRC math is C-shaped on purpose.** See the determinism
  section — the "C-looking" float expressions are the parity contract.
- **`k_contents_*` stay `int`.** They are the BSP + game-DLL ABI values (QG);
  an `enum class` here would fight the frozen SDK. Named `inline constexpr int`
  is the agreed form.
- **`std::expected` is already the error model.** There is no
  sentinel-return or `Host_Error` residue left to modernize.
- **~~No stats tier.~~ RESOLVED — this claim is now stale and superseded.**
  The 2026-07-06 pass recorded "Map loads are cold-path; the subsystem
  exposes no always-on counters by design. Not a gap." As of 2026-07-19
  (consolidation audit), `MapLoaderStats` was added — 4 plain `uint32_t`
  counters, snapshot via `MapLoader::stats()` — because the original
  exemption's own recorded revisit trigger ("when the server chunk lands")
  had fired. See the implementation-status table and L-7 above for the
  current, correct treatment. Kept here, marked resolved rather than
  deleted, so this entry is not silently dropped from the record.

______________________________________________________________________

## Open questions

- **H-2's fork**: delete `IMapLoaderObserver` outright, or assign it a
  Chunk 12 door row? This report recommends deletion (option 1 under H-2)
  as the default per the tree-wide subtraction lens's own recommendation,
  but the decision belongs with whoever owns Chunk 12's client-loading-UX
  scope, since "loading plaque, demo bookkeeping" is a real (if currently
  unscheduled) client want. Either answer requires a companion fix to
  `server-boundary.md:153` in the same commit — that file is outside this
  report's ownership.
- **H-1's re-add condition**: if `WorldData` (or any future map_loader
  buffer) is ever moved to pool-owned storage, does it route through HB-7's
  still-open aligned-allocation door, or does it stay under the existing
  `alignof(T) <= 8` ceiling? Not a blocker today — no allocation is
  proposed — but worth deciding before H-1's shape constraint is exercised.

______________________________________________________________________

## Testing note

Any change under this subsystem — even a cosmetic L-item that touches
`trace.cpp` / `pvs.cpp` / `phs.cpp` / `bsp/map_crc.cpp` — must be gated by the
existing golden fixtures (trace goldens + verbatim-kernel cross-check, map-CRC
vectors, the per-quirk BSP regression fixtures) before and after. Parser-only
cleanups (L-2 / L-3) are covered by the quirk-fixture suite. The pure
`as_chars` helper (L-2), if added, is trivially unit-testable in isolation.
H-1 and H-2 touch neither the kernel nor the disk-format structs, so the
golden-fixture gate does not apply to them — they need only the existing
`tests/map_loader/` suite (which exercises `init`/`shutdown` and, if H-2's
observer path is deleted, must have its two test-local observer fakes
removed in the same change) plus a build/test pass on both x64 and x86.
