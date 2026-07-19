# Memory Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
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

What remains is therefore short and mostly **feature-completeness** rather than
C-style-code cleanup. The one genuinely load-bearing item is the aligned-alloc
ceiling (`pool_new<T>` hard-`static_assert`s `alignof(T) <= 8`), which
extension-goals §G-5 (scripting allocator hook) and P-7 both call out. The
remaining items are a typed introspection convenience, a deferred backend-seam
promotion, and cosmetic tidy-ups. Several "C-looking" shapes here are
**deliberately** C-shaped (the strategy function-pointer triple and the
`for_each_pool` callback both avoid heap and keep the layer boundary-friendly and
`noexcept`); they are noted as *do-not-touch-yet* where relevant.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| `System` strategy (malloc/free/realloc) | **Implemented** | The only wired backend |
| `AllocStrategy::Arena` / `Slab` | **Placeholder** | Enum tags; `create_pool` falls through to `System`. Q-22: no `I*` seam until a second real backend lands |
| OOM handler (`set_oom_handler`) | **Implemented** | Atomic; init-only caller contract |
| Stats surface (`get_stats`/`pool_count`/`for_each_pool`) | **Implemented** | The Q-13 `memlist`/P-4 accounting tier |
| Typed helpers (`pool_new`/`pool_delete`/`ScopedPool`/`pool_ptr`) | **Implemented** | P-7 canonical idiom |
| Aligned allocation (`alignof(T) > 8`) | **Not implemented** | Hard `static_assert` blocks over-aligned `pool_new<T>` — **H-1** |
| Bulk-empty (`empty_pool`) / validation (`check`) / `is_allocated` | **Not implemented** | Legacy `_Mem_EmptyPool`/`_Mem_Check`/`Mem_IsAllocatedExt` — natural only once `Arena` lands / delegated to ASan. Not a modernization gap; recorded as scope note |
| `std::pmr::memory_resource` adapter | **Not implemented** | Open question in boundary spec — **O-1** |
| Legacy plugin ABI shims | **Not implemented** | Future fill-site (Chunk 12/13 + DLL bring-up) |

______________________________________________________________________

## High-priority opportunities

### H-1: Aligned-allocation API to lift the `alignof(T) <= 8` ceiling

- **File(s)**: `xash3dpp/include/xash3dpp/memory/memory.hpp` (`pool_new<T>`
  `static_assert`); `xash3dpp/src/memory/memory.cpp` (`mem_alloc` /
  `AllocHeader` payload alignment); `pool_registry.hpp` (`AllocHeader`,
  `static_assert(alignof(AllocHeader) == 4)`).

- **Current pattern**: `AllocHeader` is 8 bytes / 4-byte aligned and the payload
  begins immediately after it, so the returned pointer has only ≥8-byte
  alignment. `pool_new<T>` guards this with a hard compile error:

  ```cpp
  static_assert(alignof(T) <= 8,
      "pool_new<T>: alignof(T) > 8 unsupported — AllocHeader guarantees 8-byte payload alignment (Q-22)");
  ```

  Any over-aligned type (SIMD `__m128`/`__m256`, cache-line-aligned lock-free
  nodes, some `std::atomic` aggregates) cannot be pool-allocated at all.

- **Suggested replacement**: Add an `aligned` allocation path —
  e.g. `void* mem_alloc_aligned(PoolHandle, std::size_t size, std::size_t align)`
  that over-allocates, stores the header at a back-computed offset, and records
  enough in `AllocHeader` (or a variant) for `mem_free` to recover the raw
  pointer. Then relax `pool_new<T>` to route over-aligned `T` through it. Requires
  the matching aligned `operator delete` pair for P-7 classes.

- **Boundary-safe**: Yes — additive; `AllocHeader` layout for the existing
  8-byte path is unchanged (do not move the `pool_index`/`payload_size` fields).

