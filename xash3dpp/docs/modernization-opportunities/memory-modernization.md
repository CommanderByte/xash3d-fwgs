# Memory Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> Refreshed 2026-07-20 (tree-wide modernization audit, 16-agent campaign,
> HEAD `cc73c054`). Adversarial (Phase-2) re-verification **refuted both of
> this report's tier-bearing items** as written: former **H-1** (aligned-alloc
> door) and former **M-2** (`for_each_pool` span-fill sibling) were each a
> re-discovery of an already-tracked, explicitly-speculative backlog item
> (`HB-7`/`HB-6`) proposed as buildable work with no named consumer — exactly
> the anti-gold-plating failure mode `extension-goals.md` §6 exists to catch.
> Both are demoted to shape constraints and moved to "Investigated and
> refuted" below; **the subsystem currently has no High-priority item**. A
> follow-on cross-subsystem lens additionally found that HB-7's own rationale
> was wrong in a way worth recording: **none of the three shortlisted G-5
> scripting runtimes actually need the alignment lift** (see M-1 and Open
> questions). Two new items were added: a documentation-accuracy finding
> (**M-3**) — `threading-model.md` asserts a "pool spinlock" that does not
> exist anywhere in `src/memory/` — and an unverified caller-contract
> enforcement gap (**L-3**). `PoolConfig::reserve` (**L-2**) is upgraded from
> "leave as-is" to "delete now, re-add with Arena" per the tree-wide
> subtraction lens. Everything else below is reconfirmed unchanged against
> current `HEAD`.
> C++ standard in use: C++**23** (from `xash3dpp/src/memory/CMakeLists.txt`,
> `target_compile_features … cxx_std_23`; the tree-wide `CMAKE_CXX_STANDARD 23`).
> Boundary spec: `docs/boundaries/memory-boundary.md`
> Threading: `docs/threading-analysis/memory-threading.md`
> ABI-frozen symbols in this subsystem: the **handle width only** —
> `PoolHandle::index` is `std::uint32_t`, matching the SDK `poolhandle_t`
> (`common/xash3d_types.h`) embedded in `model_t`. The public function *names*
> are xash3dpp-internal today; the four legacy plugin shims
> (`_Mem_*` in `ref_api_t`; pool-less `pfnMemAlloc`/`pfnMemFree` in
> `render_api.h` / `menu_int.h` / `physint.h`) are **not implemented yet** and
> are future fill-site work.

## Summary

The memory subsystem is small (one `.cpp`, three public headers, one private
internals header) and already **highly modern** — `compliance_scan.py memory`
and `stub_scan.py memory` are both **clean**, and `status_table.py` reports it
**Complete**. It uses `std::atomic` throughout with explicit memory-ordering, an
`enum class SlotState` publication protocol, `static_assert`-pinned layout,
`[[nodiscard]]` on every allocating entry point, `constexpr`/`noexcept`
correctness on `PoolHandle`, and a full C++ RAII/smart-pointer suite
(`pool_new<T>`, `pool_delete`, `ScopedPool`, `PoolDeleter`, `pool_ptr`).

Unlike the legacy `zone.c`, the rewrite deliberately **dropped** the C-isms that
would normally headline a modernization report: no sentinels, no `char*`
filename/line capture, no doubly-linked `memheader` chains, no `MEM_SMALL_ALLOC_OPT`
big/small header split. Corruption detection is delegated to ASan/MSan.

What remains is therefore short and is **not** primarily C-style-code cleanup.
The 2026-07-20 refresh found that the two items this report previously ranked
highest were themselves the failure mode the campaign was watching for:
**building (or proposing to build) new surface area against a backlog item
that is explicitly speculative.** The aligned-alloc ceiling (`pool_new<T>`
hard-`static_assert`s `alignof(T) <= 8`) is real and tracked as `HB-7`, but
`HB-7` has no named consumer today — and a cross-subsystem trace of the G-5
scripting-runtime shortlist (Lua 5.4, QuickJS-ng, AngelScript) found that
**none of the three actually need the lift**; the real G-5 allocator gap is
AngelScript's context-less global hook, a per-VM *attribution* problem, not an
*alignment* one. Similarly, `for_each_pool`'s only friction is felt by one
test helper (zero production callers), so adding a second iteration API to
serve it is the textbook over-abstraction case. Both are recorded as shape
constraints, not work — see "Investigated and refuted".

