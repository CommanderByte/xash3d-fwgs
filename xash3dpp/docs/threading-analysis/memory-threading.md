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

This claim is **fully accurate** in the current implementation: not only are the
three per-bucket counters atomic, but pool lifecycle operations now use an atomic
`SlotState` enum with compare-exchange for slot claiming and acquire/release
ordering to guard all function-pointer and `name[]` field accesses.  The design
assumes that pool creation and destruction are performed by a single owner thread
— concurrent `create_pool` calls are safe (CAS-based), but `destroy_pool` while
allocations are in-flight through the same handle remains the caller's
responsibility.

That assumption is partially asserted (the `assert` on `live_bytes == 0`) but
not fully documented in the public header.

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
| `g_pools[i].active` (slot claim) | `memory.cpp` | **Fixed** | Was a plain `bool` with a TOCTOU scan+claim. Replaced by a `SlotState` enum (`kFree`/`kBusy`/`kActive`) stored in `std::atomic<SlotState>`. `create_pool` uses `compare_exchange_strong(kFree → kBusy, acquire/relaxed)` to atomically claim a slot; the CAS eliminates the race entirely. |
| `g_pools[i].name[64]` | `memory.cpp` | **Fixed** | Was written by `strncpy` and zeroed by `destroy_pool` without any fence. Name is now written while the slot is in state `kBusy` (exclusively owned), then the `release` store to `kActive` makes the write visible to any reader that `acquire`-loads `kActive`. |
| `g_pools[i].do_alloc` / `do_free` / `do_realloc` / `ctx` | `memory.cpp` | **Fixed** | Were written and read without any ordering. Now written before the `release` store of `kActive` in `create_pool`; readers `acquire`-load `kActive` first, establishing the happens-before chain. `destroy_pool` zeroes them before a `release` store of `kFree`, and `create_pool`'s CAS `acquire`s that store. |
| `g_oom_handler` | `memory.cpp` | **Fixed** | Was a plain function pointer. Now `static std::atomic<OomHandler>` with `acquire` loads in allocation paths and a `release` store in `set_oom_handler`. |
| `create_pool` — slot scan + claim | `memory.cpp` | **Fixed** | The plain-bool TOCTOU eliminated by the `compare_exchange_strong` loop described above. |
| `destroy_pool` — multi-field wipe | `memory.cpp` | **Fixed** | Fields are cleared before a `release` store of `kFree`; a concurrent `mem_alloc` through the same handle must first `acquire`-load `kActive` and finds `kFree`, so it falls through without dereferencing the now-null pointers. |
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
   not thread-safe with respect to concurrent `destroy_pool` calls while
   allocations through the same handle are in flight on other threads.  Concurrent
   `create_pool` calls are safe (CAS-based slot claiming).  `destroy_pool` must
   only be called when no other thread holds an allocation through the same handle."
   *(Not yet done.)*

2. ~~**Make `active` atomic or use a mutex for lifecycle only.**~~  **Done.**
   `bool active` replaced by `std::atomic<SlotState>` with a
   `compare_exchange_strong(kFree → kBusy, acquire/relaxed)` loop in `create_pool`.

3. ~~**Make `g_oom_handler` atomic.**~~  **Done.**
   Now `static std::atomic<OomHandler>` with `acquire` loads and `release` stores.

4. ~~**Protect function-pointer fields with acquire/release ordering.**~~  **Done.**
   All fields written before the `release` store of `kActive`; readers
   `acquire`-load `kActive` first.

5. ~~**Protect `name[]` with the same acquire/release protocol.**~~  **Done.**
   Name written while slot is in `kBusy` (exclusively owned), published by the
   same `release` store of `kActive`.

6. **Add `assert_main_thread()` stubs to lifecycle functions.**  Even if true
   thread safety is deferred, assertions are cheap and catch accidental off-thread
   calls during development.  Platform-module exposes `xash::platform::detail::assert_main_thread()`
   but `xash3dpp_memory` does not depend on `xash3dpp_platform`; the simplest
   approach is to expose a public `platform::is_main_thread()` predicate or
   capture the main-thread ID inside the memory module independently.
   *(Not yet done.)*

7. **Longer term: per-thread scratch pools.**  The most scalable design for
   high-frequency temporary allocations (per-frame scratch, network packet
   buffers) is a thread-local `PoolHandle` backed by a bump allocator.  This
   removes all contention from the hot path entirely.  `AllocStrategy::kArena` is
   the placeholder for this; implementing it would make the per-pool function
   pointer dispatch worthwhile.
