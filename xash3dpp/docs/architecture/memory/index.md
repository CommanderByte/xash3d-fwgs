# memory — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `memory/memory.hpp` | `xash::memory` | `PoolHandle`, `k_null_pool`, `PoolStats`, `PoolConfig`, `AllocStrategy`, `create_pool`, `destroy_pool`, `mem_alloc`, `mem_calloc`, `mem_realloc`, `mem_free`, `get_stats`, `pool_count`, `for_each_pool`, `set_oom_handler`, `pool_new`, `pool_delete`, `ScopedPool`, `PoolDeleter`, `pool_ptr` |

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/memory/pool_registry.hpp` | `AllocHeader`, `SlotState`, `PoolBucket`, `kMaxPools` (= `limits::memory_pool_max`) — internal bucket layout exposed for tests |

## Source files

| File | Responsibility |
|------|---------------|
| `src/memory/memory.cpp` | All function definitions: pool lifecycle, alloc/free/realloc, stats, OOM handler, typed helpers support functions |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `PoolHandle` | struct | `memory/memory.hpp` | 1-based index into the pool registry; `index==0` is `k_null_pool` |
| `PoolStats` | struct | `memory/memory.hpp` | Snapshot of one pool's counters (`name`, `live_bytes`, `total_allocs`, `total_frees`) |
| `PoolConfig` | struct | `memory/memory.hpp` | Optional configuration for `create_pool` (`strategy`, `reserve`) |
| `AllocStrategy` | enum class | `memory/memory.hpp` | Backing allocator (`System`, `Arena` (future), `Slab` (future)) |
| `ScopedPool` | class | `memory/memory.hpp` | RAII wrapper: `create_pool` in constructor, `destroy_pool` in destructor |
| `PoolDeleter` | struct | `memory/memory.hpp` | Custom deleter for `std::unique_ptr` — calls `pool_delete<T>` |
| `pool_ptr<T>` | alias | `memory/memory.hpp` | `std::unique_ptr<T, PoolDeleter>` |
| `AllocHeader` | struct | `private/memory/pool_registry.hpp` | 8-byte prefix on every allocation: `pool_index` (u32) + `payload_size` (u32) |
| `SlotState` | enum class | `private/memory/pool_registry.hpp` | `Free`, `Busy`, `Active` — atomic lifecycle state for each `PoolBucket` |
| `PoolBucket` | struct | `private/memory/pool_registry.hpp` | One registry slot: counters, name, state, and allocator function pointers |

## Free functions

| Function | Defined in | Purpose |
|----------|-----------|---------|
| `create_pool(name, cfg)` | `memory.cpp` | Claim a free slot with CAS; return `PoolHandle` |
| `destroy_pool(handle)` | `memory.cpp` | Assert `live_bytes==0`, clear slot, release with `memory_order_release` |
| `mem_alloc(pool, size)` | `memory.cpp` | Allocate `size` bytes; prepend `AllocHeader`; update pool counters |
| `mem_calloc(pool, size)` | `memory.cpp` | `mem_alloc` + `memset` to zero |
| `mem_realloc(pool, ptr, new_size)` | `memory.cpp` | Resize; supports cross-pool migration |
| `mem_free(ptr)` | `memory.cpp` | Read header; update pool counters; call `do_free` |
| `get_stats(handle)` | `memory.cpp` | Snapshot counters for one active pool |
| `pool_count()` | `memory.cpp` | Count `Active` slots via relaxed scan |
| `for_each_pool(fn, userdata)` | `memory.cpp` | Iterate active pools; call `fn` for each |
| `set_oom_handler(handler)` | `memory.cpp` | Store OOM callback in `g_oom_handler` (atomic) |
| `pool_new<T>(pool, args…)` | `memory.hpp` | `mem_alloc` + placement-new |
| `pool_delete<T>(ptr)` | `memory.hpp` | `ptr->~T()` + `mem_free` |

## Global state (file-scope, `memory.cpp`)

| Variable | Type | Purpose |
|----------|------|---------|
| `g_pools[kMaxPools]` | `PoolBucket[limits::memory_pool_max]` | Flat registry; statically zero-initialised |
| `g_oom_handler` | `std::atomic<OomHandler>` | OOM callback; null by default |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_memory` | STATIC | *(none)* | *(none — only stdlib)* |

The target requires C++20 (`target_compile_features(xash3dpp_memory PUBLIC cxx_std_20)`).
It has no external dependencies; it uses only `<cstdlib>` (malloc/realloc/free),
`<cstring>` (memset/strncpy/memcpy), `<cassert>`, and `<atomic>`.