What is left after that correction is small: one documentation-accuracy fix
(a design doc asserts synchronization this subsystem does not have), a
deferred backend-seam promotion (correctly still deferred), and cosmetic
tidy-ups. Several "C-looking" shapes here are **deliberately** C-shaped (the
strategy function-pointer triple and the `for_each_pool` callback both avoid
heap and keep the layer boundary-friendly and `noexcept`); they are noted as
*do-not-touch-yet* where relevant.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| `System` strategy (malloc/free/realloc) | **Implemented** | The only wired backend |
| `AllocStrategy::Arena` / `Slab` | **Placeholder** | Enum tags; `create_pool` falls through to `System`. Q-22: no `I*` seam until a second real backend lands |
| OOM handler (`set_oom_handler`) | **Implemented** | Atomic; init-only caller contract |
| Stats surface (`get_stats`/`pool_count`/`for_each_pool`) | **Implemented** | The Q-13 `memlist`/P-4 accounting tier |
| Typed helpers (`pool_new`/`pool_delete`/`ScopedPool`/`pool_ptr`) | **Implemented** | P-7 canonical idiom |
| Aligned allocation (`alignof(T) > 8`) | **Not implemented** | Hard `static_assert` blocks over-aligned `pool_new<T>`; tracked as open backlog `HB-7`, correctly un-built — no in-tree consumer and no G-5 shortlist candidate needs it (see "Investigated and refuted") |
| Bulk-empty (`empty_pool`) / validation (`check`) / `is_allocated` | **Not implemented** | Legacy `_Mem_EmptyPool`/`_Mem_Check`/`Mem_IsAllocatedExt` — natural only once `Arena` lands / delegated to ASan. Not a modernization gap; recorded as scope note |
| `std::pmr::memory_resource` adapter | **Not implemented** | Open question in boundary spec — **O-1** |
| Legacy plugin ABI shims | **Not implemented** | Future fill-site (Chunk 12/13 + DLL bring-up) |

______________________________________________________________________

## High-priority opportunities

None. The former H-1 (aligned-allocation API) was refuted on 2026-07-20 as
written as work — see "Investigated and refuted" below for the finding and
its disposition.

______________________________________________________________________

## Medium-priority opportunities

### M-1: Promote the strategy function-pointer triple to a typed seam **when the second backend lands** (deferred)

- **File(s)**: `xash3dpp/include/xash3dpp/private/memory/pool_registry.hpp`
  (`PoolBucket::do_alloc`/`do_free`/`do_realloc`/`ctx`); `memory.cpp`
  (`sys_alloc`/`sys_free`/`sys_realloc`, the `switch (cfg.strategy)`).

- **Current pattern**: C-style backend dispatch —

  ```cpp
  void* (*do_alloc  )(std::size_t, void* ctx) noexcept = nullptr;
  void  (*do_free   )(void*,       void* ctx) noexcept = nullptr;
  void* (*do_realloc)(void*, std::size_t, void* ctx) noexcept = nullptr;
  void*  ctx = nullptr;
  ```

  Only `System` is wired; `Arena`/`Slab` fall through to it.

- **Suggested replacement**: When a real second backend (`Arena`) is
  implemented, this triple + `ctx` is the natural `IAllocatorBackend`
  interface. **Do not** introduce the interface before then.

- **Boundary-safe**: Yes.

