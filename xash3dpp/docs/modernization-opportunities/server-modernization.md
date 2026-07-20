# server Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> Refreshed 2026-07-20 (tree-wide modernization audit, Phase 2). This pass
> re-verified every 2026-07-06 finding against current line numbers, added
> four High-priority items (a real teardown gap, a real desynced-clock live
> defect, a 16-way duplicated predicate cluster, and a threading-doc
> classification fix), six Medium-priority items (leaf-signature narrowing
> with a corrected 121-definition census, an undocumented 8&nbsp;KB mutable
> global, a CMake link-visibility fix, a snapshot OOM invariant break, a
> parallel-list consolidation, and an interface tier correction), and four
> Low-priority items (two dedup targets, one `std::ranges` opportunity, one
> stale-doc note), while refreshing the three pre-existing Low items in
> place. The HB-2 rotated-brush anchor is corrected below: the fenced block
> is `clip.cpp:245-281`, not `clip.cpp:219` as an earlier pass (and
> `decisions-architecture.md:946-954`, not owned by this report) recorded.
> C++ standard in use: C++**23** (`xash3dpp/src/server/CMakeLists.txt`,
> `target_compile_features(xash3dpp_server PUBLIC cxx_std_23)`; the tree-wide
> `CMAKE_CXX_STANDARD 23`). `std::span`, `std::string_view`,
> `std::optional`/`.has_value()`, `enum class`, and pimpl are used throughout —
> this subsystem was written directly in modern C++, not converted from C.
> Boundary spec: `docs/boundaries/server-boundary.md`
> Threading: `docs/threading-analysis/server-threading.md`
> Deep dives: `docs/legacy-survey/deep-dive-server-lifecycle.md`,
> `deep-dive-server-game-dll-bridge.md`, `deep-dive-server-clients.md`,
> `deep-dive-server-physics.md`, `deep-dive-server-world-frame.md`,
> `deep-dive-server-save-boundary.md` (plus `deep-dive-delta-encoder.md`,
> `deep-dive-trace-pvs.md`).
> ABI-/behaviour-frozen surfaces in this subsystem: the `enginefuncs_t`
> (159 slots) / `DLL_FUNCTIONS` (50) / `NEW_DLL_FUNCTIONS` (5) tables, the
> `edict_t` header + array-of-edicts representation, `entvars_t` (123 fields)
> and `globalvars_t` (`engine/progdefs.h`), the single `playermove_t` +
> `physent_t` (`pm_shared/pm_defs.h`), `server_physics_api_t` /
> `physics_interface_t` (`engine/physint.h`), and the Quake-lineage physics
> constants. See the frozen-ABI prohibition below.

## Summary

server is the largest subsystem in the rewrite (**30 TUs**, five slices) and
the dedicated-server milestone (Chunk 6, **Complete**). Despite its size it was
written **directly in modern C++23** — there is no legacy-C residue to convert.
The legacy `sv`/`svs`/`svgame` global triple folded into one heap-owned
`ServerRuntime` reached only through Main-thread entry points; entvars are read
through the **`EntityView`** zero-cost typed facade (Q-20) rather than raw
`->v.` poking; the game-DLL binding is a pimpl `Server` with injected deps
(Q-4); trace/contents/hull results are value types; errors return safe defaults
or route through the `host_error` hook rather than a scattered `Host_Error`
process-kill. `compliance_scan.py server` is **clean** (74 files, 0 findings).

The result is that the usual modernization headline — global mutable state,
raw casts over engine structs, C string handling, sentinel error returns —
**mostly does not apply to engine-internal server code**: those idioms were
either designed out (Q-2, Q-20) or **deliberately preserved** where the frozen
ABI demands them. That last clause is the crux of this report: unlike a
utilities or filesystem sweep, a large fraction of the "C-shaped" code in
`src/server/game/` (and the pmove bridge) is **load-bearing by contract**, not
technical debt. The two categories must be kept apart, so this doc leads with
the prohibition and an explicit "deliberately C-shaped" inventory before the
tiered-opportunity tail.

What the 2026-07-20 pass changed is the *shape* of the tail: it is no longer
three cosmetic items. Adversarial re-verification of the boundary spec's own
claims turned up one real resource-teardown gap (H-1), one real live
correctness defect nobody had previously found — a mirrored clock that is
declared, read four times, and written nowhere in production (H-2) — and a
16-way duplicated predicate cluster that gates entry into the HB-2 fenced
rotated-brush transform (H-3), where the previous pass had reported "3 names,
14 definitions". Four boundary-doc rows (P-3, P-5, P-7, and the G-2 row's
`EngineBridge` localisation claim) were independently found to overstate what
is actually implemented; this report records the corrected numbers where they
affect server-internal code and cross-references `server-boundary.md` where
the fix belongs to that document instead.

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern that
headlines the utilities / filesystem / cmd_cvar reports is **absent** here —
see the cross-cutting note at the end.

______________________________________________________________________

## The frozen game-DLL ABI (do NOT "modernize" the slots)

This is the single most important item and it is a **prohibition**, not an
opportunity. The server carries the highest ABI risk in the whole rewrite: two
frozen surfaces meet here, and one of them — the **game DLL ABI** — is a
byte-frozen contract shared with unmodified Half-Life mod binaries. The
following are pinned and must **not** be "cleaned up" in any way that changes a
slot signature, calling convention, struct layout, field order/width, or the
array-of-edicts representation:

- **The `enginefuncs_t` table (`abi/engine_table.cpp`).** All 159 slots are
  plain C function pointers with fixed GoldSrc signatures and **no userdata
  parameter** — that is *why* they reach engine state through the single
  file-scope `g_bridge` singleton (a deliberate Q-20 carve-out, annotated
  `compliance-allow`). Do not "fix" the global by adding a context parameter to
  a slot; the signature is the ABI. This file is the **one** TU that projects
  engine types onto the frozen ABI (`SvTrace → TraceResult`, `Vec3 →
  float[3]`), and raw `->v.` / `reinterpret_cast` at the projection edge is
  *correct here and only here* (Q-20 confines it).
- **The edict store (`abi/edict_arena.cpp`, `edict.h`).** The `edict_t` header
  layout, the `serialnumber` EHANDLE-invalidation scheme, the free-list reuse
  quarantine, the 16-byte private-data rounding (`(cb + 15) & ~15`, the Poke646
  workaround), and — critically — the **array-of-edicts representation itself**
  are ABI (game code addresses entities by byte offset from the array base via
  `pfnEntOffsetOfPEntity`). A future handle/arena flavor is a **post-parity
  load-time binding behind the `EntityView` seam** (Q-20), never an edit to the
  array.
- **`entvars_t` / `globalvars_t` (`progdefs.h`).** 123 + N byte-exact fields
  handed to the DLL by pointer. `EntityView` is a *view over* this layout — it
  must never reorder or repack it.
