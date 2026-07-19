# Deep Dive: Legacy `zone.c` — Pool Allocator Structs, Sentinels, Tags & Quirks

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy DarkPlaces-derived zone/pool
allocator at the repository root (`engine/common/zone.c`, `engine/common/common.h`,
`engine/ref_api.h`) that the `xash3dpp` `memory` subsystem replaces. Wide
survey coverage lives in `engine-common-and-platform.md`; this is the
narrow-and-exact companion. Line numbers are against the working tree on that
date; behaviour references, not design constraints. Everything here is
**legacy** — read for behaviour and the frozen `poolhandle_t` ABI contract, not
as a rewrite blueprint (the rewrite chose a different, leaner design — see §7).*

Primary sources:

- `engine/common/zone.c` — the allocator (all functions below)
- `engine/common/common.h` — `Mem_*` macros, `MEM_SMALL_ALLOC_OPT`, prototypes
- `engine/platform/swap/swap.h` + `engine/platform/misc/kmalloc.c` —
  `XASH_CUSTOM_SWAP` backing allocator (PSP-class targets)
- `common/xash3d_types.h` — `poolhandle_t` (the frozen 32-bit handle)

**Global assumptions:** the legacy engine is single-threaded at the main-loop
level; the allocator has **no locks**. All sizes are host-native; there is no
`#pragma pack`. `poolhandle_t` is a 32-bit handle (not a pointer) on purpose
(§3.1). `STATIC_CHECK_SIZEOF` pins the two header sizes on ILP32 and LP64.

______________________________________________________________________

## 1. Constants and tags (`zone.c:21-25`, `common.h:375`)

```c
#define MEMHEADER_SENTINEL_BIG   0xA1BAU  // uint16 sentinel1 in the big header
#define MEMHEADER_SENTINEL_SMALL 0xAD1EU  // uint16 sentinel1 in the compact header
#define MEMHEADER_SENTINEL2      0xDFU    // single trailing guard byte after payload
#define MEM_SMALL_MAX            UINT8_MAX // 255 — compact-header payload ceiling

#define MEM_SMALL_ALLOC_OPT (1U<<0) // pool flag: use the compact header for allocs <= 255 bytes
```

`0xA1BA` is "a1ba", the maintainer's handle (see §3.1). The trailing
`MEMHEADER_SENTINEL2` byte is written **one past the payload** in every
allocation (both header flavours), so every block is `header + size + 1` bytes.

______________________________________________________________________

## 2. Struct layouts

### 2.1 Big header — `memheader_t` (`zone.c:57-66`)

Full debug info; used for every allocation in a pool **without**
`MEM_SMALL_ALLOC_OPT`, and for allocations `> 255` bytes in pools that opted in.

```c
typedef struct memheader_s
{
    struct memheader_s *next, *prev; // intrusive doubly-linked chain within the pool
    const char         *filename;    // __FILE__ of the alloc site
    size_t              size;         // payload size (excl. header and sentinel2)
    poolhandle_t        poolptr;      // owning pool handle (1-based)
    uint16_t            fileline;     // __LINE__ of the alloc site
    uint16_t            sentinel1;    // == MEMHEADER_SENTINEL_BIG
    // immediately followed by <size> payload bytes, then one MEMHEADER_SENTINEL2 byte
} memheader_t;
STATIC_CHECK_SIZEOF( memheader_t, 24, 40 );   // ILP32, LP64
```

### 2.2 Compact header — `memheader_small_t` (`zone.c:76-83`)

No filename/line; `size` is a single byte. Only used in `MEM_SMALL_ALLOC_OPT`
pools for allocations ≤ 255 bytes.

```c
typedef struct memheader_small_s
{
    struct memheader_small_s *next, *prev;
    poolhandle_t              poolptr;
    uint8_t                   size;
    uint8_t                   pad;
    uint16_t                  sentinel1;  // == MEMHEADER_SENTINEL_SMALL
} memheader_small_t;
STATIC_CHECK_SIZEOF( memheader_small_t, 16, 24 );
```

