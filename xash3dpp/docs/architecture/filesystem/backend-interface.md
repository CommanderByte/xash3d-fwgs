# Backend Interface

> **Defined in**: `xash3dpp/include/xash3dpp/private/filesystem/i_search_backend.hpp`,
> `xash3dpp/include/xash3dpp/private/filesystem/archive_registry.hpp`\
> **Namespace**: `xash::filesystem`

## Overview

`ISearchBackend` is the polymorphic interface that every mounted path backend
implements. `Filesystem::Impl` holds a `std::deque<SearchPath>` where each entry
owns a `std::unique_ptr<ISearchBackend>`. The facade iterates this deque and
delegates each operation to the appropriate backend, stopping at the first hit.

The `archive_registry.hpp` header provides a compile-time table (`k_archive_types`)
mapping file extensions to `BackendFactory` function pointers, so
`add_game_directory` can auto-mount any archive type by extension without a
`switch`-on-string anywhere in the main filesystem code.

## `ISearchBackend`

### Fields / members

| Name | Type | Role |
|------|------|------|
| `pool_` | `xash::memory::PoolHandle` (`protected`) | Pool for allocating `File` handles returned by `open_file` |

`pool_` is initialised in the single base constructor
`explicit ISearchBackend(xash::memory::PoolHandle pool)` and is accessible to
all derived classes as a `protected` member. Derived constructors receive `pool`
as their first argument and pass it to the base.

### Key operations

#### `info() const → std::string`

Returns a human-readable path description for debug / `FS_Path_f` output.
Replaces the legacy `pfnPrintInfo(char *dst, size_t size)` out-buffer pattern.

#### `open_file(path, mode) → unique_ptr<File>`

Returns a new file handle allocated from `pool_`, or `nullptr` if the path
does not exist in this backend. Archive backends (PAK, ZIP, WAD) call
`create_os_file(pool_, fd, length, offset, deflated)` or construct a `MemFile`.
`DirBackend` resolves `path` through `CIDirectory`, then calls `create_os_file`.

**Thread-safety**: archive backends are immutable after construction — `open_file`
performs only reads on their entry tables and is safe to call concurrently.
`DirBackend::open_file` acquires the `CIDirectory` cache mutex internally if
the volume requires case-insensitive emulation.

#### `file_time(path) → optional<file_time_type>`

Returns the mtime of the named entry, or `nullopt` if not found. Archive
backends return their archive file's mtime (cached in the constructor).

#### `find_file(path) → optional<string>`

Case-insensitive name resolution. Returns the canonical (exact on-disk) spelling
of `path` within this backend, or `nullopt`. Used by `file_exists` and `disk_path`.

#### `search(pattern, case_insensitive) → vector<string>`

Glob search. Returns all entry names within this backend whose paths match
`pattern`. `DirBackend` delegates to `CIDirectory::Glob`; archive backends use
`archive_search_by_name()` from `archive_helpers.hpp`.

#### `load_file(path) → vector<byte>`

Whole-file load. Returns the full file contents or an empty vector. Avoids
allocating a `File` handle for the common one-shot load pattern.

#### `invalidate_directory(subdir)` (virtual, default no-op)

Signals that a write has occurred in `subdir`. `DirBackend` overrides this to
call `CIDirectory::Invalidate(subdir)`, purging the name cache for that
subdirectory. Archive backends do nothing (their contents are immutable).

### Pool-aware deallocation

```cpp
static void operator delete(void* p) noexcept { xash::memory::mem_free(p); }
static void operator delete(void* p, std::size_t) noexcept { xash::memory::mem_free(p); }
```

These overrides are on the *base class*. C++ dynamic dispatch for deallocation
searches the *dynamic type* first, then falls back to the base, so derived
backends that do not declare their own `operator delete` automatically use
`mem_free`. This means `std::unique_ptr<ISearchBackend>` correctly reclaims
pool memory without each derived class needing to repeat the override.

### Lifecycle / ownership

Backend objects are allocated via `pool_new<Derived>(pool, pool, ...)` inside
each backend's static `Create()` factory. The `unique_ptr<ISearchBackend>` inside
`SearchPath` owns the allocation; when the `SearchPath` is destroyed (e.g. by
`clear_paths()`), `unique_ptr`'s destructor calls `~Derived()`, then dispatches
`operator delete` to `mem_free`.

The `pool_` handle carried inside each backend is a lightweight 32-bit index. It
does **not** extend the pool's lifetime; the pool is owned exclusively by
`Filesystem::Impl::pool_` and must outlive all backends.

### `is_write_mode(mode)` helper

```cpp
inline bool is_write_mode(std::string_view mode) noexcept;
```

Returns `true` if `mode` contains `'w'` or `'a'`. All read-only backends
(PAK, ZIP, WAD) call this at the top of `open_file` and return `nullptr`
immediately for write/append requests.

## `ArchiveType` and `k_archive_types`

```cpp
struct ArchiveType {
    std::string_view extension;   // "pak", "pk3", "pk3dir", "wad"
    SearchPathFlags  default_flags;
    bool             mounts_wads;
    bool             allow_exec;
    BackendFactory   factory;
};

inline constexpr std::array<ArchiveType, 4> k_archive_types = {{
    { "pak",    SearchPathFlags::Exec, true,  true,  &backends::create_pak    },
    { "pk3",    SearchPathFlags::None, true,  false, &backends::create_zip    },
    { "pk3dir", SearchPathFlags::None, true,  false, &backends::create_pk3dir },
    { "wad",    SearchPathFlags::None, false, false, &backends::create_wad    },
}};
```

`add_game_directory` iterates `k_archive_types` in array order for each directory
entry; archives earlier in the array have **lower** priority than archives later
in the same `add_game_directory` call (all archives are pushed before the plain
directory, which is added last and therefore highest priority). Within the same
type (e.g. multiple `.pak` files), alphabetical order determines priority —
`pak1.pak` beats `pak0.pak` because it is pushed later.

`BackendFactory` is:

```cpp
using BackendFactory =
    std::unique_ptr<ISearchBackend>(*)(xash::memory::PoolHandle pool,
                                       std::string_view         path,
                                       SearchPathFlags          flags);
```

The `create_*` free functions are declared in `archive_registry.hpp` and
defined in the respective backend `.cpp` files. They are thin wrappers that
call `backends::XxxBackend::Create(pool, path, flags)`.

## Threading model

`k_archive_types` is `inline constexpr` — read-only for the process lifetime.
No synchronisation is needed to read it.

`ISearchBackend` itself has no instance-level synchronisation; thread safety is
provided by the caller (`paths_mutex` in `Filesystem::Impl`). After a backend
is constructed and pushed into `search_paths`, it is accessed only under a
`shared_lock`, which is sufficient because archive backends are immutable and
`DirBackend` carries its own `CIDirectory` mutex.

## See also

- [archive-backends.md](./archive-backends.md) — concrete backend implementations
- [search-path.md](./search-path.md) — how `ISearchBackend` instances are stored and iterated
- [file-io.md](./file-io.md) — `File` handles returned by `open_file`