- **The pmove bridge (`physics/pmove.cpp`, `init_client_move.cpp`,
  `pm_trace.cpp`).** The single global `playermove_t` and its ~30 callback
  slots, the `physent_t` layout, and the exact entvars↔playermove copy rules
  (MP `onground = -1`, `waterjumptime ↔ teleport_time` aliasing, `pitch =
  -v_angle/3` copy-back, the ±256 gather box, the 600/64 caps) are frozen. This
  is the exact surface G-2 reworks — **alongside**, never in place.
- **The Quake-lineage physics constants (`physics/physics.cpp`).** The
  whole-vector maxvelocity clamp, ClipVelocity ±1.0 snap-to-zero, 4-bump
  FlyMove, `SV_AddGravity` basevelocity fold, pusher ltime clock + ±3600 angle
  wrap, chase-dir `215.0f` typo, drown `dmg<15→10`, friction
  consumed-then-reset-to-1.0, and the `pushed[256]` cap are **behavioural
  contract** — a rewrite may add a bounds-check-and-log above the cap but must
  not change behaviour below it.

Any future refactor touching these must pass the hl.dll smoke (real `hl.dll`
loads, `c0a0` spawns, one frame runs clean) as the acceptance gate, exactly as
`map_loader` uses its golden trace vectors and `networking` its wire-format
tests. The **`EntityView` + `EngineBridge` + Q-20 confinement is the firewall**:
a modernized (v2) game ABI is a new sibling flavor selected at load time behind
that seam, never an edit to the frozen tables.

______________________________________________________________________

## Deliberately C-shaped ABI slot bodies (not opportunities)

These *look* like modernization targets but are correct as written because they
sit at (or just inside) the frozen boundary. Recorded so a future reader does
not "discover" and regress them:

- **`abi/engine_table.cpp` `strcmp` / `strncpy` over `v->name`, message names,
  cvar names.** C-string, NUL-terminated slot bodies — the ABI hands `const
  char*`. These are **not** `string_view` over-reads (see the note at the end).
- **`clients/info_string.cpp` `Info_ValueForKey` static-buffer key parser.**
  A faithful port of the GoldSrc `\key\value` codec (MAX_KV_SIZE 128 field
  truncation, `*`-key protection, the `c > 13` ascii filter, the "team"
  lowercasing quirk). The static return buffers (`s_value[256]` etc.) are the
  frozen slot contract — a `const char*` valid only for the call duration
  (threading Race-static-buf row, safe under OQ-9). Rewriting to
  `std::string_view` would break the ABI return type.
- **`abi/engine_table.cpp` varargs `pfnAlertMessage` / print slots.** The
  `<cstdarg>` `va_list` handoff is mandated by the frozen `...` signature; the
  modern move (format then hand off a bounded buffer) is already the shape —
  the varargs entry cannot be removed.
- **`abi/engine_table.cpp:61-64` `s_fatpvs` / `s_fatphs`.** 4096-byte
  file-scope working buffers backing `pfnSetFatPVS`/`pfnSetFatPAS`, whose
  contract is "pointer to engine-owned storage valid until the next call"
  (`engine/server/sv_game.c:31-32`). Correct as file-scope state for the v1
  ABI — see M-2 for the annotation gap this leaves, and the Out-of-scope
  section for what changes if a v2, context-carrying flavor is ever built.
- **The two RNG statics (`s_rng_state` / `s_pm_rng`).** Marked
  `XASH3DPP-STUB(chunk6)` — the tracked idtech `COM_RandomLong`/`Float`
  parity port. Both are installed at frozen `enginefuncs_t`/`physint.h` slots
  (`t.pfnRandomLong`/`t.pfnRandomFloat` at `engine_table.cpp:2389-2390`;
  `pm.RandomLong`/`pm.RandomFloat` at `init_client_move.cpp:439-440`), so a
  dedup of their bodies touches frozen-ABI *implementation* only — see L-3.
  When ported, the single shared stream stays Main-thread-only (threading
  note). Not a modernization item; a parity follow-up (HB-12).

______________________________________________________________________

## Implementation-status table

Every design element the boundary spec calls for is either implemented or a
tracked stub; there is no "convert from C" backlog. The table records the modern
idioms in place (so a future reader does not regress them) plus the deferred
work.

| Design element | Status | Notes |
|----------------|--------|-------|
| `ServerRuntime` heap aggregate (no `sv`/`svs`/`svgame` globals) | **Implemented** | Q-2 held; reached only through Main-thread entry points |
| `EntityView` zero-cost typed entvars facade (Q-20) | **Implemented** | value-semantic accessors; raw `->v.` confined to `abi/` + pmove + save. It is a private-header accessor, **not** a P-4 introspection channel — see the Open questions note |
| `EdictArena` single authoritative store (Q-20) | **Implemented** | free-list, serialnumbers, freetime grace, 16-byte rounding; the freetime grace is currently broken by the H-2 clock defect on the ABI-free path |
| `EngineBridge` slot-state struct behind the 159-slot table | **Implemented** | one `g_bridge` file-scope singleton (deliberate ABI carve-out); 4 of its 11 hand-synced scalars have no production writer — see H-2 |
| pimpl `Server` + injected deps (Q-4) | **Implemented** | `ServerInitParams` non-owning `cvars`/`fs`/`maps`/`net` + `host_error` hook; `Server::~Server` does not call `unload_progs`/`shutdown` — see H-1 |
| `ILevelChangeExecutor` seam (map_loader FSM) | **Implemented** | `Server : ILevelChangeExecutor`; `exec_load_level` full chain |
| PHS consumed read-only from map_loader (Q-19) | **Implemented** | `EngineBridge::phs` = `const PhsTable*`; no build code in server |
| Snapshot pipeline (PVS/PHS mask, delta, baselines) | **Implemented** | `clients/snapshot.cpp` (14 assert sites); wire byte-exact via networking. `snapshot_alloc_ring` breaks its own invariant on partial OOM — see M-4 |
| `std::optional` model/brush resolves over the world | **Implemented** | `.has_value()` guards throughout `world/` + `physics/` |
| Tier-1 `ServerStats` atomic (`frames_run`) | **Implemented** | any-thread read surface; Tier-2/3 compile-gated TODO |
| `ITrustOracle` answer wired (cmd_cvar D2) | **Deferred** | `server.hpp` TODO — G-1 precondition, no consumer yet; cmd_cvar's `ITrustOracle` itself has zero production implementations tree-wide (cmd_cvar report) |
| `ICompatPolicy` (Q-12) for peoei/gsmrf/HLMODS | **Deferred** | `peoei_broken` currently a plain init bool; policy seam owed Chunk 7. Separately, cmd_cvar's `get_compat_policy()` is unreachable (no header declaration, no caller) so `GoldSrcCompatPolicy` is never instantiated in production — a cmd_cvar-owned defect, noted here because server's compat behaviour depends on it |
| Studio-hitbox trace loop + LRU (OQ-2) | **Deferred** | geometric core done (content); trace parity gated on hl.dll goldens |
| Chunk 8 save serializer (four primitives) | **Implemented** | `lifecycle/save_bridge.cpp` (1158 lines), wired from `Server::exec_load_game`/`exec_change_level`; the previous "Stubbed" row here was stale — no `XASH3DPP-STUB(chunk8)` marker remains anywhere in `src/server` |
| OQ-8 milestone trims (voice/HLTV/testpacket/NAT/A2S) | **Stubbed** | `// XASH3DPP-STUB(chunk6)` markers (134 tree-wide), finish-subsystem gate |

