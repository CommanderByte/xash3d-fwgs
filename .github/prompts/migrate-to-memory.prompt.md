---
name: "Migrate subsystem to xash3dpp memory"
description: "Audit a xash3dpp subsystem for direct malloc/free/new/delete usage and migrate all allocation sites to the xash3dpp memory subsystem (create_pool, mem_alloc, mem_free, pool_new, pool_delete). Use when writing a new subsystem or porting an existing one."
argument-hint: "subsystem name (e.g. filesystem, sound, networking, renderer)"
agent: agent
tools: [read, search, edit]
model: claude-sonnet-4-6
---

# Migrate `$ARGUMENTS` to the xash3dpp memory subsystem

Your task is to ensure the **$ARGUMENTS** subsystem routes all dynamic memory
operations through the xash3dpp memory subsystem so that engine-wide statistics,
OOM handling, and future allocator strategies are consistent.

---

## API reference

The canonical header is
[`xash3dpp/include/xash3dpp/memory/memory.hpp`](../../xash3dpp/include/xash3dpp/memory/memory.hpp).
Key types and functions:

```cpp
// Pool lifecycle
PoolHandle create_pool(const char* name, PoolConfig cfg = {}) noexcept;
void       destroy_pool(PoolHandle handle) noexcept;

// Raw allocation  (all return nullptr on OOM or size == 0)
void* mem_alloc  (PoolHandle pool, std::size_t size)                noexcept;
void* mem_calloc (PoolHandle pool, std::size_t size)                noexcept;
void* mem_realloc(PoolHandle pool, void* ptr, std::size_t new_size) noexcept;
void  mem_free   (void* ptr)                                        noexcept;  // reads header — no pool arg

// Typed helpers
template<typename T, typename... Args>
T*   pool_new   (PoolHandle pool, Args&&... args) noexcept;  // placement-new, returns nullptr on OOM
void pool_delete(T* ptr)                          noexcept;  // calls destructor + mem_free

// RAII wrapper — owns the pool for a scope
class ScopedPool { ... };  // holds handle(), convertible to bool

// Diagnostics
PoolStats  get_stats  (PoolHandle) noexcept;
void       for_each_pool(void (*fn)(PoolStats, void*), void*) noexcept;
void       set_oom_handler(void (*)(std::size_t, PoolHandle) noexcept) noexcept;
```

Design contract to keep in mind:
- `mem_free` never needs a pool argument — the 8-byte header carries it.
- A `PoolHandle` is a 1-based `uint32_t`; `kNullPool` (0) is the invalid sentinel.
  Allocations through `kNullPool` still succeed but are untracked.
- `destroy_pool` debug-asserts `live_bytes == 0`. Clean up all allocations first.
- Pool lifecycle (create/destroy) is single-owner. Do not destroy a pool while
  other threads are still allocating from it.

---

## Step 1 — Read the subsystem

Read all source files under `xash3dpp/src/$ARGUMENTS/` and headers under
`xash3dpp/include/xash3dpp/$ARGUMENTS/` (and `private/$ARGUMENTS/`).

Understand:
- What types own heap memory (structs with raw pointer fields, containers, etc.)
- The lifetime model of those types (stack, static, per-request, etc.)
- Whether any allocations cross subsystem boundaries (caller allocates, callee frees)

---

## Step 2 — Audit allocation sites

Search every file in `xash3dpp/src/$ARGUMENTS/` and its headers for:

| Pattern | Replacement |
|---------|-------------|
| `std::malloc(n)` / `malloc(n)` | `mem_alloc(pool, n)` |
| `std::calloc(1, n)` / `calloc(1, n)` | `mem_calloc(pool, n)` |
| `std::realloc(ptr, n)` / `realloc(ptr, n)` | `mem_realloc(pool, ptr, n)` |
| `std::free(ptr)` / `free(ptr)` | `mem_free(ptr)` |
| `new T(...)` / `new T[n]` | `pool_new<T>(pool, ...)` / `mem_alloc` + manual init |
| `delete ptr` / `delete[] ptr` | `pool_delete(ptr)` / `mem_free(ptr)` |
| `std::make_unique<T>(...)` | `pool_new<T>(pool, ...)` wrapped in a custom handle or raw pointer |
| `std::vector<T>` with default allocator | acceptable for local/temporary use; flag if used for long-lived subsystem state |
| `std::string` (long-lived) | flag for review — short strings via SSO are fine |

