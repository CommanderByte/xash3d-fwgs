# Pool Registry

> **Defined in**: `xash3dpp/include/xash3dpp/private/memory/pool_registry.hpp`,
> `xash3dpp/src/memory/memory.cpp`  
> **Namespace**: `xash::memory` / `xash::memory::internal`

## Overview

The pool registry is a fixed-size flat array (`g_pools[kMaxPools]`) of
`PoolBucket` structs, each representing one named pool. The capacity is
controlled by `limits::memory_pool_max` (default 128) from
`<xash3dpp/limits.hpp>`; `kMaxPools` is an alias that reads from it.

`PoolBucket` serves three roles:
1. **Slot metadata** — name string, atomic lifecycle state.
2. **Accounting buckets** — atomic counters for in-flight bytes and call counts.
3. **Allocator dispatch** — function pointers for the backing strategy
   (`do_alloc`, `do_free`, `do_realloc`).

`AllocHeader` is the 8-byte struct prepended to every allocation; it carries the
pool index and payload size so `mem_free` can locate the owning bucket without
any additional bookkeeping.

---

## `AllocHeader`

**Header**: `private/memory/pool_registry.hpp`

```
Memory layout of every allocation:
  [ AllocHeader (8 bytes) | payload (size bytes) ]
```

| Field | Type | Role |
|-------|------|------|
| `pool_index` | `uint32_t` | 1-based `PoolHandle::index`; 0 means untagged/kNullPool |
| `payload_size` | `uint32_t` | Payload bytes, exclusive of the header |

`sizeof(AllocHeader) == 8`, `alignof(AllocHeader) == 4` (both enforced by
`static_assert`). The system allocator returns `max_align_t`-aligned raw memory;
after the 8-byte header the payload pointer is at least 8-byte aligned on all
supported targets.

---

## `SlotState`

**Header**: `private/memory/pool_registry.hpp`

```cpp
enum class SlotState : uint8_t { kFree = 0, kBusy = 1, kActive = 2 };
```

Each `PoolBucket` holds `std::atomic<SlotState> state`. Transitions:

| From | To | Who | Memory order |
|------|----|-----|-------------|
| `kFree` | `kBusy` | `create_pool` (CAS) | `acquire` on success |
| `kBusy` | `kActive` | `create_pool` | `release` |
| `kActive` | `kFree` | `destroy_pool` | `release` |

The `kBusy` intermediate state ensures:
- Only one thread claims a slot (CAS failure causes retry on the next slot).
- All field writes during `kBusy` (name, counters, function pointers) happen
  *before* the `release` store of `kActive`.
- Any thread that subsequently `acquire`-loads `kActive` is guaranteed to see
  those field writes.

---

## `PoolBucket`

**Header**: `private/memory/pool_registry.hpp`

| Field | Type | Role |
|-------|------|------|
| `name[64]` | `char[]` | Null-terminated pool name; written during `kBusy`, read after `kActive` |
| `live_bytes` | `std::atomic<size_t>` | Bytes currently in flight |
| `total_allocs` | `std::atomic<size_t>` | Cumulative allocation count |
| `total_frees` | `std::atomic<size_t>` | Cumulative free count |
| `state` | `std::atomic<SlotState>` | Lifecycle state; drives the CAS protocol |
| `do_alloc` | `void*(*)(size_t, void*)` | Backing allocator; null → `malloc` |
| `do_free` | `void(*)(void*, void*)` | Backing deallocator; null → `free` |
| `do_realloc` | `void*(*)(void*, size_t, void*)` | Backing resizer; null → `realloc` (or alloc+copy) |
| `ctx` | `void*` | Opaque context pointer passed to all three functions |

All three stat counters and `state` are `std::atomic`. The function pointers and
`ctx` are plain pointers protected by the `kBusy`→`kActive` acquire/release
ordering.

---

## `create_pool`

**Source**: `memory.cpp`

```cpp
PoolHandle create_pool(const char* name, PoolConfig cfg = {}) noexcept;
```

### Algorithm

1. Scan `g_pools[0..limits::memory_pool_max-1]` for a slot whose `state` is `kFree`.
2. For each candidate, perform a CAS:
   - `expected = kFree` → `kBusy`, `memory_order_acquire` on success,
     `memory_order_relaxed` on failure.
   - On failure, another thread claimed this slot first; continue to the next.