______________________________________________________________________

## High-priority opportunities

### H-1: `Server`/`EdictArena`/`StringPool`/`PrecacheTables` have no working teardown path

- **File(s)**: `include/xash3dpp/private/server/edict_arena.hpp:36-51`,
  `src/server/game/edict_arena.cpp:56-68`,
  `include/xash3dpp/private/server/string_pool.hpp:41-55`,
  `src/server/game/string_pool.cpp:51-58`; the actual fix site is
  `src/server/server.cpp` (`Server::~Server`) / the pimpl `Impl`.
- **Current pattern**: `EdictArena`, `StringPool`, and `PrecacheTables` all use
  a two-phase `init(PoolHandle,...)` / `shutdown()` lifecycle with owning raw
  pointers (`edicts_`, `block_`, `Table::names`, `model_flags_`) and **no
  destructor anywhere in the tree** (`grep -rn '~EdictArena|~StringPool|
  ~PrecacheTables'` returns nothing). The boundary spec's claim that these are
  "Q-22 RAII classes" is false. Correctness today depends entirely on every
  code path reaching `unload_progs`, which is the only caller of
  `::xash::memory::destroy_pool(rt.game_pool)` (`game_host.cpp:311`).
- **Suggested replacement**: The originally-proposed fix (add member
  destructors that call `shutdown()`) is a **non-sequitur**: `PoolHandle` has
  a trivial destructor, so member destruction order is irrelevant to whether
  the game pool is freed — the pool's actual lifetime already ends explicitly
  inside `unload_progs`, called while `ServerRuntime` is fully alive. Worse,
  three narrow member destructors would not by themselves guarantee
  `unload_progs` runs on every `Server` teardown path. The one-line fix that
  closes the whole hole is at the `Server` level:
  `struct Server::Impl { ... ~Impl() { unload_progs(rt); } };` (or an
  equivalent `Server::~Server()` body calling `shutdown()`, which is already
  idempotent — `unload_progs` early-returns when `!game_loaded`,
  `shutdown()` guards `save_cmds_registered`). That recovers the DLL handle,
  the bridge, and the three owning-pointer aggregates on every teardown path,
  not just the ones that remember to call `unload_progs` by hand.
- **Boundary-safe**: Yes. `EdictArena`/`StringPool`/`PrecacheTables` are
  xash3dpp-internal pool-lifecycle classes, not vendored ABI structs, and not
  named in the HB-2 kernel list. Adding a destructor to `Server`'s pimpl
  changes no ABI shape, name, or calling convention.
- **Rationale**: Real resource-lifecycle hazard — every early-return or
  exceptional teardown path that skips `unload_progs` leaks the game DLL
  handle, the bridge state, and three pool-owned aggregates. `[EXT:P-7]`
  (pool-owned RAII lifecycle) — this is exactly the door P-7 exists to close.

### H-2: `EngineBridge::sv_time` is declared, read four times, and written nowhere in production

- **File(s)**: `include/xash3dpp/private/server/engine_bridge.hpp:96`;
  read at `src/server/game/engine_table.cpp:100, 711, 1097, 2245`; the only
  assignment anywhere in the tree is `tests/server/abi/test_engine_table.cpp:146`.
  Companion dead fields: `novis` (`engine_table.cpp:1872, :1894`, read never
  written), `autoaim_threshold` (`:1013`, read never written), `group_mask`/
  `group_op` (`:2033-2034`, written never read — the live copies are
  `move_env->group_mask` and `links->set_group_op`).
- **Current pattern**: `EdictArena::alloc_edict`/`free_edict` implement the
  legacy reuse-grace policy verbatim
  (`edict_arena.cpp:77-78`: `e->free && (e->freetime < k_reuse_relax_window
  || (sv_time - e->freetime) > k_reuse_grace)`, vs.
  `engine/server/sv_game.c:1051`). Internal callers drive it with the real
  clock, `rt.level.time` (`entity_parse.cpp:63,279`, `physics.cpp:97`,
  `game_host.cpp:366`, `spawn.cpp:134`); the ABI path drives it with
  `g_bridge->sv_time`, which is frozen at `0.0`. The two clocks disagree in
  both directions: an edict freed through `pfnRemoveEntity` gets
  `freetime = 0.0f`, which is always `< 2.0f`, so it is recycled by the very
  next `pfnCreateEntity` with **no grace window at all** (EHANDLE / entity-
  index aliasing risk on entity-churn-heavy maps); an edict freed internally
  can never satisfy `(0.0 - freetime) > 0.5`, so the ABI create path **never**
  reuses an internally-freed slot and the arena grows toward `max_edicts` /
  the "no free edicts" `Host_Error` path.
- **Suggested replacement**: Assign `rt.bridge.sv_time = rt.level.time`
  wherever the server frame advances level time — the same place
  `merge_visibility` is already maintained (`snapshot.cpp:228/231/887`) — and
  add a regression test that frees an edict via `pfnRemoveEntity` at
  `t > 2.0` and asserts the 0.5&nbsp;s grace is honoured. In the same pass:
  wire `novis` from the `sv_novis` cvar and `autoaim_threshold` from
  `sv_aim`/`sv_allow_autoaim` (both currently read, never written, so both
  features are silently permanently off), and delete `group_mask`/
  `group_op` from `EngineBridge` entirely (written, never read — dead third
  copy of state that already lives correctly elsewhere).
- **Boundary-safe**: Yes. Restores legacy `sv_game.c:1046-1056` behaviour
  rather than diverging from it; touches frozen-ABI *call sites* (the slots
  already read these fields) but changes no slot shape, name, or calling
  convention.
- **Rationale**: A live correctness defect, not a style issue — silent
  desync between two clocks driving one arena, invisible to every mechanical
  gate the repo owns (`stub_scan.py` sees zero hits for `sv_time` because
  there is no `XASH3DPP-STUB` marker on a field that was simply never wired;
  it is not flagged as a stub, a compliance violation, or a build error).
  `[EXT:G-2]` — this field exists specifically so a v2 slot design can read
  engine time through `EngineBridge` instead of a global; shipping the v2
  door on a silently-broken v1 clock would carry the defect forward.

### H-3: 16 definitions of the same handful of world/physics micro-predicates under 4 competing names

- **File(s)**: `src/server/world/clip.cpp:38` (`vector_is_null`),
  `:36` (`bounds_intersect`), `:43` (`check_angles`); `world/contents.cpp:25,30`;
  `world/links.cpp:45`; `world/hulls.cpp:119`; `physics/pm_trace.cpp:65,73`;
  `physics/physics.cpp:67` (`is_null`); `physics/run_cmd.cpp:45`
  (`vec_is_null`); `physics/pmove.cpp:52` (`vec3_is_null`) — a fourth name for
  the same predicate the original 2026-07-06 sweep missed.