- **Rationale**: Explicitly **deferred** by Q-22's seam rule ("no `I*` interface
  until a second real backend lands"). Recorded here so the promotion is not
  forgotten and is not done prematurely. This item is the G-5 scripting allocator
  bridge point — keep the triple injectable (a script VM's allocator maps onto a
  `PoolConfig` strategy + its own `ctx`). **2026-07-20 reconfirmed unchanged**:
  `AllocStrategy::Arena`/`Slab` are still enum tags only, `create_pool`'s
  `switch (cfg.strategy)` (`memory.cpp:100-111`) still falls both through to
  `System`, and no `IAllocatorBackend` exists anywhere in the tree. A
  cross-subsystem trace of the G-5 shortlist confirms this deferral does not
  block scripting-runtime adoption either: Lua 5.4's `lua_Alloc` and
  QuickJS-ng's `JSMallocFunctions` both carry a per-VM opaque context that can
  box a `PoolHandle` and call `mem_alloc`/`mem_realloc`/`mem_free` directly —
  no interface required. Bridging either runtime is calling the existing free
  functions from inside the satellite's `noexcept` shims, not a new
  abstraction layer.

### M-3: `docs/design/threading-model.md` asserts a "pool spinlock" that does not exist

- **File(s)**: `xash3dpp/docs/design/threading-model.md:568` (the claim);
  `xash3dpp/src/memory/memory.cpp`, `xash3dpp/include/xash3dpp/memory/`,
  `xash3dpp/include/xash3dpp/private/memory/` (the code the claim describes).

- **Current pattern**: The subsystem's own synchronization is atomics-only —
  `PoolBucket::live_bytes`/`total_allocs`/`total_frees`/`state` and
  `g_oom_handler` are all `std::atomic<T>` (`pool_registry.hpp:43-46`,
  `memory.cpp:43`); there is no lock of any kind. `threading-model.md`'s
  cross-subsystem synchronization table nonetheless states:

  > `| memory — allocate | Any (pool spinlock) | **Today** | Documented in
  > memory subsystem |`

  A tree-wide grep for `spinlock|atomic_flag|std::mutex|shared_mutex` across
  `src/memory/`, `include/xash3dpp/memory/`, and
  `include/xash3dpp/private/memory/` returns **zero matches**. No spinlock
  exists; the row's own "documented in memory subsystem" pointer is false —
  neither `memory-boundary.md` nor `memory-threading.md` describes a spinlock
  either, both correctly describe the lock-free atomic design instead.

- **Suggested replacement**: Correct the row to describe the actual mechanism
  (lock-free atomics, not a spinlock) and, more importantly, flag that "Any"
  thread safety for **allocation** is true only in the narrow sense of the
  counters — it says nothing about whether callers pass a valid, still-live
  `PoolHandle`, which is a caller contract (see L-3), not something the
  atomics enforce.

- **Boundary-safe**: Yes — documentation-only; no code change.

- **Rationale**: This is not cosmetic. The table is consulted as ground truth
  by whoever next moves an allocation path off the main thread (the
  networking-transport or renderer options in the tree's open thread-model
  decision packet). Today the claim is harmless by accident — the one thread
  spawn in the tree (`sound/topology.cpp`) allocates via the **system heap**,
  never a pool, so no code currently depends on the false claim being true.
  Whoever executes an off-main allocation path next will read this row,
  believe a synchronization primitive protects concurrent pool access that
  does not exist, and ship a race. Fixing the row costs one line; the race it
  currently invites would not.

______________________________________________________________________

## Low-priority / cosmetic opportunities

### L-1: `std::strncpy` + manual terminator in `create_pool` → a bounded copy helper

- **File(s)**: `xash3dpp/src/memory/memory.cpp` (`create_pool`).

- **Current pattern**:

  ```cpp
  std::strncpy(b.name, name, sizeof(b.name) - 1);
  b.name[sizeof(b.name) - 1] = '\0';
  ```

- **Suggested replacement**: Route through the `utilities` bounded string-copy
  (the `Q_strncpy`-parity helper that always null-terminates) or a tiny local
  `copy_bounded(std::span<char>, std::string_view)`. Note: memory sits below
  `utilities` in the stack, so a `utilities` dependency may be undesirable — a
  local 3-line helper is the safer route.

- **Boundary-safe**: Yes — behaviour identical (already null-terminates).

- **Rationale**: Removes the last raw CRT string call; cosmetic. Watch the layer
  ordering — do **not** add a `utilities` link just for this. **2026-07-20
  reconfirmed unchanged** — single call site, `memory.cpp:90-91`.

