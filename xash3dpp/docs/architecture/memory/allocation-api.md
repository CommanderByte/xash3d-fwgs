# Allocation API

> **Defined in**: `xash3dpp/include/xash3dpp/memory/memory.hpp` (declarations),
> `xash3dpp/src/memory/memory.cpp` (implementations)\
> **Namespace**: `xash::memory`

## Overview

Four functions handle all dynamic memory: `mem_alloc`, `mem_calloc`,
`mem_realloc`, and `mem_free`. Every allocation is wrapped with an 8-byte
`AllocHeader` so `mem_free` can update pool counters without any caller-side
bookkeeping. The backing allocator is dispatched through `PoolBucket::do_alloc` /
`do_free` / `do_realloc`, which today points to `malloc`/`free`/`realloc`.

______________________________________________________________________

## `AllocHeader` — the allocation contract

Every live allocation has this layout in memory:

```text
  low address
  ┌──────────────────────────────────────┐
  │ AllocHeader (8 bytes)                │
  │   pool_index    : uint32_t           │  ← 1-based PoolHandle::index
  │   payload_size  : uint32_t           │  ← size originally requested
  ├──────────────────────────────────────┤
  │ payload (payload_size bytes)         │  ← pointer returned to caller
  └──────────────────────────────────────┘
  high address
```

`pool_index` tracks the **originating pool** — it is set when the allocation is
first created and is **not** updated if the object later migrates to another pool
via `mem_realloc(dst_pool, ptr, new_size)`. The header in the newly-allocated
block will carry `dst_pool`'s index.

Callers **must not** overwrite the header. Any write that reads `ptr - 8` bytes
before the returned pointer is undefined behaviour.

______________________________________________________________________

## `mem_alloc`

```cpp
void* mem_alloc(PoolHandle pool, std::size_t size) noexcept;
```

### Steps

1. **Null pool / `k_null_pool`**: calls `malloc(size)` directly, returning
   untracked memory. This is intentional — `k_null_pool` is a valid sentinel for
   "unowned" allocations.
1. **Resolve bucket**: `acquire`-load `state`; if not `Active`, fall back to
   raw `malloc` (pool is being destroyed or was not created; counters stay at 0).
1. **Overflow guard**: check `size + sizeof(AllocHeader)` overflows `size_t`
   before calling `malloc`. On overflow, invoke the OOM handler and return
   `nullptr`.
1. **Allocate raw**: `bucket.do_alloc(sizeof(AllocHeader) + size, bucket.ctx)`.
1. **Write header**: store `pool.index` and `size` into the `AllocHeader` prefix.
1. **Update counters**: `live_bytes.fetch_add(size, relaxed)`,
   `total_allocs.fetch_add(1, relaxed)`.
1. Return `raw + sizeof(AllocHeader)` as the payload pointer.

On `malloc` failure, the OOM handler is called (if set) and `nullptr` is
returned. No exception is thrown.

### Alignment

The system allocator guarantees `max_align_t`-aligned raw memory. After the
8-byte `AllocHeader` prefix, the payload pointer is at least 8-byte aligned on
all supported targets (x86, x86-64, ARM, ARM64). Types requiring alignment
greater than 8 bytes (e.g. AVX-512 data) must use `_aligned_malloc` outside this
subsystem.

______________________________________________________________________

## `mem_calloc`

```cpp
void* mem_calloc(PoolHandle pool, std::size_t size) noexcept;
```

`mem_alloc(pool, size)` followed by `memset(ptr, 0, size)`. All counter effects
match `mem_alloc`.

______________________________________________________________________

## `mem_free`

```cpp
void mem_free(void* ptr) noexcept;
```

1. `nullptr` is a no-op.
1. `header_of(ptr)` reads the `AllocHeader` at `ptr - sizeof(AllocHeader)`.
1. If `pool_index == 0` (untracked): calls `free(header)` and returns.
1. Otherwise resolves `g_pools[pool_index - 1]`; `acquire`-loads `state`.
1. `live_bytes.fetch_sub(payload_size, relaxed)`,
   `total_frees.fetch_add(1, relaxed)`.
1. Calls `bucket.do_free(header, bucket.ctx)` (which calls `free`).

The `acquire`-load in step 4 pairs with the `release` store of `Active` in
`create_pool`, ensuring the `do_free` pointer and `ctx` are visible.