- **Current pattern**: `vector_is_null`-equivalent logic is defined 7 times
  under 4 names; `bounds_intersect` is defined 4 times (one, in `pmove.cpp:21`,
  is a loop-shaped variant over `abi::vec3_t`); `check_angles` twice; `fbit`
  once more beyond its canonical home. A mechanical grep over `src/server`
  puts the real blast radius at ~69 call sites (not the ~87 the first pass
  estimated). Three of these predicates — `vector_is_null`, `bounds_intersect`,
  `check_angles` — are the gating checks at `clip.cpp:228` and `:228-229` that
  decide whether the HB-2-fenced `if (rotated)` rotated-brush transform at
  `clip.cpp:245-281` runs.
- **Suggested replacement**: Delete 9 of the 16 definitions in two groups.
  Group A (Vec3-typed, pure move): promote `vector_is_null(const Vec3&)`,
  `bounds_intersect(const Vec3&,...)`, and `check_angles(float)` into
  `private/server/world_trace.hpp` (which already documents them) as `inline`
  free functions; delete the copies at `clip.cpp:30/36/43`,
  `contents.cpp:25/30`, `links.cpp:45`, `hulls.cpp:119`, `pm_trace.cpp:65/73`;
  rename the `is_null`/`vec_is_null`/`vec3_is_null` call sites to the single
  canonical name. Group B: promote `fbit` into `private/server/physics.hpp`
  the same way. **Constraint, not optional**: the Group A bodies gate entry
  into the ULP-fenced transform and must be relocated **verbatim** —
  bit-for-bit identical comparison logic, no tolerance/epsilon "improvement" —
  since they are themselves exact boolean/integer comparisons with no float
  accumulation or reordering, so a verbatim move cannot perturb the ULP result
  of the transform itself, but a *changed* comparison could change whether the
  transform runs at all.
- **Boundary-safe**: Yes, with the verbatim-copy constraint above. None of the
  nine deleted bodies performs float accumulation or reordering; the fenced
  kernel is the `if (rotated)` transform math itself
  (`clip.cpp:245-281`, in-code `TODO(Q-18)`), not the boolean gates that
  decide whether it runs.
- **Rationale**: The largest pure-duplication cluster found in this
  subsystem — one canonical definition per predicate instead of up to seven,
  with the relocation constraint documented so a future physics-boundary
  spec (Chunk 11) does not have to re-derive where these live before writing
  a new physics TU.

### H-4: Main-thread pinning in server mixes by-design and incidental causes under one rationale

- **File(s)**: `src/server/game/engine_table.cpp:55`,
  `src/server/clients/query.cpp:48`, `src/server/clients/filter.cpp:41`,
  `src/server/clients/log.cpp:25`. Documentation-only; the fix lands in
  `docs/threading-analysis/server-threading.md`, not this file.
- **Current pattern**: `server-threading.md` documents all 101
  `assert_thread_role`/`assert_main_thread` sites uniformly under one
  rationale (OQ-9, "server is single-threaded"). Tracing the call bodies
  shows two distinct causes: `abi/`, `physics/`, and `lifecycle/` are pinned
  **by design** — they reach the frozen, non-reentrant `enginefuncs_t`/
  `DLL_FUNCTIONS` ABI through the single `g_bridge` singleton and static
  return buffers (`grep -c 'g_bridge|pfn|rt\.game\.funcs'` returns 0 for
  `query.cpp`/`filter.cpp`/`log.cpp`, vs. real hits in `engine_table.cpp`).
  `query.cpp`/`filter.cpp`/`log.cpp` are pinned **incidentally** — their 8
  asserts (`grep -c assert_thread_role` = 1/6/1, matching exactly) reach no
  ABI singleton at all.
- **Suggested replacement**: Record the split in `server-threading.md`'s
  Recommendations with three tiers rather than two: (a) permanently
  Main-pinned by the frozen ABI — `abi/`, `physics/`, `lifecycle/`, which can
  never be relaxed without breaking the game-DLL contract; (b) Main-pinned by
  history but not by the ABI singleton — the `query.cpp`/`filter.cpp`/
  `log.cpp` cluster, which is a real future candidate for a G-1/G-3 off-main
  reader **once** it has a state-specific lock or a published snapshot, not a
  free relaxation today; (c) already-synchronised sites where an assert would
  be redundant rather than protective. Do not treat (b) as "cheap to relax
  today" — it still needs a synchronisation primitive that does not exist
  yet.
- **Boundary-safe**: Yes — documentation/classification only, no code change,
  no ABI or kernel surface touched.
- **Rationale**: `[EXT:G-3,P-2]` — the campaign's G-3 dedicated debug thread
  and P-2 published-snapshot goals both need to know which Main-pinned sites
  are movable and which never can be; a flat 101-site table with one
  rationale actively misleads that assessment.

______________________________________________________________________

## Medium-priority opportunities

### M-1: Narrow the pure leaf-adapter `ServerRuntime&` signatures; correct the "orchestrator" census

- **File(s)**: `src/server/physics/pmove.cpp:526-533` (`pm_clear_phys_ents`),
  `src/server/clients/snapshot.cpp:482-514` (`snapshot_reset`, `calc_ping`,
  `829/855/1079` `emit_pings`/`should_update_ping`/
  `update_to_reliable_messages`), `src/server/physics/physics.cpp:248/273/149`
  (`angular_move`/`linear_move`/`check_velocity`); the cvar-read shims listed
  at L-7.
- **Current pattern**: `docs/boundaries/server-boundary.md:526` claims "the
  whole-`ServerRuntime` signatures are the frame/lifecycle orchestrators the
  Q-22 carve-out names". A full mechanical scan of all `(ServerRuntime &rt`
  definitions in `src/server/**` finds **121** (not the 54/132 upper bounds
  in the tree-wide fact base, nor the ~48 this pack first estimated), with
  histogram 0-members:10, 1:41, 2:21, 3:20, 4:6, 5:6, ≥6:17. Of the 51 that
  touch ≤1 member, 30 are pure leaves that never forward `rt` further and 21
  form a corridor that forwards `rt` into deeper callees (mostly
  `physics/`). Only ~10 are genuine Q-22 orchestrators (`load_progs` 20
  members, `unload_progs` 14, `deactivate_server` 13,
  `install_world_bridge` 13, `spawn_server` 12, `create_baselines` 10,
  `sv_run_cmd` 9, plus `activate_server`/`save_exec_*` at 8). The remaining
  ~60 touch 2-5 members and are judgement calls.
