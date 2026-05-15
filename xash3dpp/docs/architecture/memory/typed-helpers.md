# Typed Helpers

> **Defined in**: `xash3dpp/include/xash3dpp/memory/memory.hpp`\
> **Namespace**: `xash::memory`

## Overview

The typed helpers layer thin wrappers over `mem_alloc`/`mem_free` for C++
objects. They handle constructor and destructor calls, `std::unique_ptr`
integration, and RAII pool lifetime. No new memory management logic is introduced
— every function delegates to the core allocation API.

______________________________________________________________________

## `pool_new<T>`

```cpp
template <typename T, typename... Args>
T* pool_new(PoolHandle pool, Args&&... args) noexcept;
```

1. Calls `mem_alloc(pool, sizeof(T))`.
1. On success, calls `::new (ptr) T(std::forward<Args>(args)...)` (placement
   new).
1. Returns the typed pointer, or `nullptr` if `mem_alloc` failed.

All allocation counter effects of `mem_alloc` apply. Because exceptions are
disabled (`/EHs-c-`), placement new must not throw; use `noexcept` constructors.

______________________________________________________________________

## `pool_delete<T>`

```cpp
template <typename T>
void pool_delete(T* ptr) noexcept;
```

1. No-op if `ptr == nullptr`.
1. Calls `ptr->~T()` (explicit destructor invocation).
1. Calls `mem_free(ptr)`.

All counter effects of `mem_free` apply. Must only be called with pointers
returned by `pool_new<T>` (or `mem_alloc` for POD types); passing foreign
pointers is undefined behaviour.

______________________________________________________________________

## `ScopedPool`

```cpp
class ScopedPool {
public:
    explicit ScopedPool(const char* name) noexcept;
    ~ScopedPool() noexcept;

    ScopedPool(const ScopedPool&) = delete;
    ScopedPool& operator=(const ScopedPool&) = delete;

    PoolHandle handle() const noexcept;
    explicit operator bool() const noexcept;
};
```

RAII wrapper for pool lifetime. Calls `create_pool(name)` in the constructor
and `destroy_pool(handle_)` in the destructor. `operator bool` returns
`handle_.valid()`.

**Invariant**: by the time `~ScopedPool` runs, all allocations from the pool must
have been freed. Violating this triggers the `assert(live_bytes == 0)` in
`destroy_pool`.

```cpp
{
    ScopedPool scratch("scratch");
    if (!scratch) { /* registry full */ }

    void* buf = mem_alloc(scratch.handle(), 4096);
    // … work …
    mem_free(buf);
}   // destroy_pool called here; assert fires if buf was not freed
```

`ScopedPool` is non-copyable. Moving is also not supported (no move constructor).
For heap-allocated pools with shared lifetime, use a raw `PoolHandle` and call
`create_pool`/`destroy_pool` explicitly.

______________________________________________________________________

## `PoolDeleter`

```cpp
struct PoolDeleter {
    template <typename T>
    void operator()(T* ptr) const noexcept { pool_delete(ptr); }
};
```

Custom deleter for use as the second template argument to `std::unique_ptr`.
Calls `pool_delete<T>(ptr)` (destructor + `mem_free`).

______________________________________________________________________

## `pool_ptr<T>`

```cpp
template <typename T>
using pool_ptr = std::unique_ptr<T, PoolDeleter>;
```

`std::unique_ptr` that frees through `pool_delete`. Combine with `pool_new` to
get automatic destruction:

```cpp
pool_ptr<Widget> w { pool_new<Widget>(pool, 42, 1.5f) };
// w is released automatically when it goes out of scope
```

______________________________________________________________________

## Integrating with class-level `operator delete`

The pattern used by `filesystem`'s `File` and `ISearchBackend` is preferable when
a class always lives in a specific pool or any pool:

```cpp
class Foo {
    PoolHandle pool_;
public:
    void operator delete(void* ptr) noexcept { mem_free(ptr); }
    // …
};
```

With this in place, the default `std::unique_ptr<Foo>` deleter (which calls
`delete`) goes through `mem_free`. No `PoolDeleter` or `pool_ptr` is needed at
call sites. The `AllocHeader` carries the pool index, so `mem_free` updates the
right bucket automatically even without knowing which pool `Foo` was allocated
from.

Use `pool_ptr<T>` when `T` does not define `operator delete`, or when you want
the deleter type to be explicit in the `unique_ptr` type signature.

______________________________________________________________________

## Summary

| Helper | When to use |
|--------|------------|
| `pool_new<T>` / `pool_delete<T>` | Manual lifetime management; POD types; test code |
| `ScopedPool` | Single-scope pool — scratch buffers, per-frame arenas |
| `pool_ptr<T>` | Owned, single-owner objects without class-level `operator delete` |
| `PoolDeleter` | As a deleter type when constructing `std::unique_ptr` by other means |
| Class `operator delete` | Objects that always live in this subsystem's allocator |

______________________________________________________________________

## Thread safety

The typed helpers add no synchronisation of their own.

| Helper | Thread safety |
|--------|--------------|
| `pool_new<T>` | Same as `mem_alloc`: safe from multiple threads for the same pool |
| `pool_delete<T>` | Safe from multiple threads; but the destructor `~T()` is the caller's responsibility to serialise |
| `ScopedPool` | Non-thread-safe for the pool lifecycle calls themselves; allocations through `handle()` follow the same rules as `mem_alloc`/`mem_free` |
| `PoolDeleter` / `pool_ptr<T>` | Same as the `unique_ptr` model: the owning thread holds exclusive access to the pointer |

______________________________________________________________________

## See also

- [allocation-api.md](./allocation-api.md) — `mem_alloc`, `mem_free`, and the `AllocHeader` layout
- [pool-registry.md](./pool-registry.md) — `create_pool`, `destroy_pool`, and `PoolHandle` handle space