### 2.3 Pool — `mempool_t` (`zone.c:85-96`)

```c
typedef struct mempool_s
{
    memheader_t       *chain;         // big-allocation chain head
    memheader_small_t *chain_small;   // compact-allocation chain head
    size_t             totalsize;     // sum of payload sizes
    size_t             realsize;      // sum of actual malloc sizes (incl. headers/sentinel/mempool_t)
    size_t             lastchecksize; // memlist delta baseline
    const char        *filename;      // AllocPool site; NULL marks a free slot
    int                fileline;
    unsigned int       flags;         // MEM_SMALL_ALLOC_OPT, ...
    char               name[64];      // pool name (Q_strncpy, always null-terminated)
} mempool_t;

static mempool_t *poolchain = NULL;   // grown with realloc; "critical stuff"
static size_t     poolcount = 0;      // number of slots ever allocated (incl. freed)
```

A **free slot** is one whose `filename == NULL`; `_Mem_AllocPool` reuses the
first such slot before growing `poolchain`.

______________________________________________________________________

## 3. Handle scheme and quirks

### 3.1 Why `poolhandle_t` is 32-bit (`zone.c:107-109`)

> `// a1ba: due to mempool being passed with the model through reused 32-bit
> field which makes engine incompatible with 64-bit pointers I changed mempool
> type from pointer to 32-bit handle, thankfully mempool structure is private`

`model_t` (and other engine structs) reuse a 32-bit field to carry a pool
handle. The handle is a **1-based index** into `poolchain`
(`Mem_PoolIndex = (pool - poolchain) + 1`; `Mem_FindPool` reverses it). Handle
`0` is the null/invalid sentinel. **This 32-bit width is the frozen ABI
constraint the rewrite must keep.**

### 3.2 Slot reuse aliasing

A freed pool's slot is recycled on the next `_Mem_AllocPool`, so a stale
`poolhandle_t` to a freed pool silently aliases a *new* pool. There is no
generation counter. (The rewrite inherits this; noted as an open question.)

### 3.3 `Mem_FindPool` is fatal on a bad handle

`Mem_FindPool` calls `Sys_Error("not allocated or double freed pool %d")` for
any handle outside `1..poolcount` — a bad handle is unrecoverable, not a
returned error.

______________________________________________________________________

## 4. Algorithms (exact behaviour)

### 4.1 Allocation — `_Mem_Alloc` (`zone.c:239`)

- `size <= 0` → `NULL`; `poolptr == 0` → `Sys_Error`.
- If `MEM_SMALL_ALLOC_OPT` set **and** `size <= 255`: allocate
  `sizeof(memheader_small_t) + size + 1`, init compact header, link into
  `chain_small`.
- Else: allocate `sizeof(memheader_t) + size + 1`, capture `filename`/`fileline`,
  init big header, link into `chain`.
- Both write `MEMHEADER_SENTINEL2` one byte past the payload and, if `clear`,
  `memset` the payload to 0.
- `Q_malloc` failure → `Sys_Error("out of memory …")` (never returns `NULL` to
  the caller for a real OOM).

### 4.2 Free — `_Mem_Free` (`zone.c:340`) — sentinel-dispatched

The caller passes **only** the payload pointer, no pool. `Mem_ReadSentinel(data)`
reads the `uint16` immediately before the payload (index `-1` of
`(const uint16 *)data`) and compares it to `MEMHEADER_SENTINEL_SMALL`: match → treat as compact header, else
→ big header. Each path re-validates `sentinel1` + trailing `sentinel2` (fatal
`Sys_Error` on mismatch), checks chain consistency (double-free detection via
`prev->next`/`next->prev`), subtracts pool totals, unlinks, and `Q_free`s.