- **Suggested replacement**: Narrow only the 30 pure leaves — highest value
  first because they are header-visible or already duplicated: the five
  cvar-read shims (L-7, consolidate to `cvar_value_or(CmdCvarContext*, ...)`),
  `pm_clear_phys_ents(abi::playermove_t&)`, `snapshot_reset(SnapshotState&)`,
  `emit_pings`/`should_update_ping(ClientMachinery&, ...)`. Explicitly do
  **not** touch the 21-function corridor in the same pass — narrowing it
  requires narrowing its callees first, and it sits adjacent to Chunk 11's
  frozen `pm_shared` work, so it is a scheduling collision to avoid, not a
  correctness question. Leave the physics.cpp orchestrator-adjacent adapters
  (`sv_move`, `pt_contents`) alone.
- **Boundary-safe**: Yes. None of the 30 leaves sits in the `clip.cpp:245-281`
  ULP kernel; behaviour-preserving by construction since only the parameter
  type narrows, not the body.
- **Rationale**: `[EXT:P-5]` (narrowest-state signatures) — makes read/write
  sets visible in the type system, the stated prerequisite for both P-2
  snapshotting and G-2's eventual parallel tick phases, at contained (S-M)
  effort. The corrected 121/30/21/~60 split, not the flat "~48", is the
  number any future migration-wave decision should use — `server-boundary.md`
  needs the same correction, tracked separately since this report does not
  own that file.

### M-2: `s_fatpvs`/`s_fatphs` are 8&nbsp;KB of undocumented file-scope mutable state

- **File(s)**: `src/server/game/engine_table.cpp:59-62` (declaration),
  `:1865-1866` (fill sites); `docs/architecture/server/index.md:103-110`
  (Module-statics table, currently omits both).
- **Current pattern**: `s_fatpvs`/`s_fatphs` are 4096-byte file-scope mutable
  arrays in the anonymous namespace, filled by `pfn_set_fat_pvs`/
  `pfn_set_fat_pas` via map_loader before returning a pointer into them. They
  are working buffers, not return-buffer statics in the sense
  `s_value`/`s_empty` are (function-local, returned verbatim) — but they
  carry no `compliance-allow(mutable-global)` annotation and appear in
  neither the architecture doc's Module-statics table nor
  `server-boundary.md`'s "Owned state" list, which currently claims `g_bridge`
  is "the ONE" file-scope mutable global.
- **Suggested replacement**: The buffers themselves are correct **by
  choice** — `pfnSetFatPVS`/`pfnSetFatPAS` are frozen ABI slots whose return
  contract is "pointer to engine-owned storage valid until the next call",
  exactly mirroring `sv_game.c:31-32`. Do not move them for the v1 ABI. The
  fix is annotation and documentation only: add
  `compliance-allow(mutable-global)` with the slot-contract rationale at
  `engine_table.cpp:63-64`, and add both rows to
  `docs/architecture/server/index.md`'s Module-statics table.
- **Boundary-safe**: Yes. Directly backs a frozen-ABI return-buffer contract;
  the proposal is annotation/doc-only and leaves the buffers untouched.
- **Rationale**: `[EXT:P-3,P-8]` — P-3 (context-first) and P-8 (annotation
  discipline) both require file-scope mutable state to be inventoried and
  annotated; today it silently is not. Shape constraint for later: if a v2,
  context-carrying ABI flavor is ever built, these 8&nbsp;KB must become
  context members — file-scope state is correct for v1 only.

### M-3: CMakeLists PUBLIC-links three dependencies server's public header does not need

- **File(s)**: `src/server/CMakeLists.txt:68-73`,
  `include/xash3dpp/server/server.hpp:17-27`.
- **Current pattern**: `server.hpp`, the subsystem's only public header,
  includes exactly one cross-subsystem xash3dpp header
  (`map_loader/map_loader.hpp:17`, needed for the public `ILevelChangeExecutor`
  base). `CmdCvarContext`/`Filesystem`/`NetworkContext` are bare forward
  declarations used only as pointers. The CMakeLists nonetheless marks 6 of 7
  dependencies `PUBLIC`.
- **Suggested replacement**: Flip only `xash3dpp_networking`,
  `xash3dpp_cmd_cvar`, and `xash3dpp_content` from `PUBLIC` to `PRIVATE`.
  **Not** the other three, and this matters: `src/map_loader/CMakeLists.txt:
  24-27` already `PUBLIC`-links `xash3dpp_memory`, `xash3dpp_filesystem`, and
  `xash3dpp_utilities`, and `xash3dpp_map_loader` must itself stay `PUBLIC`
  in server (required by the `ILevelChangeExecutor` base) — so those three
  remain in server's transitive `INTERFACE_LINK_LIBRARIES` no matter what
  `src/server/CMakeLists.txt` says, and marking them `PRIVATE` there would be
  a comment that lies about the actual link surface. Leave a one-line note
  explaining why those three stay `PUBLIC`-annotated even though they look
  like the same category as the three that move.
- **Boundary-safe**: Yes — pure link-visibility tightening, no ABI or kernel
  surface involved.
- **Rationale**: `[EXT:P-6]` (services are satellites) — tightens the
  header/link surface the one real consumer, `xash3dpp_host`, is transitively
  exposed to. The original finding claimed "6 of 7 over-exposed"; the real,
  actionable count is 3, and the corrected scope avoids landing a
  self-contradicting comment.

### M-4: `snapshot_alloc_ring` breaks its own pointer/count invariant on partial OOM

- **File(s)**: `src/server/clients/snapshot.cpp:438` (`num_client_entities`
  write), `:441`, `:444` (the two `mem_calloc` calls), `:462` (failure
  return); caller at `src/server/lifecycle/spawn.cpp:205`.
- **Current pattern**: `snapshot_alloc_ring` writes
  `rt.snapshot.num_client_entities = static_cast<int>(count)` at `:438`,
  **before** the two `mem_calloc` calls at `:441`/`:444`, and returns
  `ok=false` at `:462` on failure without calling `free_rings`. This is
  reachable: the only production caller,
  `(void)snapshot_alloc_ring(rt)` at `spawn.cpp:205`, discards the boolean
  result entirely, so nothing stops the server proceeding into
  `find_best_baseline` (`snapshot.cpp:124,134`), which reads
  `num_client_entities` as a modulus and then dereferences
  `packet_entities[...]` — a null-pointer dereference on allocation failure.
- **Suggested replacement**: Reorder: perform both `mem_calloc` calls (and the
  per-client frames loop) first into locals, and only on full success write
  `update_backup`/`update_mask`/`ring_maxclients`/`num_client_entities`/
  `next_client_entities` and publish the pointers. On any failure, call
  `free_rings(rt)` and return `false`, leaving the all-zero state
  `free_rings` already produces. (The earlier-considered fix of giving
  `SnapshotState` a destructor is not the right shape here — see the note in
  the Out-of-scope section for why a destructor was considered and dropped.)
- **Boundary-safe**: Yes. `SnapshotState` (`snapshot.hpp:105-129`) is an
  internal owning-pointer aggregate, not a vendored ABI struct; the
  `entity_state_t`/baseline payloads it points at are ABI PODs but the
  container's own lifecycle is xash3dpp's to change freely. No wire codec or
  kernel arithmetic touched.
- **Rationale**: A real, reachable defect (safety hazard, not style) with a
  contained, mechanical fix.

