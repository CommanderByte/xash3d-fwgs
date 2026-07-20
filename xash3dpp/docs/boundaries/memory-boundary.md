# Memory Boundary Spec

> Refreshed 2026-07-06 (as-built pass). The original spec below is written from
> the **legacy `zone.c`** perspective (sentinels, small/big headers,
> `MEM_SMALL_ALLOC_OPT`, `poolchain` linked lists). The xash3dpp rewrite chose a
> **different design** — a *stats-facade allocator* — so much of the legacy
> detail is superseded by design rather than pending. The new
> `## As-built reconciliation (2026-07-06)`, `## Extension axes (Q-21)`
> sections and the refreshed Q-11 threading pointer are authoritative for the
> current code; prior legacy-flavoured prose is retained for parity reference
> and marked where superseded.

## Responsibility

The memory module provides a **pool-based, debug-instrumented allocator** layered
on top of the platform heap (`malloc`/`free`, or `SWAP_Malloc`/`SWAP_Free` on
custom-swap embedded targets). Callers create named pools and allocate from them;
every live allocation in a pool can be bulk-freed by emptying or destroying the
pool. The module adds sentinel bytes around every allocation, and — for "big"
allocations — records the source filename and line number at allocation time. It
does **not** manage virtual address space, NUMA topology, or raw OS allocations;
it is purely a heap bookkeeping layer.

______________________________________________________________________

## As-built reconciliation (2026-07-06)

The current implementation lives in a single translation unit
(`src/memory/memory.cpp`, `status_table.py` → **Complete**;
`compliance_scan.py` and `stub_scan.py` both **clean**) with three public
headers plus one private internals header. The rewrite did **not** port the
legacy debug-instrumented allocator; it implemented a leaner *stats-facade*
design and delegates corruption detection to ASan/MSan.

### Component map (as-built)

| File | Role |
|------|------|
| `include/xash3dpp/memory/memory.hpp` | Public API: `PoolHandle`, `AllocStrategy`, `PoolConfig`, lifecycle (`create_pool`/`destroy_pool`), `mem_alloc`/`mem_calloc`/`mem_realloc`/`mem_free`, `get_stats`/`pool_count`/`for_each_pool`/`set_oom_handler`, typed helpers (`pool_new`/`pool_delete`, `ScopedPool`, `PoolDeleter`, `pool_ptr`) |
| `include/xash3dpp/memory/stats.hpp` | `PoolStats` POD snapshot (name + live_bytes + total_allocs + total_frees) |
| `include/xash3dpp/private/memory/pool_registry.hpp` | Internals: 8-byte `AllocHeader` (`static_assert size==8, align==4`), `SlotState` enum, `PoolBucket`, `kMaxPools` |
| `src/memory/memory.cpp` | The only `.cpp`; system-allocator wrappers, the flat `g_pools[]` registry, the atomic `g_oom_handler` |

### Owned state (as-built — supersedes the legacy `poolchain`/`poolcount` table below)

| Symbol | Type | Notes |
|--------|------|-------|
| `g_pools[kMaxPools]` | `static PoolBucket[128]` | Fixed flat array — **no heap growth** (legacy grew `poolchain` with `realloc`). Zero-initialised statically; `kMaxPools = limits::memory_pool_max` |
| `g_oom_handler` | `static std::atomic<OomHandler>` | Null by default; set via `set_oom_handler` (release store) |
| per-bucket `live_bytes` / `total_allocs` / `total_frees` | `std::atomic<std::size_t>` | Relaxed counters — the `memlist`/stats surface |
| per-bucket `state` | `std::atomic<SlotState>` | `Free`→`Busy`→`Active`→`Free`; CAS slot claim |
| per-bucket `do_alloc`/`do_free`/`do_realloc`/`ctx` | function pointers | Strategy backend; only `System` (malloc/free/realloc) wired today |

### Allocator API drift (as-built vs the legacy `_Mem_*` Interface table)

- **Header design changed.** Every allocation carries an 8-byte `AllocHeader`
  (`pool_index:u32`, `payload_size:u32`). There are **no sentinels**, **no
  filename/line capture**, **no big/small header split**, and **no
  `MEM_SMALL_ALLOC_OPT`** — the legacy quirks section on those is *superseded by
  design* (ASan/MSan replace the sentinel/`_Mem_Check` machinery). `payload_size`
  being `uint32_t` caps a single allocation at `UINT32_MAX`; requests above that
  (and header-size overflow) return `nullptr` and fire the OOM handler.