> **Quirk**: stored binary data whose last two payload-preceding bytes happen to
> equal `0xAD1E` would be misclassified — but since the sentinel is *before* the
> payload (in the header), that only fires under real heap corruption.

### 4.3 Realloc — `_Mem_Realloc` (`zone.c:379`)

- `size <= 0` → returns `data` unchanged (no free). `data == NULL` → delegates to
  `_Mem_Alloc`.
- Sentinel-dispatched like free. **Cross-pool migration**: if
  `mem->poolptr != poolptr`, the block is unlinked from the old pool and relinked
  into the new one **in place** (`Mem_MigratePool{Big,Small}`, no memcpy) and
  totals move with it.
- **Small→big promotion**: in a compact block, if the new `size > 255` **or** the
  target pool lacks `MEM_SMALL_ALLOC_OPT`, it does a full alloc+copy+free into a
  big block (the only realloc path that copies).
- Otherwise `Q_realloc` in place; if the block moved, the neighbours'
  `prev`/`next` (or the pool chain head) are fixed up. Growth zero-fills the new
  tail when `clear`.

### 4.4 Pool lifecycle

- `_Mem_AllocPool` (`zone.c:379`-ish → `Mem_InitPool`): reuse first `filename==NULL`
  slot else `Q_realloc` `poolchain` by one; `Mem_InitPool` zeroes the slot, sets
  `realsize = sizeof(mempool_t)`, copies the name.
- `_Mem_EmptyPool`: `Mem_FreeBlock*` every node in both chains, pool stays alive.
- `_Mem_FreePool(&handle)`: empties, then `memset(pool, 0xBF, …)` (poison),
  re-clears the chain heads + `filename = NULL` (marks slot free), zeroes
  `*handle`. Already-freed pool → `Sys_Error`.

### 4.5 Validation & introspection

- `_Mem_Check`: walks every pool's big and small chains, re-checking both
  sentinels of every live block (`Sys_Error` on any breach).
- `Mem_IsAllocatedExt` → `Mem_CheckAlloc`: linear chain walk to confirm a pointer
  belongs to a pool (or any pool if handle is 0). O(allocations).
- `Mem_PrintStats` / `Mem_PrintList` (the `memlist` command): per-pool
  `totalsize`/`realsize` with a `lastchecksize` delta; the list variant prints
  each big alloc's `filename:fileline`. Both call `Mem_Check` first.

______________________________________________________________________

## 5. `XASH_CUSTOM_SWAP` backing allocator (`zone.c:27-53`)

On targets with no OS heap (PSP-class, the `swap` platform), `Q_malloc`/`Q_free`
are `#define`d to `SWAP_Malloc`/`SWAP_Free` (sbrk-style, `kmalloc.c`) and a local
`Q_realloc` is synthesised (alloc-new + `memcpy` + free-old; note it copies
`size` bytes, i.e. the *new* size — benign because callers only shrink/grow
tracked blocks). All pool backing memory routes through this abstraction — the
rewrite preserves it as the injectable strategy seam.

______________________________________________________________________

## 6. Quirk / parity checklist (for any future reimplementation)

1. `poolhandle_t` is a **1-based 32-bit index**, `0` = null; width is frozen ABI.
2. Two header flavours; compact only under `MEM_SMALL_ALLOC_OPT` and `size ≤ 255`.
3. `MEMHEADER_SENTINEL_BIG 0xA1BA`, `_SMALL 0xAD1E`, `SENTINEL2 0xDF` (one trailing byte).
4. `_Mem_Free`/`_Mem_Realloc` are **sentinel-dispatched** — no pool argument.
5. Realloc migrates pools in place; small→big promotion is the only copying path.
6. `_Mem_FreePool` poisons the slot with `0xBF` then marks `filename = NULL`.
7. OOM is **fatal** (`Sys_Error`), never a `NULL` return to the caller.
8. Slot reuse ⇒ stale-handle aliasing; no generation counter.
9. No locks — single-threaded main-loop assumption.
10. `Memory_Init` (`common.h:365`) frees `poolchain` and resets to
    `NULL`/`0`, dropping all pool metadata without honouring live allocations.