### M-5: `PrecacheTables` is four parallel lists kept in sync only by convention

- **File(s)**: `include/xash3dpp/private/server/precache.hpp:31,48-54,128-129`,
  `src/server/lifecycle/precache.cpp:55-58`.
- **Current pattern**: Five structures must stay in the same order with
  nothing enforcing it: the `PrecacheKind` enumerator order, the
  `PrecacheCaps` field order, the `models_, sounds_, events_, generics_`
  member declaration order, the `Table *tables[4]` initialiser, and the
  `const std::size_t cap[4]` initialiser. There is no `static_assert` linking
  `tables[static_cast<int>(PrecacheKind::Sound)]` to `&sounds_`, and the same
  lookup pattern is hand-duplicated across four near-identical `*_index()`
  bodies.
- **Suggested replacement**: Replace the four named members with
  `Table tables_[4]` indexed by `PrecacheKind`, replace `PrecacheCaps` with
  `std::size_t caps_[4]` (or keep the named struct and add a
  `constexpr std::size_t cap_of(PrecacheKind)` accessor with a
  `static_assert` on enumerator count), and collapse the four `*_index()`
  bodies into one `int index_for(PrecacheKind, const char*)` driven by a
  small `constexpr` spec table. Public accessor signatures are unchanged.
- **Boundary-safe**: Yes — internal reshape with public accessor wrappers
  preserved unchanged; no ABI or kernel surface touched.
- **Rationale**: Removes a class of "silently reorder one array, break a
  different one" bug that no compiler diagnostic currently catches.

### M-6: `IClipHooks` has zero implementations — record as Chunk-11 door-debt, not deletion

- **File(s)**: `include/xash3dpp/private/server/world_trace.hpp:113-145,167`,
  `src/server/world/clip.cpp:155-160,379,415,452`.
- **Current pattern**: A tree-wide grep for `: IClipHooks`/`public IClipHooks`
  returns nothing — no production implementation, and (unlike every other
  server interface) no test double either. `MoveEnv::hooks` is never
  assigned anywhere. Its four guard branches (`should_collide`/`sphere_cull`/
  `custom_clip` at `clip.cpp:379,415,452`) and the out-of-line default
  `custom_clip` are unreachable in every build today.
- **Suggested replacement**: Do **not** delete. `save_bridge.cpp:778` names
  the day-one consumer explicitly: the Chunk-11 `svgame.physFuncs`/
  `SV_InitPhysicsAPI` negotiation, which is exactly what installs a
  `SOLID_CUSTOM` clip provider. Record this as door-debt in
  `server-boundary.md`'s Q-21 Extension-axes table with the Chunk-11 owner
  named, rather than leaving it silently unreferenced. The guard sites sit
  structurally distant from the actual HB-2 fenced block (`clip.cpp:245-281`)
  — the caution that they might interact with the ULP transform is prudent
  to check once Chunk 11 wires them, but nothing in the current dead code
  touches it.
- **Boundary-safe**: NeedsVerification for the eventual Chunk-11
  implementation (new code, not yet written); the door-debt recording itself
  is doc-only.
- **Rationale**: `[EXT:G-2]` — `IClipHooks` is the named day-one consumer for
  the Chunk-11 physFuncs door, which is exactly the extension-bump case: a
  finding that protects a north-star door is promoted one tier. The original
  2026-07-06-era assessment under-tiered this as cosmetic; it is not.

______________________________________________________________________

## Low-priority / cosmetic opportunities

### L-1: centralise the byte→char aliasing at the info-string edge

- **File(s)**: `clients/info_string.cpp:95-166` — the `read_field` cursor walk
  and the four `const_cast` splice sites in `info_remove_key`/
  `info_remove_prefixed_keys` over the caller-owned mutable info buffer
  (Q-16-annotated).