3. After a successful CAS, the current thread owns the slot exclusively.
4. Reset all counter atomics to 0 (`memory_order_relaxed`).
5. Copy `name` into `bucket.name` with `strncpy` (truncates at 63 chars + NUL).
6. Wire up `do_alloc`, `do_free`, `do_realloc` based on `cfg.strategy`.
   Today only `kSystem` is implemented; `kArena` and `kSlab` fall through to
   `kSystem` with no error.
7. Release-store `kActive` — makes all prior writes visible to other threads.
8. Return `PoolHandle { i + 1 }` (1-based).

If no free slot is found, returns `kNullPool`. Subsequent allocations through
`kNullPool` still succeed (untracked `malloc`) — the sentinel is not an error.

### Thread safety of `create_pool`

- The CAS loop makes concurrent `create_pool` calls race-free: no two threads can
  claim the same slot index.
- However, `create_pool` itself is **not safe to call** while another thread may
  be executing `destroy_pool` for the same pool (the transitions `kActive →
  kFree` and `kFree → kBusy` have no additional serialisation beyond the atomic).
  The contract is that lifecycle calls for a given slot are serialised by the
  caller.

---

## `destroy_pool`

**Source**: `memory.cpp`

```cpp
void destroy_pool(PoolHandle handle) noexcept;
```

1. Resolves `bucket_of(handle)`.
2. `acquire`-loads `state`; returns if not `kActive`.
3. `assert(live_bytes == 0)` — fires in debug builds if leaks remain.
4. Clears `name[0]`, nulls all function pointers and `ctx`.
5. Resets counter atomics to 0 (`relaxed`).
6. Release-stores `kFree` — slot is now available for the next `create_pool`.

**Slot reuse**: the freed slot index is recycled immediately. The next
`create_pool` call may return the same `PoolHandle::index`. Stale handles to
destroyed pools will silently alias a new pool's counters; the caller is
responsible for not using stale handles.

---

## `AllocStrategy` and `PoolConfig`

| Value | Status | Backing allocator |
|-------|--------|-------------------|
| `kSystem` | Implemented | `malloc` / `realloc` / `free` |
| `kArena` | Placeholder | Falls back to `kSystem`; reserved for future bump allocator |
| `kSlab` | Placeholder | Falls back to `kSystem`; reserved for future fixed-size slab |

`PoolConfig::reserve` is stored but currently ignored; it is a pre-allocation
hint for future `kArena` implementations.

---

## Threading model

| State | Mechanism | Notes |
|-------|-----------|-------|
| Slot claim | CAS on `state` (`acquire`/`relaxed`) | Safe for concurrent `create_pool` |
| Field publish | `release` store of `kActive` | Pairs with `acquire` load by readers |
| `live_bytes` / `total_allocs` / `total_frees` | `std::atomic`, `memory_order_relaxed` | Lock-free; no inter-pool ordering needed |
| `name[64]` | Protected by `kBusy`/`kActive` ordering | Safe: read only after `acquire` load of `kActive` |
| `do_alloc` / `do_free` / `do_realloc` / `ctx` | Protected by `kBusy`/`kActive` ordering | Same as `name` |
| `destroy_pool` vs. in-flight `mem_alloc` | **No protection** | Caller contract: destroy only when no allocs are in flight |

`mem_alloc`, `mem_free`, and `mem_realloc` each `acquire`-load `state` before
using any function pointer or updating any counter. This is the read side of the
acquire/release pair established by `create_pool`. The load also ensures
`destroy_pool`'s clears are visible: a reader that sees `state != kActive` falls
back to `malloc`/`free` directly.

## Error handling

- `create_pool`: returns `kNullPool` when the registry is full; does not call the
  OOM handler (pool exhaustion is a programming error, not a runtime OOM).
- `destroy_pool`: silent no-op for invalid handles; debug assert for leaked bytes.
- No exceptions.

## Edge cases and invariants

- `create_pool(nullptr)` stores an empty string (`name[0] = '\0'`); the pool is
  fully usable. `get_stats` returns a pointer to the empty string (not `nullptr`).
- Pool name longer than 63 characters is silently truncated.
- `create_pool` with `AllocStrategy::kArena` or `kSlab` currently falls back to
  `kSystem` without a warning. This is intentional — future implementations will
  be backward-compatible.
- The `reserve` hint in `PoolConfig` is stored but not acted on today.

## See also

- [allocation-api.md](./allocation-api.md) — functions that use `AllocHeader` and `PoolBucket`
- [stats-api.md](./stats-api.md) — functions that read `PoolBucket` counters
- [docs/threading-analysis/memory-threading.md](../../threading-analysis/memory-threading.md) — full threading hazard analysis