### L-2: Delete the currently-unread `PoolConfig::reserve` field

- **File(s)**: `xash3dpp/include/xash3dpp/memory/memory.hpp:67` (`PoolConfig`).

- **Current pattern**: `std::size_t reserve { 0 }; // pre-allocation hint; currently ignored`.
  A tree-wide grep confirms zero read sites: `create_pool`'s
  `switch (cfg.strategy)` (`memory.cpp:100-111`) never touches `cfg.reserve`,
  and no other TU references the field.

- **Suggested replacement** (**updated 2026-07-20** — was "wire it in or drop
  it, leave as-is if `Arena` is imminent"; the tree-wide subtraction lens
  reclassifies this as a plain deletion, not a judgement call, since `Arena`
  is not imminent and the field has never had a reader): **delete the field
  now.** Re-add it only together with the `Arena` backend that would actually
  honour it (M-1) — a reserve hint is meaningful for a bump allocator, but not
  before one exists.

- **Boundary-safe**: Yes — `PoolConfig` is a designated-initializer-friendly
  aggregate with `blast_radius` 0 for this field; removing it does not change
  any existing `create_pool("name")` or `create_pool("name", {.strategy = …})`
  call site.

- **Rationale**: An inert public field with zero readers is the same failure
  category the campaign's subtraction lens flagged tree-wide — structure kept
  because it looks like forward planning rather than because anything reads
  it. Prefer deletion; the field is one line to restore alongside `Arena`.

### L-3: `create_pool`/`destroy_pool` single-owner caller contract is documented but unenforced (unverified)

> **Unverified** — surfaced by the 2026-07-20 campaign but outside Phase-2
> adversarial review; treat as a lead, not an established defect.

- **File(s)**: `xash3dpp/docs/threading-analysis/memory-threading.md:118-136`
  (the "Required caller contracts" section); `xash3dpp/src/memory/memory.cpp:64`
  (`create_pool`); `xash3dpp/include/xash3dpp/memory/memory.hpp:71-75`
  (`create_pool`/`destroy_pool` declarations).

- **Current pattern**: `memory-threading.md` documents five caller contracts
  that callers must uphold but that nothing in the code checks — "Pool
  lifecycle is single-owner", "`set_oom_handler` is called at init time only",
  "No allocation through a pool handle after `destroy_pool`", "No concurrent
  `create_pool` calls [for lifecycle purposes]", "Not called from signal
  handlers". `destroy_pool` enforces exactly one of these, and only in debug
  builds: `XASH_ASSERT(live_bytes == 0)` (`memory.cpp:133`). The contract is
  invisible from `memory.hpp` itself — a reader of the public header has no
  signal that `create_pool`/`destroy_pool` carry an init-window caller
  discipline at all.