- **Modernization**: the port is faithful and correct, but the `char*` cursor
  plus the `const_cast` splice is the one place a bounded `std::span<char>`
  view could make the in-place edit explicit without changing the ABI (the
  buffer stays a caller-owned `char[]`; the still-stubbed `pfnInfo_RemoveKey`
  slot at `engine_table.cpp:1810-1812/2415` touches this buffer's identity,
  which is why the buffer's shape itself must not move). Cosmetic; low
  priority — the SAFETY annotations already document the invariant.

### L-2: `physics.cpp` `switch` MOVETYPE dispatch → table — leave as-is

- **File(s)**: `physics/physics.cpp:1532` — the 13-case MOVETYPE_* dispatch
  switch, mirroring legacy `SV_Physics_Entity` exactly.
- **Modernization**: a `constexpr` dispatch table keyed by MOVETYPE would DRY
  the switch, but each arm carries a distinct stub/behaviour note and the
  switch is where the still-pending S9 physFuncs override stubs slot in.
  **Verdict unchanged: leave as switch** until those stubs land. Recorded
  only so a reader does not table-ify prematurely. The companion
  `switch(solid)`/`switch(waterlevel)` sites (`physics.cpp:683,1132,1143`,
  `pmove.cpp:152`) carry the same leave-verdict for the same reason.

### L-3: fold the RNG-unification stub into one shared stream (HB-12)

- **File(s)**: `abi/engine_table.cpp:1556-1585` (`s_rng_state`, installed at
  `t.pfnRandomLong`/`t.pfnRandomFloat`, `:2387-2388`),
  `physics/init_client_move.cpp:278-303` (`s_pm_rng`, installed at
  `pm.RandomLong`/`pm.RandomFloat`, `:439-440`). Line numbers shifted from
  the 2026-07-06 pass (1543→1554, 280→278); the finding itself is unchanged.
- **Modernization**: the two `rng_next`/`pfn_random_long`/`pfn_random_float`
  bodies are character-identical modulo the seed constant (`0x29A` vs.
  `0x1a2b`) and parameter names. Legacy has **one** shared generator
  (`engine/common/common.c:54`, `static int idum`, feeding both
  `COM_RandomLong` and `COM_RandomFloat` and every caller tree-wide via
  `COM_SetRandomSeed`), so the eventual HB-12 fix is a single shared stream
  matching that generator byte-for-byte — not two independently-seeded
  streams, and not a `std::mt19937` substitute (would diverge from the
  legacy sequence). Until HB-12 lands, a pure mechanical dedup is still
  worthwhile at low risk: extract `struct XorShift32 { std::uint32_t state;
  std::uint32_t next() noexcept; }` into a **server-private** header (not
  `utilities`, not a public header — this is stub code both call sites
  already mark for wholesale replacement), instantiate it twice with the
  existing independent seeds so behaviour is unchanged, and keep
  `pfn_random_long`/`pfn_random_float` as thin per-TU wrappers around it.
  This is a **parity-adjacent** dedup only — it must not be mistaken for the
  HB-12 fix itself, which requires the single legacy-matching stream and its
  own golden test.
- **Boundary-safe**: Yes for the dedup — touches frozen-ABI *implementation*
  only (`pfnRandomLong`/`pfnRandomFloat` are installed frozen `enginefuncs_t`
  slots; the shape, name, and calling convention of both do not change).

### L-4: `copy_cstr` in `pmove.cpp` re-implements `xash::utilities::strncpy`

- **File(s)**: `physics/pmove.cpp:83-93` (`copy_cstr`),
  `src/utilities/string.cpp:31-41` / `include/xash3dpp/utilities/string.hpp:
  27-28` (`ut::strncpy`), 4 call sites including `pmove.cpp:132-139`.
- **Modernization**: `copy_cstr` and `ut::strncpy` are byte-identical in
  semantics — both no-op on a zero cap, both tolerate a null `src`, both
  always NUL-terminate, both stop at `size - 1`. The only difference is the
  unused `char*` return. 20+ other server call sites already use
  `ut::strncpy` directly. Delete `copy_cstr` (11 lines) and repoint its four
  call sites at `::xash::utilities::strncpy`. Zero behavioural change.
- **Boundary-safe**: Yes — plain string-copy helper unrelated to any ABI
  struct or ULP math.

### L-5: seven heap allocations exist only to NUL-terminate a `string_view`

- **File(s)**: `clients/client_state.cpp:204-206,214-216,395,552-557`
  (5 sites feeding `ut::strncpy` into a fixed `char[]`),
  `lifecycle/save_bridge.cpp:964,1048` (2 sites feeding `spawn_server`).
- **Modernization**: `std::string(sv).c_str()` appears exactly 7 times in
  server, each allocating a temporary heap string purely to obtain a
  NUL-terminated pointer from a `std::string_view` that already points at
  the source bytes. Two of the five `client_state.cpp` sites sit on the
  client-`connect` OOB path, so every connect packet currently does two
  avoidable allocations. `utilities/string.hpp:28` declares only the
  `const char*` overload. Add
  `char *strncpy(char *dst, std::string_view src, std::size_t size) noexcept`
  beside it (a five-line body sharing the existing loop; must stop copying
  at the first embedded NUL in the view, matching what
  `std::string(sv).c_str()` followed by the existing overload does today,
  not blindly copy the full view length) and delete the five
  `client_state.cpp` temporaries. **Leave the two `spawn_server` sites
  alone** — changing `spawn_server`'s parameters to `std::string_view` was
  part of the original proposal but was not re-verified for overload-
  resolution safety at those call sites; the five-site win stands on its
  own and needs no header-visible signature change to `spawn_server`.
- **Boundary-safe**: Yes — new overload has no overload-ambiguity risk
  against the existing `const char*` form for literal/`const char*`
  arguments (an exact array-to-pointer match beats the user-defined
  conversion), and five existing call sites are the day-one consumer.

### L-6: `std::qsort` with a `void*` comparator is server's one genuine `std::ranges` opportunity

- **File(s)**: `clients/snapshot.cpp:26,58-67,894-895` (gather-scratch sort,
  `entity_numbers_cmp`).
- **Modernization**: the only untyped sort in the subsystem, and the only
  place `std::ranges` would be a real improvement rather than churn — every
  other server loop is a byte-exact parity walk over C arrays/edict indices
  where a range adaptor adds risk without benefit. The sort key
  (`entity_state_t::number`) is unique within the array by construction (the
  dedup visbit mask at `:49-56` guarantees each edict appears once). Replace
  with `std::ranges::sort(std::span(rt.snapshot.gather_ents, num), {},
  &abi::entity_state_t::number);` and delete `entity_numbers_cmp` (10 lines)
  and the `<cstdlib>` include.
- **Boundary-safe**: NeedsVerification — not a fence question (the sort
  itself performs no bit-codec/delta-width/LZSS operation; those happen
  downstream in networking's actual fenced kernel), but the uniqueness
  invariant the replacement relies on is worth a one-line assertion when
  landed.

### L-7: five near-identical cvar-read shims across four TUs

- **File(s)**: `clients/query.cpp:29-41`, `clients/snapshot.cpp:69-73`,
  `lifecycle/save_bridge.cpp:458-480`, `physics/movevars.cpp:25-28`; the
  correctly-shaped sixth instance is `cvar_or_default` in `hulls.cpp:118`.
- **Modernization**: six anonymous-namespace helpers across six TUs all wrap
  the same two-line pattern — null-check `rt.cvars`, then call
  `cvar_variable_value`/`cvar_get_string`, returning a default on absence.
  Five take `ServerRuntime&` (a P-5 deviation — they touch only `rt.cvars`);
  the sixth, `hulls.cpp:118`, already takes `CmdCvarContext*`, the correct
  shape. Promote a single `float cvar_value_or(CmdCvarContext *cvars, const
  char *name, float fallback) noexcept` plus a string sibling into
  `private/server/cvar_read.hpp`, taking `CmdCvarContext*` not
  `ServerRuntime&`, with the `hulls.cpp` registered-default semantics as the
  documented contract; delete the five other shims and audit the ~15 call
  sites for the widened parameter. This item overlaps M-1's leaf-narrowing
  set — land it as part of that pass, not separately.
- **Boundary-safe**: NeedsVerification for the ~15 call-site audit; the
  consolidation itself touches no ABI struct or kernel arithmetic.

### L-8: `server-threading.md`'s per-file assert count is stale

- **File(s)**: `docs/threading-analysis/server-threading.md:1-24`;
  `src/server/lifecycle/save_bridge.cpp` (7 sites, omitted from the table
  entirely).
- **Modernization**: doc-only. The table sums to 92 sites across 26 files and
  omits `lifecycle/save_bridge.cpp` entirely. A direct grep of the current
  tree gives 101 sites across 27 files (matching the tree-wide fact base's
  exact count, not this subsystem's own doc); `game_dll.cpp` (3 actual vs. 2
  documented) and `messages.cpp` (7 actual vs. 6 documented) have also
  drifted. Re-run the per-file count and update the table the next time this
  doc is touched. Low priority since `compliance_scan` already enforces the
  assertions mechanically — the doc's number is descriptive, not
  load-bearing.

______________________________________________________________________

## Out of scope / ABI-frozen

- **Do not add a context parameter to any `enginefuncs_t` slot to "remove"
  `g_bridge`.** The slot signatures are the frozen ABI; the global is the
  documented Q-20 carve-out for the userdata-less C surface. A future v2
  flavor carries the context as a *new* interface alongside the frozen one.
  Converting the slot **bodies** (not the installed signatures) to take
  `EngineBridge &b` as an internal implementation detail, with the frozen
  slots reduced to one-line adapters, is a real shape that makes a future v2
  flavor mechanical rather than a rewrite — but it has no day-one consumer
  today (the v2 ABI-slot brief, `extension-goals.md` §5, is unscheduled), so
  it is recorded here as a **shape constraint**, not work: *a v2 ABI slot
  must receive its state as a parameter; `g_bridge` may exist only as the v1
  capture-less adapter layer, never as the reach-in for a v2 slot body.*
- **Do not replace the array-of-edicts with a `std::vector<Entity>` or a
  handle map.** Game DLLs address entities by byte offset from the array base
  (`pfnEntOffsetOfPEntity`); the representation is ABI. Handleization is a
  post-parity load-time flavor behind `EntityView` (Q-20), not a refactor.
- **Do not "modernize" the Quake physics constants.** The ±1.0 ClipVelocity
  snap, the `215.0f` chase-dir typo, the friction reset-to-1.0, and the
  `pushed[256]` cap are behavioural contract (deep-dive-server-physics.md).
  A bounds-check-and-log above the cap is allowed; changing behaviour below it
  is not.
- **Do not touch the rotated-brush / trace math for tidiness (Q-18).** The
  HB-2-fenced kernel is the `if (rotated)` block at `world/clip.cpp:245-281`
  (in-code `TODO(Q-18)` noting the rotated-brush transform is ULP-inexact vs.
  legacy) — **not** `clip.cpp:219` as the 2026-07-06 pass and
  `decisions-architecture.md:946-954` (owned elsewhere) record; that anchor
  has drifted. The fix there is toward *more* exactness, not a
  `std::ranges`/FMA rewrite. The trace/contents/hull kernels join the
  tree-wide float-exact no-touch set (map_loader trace/PVS/CRC + networking
  wire codec + content studio bone math).
- **Do not unify the pmove `RandomLong` with a `std::mt19937`.** The idtech
  RNG parity port (HB-12, L-3) is byte-exact by requirement; a
  standard-library generator would diverge from the shared legacy stream.
- **`SnapshotState` does not get a destructor.** Considered for M-4 and
  dropped: it would deliver only 3 of the struct's 5 owning pointers (the
  per-client `ServerClient::frames` ring and `ServerRuntime::signon_buf` live
  outside `SnapshotState` and are not reachable from it), so it would look
  like a complete RAII fix while leaving two of the five pointers exactly as
  manually-managed as before. The real defect (M-4) is an ordering bug, not
  a missing destructor.

______________________________________________________________________

## `strnicmp` / `strncmp` string_view over-read — ABSENT

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern
(utilities M-4 / filesystem M-7 / cmd_cvar M-5) is **absent** in server. Every
bounded/length compare here is over **NUL-terminated C-strings**, matching the
prior abi-phase note that `src/server/game/**` `strcmp`/`strncpy` are C-string
slot bodies, not `string_view.data()` over-reads. Verified sites:

- `abi/engine_table.cpp:73` — `strcmp(v->name, name)` over NUL-terminated
  cvar names (ABI slot body).
- `clients/info_string.cpp` — `Info_ValueForKey` compares `strcmp(key, pkey)`
  where `pkey` is a `read_field`-produced NUL-terminated `char[128]`; the key
  parser is fully NUL-terminated C-string work.
- `clients/filter.cpp:61` — `std::strncmp(id, f.id, len)` where
  `len = min(strlen(id), strlen(f.id))`, so the compare reads at most `len`
  bytes of two **NUL-terminated** C-strings — a bounded compare over C-strings,
  **not** a non-terminated `string_view` fed to a length-bounded compare.
- `clients/client_state.cpp` / `messages.cpp` / `query.cpp` — `strcmp` /
  `strncpy` over NUL-terminated client names / message names / userinfo.

The candidate the cross-cutting note flags — **userinfo / `Info_ValueForKey`
key parsing** — *exists here* (`info_string.cpp`), but is implemented as a
NUL-terminated C-string parser (the frozen GoldSrc codec), so it is **not** an
over-read site. This is server's **negative** data point (the 9th across
platform / core / host / abi / launcher / map_loader / networking / content) —
the over-read pattern is confined to the four early text-heavy subsystems
(utilities / filesystem / cmd_cvar), and the shared bounded `ci_compare(sv, sv)`
the sweep proposes has no server caller.

