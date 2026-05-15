# Memory Threading Analysis

> Boundary spec: `docs/boundaries/memory-boundary.md`

---

## Ownership model

The legacy engine is explicitly single-threaded at the main-loop level. The
boundary spec notes that the filesystem module uses stub wrappers (`Mem_AllocStub`
etc.) that **bypass** the pool system entirely when it is loaded as a standalone
DLL, so the only off-thread pressure in the legacy design is avoided by
abstraction rather than by synchronisation.

The xash3dpp rewrite does not document a threading contract anywhere.  The
`memory.hpp` design comment states:

> "thread-safe: all counter mutations use `std::atomic<std::size_t>` with
>  `memory_order_relaxed`"

This claim is **partially true**: the three per-bucket counters are atomic.
However, pool lifecycle operations (`create_pool`, `destroy_pool`) and the
function-pointer fields read on every allocation path are **not** protected by
any synchronisation primitive.  The design currently assumes that pool creation
and destruction are performed by a single owner thread, and that no pool is
destroyed while allocations through it are in flight on other threads.

That assumption is not asserted or documented in the code.

---

## Safe items

- **`live_bytes` / `total_allocs` / `total_frees`** (`std::atomic<std::size_t>`,
  `memory_order_relaxed` in all paths) — concurrent `fetch_add`/`fetch_sub` from
  any thread are individually atomic; the counters will not tear.  The relaxed
  ordering is correct because no inter-pool happens-before relationship is needed.

- **`g_pools[kMaxPools]` (the array itself)** — the array is a `static`
  file-scope variable with trivial (zero) initialisation.  `std::atomic` members
  have a `constexpr` default constructor; the whole array is statically
  initialised before any dynamic initialisation runs.  No static-init race.

- **`g_oom_handler` initial value** — `nullptr`, statically initialised;
  safe before `set_oom_handler` is ever called.

- **`AllocHeader` reading in `mem_free` / `mem_realloc`** — the header is in the
  caller-owned allocation block.  If the caller does not race on the same
  pointer, reading the header is safe.

---

## Hazards

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `g_pools[i].active` | `memory.cpp` | **Race-shared** | Plain `bool`, not atomic. `create_pool` reads it with `if (b.active) continue` while scanning for a free slot; `destroy_pool` writes `b->active = false`. Two concurrent `create_pool` calls can both observe the same slot as inactive and both claim it, producing two handles with the same `index`. |
| `g_pools[i].name[64]` | `memory.cpp` | **Race-shared** | `char` array written by `create_pool` (`strncpy`) and `destroy_pool` (`name[0] = '\0'`); read by `get_stats` and `for_each_pool`. A concurrent read during write is a data race on `char`. |
| `g_pools[i].do_alloc` / `do_free` / `do_realloc` / `ctx` | `memory.cpp` | **Race-shared** | Function pointers written once in `create_pool` and zeroed in `destroy_pool`; read on every allocation and free.  `destroy_pool` while an allocation is in flight on another thread causes a read of a null or stale function pointer. |
| `g_oom_handler` | `memory.cpp` | **Race-shared** | Plain function pointer.  `set_oom_handler` writes it; `mem_alloc`, `mem_realloc` read it.  No atomic or fence.  A torn read could load a half-written pointer and call an arbitrary address. |
| `create_pool` — slot scan + claim | `memory.cpp` | **Race-shared** | The scan (`if (b.active) continue`) and the claim (`b.active = true`) are two non-atomic steps on a plain `bool`.  This is a classic TOCTOU (time-of-check / time-of-use) data race even if each individual byte access were individually safe. |
| `destroy_pool` — multi-field wipe | `memory.cpp` | **Race-shared** | `destroy_pool` writes `active`, `name`, four function pointers, and the three atomic counters in sequence with no fence.  A concurrent `mem_alloc` using the same handle can read `do_alloc != nullptr` after `active` is cleared, call the stale pointer, then increment the now-zeroed counter — corrupting both stats and memory. |
| `mem_alloc` OOM handler dispatch | `memory.cpp` | **Signal-unsafe** | `mem_alloc` calls `std::malloc` (not async-signal-safe) and may then invoke the user-supplied `g_oom_handler`.  If either is called from a POSIX signal handler the behaviour is undefined.  No documentation currently forbids this. |
| `mem_calloc` — memset after alloc | `memory.cpp` | **Signal-unsafe** | `std::memset` is async-signal-safe in practice but `mem_alloc` (which it calls) is not; the combined function shares the signal-safety hazard. |