**No double-free detection** is performed at runtime. Use ASan for that.

______________________________________________________________________

## `mem_realloc`

```cpp
void* mem_realloc(PoolHandle pool, void* ptr, std::size_t new_size) noexcept;
```

Handles four cases:

| `ptr` | `new_size` | Behaviour |
|-------|-----------|-----------|
| `nullptr` | > 0 | Acts as `mem_alloc(pool, new_size)` |
| not null | 0 | Acts as `mem_free(ptr)`, returns `nullptr` |
| not null | > 0, same pool | Fast path: `do_realloc` on the raw block, update header + counters |
| not null | > 0, different pool | Cross-pool migration: `mem_alloc(pool, new_size)` → `memcpy` → `mem_free(ptr)` |

### Same-pool fast path

1. `old_pool_index = header.pool_index`, `old_size = header.payload_size`.
1. `acquire`-load `state` on both old and new buckets; fall through to cross-pool migration if not `Active`.
1. `bucket.do_realloc(raw, sizeof(AllocHeader) + new_size, ctx)` (which calls
   `realloc`).
1. Update `header.payload_size = new_size`.
1. `live_bytes.fetch_add(new_size - old_size, relaxed)` (or subtract if
   shrinking).
1. `total_allocs.fetch_add(1, relaxed)`, `total_frees.fetch_add(1, relaxed)`.

### Cross-pool migration

The old allocation's payload is copied to the new allocation and the old block is
freed. The `src` pool loses `old_size` and gains one free counter; the `dst` pool
gains `new_size` and one alloc counter.

`mem_realloc` therefore **always** counts as one new alloc and one free (even for
same-pool). The test suite asserts this explicitly in
`test_realloc_same_pool_counts`.

______________________________________________________________________

## OOM handler

```cpp
using OomHandler = void(*)(std::size_t requested, PoolHandle pool) noexcept;
void set_oom_handler(OomHandler handler) noexcept;
```

`g_oom_handler` is `std::atomic<OomHandler>` (release store in `set_oom_handler`,
acquire load before use in the alloc path). The handler receives the originally
requested size (before the `AllocHeader` overhead) and the `PoolHandle`. It
**must not** call `mem_alloc`, as doing so from the OOM path risks unbounded
recursion.

After the handler returns, `mem_alloc` returns `nullptr`. The handler is not
called on overflow detection — it is only called when `malloc` itself fails.

Setting `nullptr` clears the handler; calling `set_oom_handler(nullptr)` is safe
from any thread.

______________________________________________________________________

## Thread safety

| Operation | Safety | Notes |
|-----------|--------|-------|
| `mem_alloc` concurrent with `mem_alloc` | Safe | Each thread's counter update is `relaxed fetch_add`; the `acquire` load of `state` guards function-pointer visibility |
| `mem_free` concurrent with `mem_alloc` | Safe | Same as above |
| `mem_realloc` same pool concurrent | Safe (counters) | The `realloc` call itself is serialised inside `do_realloc` by the system allocator |
| `mem_alloc` while `destroy_pool` runs | **Unsafe** | Caller contract: no in-flight allocs when `destroy_pool` is called |
| `set_oom_handler` from any thread | Safe | Atomic release store |

The stat counters use `memory_order_relaxed`. Under concurrent alloc/free the
counters are **eventually consistent** — they reflect the true state after all
threads quiesce, but may appear briefly inconsistent to concurrent readers. The
test suite (`test_concurrent_alloc_free`) verifies consistency after `join`.

______________________________________________________________________

## Error handling

- `mem_alloc` overflow: if `size + sizeof(AllocHeader)` overflows, calls OOM
  handler (if set) and returns `nullptr`.
- `malloc` failure: calls OOM handler and returns `nullptr`.
- `nullptr` pointer to `mem_free` / `mem_realloc`: no-op / alloc respectively.
- `mem_realloc(pool, ptr, 0)`: frees `ptr`, returns `nullptr`.
- No exceptions.

## See also

- [pool-registry.md](./pool-registry.md) — `AllocHeader`, `SlotState`, `PoolBucket`, `create_pool` CAS protocol
- [stats-api.md](./stats-api.md) — reading counters that `mem_alloc`/`mem_free` update
- [typed-helpers.md](./typed-helpers.md) — type-safe wrappers over `mem_alloc`/`mem_free`