- **No `_Mem_EmptyPool` / `_Mem_Check` / `Mem_IsAllocatedExt` / `Mem_PrintStats`
  console command** exist yet. Bulk-empty is not implementable for the `System`
  strategy without a per-pool allocation chain; it becomes natural only when
  `AllocStrategy::Arena` lands (bump-then-bulk-free). Introspection is served by
  `get_stats` / `pool_count` / `for_each_pool` instead of a `memlist` command.
- **`mem_realloc` migrates pools** like the legacy path, but via alloc+copy+free
  across pools (or native `realloc` on the same `System` pool) — there is no
  in-place chain relink because there is no chain.
- **`destroy_pool` asserts `live_bytes == 0`** (debug) rather than force-freeing;
  callers own lifetime. Slot is recycled — stale-handle aliasing risk unchanged
  from legacy (no generation counter yet; see Open questions #4).
- **Typed C++ surface added** with no legacy analog: `pool_new<T>` (with a
  binding `alignof(T) <= 8` `static_assert` — the pools guarantee only ≥8-byte
  payload alignment), `pool_delete`, `ScopedPool` (RAII pool), `PoolDeleter` /
  `pool_ptr` (the P-7 smart-pointer idiom).

### External-ABI touchpoints (as-built)

None of the four legacy plugin shims (`_Mem_*` in `ref_api_t`; pool-less
`pfnMemAlloc`/`pfnMemFree` in `render_api.h` / `menu_int.h` / `physint.h`) are
implemented yet — they are future fill-site work (Chunk 12/13 + game/physics
DLL bring-up). The **frozen constraint that already binds** is the handle width:
`PoolHandle::index` is `std::uint32_t`, matching the SDK `poolhandle_t`
(`common/xash3d_types.h`) embedded in `model_t`. Widening it is an ABI break.

### Threading

Fully documented in `docs/threading-analysis/memory-threading.md` (folder doc,
refreshed 2026-07-06). Summary: alloc/free/realloc and all counters are
atomic-backed and safe from any thread; `create_pool` is CAS-safe against
concurrent creators; `destroy_pool` while allocations are in flight through the
same handle remains a caller contract. `set_oom_handler` carries a
`compliance-allow(thread-assert)` annotation — memory sits **below platform**,
so it has no `ThreadRole` dependency and cannot call `assert_thread_role`.

______________________________________________________________________

## External ABI contracts

Memory functions are exposed to four external plugin surfaces. Each surface passes
raw function pointers into the loaded DLL and those signatures are frozen.

| Surface | Header | Exported symbols |
|---------|--------|-----------------|
| Renderer (new) | `engine/ref_api.h` → `ref_api_t` | `_Mem_AllocPool`, `_Mem_FreePool`, `_Mem_Alloc`, `_Mem_Realloc`, `_Mem_Free` |
| Renderer (legacy GoldSrc compat) | `common/render_api.h` | `pfnMemAlloc(size)`, `pfnMemFree(ptr)` — pool-less; engine binds to an internal pool |
| GameUI / Menu DLL | `engine/menu_int.h` | `pfnMemAlloc(size)`, `pfnMemFree(ptr)` — same pool-less pattern |
| Physics DLL | `engine/physint.h` | `pfnMemAlloc(size)`, `pfnMemFree(ptr)` — same pool-less pattern |

The game DLL (`eiface.h`) and client DLL (`cdll_int.h`) do **not** expose memory
functions; they allocate through the engine's own internal calls or through the
filesystem/model layer.

The `poolhandle_t` type (`uint32_t`) is part of the stable SDK (`common/xash3d_types.h`)
and is embedded inside `model_t` (a 32-bit field reused to hold the pool handle).
The 32-bit handle width is therefore a **fixed ABI constraint**.

______________________________________________________________________

## Interface (what the rest of the engine calls)

| Function / macro | Notes |
|------------------|-------|
| `Memory_Init()` | Reset global pool table; called on engine init and between subsystem resets |
| `_Mem_AllocPool(name, flags, file, line)→poolhandle_t` | Create a named pool; returns a 1-based index handle |
| `_Mem_FreePool(&handle, file, line)` | Empty the pool then mark its slot as free; zeroes `*handle` |
| `_Mem_EmptyPool(handle, file, line)` | Free all allocations in the pool but keep the pool alive |
| `_Mem_Alloc(handle, size, clear, file, line)→void*` | Allocate `size` bytes; optionally zero-fill |
| `_Mem_Realloc(handle, ptr, size, clear, file, line)→void*` | Resize (can migrate across pools; handles small→big promotion) |
| `_Mem_Free(ptr, file, line)` | Free a single allocation; dispatches on sentinel value |
| `Mem_IsAllocatedExt(handle, ptr)→qboolean` | Walk pool chain to verify ownership |
| `_Mem_Check(file, line)` | Validate sentinels of every live allocation in every pool |
| `Mem_PrintStats()` / `Mem_Stats_f()` | Console `memlist` command output |
| **Convenience macros** | `Mem_Malloc`, `Mem_Calloc`, `Mem_Realloc`, `Mem_Free`, `Mem_Free2`, `Mem_AllocPool`, `Mem_AllocPoolExt`, `Mem_FreePool`, `Mem_EmptyPool`, `Mem_IsAllocated`, `Mem_Check` (all inject `__FILE__`/`__LINE__`) |

______________________________________________________________________

## Dependencies (what this module calls)

| Dependency | Why |
|------------|-----|
| **Platform heap** (`malloc`/`free`/`realloc` or `SWAP_*`) | Backing allocator; all pool memory ultimately comes from here |
| `Sys_Error()` | Fatal error on double-free, sentinel corruption, OOM, or NULL pool |
| `Con_Printf()` | `memlist` diagnostic output |
| `Q_strncpy()`, `Q_memprint()` | String helpers for pool names and size display |
| `Cmd_Argc()` / `Cmd_Argv()` | `Mem_Stats_f` console command handler |

No subsystem other than the platform layer is required at init time.

______________________________________________________________________

## Owned state

| Variable | Type | Notes |
|----------|------|-------|
| `poolchain` | `mempool_t *` (dynamic array) | Grown with `realloc`; freed `Memory_Init` |
| `poolcount` | `size_t` | Number of slots currently allocated (freed slots have `filename == NULL`) |

No per-thread state. No mutexes (the legacy engine is single-threaded on the
main loop; filesystem/threading callbacks are the only off-thread allocators, and
they use the stub wrappers in `filesystem.c` that bypass the pool system entirely).

### Key pool handles owned by other subsystems at runtime

- `host.mempool` — "Zone Engine" (core engine)
- `cvar_pool` — console variables
- `cmd_pool` / `basecmd_pool` — command system
- `net_mempool` — networking
- `gameui.mempool` — GameUI DLL (engine side)
- `host.imagepool` / `host.soundpool` — imagelib / soundlib
- `model->mempool` — one pool per loaded model
- `svgame.mempool` / `svgame.stringspool` — server game
- `com_studiocache` — studio model extra data

______________________________________________________________________

## Quirks and invariants

- **`poolhandle_t` must stay 32-bit.** `model_t::cache_user` (and other engine
  structs) reuse a 32-bit field to store the pool handle. Widening to 64 bits
  would silently corrupt model data on LP64 systems — this was an intentional
  design choice documented in zone.c (`a1ba:` comment).

- **Two allocation header sizes.** Pools created with `MEM_SMALL_ALLOC_OPT`
  use a compact 1-byte-size header (`memheader_small_t`, max payload 255 bytes)
  with no filename tracking. Allocations larger than 255 bytes in such pools
  automatically use the big header. The rewrite must either preserve this
  distinction or ensure the `MEM_SMALL_ALLOC_OPT` flag can be dropped (it exists
  purely to reduce per-allocation overhead for high-frequency tiny allocs such
  as cvars and commands).

- **`_Mem_Free` detects header type from sentinel value** — the caller does not
  pass the pool; the header sentinel is read to pick big vs small path. This
  means a misidentified sentinel from heap corruption calls `Sys_Error`.

- **`_Mem_Realloc` can migrate between pools.** If `poolptr != mem->poolptr`,
  the allocation is unlinked from the old pool and re-linked into the new one in
  place (no memcpy). Small→big promotion (size exceeds 255 or target pool lacks
  `MEM_SMALL_ALLOC_OPT`) does a full copy+free.

- **`Memory_Init` reinitialises without freeing.** It calls `Q_free(poolchain)`
  and resets `poolchain = NULL, poolcount = 0`, dropping all existing pool
  metadata. It is called by the imagelib and soundlib reset macros in their own
  fuzz/unit-test entry points, meaning those subsystems expect a clean slate and
  do not need the allocator to respect existing live allocations.

- **Pool slot reuse.** When a pool is freed its slot (`poolchain[i].filename = NULL`)
  is reused on the next `_Mem_AllocPool` call. Pool handles are therefore not
  unique across lifetime — a stale handle to a freed pool will silently alias a
  new pool.

- **No thread safety.** The allocator has no locks. The filesystem uses stub
  wrappers (`Mem_AllocStub` etc.) that bypass the pool system entirely when loaded
  as a standalone DLL without the engine.

- **Sentinel constants are fixed.** `MEMHEADER_SENTINEL_BIG = 0xA1BA`,
  `MEMHEADER_SENTINEL_SMALL = 0xAD1E`, `MEMHEADER_SENTINEL2 = 0xDF`. These are
  read by `_Mem_Free` and `_Mem_Check`; any stored binary data that happens to end
  with `0xAD1E` will be misdetected as a small allocation, causing `Sys_Error`.

- **`XASH_CUSTOM_SWAP` platform variant.** Certain embedded targets (e.g. Sony
  PSP via the `swap` platform) have no OS heap and use a custom sbrk-based
  allocator (`engine/platform/misc/kmalloc.c`). The zone allocator routes all
  backing allocations through `SWAP_Malloc`/`SWAP_Free` in that case. The rewrite
  must preserve this abstraction point.

______________________________________________________________________

## Open questions

1. **Should the rewrite wrap `std::pmr::memory_resource`?** C++17 PMR provides
   a standardised pool interface. Using it would allow `std::pmr::string` etc. to
   allocate from engine pools without wrappers — but adds template complexity and
   requires `<memory_resource>`.

1. **Thread safety.** The legacy allocator has none. Should the new module be
   lock-free (e.g. per-thread pool arenas) or use a lightweight mutex? The
   filesystem module is the main thread-safety concern today.

1. **Drop `MEM_SMALL_ALLOC_OPT`?** The compact header exists to save ~24 bytes
   per small allocation. With modern allocators (tcmalloc, jemalloc, or even
   a slab allocator) the pool bookkeeping overhead can be eliminated entirely.
   Decision needed before designing the pool API.

1. **Stale handle safety.** Pool slot reuse means a stale `poolhandle_t` may
   silently alias a live pool. The rewrite should decide whether to use
   generation counters or a different handle scheme.

1. **`XASH_CUSTOM_SWAP` port.** Does the rewrite need to support platforms that
   lack a heap allocator? If yes, the backing allocator must be injectable. If
   no, the platform variant can be dropped.

1. **`pfnMemAlloc` pool-less surface.** The legacy/GoldSrc renderer API,
   GameUI, and physics interfaces use a two-function `(alloc, free)` pair with
   no pool parameter. The engine side hardcodes allocating from a single fixed
   pool per plugin. The rewrite should decide whether to keep this pattern or
   require plugins to manage their own pools.

______________________________________________________________________

## Fixed Limits (xash3dpp rewrite)

All configurable limits for the rewrite's memory subsystem live in
`xash3dpp/include/xash3dpp/limits.hpp` under the `// memory subsystem` block.

| Constant | Default | Override macro | Notes |
|----------|---------|---------------|-------|
| `memory_pool_max` | `128` | `XASH_LIMIT_MEMORY_POOL_MAX` | Max simultaneously active named pools |
| `memory_pool_name_len` | `64` | `XASH_LIMIT_MEMORY_POOL_NAME_LEN` | Max bytes in a `PoolBucket::name` buffer (including null terminator) |

______________________________________________________________________

## Q-11 satellite verdict

Not a satellite candidate: one concrete allocator implementation serving every
consumer (Q-11 score 0). The `AllocStrategy` enum tags future backends
(Arena/Slab) as data, not as interface seams — per the Q-22 seam rule no `I*`
interface exists until a second real backend lands. Threading is documented in
`docs/threading-analysis/memory-threading.md` (folder doc) and via the
`@thread-safety:` annotations in `memory.hpp`.

______________________________________________________________________

## Role & parity

- **Role:** role-neutral substrate — the pool allocator beneath every role. No
  cross-role parity obligation.

## Extension axes (Q-21)

> Added 2026-07-06 (as-built pass); axis set completed 2026-07-19
> (consolidation audit). Evaluated against
> `docs/design/extension-goals.md` (G-1..G-5, P-1..P-8). Memory is unusual: it
> is not itself an extension target but the **substrate** the primitives sit on
> — P-6/P-7 heap-ownership and the Q-13 pool-accounting policy are *defined by
> this subsystem*, and G-5's scripting allocator hook plugs directly into its
> strategy seam. The door-keep verdicts therefore lean "provider" rather than
> "consumer."

| Goal / primitive | Applies? | Verdict / door-keep |
|------------------|----------|---------------------|
| **P-7** pool-owned classes + RAII | **Yes — provider/headline** | Memory *is* the canonical idiom's home: `pool_new<T>`, `PoolDeleter`, `pool_ptr`, `ScopedPool` (extension-goals P-7 cites `memory.hpp` `PoolDeleter` by name). Binding constraint to preserve: `pool_new<T>` requires `alignof(T) ≤ 8` — do not silently widen `AllocHeader`; add an explicit aligned-alloc API instead (modernization H-1). Class-scoped `operator new` stays forbidden (cannot carry the injected handle). |
| **Q-13** ALLOC_POLICY / `memlist` observability | **Yes — provider** | `get_stats` / `for_each_pool` / `pool_count` + the atomic `live_bytes`/`total_allocs`/`total_frees` counters are the accounting the pre-reserve discipline and the future `PoolAllocator<T>` migration trigger measure against. Keep the counter surface stable; it is the memory tier of the three-tier stats model (P-4). |
| **P-6** services are satellites (heap ownership) | **Door-keep** | Satellites (`xash3dpp_mcp`, `xash3dpp_script`, debug services) consume memory as a public seam and account per pool; memory never links toward them. `AllocStrategy::Arena` is the placeholder for the per-thread bump/arena primitive those services (and per-frame scratch) will want — data-tagged today (Q-22: no `I*` until a second backend). |
| **P-4** typed introspection | **Yes — provider** | `PoolStats` + `for_each_pool` are the typed memory-introspection surface G-1 (MCP memory reports), G-3 (debug-thread memory dumps) and G-4 (overlays) read. Door-keep: new needs add a typed query here, never an `extern` poke into `g_pools`. A snapshot-range API is a modernization candidate (M-2). |
| **G-5** scripting allocator hook | **Yes — direct door** | Extension-goals §G-5 requires the runtime expose "a complete allocator hook bridgeable to the memory pools (accounting per pool at minimum; ≥8-byte alignment, no aligned-alloc API)." The per-pool `do_alloc`/`do_free`/`do_realloc`/`ctx` function-pointer seam **is** that bridge point (a script VM's `lua_Alloc`/`js_malloc` maps onto a `PoolConfig` strategy with its own `ctx`). Keep the strategy triple injectable. |
| **G-3** dedicated debug thread | **Yes — already open** | Off-main memory reads are already safe (atomic counters); the only caller contract is that `set_oom_handler` is init-only (threading doc §Required caller contracts). No retrofit needed. |
| **G-2** game ABI v2 | **Door-keep** | The frozen 32-bit `poolhandle_t` and the future pool-less `pfnMemAlloc` plugin shims are the ABI-confinement point; a v2 ABI may hand a richer allocator context but must still be able to produce a raw `poolhandle_t` at the edge. Do not widen the handle. |
| **P-1** main-thread inbox / **P-2** snapshots / **P-3** context-first / **P-5** narrowest-state | Mostly N/A | Memory is already context-light (free functions over an explicit `PoolHandle`). Its file-scope state (`g_pools`, `g_oom_handler`) is the **documented allocator-registry exception** to P-3: a single global registry is forced by the 32-bit `poolhandle_t` ABI (the handle indexes a process-global table). List it in the module-statics table with that justification; it must not grow. |
| **G-1** in-engine MCP service | Consumer via P-4 | MCP memory reports read `get_stats`/`for_each_pool`/`PoolStats` — the already-typed surface the P-4 row guards (HB-6 names memory as one of the introspection channels). No memory-side service code. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Memory overlays consume the same typed pool census; the P-4 door rule (typed query, never an `extern` poke into `g_pools`) is the whole G-4 obligation. |
| **P-8** annotation discipline | **Yes — satisfied (denominatored)** | 2026-07-19 `annotation-coverage` scan: all marker classes at 100% for memory; the `set_oom_handler` init-only caller contract carries its recorded `compliance-allow`. Keep coverage statements denominatored. |

______________________________________________________________________
