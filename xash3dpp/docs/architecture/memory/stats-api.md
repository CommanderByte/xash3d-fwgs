# Stats API

> **Defined in**: `xash3dpp/include/xash3dpp/memory/memory.hpp` (declarations),
> `xash3dpp/src/memory/memory.cpp` (implementations)  
> **Namespace**: `xash::memory`

## Overview

The stats API reads per-pool accounting counters that are maintained as
`memory_order_relaxed` atomics by `mem_alloc`, `mem_realloc`, and `mem_free`.
Reads are always non-blocking and lock-free. Under concurrent allocation the
values may be briefly inconsistent, but they converge to exact values once all
allocating threads quiesce.

---

## `PoolStats`

```cpp
struct PoolStats {
    const char*  name;        // points into PoolBucket::name[]
    std::size_t  live_bytes;  // current in-flight bytes
    std::size_t  total_allocs;
    std::size_t  total_frees;
};
```

`PoolStats` is a snapshot; it has no ownership of the pool. `name` points into
the static `PoolBucket::name[64]` array — it is valid until `destroy_pool` clears
the slot, so the snapshot must not be used after the owning pool is destroyed.

`live_bytes`, `total_allocs`, and `total_frees` are plain values loaded from
atomics at snapshot time. They are **not** atomically consistent with each other:
a reader may see `total_allocs > total_frees` even when `live_bytes == 0` during
a realloc (which records a free then an alloc). Once all allocating threads have
joined, the relationship `live_bytes == (total_allocs - total_frees) *
avg_object_size` holds in aggregate, not point-by-point.

---

## `get_stats`

```cpp
PoolStats get_stats(PoolHandle handle) noexcept;
```

1. Resolves `bucket_of(handle)`.
2. `acquire`-loads `state`. If not `kActive`, returns a zeroed `PoolStats` with
   `name = nullptr`.
3. Relaxed-loads `live_bytes`, `total_allocs`, `total_frees` from the bucket
   atomics.
4. Sets `PoolStats::name` to point directly into `bucket.name[]`.
5. Returns the snapshot.

The `acquire` load in step 2 pairs with the `release` store of `kActive` in
`create_pool`, ensuring `name[]` was written before this load returns.

---

## `pool_count`

```cpp
std::size_t pool_count() noexcept;
```

Scans `g_pools[0..kMaxPools-1]`, counting slots whose `state` relaxed-loads as
`kActive`. Returns the count.

This is a point-in-time snapshot. Slots being concurrently created or destroyed
may be counted or not, depending on the exact order of `memory_order_relaxed`
loads. After all lifecycle operations complete, the count is exact.

Use `pool_count` for diagnostics and tests, not for control flow that must be
linearised with specific create/destroy calls.

---

## `for_each_pool`

```cpp
using PoolIterFn = void(*)(PoolStats stats, void* userdata) noexcept;
void for_each_pool(PoolIterFn fn, void* userdata) noexcept;
```

Iterates `g_pools[0..kMaxPools-1]`. For each slot that `acquire`-loads as
`kActive`, builds a `PoolStats` snapshot and calls `fn(stats, userdata)`.

The `fn` callback **must not** call `create_pool` or `destroy_pool` — doing so
modifies the array being iterated and may cause slots to be visited twice or not
at all. It **may** call `mem_alloc`/`mem_free` (counter updates are independent
of the iteration).

Passing `nullptr` for `fn` is a no-op — the function returns immediately without
iterating.

### Typical uses

```cpp
// Print all pool stats to stdout
for_each_pool([](PoolStats s, void*) {
    std::printf("%-40s  live=%-8zu  allocs=%-8zu  frees=%-8zu\n",
                s.name, s.live_bytes, s.total_allocs, s.total_frees);
}, nullptr);

// Sum all in-flight bytes
std::size_t total = 0;
for_each_pool([](PoolStats s, void* ud) {
    *static_cast<std::size_t*>(ud) += s.live_bytes;
}, &total);
```

---

## `set_oom_handler`

```cpp
using OomHandler = void(*)(std::size_t requested, PoolHandle pool) noexcept;
void set_oom_handler(OomHandler handler) noexcept;
```

Although logically an allocation-path concern, `set_oom_handler` is documented
here because it is the only global write path in the stats/control surface.

`g_oom_handler` is `std::atomic<OomHandler>`. `set_oom_handler` performs a
`release` store; the alloc path performs an `acquire` load. Concurrent calls to
`set_oom_handler` from multiple threads are safe but last-write-wins with no
stronger ordering guarantee.

---

## Thread safety

| Operation | Safety | Notes |
|-----------|--------|-------|
| `get_stats` concurrent with `mem_alloc`/`mem_free` | Safe | Relaxed loads; values eventually consistent |
| `pool_count` concurrent with `create_pool`/`destroy_pool` | Safe | Relaxed scan; approximate during concurrent lifecycle |
| `for_each_pool` callback calls `mem_alloc`/`mem_free` | Safe | No iterator invalidation from counter updates |
| `for_each_pool` callback calls `create_pool`/`destroy_pool` | **Unsafe** | Must not modify array during iteration |
| `set_oom_handler` from any thread | Safe | Atomic release store |

---

## Related links

- [pool-registry.md](./pool-registry.md) — how the atomic counters are maintained
- [allocation-api.md](./allocation-api.md) — the functions that write to the counters