Record every allocation site: file, line, current form, and intended pool.

---

## Step 3 — Design the pool layout

One pool per logical lifetime boundary is the right granularity. Typical patterns:

| Lifetime | Pool name convention | Creation point |
|----------|---------------------|----------------|
| Entire engine run | `"$ARGUMENTS"` | subsystem init function |
| Per-map / per-level | `"$ARGUMENTS/level"` | level load |
| Per-request / per-frame (short) | `"$ARGUMENTS/frame"` | frame begin; bulk-free at frame end (future: kArena) |
| Per-object (variable) | tag the object's pool | object constructor |

Rules:
- Do **not** create a pool per allocation — pools are accounting buckets, not
  per-object arenas.
- Do **not** share a pool across unrelated subsystems — statistics must be
  attributable to a single owner.
- If the subsystem is initialised once and lives for the engine lifetime, a single
  `"$ARGUMENTS"` pool is sufficient.
- Name the pool with a forward-slash hierarchy if sub-lifetimes are needed.

---

## Step 4 — Implement the migration

For each allocation site identified in Step 2:

1. Add `#include <xash3dpp/memory/memory.hpp>` to the file if not present.
2. Ensure the owning type or module holds a `PoolHandle` for the appropriate
   lifetime (or uses `ScopedPool` for RAII).
3. Replace the allocation call using the table in Step 2.
4. Replace the free/delete call with `mem_free` or `pool_delete`.
5. If the type owns heap memory through a smart pointer
   (`std::unique_ptr<T, std::default_delete<T>>`), replace the deleter with a
   custom one that calls `pool_delete`:

   ```cpp
   struct PoolDeleter {
       void operator()(T* p) const noexcept { pool_delete(p); }
   };
   using UniqueT = std::unique_ptr<T, PoolDeleter>;
   ```

6. If `std::vector` is used for long-lived subsystem state, either keep it
   (vectors only malloc once per doubling) and add a comment, or replace with a
   pool-allocated dynamic array if accounting precision is required.

Migration must be complete — no `malloc`/`free`/`new`/`delete` should remain for
long-lived subsystem allocations after this step. Short-lived local temporaries
(e.g. format buffers on the stack) do not need to be migrated.

---

## Step 5 — Verify destroy order

After migrating, verify that every `destroy_pool` call is:
- Preceded by the destruction/deallocation of all objects that were allocated
  from that pool.
- Not called while other threads may still be accessing memory from the pool
  (document the threading contract if relevant).

In debug builds `destroy_pool` will assert `live_bytes == 0`. Add a comment
above the call that names what must be cleaned up before it.

---

## Step 6 — Update unit tests

Open `xash3dpp/tests/$ARGUMENTS/` (create the directory if it does not exist).

For each pool the subsystem creates, add tests that:
- Verify `get_stats(pool).live_bytes == 0` after a complete create/use/destroy cycle.
- Verify `get_stats(pool).total_allocs > 0` after normal use (proves allocations
  are actually routed through the pool).
- If the subsystem has an error path that short-circuits cleanup, verify that the
  error path also reaches `live_bytes == 0` before `destroy_pool`.

Follow the project test style: `#include "../test_helpers.hpp"`,
`static int g_pass = 0, g_fail = 0;`, one `static void test_*()` per scenario,
plain `main()`. See [`xash3dpp/tests/memory/test_memory.cpp`](../../xash3dpp/tests/memory/test_memory.cpp)
for pool lifecycle test patterns.

---

## Step 7 — Checklist before finishing

- [ ] No `malloc` / `free` / `new` / `delete` remain for long-lived subsystem
      allocations (grep to confirm).
- [ ] Every `create_pool` is paired with a `destroy_pool`.
- [ ] `destroy_pool` is always preceded by full deallocation of pool contents.
- [ ] Pool names follow the `"subsystem"` or `"subsystem/sub-lifetime"` convention.
- [ ] `#include <xash3dpp/memory/memory.hpp>` is present in every migrated file.
- [ ] Unit tests verify zero `live_bytes` after the subsystem lifecycle.
- [ ] `CMakeLists.txt` for the subsystem links `xash3dpp_memory`.