______________________________________________________________________

## 7. As-built mapping (legacy → `xash3dpp/memory`)

The rewrite is a **stats-facade allocator**, not a port. It kept the frozen
handle scheme and the per-pool accounting idea, and dropped the sentinel /
filename / linked-chain / small-header machinery (ASan/MSan replace the debug
guards).

| Legacy piece | `xash3dpp` home | Notes / status |
|---|---|---|
| `poolhandle_t` (1-based 32-bit index) | `memory::PoolHandle { uint32_t index }` (`memory.hpp`) | **Kept** — frozen ABI; `k_null_pool` == index 0; `valid()`/`operator bool`/`==` |
| `poolchain` (realloc-grown array) + `poolcount` | `static PoolBucket g_pools[kMaxPools]` (`memory.cpp`) | **Flat fixed array**, no heap growth; `kMaxPools = limits::memory_pool_max` (128) |
| `mempool_t` | `internal::PoolBucket` (`pool_registry.hpp`) | `name[64]` kept; totals → atomic `live_bytes`/`total_allocs`/`total_frees`; `flags`/chains/`filename` **dropped**; adds atomic `SlotState` + strategy function pointers |
| `memheader_t` / `memheader_small_t` (24/40, 16/24 B; sentinels; filename/line; chain ptrs) | `internal::AllocHeader` (8 B: `pool_index`,`payload_size`) | **Redesigned** — single 8-byte header, no sentinels, no filename/line, no chains, no small/big split |
| `MEM_SMALL_ALLOC_OPT` | *dropped* | Superseded by design; one header flavour |
| `_Mem_AllocPool(name, flags, file, line)` | `create_pool(name, PoolConfig)` | Returns `k_null_pool` when full (128) instead of growing; `PoolConfig.strategy` replaces `flags` |
| `_Mem_FreePool(&h)` | `destroy_pool(h)` | Asserts `live_bytes == 0` (debug) instead of force-empty; recycles slot via atomic `SlotState` |
| `_Mem_EmptyPool` | *not implemented* | Natural only once `AllocStrategy::Arena` (bump/bulk-free) lands |
| `_Mem_Alloc(pool, size, clear, …)` | `mem_alloc` + `mem_calloc` | `clear` split into a separate `mem_calloc`; UINT32_MAX payload cap + overflow guards; OOM returns `nullptr` and fires `g_oom_handler` (not fatal) |
| `_Mem_Realloc` (migrate + small→big) | `mem_realloc` | Migrates pools via native realloc (same `System` pool) or alloc+copy+free (cross-pool); no chain relink |
| `_Mem_Free` (sentinel-dispatched) | `mem_free` | Reads the 8-byte header for the owning pool; no sentinel dispatch |
| `_Mem_Check` / `Mem_IsAllocatedExt` | *not implemented* | Delegated to ASan/MSan |
| `Mem_PrintStats` / `memlist` command | `get_stats` / `pool_count` / `for_each_pool` + `PoolStats` | Query API instead of a console command (a `memlist` frontend is future host/console work) |
| `Sys_Error` OOM abort | `set_oom_handler` callback (atomic) | Non-fatal; caller-supplied handler |
| `XASH_CUSTOM_SWAP` `SWAP_Malloc`/`SWAP_Free` | `PoolBucket::do_alloc`/`do_free`/`do_realloc`/`ctx` strategy seam | The injectable backing-allocator abstraction; `System` (malloc/free) is the only wired strategy today |
| `Mem_Malloc`/`Mem_Calloc`/… convenience macros | `pool_new<T>`/`pool_delete`, `ScopedPool`, `PoolDeleter`/`pool_ptr` | Typed C++ RAII suite (no `__FILE__`/`__LINE__` injection); `pool_new<T>` requires `alignof(T) ≤ 8` |