- **Suggested replacement**: A lightweight, dependency-free check — e.g.
  lazily capture `std::this_thread::get_id()` on first `create_pool` call and
  debug-assert subsequent `create_pool`/`destroy_pool` calls originate from
  the same thread. This is the same pattern already used elsewhere in the
  tree for lazily-captured thread identity (see the documented
  `assert_main_thread()` divergence in the campaign's known-open backlog) —
  memory sits at L0, below `platform` (where `ThreadRole`/`assert_thread_role`
  live), so a `platform`-style role assertion is not available without a
  layering violation; a local, dependency-free thread-id capture is the
  correct shape here, not a `platform` link.

- **Boundary-safe**: Yes — additive, debug-only, no public signature change.

- **Rationale**: This is the concrete, in-subsystem instance of the
  by-design-vs-incidental distinction the campaign's threading lens asked
  every pack to surface. Here it is **incidental**: the reason no assertion
  exists is memory's position below `platform` in the link DAG, not a
  deliberate design choice that lifecycle calls may come from any thread.
  Blast radius is small (`create_pool`/`destroy_pool` call sites, ~21 across
  the tree) and the check is debug-only, so it costs nothing at runtime in
  release builds while catching a real class of misuse (e.g. a satellite
  service destroying a pool it does not own) at the point of the violation
  instead of downstream as a use-after-free.

______________________________________________________________________

## Out of scope / ABI-frozen

- **`PoolHandle::index` width and value space.** Frozen at `std::uint32_t`,
  1-based, to stay wire-compatible with the legacy `poolhandle_t`
  (`common/xash3d_types.h`) embedded in `model_t` — see the header blockquote.
  Not a modernization target: a wider or differently-shaped handle would break
  the SDK layout this subsystem must eventually hand to `ref_api_t`/
  `render_api.h`/`menu_int.h`/`physint.h` consumers.
- **The legacy `zone.c` allocator itself** — reference-only, at the repository
  root. Its sentinel/`memheader`/big-small-header design is deliberately **not**
  ported (see Summary); nothing in `zone.c` is a target here.
- **No HB-2 fenced kernel applies to this subsystem.** Memory is not one of
  the five subsystems carrying a byte-exact no-touch kernel (map_loader,
  content, networking, server, utilities); every finding above is fence-clear.

## Investigated and refuted

Findings re-derived or re-proposed during the 2026-07-20 campaign that were
adversarially refuted. Recorded so neither is re-discovered at full cost.

- **Former H-1 — aligned-allocation API to lift `alignof(T) <= 8`
  (`mem_alloc_aligned`/`pool_new_aligned<T>`).** **REFUTED as work.** The
  technical description was accurate (`AllocHeader` is a fixed 8 bytes with no
  back-offset field, `pool_new<T>` hard-blocks at `memory.hpp:127-128`,
  `mem_free`'s `header_of` does a fixed unconditional `ptr - 1`,
  `memory.cpp:55-58`) but the finding is a verbatim re-discovery of the
  tracked open backlog item `HB-7` (`implementation-plan.md:610`), proposed as
  High-tier buildable work despite its own `consumer_status` being
  self-declared `speculative` — "no concrete consumer exists in-tree today"
  and "G-5 has no assigned chunk number". That is exactly what
  `extension-goals.md` §6 forbids. A follow-on cross-subsystem trace went
  further and found the backlog item's own stated rationale is wrong: of the
  three shortlisted G-5 scripting runtimes, **none need the alignment lift**
  — Lua 5.4's `lua_Alloc` and QuickJS-ng's `JSMallocFunctions` both carry a
  per-VM context pointer that bridges to `mem_alloc`/`mem_realloc`/`mem_free`
  today at ≤8-byte alignment; AngelScript's gap
  (`asSetGlobalMemoryFunctions` is global and context-less) is a per-VM
  *attribution* problem needing a TLS-routing shim, not an alignment lift.
  **Disposition**: `HB-7` stays open and un-built. Its backlog text should be
  amended (out of scope for this doc, which does not own
  `implementation-plan.md`) to record that it is not a G-5 gating fact. **If
  ever built**, the correct shape is additive and strictly separate from the
  existing 8-byte path: a *second* header carrying a back-offset field
  (`~16` bytes after alignment padding), and a *paired*, distinct free
  entry point (`mem_free_aligned`/`pool_delete_aligned<T>`, mirroring the
  existing P-7 dual-`operator delete` idiom) — never a growth of the shared
  `AllocHeader`, since all 11 real `pool_new<T>` sites in the tree
  (`cmd_cvar/context.cpp:26`; 7 filesystem backends; `file.cpp:341`;
  `save_buffer.cpp:245`; `game_host.cpp:217`) depend on the fixed offset
  unconditionally. This shape constraint is already recorded at
  `docs/architecture/memory/typed-helpers.md:30-35` for the operator-delete
  half; the header/back-offset half is not yet written down there. A separate
  cross-backlog dependency worth one line in `HB-7`'s own entry: `alignof(T)
  <= 8` also collides with `alignas(64)` false-sharing padding, so any future
  published-snapshot double-buffer primitive (`HB-5`) wanting cache-line
  separation between its two slots cannot be `pool_new`'d either — a second,
  more concrete requirer than G-5's speculative allocator hook.

- **Former M-2 — `snapshot_pools(std::span<PoolStats>)` alongside
  `for_each_pool`.** **REFUTED**, twice, in this campaign. Verified facts:
  `for_each_pool` (`memory.hpp:108`, `memory.cpp:337-351`) has **zero**
  production call sites anywhere in `src/`; every caller is a test
  (`tests/memory/test_memory.cpp:310,320,774`,
  `tests/cmd_cvar/test_memory_accounting.cpp:44`). The finding proposed
  building a **second** iteration API to relieve friction on a **first**
  iteration API that has zero production consumers of its own — the same
  §6 failure mode as former H-1, and the specific case the tree-wide
  subtraction lens singled out as "the textbook case". `for_each_pool` is
  named as a channel for the still-open `HB-6` ("name the one introspection
  layer channels", `implementation-plan.md:601`, `memory-boundary.md:307`).
  **Disposition**: do not delete `for_each_pool` either — it is 15 lines,
  named in two authoritative documents, and cheaper to keep than to delete
  and later reconstruct. **Do not add a sibling API.** When `HB-6` is
  eventually briefed, `for_each_pool` is *the* channel to extend or replace;
  no subsystem may ship a second API shape for a capability whose first
  shape has zero production consumers. This constraint generalises beyond
  memory and is recorded at the tree level as well as here.

______________________________________________________________________

## Open questions (carried from the boundary spec)

- **O-1 — `std::pmr::memory_resource` adapter.** Wrapping a pool as a
  `std::pmr::memory_resource` would let `std::pmr::string`/`std::pmr::vector`
  allocate from engine pools (Q-13 observability for STL) without the deferred
  `PoolAllocator<T>`. Adds `<memory_resource>` and template surface; revisit
  when a concrete STL-heavy consumer wants pool accounting. Ties to the Q-13
  `PoolAllocator<T>` migration trigger (> 500 KB of `memlist`-invisible STL, or
  profiler-visible fragmentation).
- **O-2 (new, 2026-07-20) — should `HB-7`'s backlog text be amended here or
  only at its own entry?** This report does not own
  `xash3dpp/docs/implementation-plan.md:610` or
  `xash3dpp/docs/design/scripting-runtime-brief.md`, both of which currently
  imply the alignment lift gates the G-5 allocator-hook decision. The
  correction (recorded above under "Investigated and refuted") should land in
  those documents, not be re-derived here on the next audit pass.
- **O-3 (new, 2026-07-20) — is `PoolConfig::reserve`'s deletion (L-2) worth
  bundling with anything else?** No other `PoolConfig` field is currently
  dead, so this is a single-field, single-commit change; flagged only in
  case a reviewer wants to fold it into the same commit as an unrelated
  `PoolConfig` touch rather than landing it alone.

______________________________________________________________________

## Cross-cutting flags (for the Phase 14 synthesis)

- **The aligned-alloc door (`HB-7`) is a *shared*, but currently
  under-motivated, backlog item.** It would still be the enabler for P-7
  over-aligned pool classes and any future cache-line-separated
  published-snapshot primitive (`HB-5`), and it recurs wherever a subsystem
  wants SIMD-aligned or cache-line-aligned pool storage — but it is **not**,
  as previously stated here, motivated by the G-5 scripting-runtime allocator
  hook (see "Investigated and refuted"). Correct the G-5 framing wherever
  `HB-7` is next referenced tree-wide.
- The strategy function-pointer triple (M-1) is the concrete anchor for the
  future `IAllocatorBackend` seam, correctly deferred behind Q-22's "no `I*`
  until a second real backend lands" rule. It is **not** required to bridge
  Lua or QuickJS-ng into the pools (both route through the existing
  `mem_alloc`/`mem_realloc`/`mem_free` free functions directly) — only a
  concrete `Arena`/`Slab` implementation motivates promoting it.
- **`threading-model.md`'s false "pool spinlock" claim (M-3)** is
  load-bearing for the tree's still-open off-main-allocation decision packet:
  every option that would move allocation off the main thread implicitly
  relies on that row being true. It is not a live bug only because nothing
  pool-allocates off-main today (the one production thread spawn,
  `sound/topology.cpp`, uses the system heap). Fix it before, not after, that
  decision is made.