______________________________________________________________________

## Open questions

- **P-5 migration wave.** `server-boundary.md`'s P-5 row says existing
  whole-`ServerRuntime` signatures "migrate in Chunk 6B (the scheduled
  full-retrofit wave)". Chunk 6B shipped and 121 such signatures remain (see
  M-1). Is the rule now false, or is a narrower wave still owed — and if so,
  is it the 30-leaf slice M-1 scopes, the 21-function corridor, or both? This
  needs a decision recorded in `server-boundary.md`, not this file.
- **Cvar storage ownership** (affects server but is a cmd_cvar decision).
  Server's six frozen cvar slots (`engine_table.cpp:1189-1225,1798,2058`)
  resolve only against `EngineBridge::external_cvars`, a second registry that
  never falls through to the engine's real cvar store — so
  `CVAR_GET_FLOAT("sv_gravity")` called from a game DLL returns `0.0f` today.
  Fixing this is a storage-ownership decision that belongs in cmd_cvar's
  report (its `cvar_register_dll`/`cvar_unlink` API already exists with zero
  production callers); flagged here because the four in-code
  `TODO(chunk6-S7): fall through to the engine cvar registry` markers
  (`engine_table.cpp:1199,1212,1223,1799`) are server-visible symptoms of a
  decision this report does not own.
- **`server-boundary.md` corrections owed** (this report does not own that
  file, so these are flagged rather than fixed): the P-3 "the ONE
  file-scope mutable global is `g_bridge`" row (real list: `g_bridge`,
  `s_fatpvs`, `s_fatphs`, `s_rng_state`, `s_pm_rng` — M-2); the P-5
  "orchestrators" row (corrected census — M-1); the P-7 "Q-22 RAII classes"
  row (no destructors exist — H-1); the G-2 row's `EngineBridge`
  localisation claim (directionally right about the slot-state struct and
  `playermove_t*`, but silent on the `runtime` back-pointer that re-widens
  the surface, and on the four dead mirrored scalars — H-2); the `:153`
  claim that Server implements `IMapLoaderObserver` (it derives from and
  implements no such interface anywhere — the interface itself is
  map_loader's, not server's, but the false row is in this subsystem's
  boundary doc); and the `:516` claim that a compliance-scan rule already
  guards raw `entvars_t`/`edict_t` confinement "when the server scaffold
  lands" — the scaffold shipped through Chunk 10 and no such rule exists in
  `xtools/rules.py`/`compliance_scan.py`.
- **`EntityView` is not a P-4 introspection channel.** It is a private-header,
  zero-cost Q-20 accessor facade over a live `edict_t`, unreachable from any
  frontend and not a snapshot. `implementation-plan.md:601-609` currently
  names it as an HB-6 channel; that is incorrect and needs correcting there,
  not here. When Chunk 12/G-4 needs entity introspection it must add a
  by-value row-snapshot on the public server surface, not promote
  `EntityView` to a public header.
- **`IMapLoaderObserver`'s fate.** Zero production implementations, zero
  production `attach_observer`/`detach_observer` calls, no chunk marker, and
  (per the point above) server does not even implement it despite the false
  boundary-doc row. It is map_loader's interface to assign a chunk number to
  or delete — out of scope for this report, but its false attribution lives
  in `server-boundary.md`.