---

## Required caller contracts

The module currently relies on the following contracts being upheld by callers;
none are asserted in the code:

1. **Pool lifecycle is single-owner.** `create_pool` and `destroy_pool` for a
   given slot must be called from the same thread, and `destroy_pool` must not
   be called while any other thread may be executing `mem_alloc`, `mem_free`, or
   `mem_realloc` with the same handle.

2. **`set_oom_handler` is called at init time only.** Once the engine is running
   with multiple threads, `set_oom_handler` must not be called again.  There is
   no mechanism to enforce this.

3. **No allocation through a pool handle after `destroy_pool`.** A stale handle
   whose slot has been recycled will silently credit a different pool's stats and
   call a different pool's `do_alloc`.

4. **No concurrent `create_pool` calls.** The slot-claim algorithm is not
   thread-safe; external serialisation (e.g. call only from the main thread
   during startup) is required.

5. **Not called from signal handlers.** The full allocation path uses `malloc`,
   which is not async-signal-safe.

---

## Recommendations

In priority order (cheapest first):

1. **Document the threading contract in `memory.hpp`.**  Add a comment block
   above `create_pool` / `destroy_pool` stating: "Pool lifecycle functions are
   not thread-safe.  Call only from the owning thread.  No pool may be destroyed
   while allocations through it are in flight on other threads."  This costs
   nothing and makes the implicit contract explicit.

2. **Make `active` atomic or use a mutex for lifecycle only.**  Change
   `bool active` to `std::atomic<bool>` and replace the scan+claim in
   `create_pool` with a compare-exchange loop:
   ```cpp
   bool expected = false;
   if (b.active.compare_exchange_strong(expected, true,
           std::memory_order_acquire, std::memory_order_relaxed))
   { ... claim slot ... }
   ```
   This eliminates the TOCTOU race for concurrent `create_pool` calls at
   negligible cost.

3. **Make `g_oom_handler` atomic.**  Replace the plain function pointer with
   `std::atomic<void(*)(std::size_t, PoolHandle) noexcept>` and load with
   `memory_order_acquire` in the allocation path.  This costs one extra load
   instruction on the OOM (rare) path.

4. **Protect function-pointer fields with acquire/release ordering.**  The
   `do_alloc`, `do_free`, `do_realloc`, and `ctx` fields written by `create_pool`
   and read by the allocation functions need an acquire load on the read side and
   a release store (or fence) on the write side.  The simplest approach is to
   store them before setting `active = true` with a `std::memory_order_release`
   store; callers then load `active` with `memory_order_acquire` before reading
   the pointers, establishing the happens-before chain.

5. **Protect `name[]` with the same acquire/release protocol as item 4.**  A
   single `strncpy` into an array cannot be made atomic, but if `name` is only
   read after an `acquire` load of `active`, and only written before a `release`
   store of `active`, the C++ memory model guarantees the write is visible.  The
   current code sets `active = true` without a release fence and reads `name`
   without an acquire fence.

6. **Add `assert_main_thread()` stubs to lifecycle functions.**  Even if true
   thread safety is deferred, assertions are cheap and catch accidental off-thread
   calls during development.  A platform-provided `platform::is_main_thread()`
   predicate (or `std::this_thread::get_id() == g_main_thread_id`) is sufficient.

7. **Longer term: per-thread scratch pools.**  The most scalable design for
   high-frequency temporary allocations (per-frame scratch, network packet
   buffers) is a thread-local `PoolHandle` backed by a bump allocator.  This
   removes all contention from the hot path entirely.  `AllocStrategy::kArena` is
   the placeholder for this; implementing it would make the per-pool function
   pointer dispatch worthwhile.
