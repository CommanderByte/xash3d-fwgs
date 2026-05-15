# Memory Boundary Spec

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