- **Rationale**: This is the single constraint extension-goals names twice —
  §G-5 ("pools guarantee only ≥8-byte payload alignment — no aligned-alloc API")
  and P-7 ("`pool_new<T>` requires `alignof(T) ≤ 8`"). Building it unblocks
  over-aligned pool-owned classes and the scripting-runtime allocator bridge
  without forcing those consumers onto raw `::operator new` (which would be
  `memlist`-invisible, violating Q-13). Rank High as a *door-opening* item, not
  because current code is wrong.

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
  `PoolConfig` strategy + its own `ctx`).

### M-2: A snapshot-range introspection API alongside the `for_each_pool` C callback

- **File(s)**: `xash3dpp/include/xash3dpp/memory/memory.hpp`
  (`for_each_pool(void(*)(PoolStats, void*), void*)`).

- **Current pattern**: Iteration is a C-style callback + `void* userdata`:

  ```cpp
  void for_each_pool(void (*fn)(PoolStats, void*), void* userdata) noexcept;
  ```

  Callers that just want a list must thread a capturing lambda's state through
  `userdata` by hand.

- **Suggested replacement**: Keep the `noexcept` callback (it is heap-free and
  boundary-safe), but **add** a convenience that fills a caller-supplied
  `std::span<PoolStats>` and returns the count written — e.g.
  `std::size_t snapshot_pools(std::span<PoolStats> out) noexcept`. The bound is
  `limits::memory_pool_max`, so callers can stack-allocate
  `std::array<PoolStats, limits::memory_pool_max>` with no heap use (Q-13-clean).

- **Boundary-safe**: Yes — additive, no allocation.

- **Rationale**: P-4 names `for_each_pool`-class surfaces as the memory tier the
  MCP service (G-1), debug thread (G-3) and overlays (G-4) read. A span-fill
  snapshot is friendlier to those typed frontends than a raw callback while
  staying allocation-free.

______________________________________________________________________

## Low-priority opportunities

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
  ordering — do **not** add a `utilities` link just for this.

### L-2: Currently-ignored `PoolConfig::reserve` field

- **File(s)**: `xash3dpp/include/xash3dpp/memory/memory.hpp` (`PoolConfig`).

- **Current pattern**: `std::size_t reserve { 0 }; // pre-allocation hint; currently ignored`.

- **Suggested replacement**: Either wire it into the `Arena` backend when H-1/M-1
  land (a reserve hint is meaningful for a bump allocator), or drop it until then
  to avoid an inert public field. Leave as-is if `Arena` is imminent.

- **Boundary-safe**: Yes.

- **Rationale**: An ignored public field is mild API noise; low urgency because
  it is documented as a forward-looking hint.

______________________________________________________________________

## Open questions (carried from the boundary spec)

- **O-1 — `std::pmr::memory_resource` adapter.** Wrapping a pool as a
  `std::pmr::memory_resource` would let `std::pmr::string`/`std::pmr::vector`
  allocate from engine pools (Q-13 observability for STL) without the deferred
  `PoolAllocator<T>`. Adds `<memory_resource>` and template surface; revisit
  when a concrete STL-heavy consumer wants pool accounting. Ties to the Q-13
  `PoolAllocator<T>` migration trigger (> 500 KB of `memlist`-invisible STL, or
  profiler-visible fragmentation).

______________________________________________________________________

## Cross-cutting flags (for the Phase 14 synthesis)

- **H-1 (aligned alloc)** is a *shared* door: it is the enabler for both P-7
  over-aligned pool classes and the G-5 scripting allocator hook, and it recurs
  wherever a subsystem wants SIMD-aligned or cache-line-aligned pool storage.
  Worth a single design brief rather than per-subsystem workarounds.
- The strategy function-pointer triple (M-1) is the concrete anchor for the
  future `IAllocatorBackend` seam **and** the G-5 allocator bridge — both live
  here, so both should be designed together at Arena time.
