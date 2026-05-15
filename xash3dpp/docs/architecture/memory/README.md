# memory — Architecture Overview

> **Source**: `xash3dpp/src/memory/`  
> **Public API**: `xash3dpp/include/xash3dpp/memory/`  
> **Legacy reference**: `engine/common/zone.c` (DarkPlaces-derived pool allocator)

## Purpose

The memory module is a **stats-facade allocator**: named pools are accounting
buckets over the platform heap (`malloc`/`free`). Every allocation is prefixed
with an 8-byte `AllocHeader` that records the pool index and payload size, so
`mem_free()` can locate the owning bucket without any caller-side bookkeeping.

It does **not** manage virtual address space, NUMA topology, memory mapping, or
raw OS allocations. It does **not** implement sentinels, linked allocation lists,
or filename/line tracking — correctness checking is delegated to AddressSanitizer
and MemorySanitizer.

## Design goals

- **Zero bookkeeping overhead beyond the 8-byte header**: no linked lists, no
  sentinels, no per-file/line metadata.
- **Thread-safe counters without global locks**: per-pool `live_bytes`,
  `total_allocs`, and `total_frees` are `std::atomic<std::size_t>` updated with
  `memory_order_relaxed`.
- **Lock-free slot claim**: `create_pool` uses a CAS loop on a `SlotState` atomic
  to claim a registry slot, eliminating TOCTOU races on concurrent pool creation.
- **Acquire/release ordering for function pointers**: `PoolBucket::do_alloc`,
  `do_free`, `do_realloc`, and `ctx` are written during the `kBusy` phase and
  published via a `release` store of `kActive`; readers `acquire`-load `state`
  before using the pointers, providing the happens-before chain.
- **Pluggable backing strategy**: `PoolBucket` stores `do_alloc`/`do_free`/
  `do_realloc` function pointers so future `kArena` and `kSlab` strategies can be
  added without touching call sites. Today only `kSystem` (malloc/free) is
  implemented.
- **No exceptions, no RTTI**: compiled with `/EHs-c-` and `/GR-`. All failure
  paths return `nullptr` or `kNullPool`.
- **Compatible `PoolHandle` width**: `PoolHandle::index` is `uint32_t` —
  compatible with the legacy `poolhandle_t` embedded in `model_t` and exposed
  through `ref_api_t` / `physint_t`.

## Key invariants

- Pool lifecycle (`create_pool`, `destroy_pool`) is **not** thread-safe with
  respect to other lifecycle calls. These must be serialised by the caller (e.g.
  called only from the main thread before/after worker threads run).
- `destroy_pool` must be called only after **all** allocations from the pool have
  been freed. In debug builds, `assert(live_bytes == 0)` fires otherwise.
- `mem_free(ptr)` reads the 8-byte header embedded before `ptr` to determine the
  pool. A pointer not allocated through this subsystem (or with a corrupted
  header) produces undefined behaviour.
- `mem_alloc`, `mem_free`, and `mem_realloc` **may** be called concurrently from
  multiple threads for the same pool, provided the pool is active for the duration
  of all calls.
- The `PoolHandle` value space is 1-based; index 0 (`kNullPool`) is always invalid.
  A null-pool allocation still succeeds (it calls `malloc` untracked) so the
  `kNullPool` sentinel is useful for "unowned" allocations.
- The registry holds at most `kMaxPools` = 128 simultaneous active pools.
  `create_pool` returns `kNullPool` when the registry is full.

## Relationship to legacy code

The legacy `engine/common/zone.c` uses a doubly-linked allocation chain per pool,
sentinel bytes (`0xA1BA`, `0xAD1E`) for corruption detection, and optional
per-allocation source-file/line tracking. Key differences in the rewrite:

- Linked chains replaced by flat counter atomics — O(1) alloc/free vs. O(1) with
  pointer patching; no walk needed at `mem_free`.
- Sentinels removed — rely on ASan/MSan for corruption detection.
- File/line tracking removed from the fast path.
- `bool active` replaced by `std::atomic<SlotState>` with a CAS protocol.
- The OOM handler is an `std::atomic<OomHandler>` rather than a plain pointer.
- `PoolHandle` is a typed struct rather than a bare `uint32_t`; the 32-bit index
  is preserved for ABI compatibility with legacy renderer/physics plugin surfaces.

## Architecture at a glance

```
 ┌─────────────────────────────────────────────────────────┐
 │  Callers (engine subsystems, filesystem, tests)         │
 │  create_pool / destroy_pool                             │
 │  mem_alloc / mem_calloc / mem_realloc / mem_free        │
 │  pool_new<T> / pool_delete<T> / ScopedPool             │
 └─────────────────────────┬───────────────────────────────┘
                           │
 ┌─────────────────────────▼───────────────────────────────┐
 │  g_pools[kMaxPools]  (file-scope static PoolBucket[])   │
 │                                                         │
 │  PoolBucket {                                           │
 │    name[64]                                             │
 │    atomic<size_t> live_bytes                            │
 │    atomic<size_t> total_allocs                          │
 │    atomic<size_t> total_frees                           │
 │    atomic<SlotState> state  ← CAS claim/release         │
 │    do_alloc / do_free / do_realloc / ctx                │
 │  }                                                      │
 └─────────────────────────┬───────────────────────────────┘
                           │ do_alloc / do_free / do_realloc
 ┌─────────────────────────▼───────────────────────────────┐
 │  Platform heap                                          │
 │    kSystem: malloc / realloc / free                     │
 │    kArena:  (future — bump allocator)                   │
 │    kSlab:   (future — fixed-size slab)                  │
 └─────────────────────────────────────────────────────────┘

 Every allocation:
   [ AllocHeader (8 bytes) | payload (n bytes) ]
     pool_index (u32) + payload_size (u32)
```

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [pool-registry.md](./pool-registry.md) — `PoolBucket`, `SlotState`, lifecycle, CAS protocol, `AllocHeader`
- [allocation-api.md](./allocation-api.md) — `mem_alloc`, `mem_calloc`, `mem_realloc`, `mem_free`, OOM handler
- [stats-api.md](./stats-api.md) — `PoolStats`, `get_stats`, `pool_count`, `for_each_pool`
- [typed-helpers.md](./typed-helpers.md) — `pool_new`, `pool_delete`, `ScopedPool`, `PoolDeleter`, `pool_ptr`
