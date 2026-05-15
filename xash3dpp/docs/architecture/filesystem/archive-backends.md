# Archive Backends

> **Defined in**: `xash3dpp/include/xash3dpp/private/filesystem/backends/`,
> `xash3dpp/src/filesystem/backends/`  
> **Namespace**: `xash::filesystem::backends`

## Overview

Six concrete classes implement `ISearchBackend`, each covering one mounted-path
type. All six follow the same pattern:

1. A static `Create(pool, path, flags)` factory allocates the backend via
   `pool_new<T>(pool, pool, path, flags)`.
2. The constructor reads any archive header and populates immutable state
   (entry table, file mtime, validity flag).
3. After construction the object is never mutated; all operations are
   read-only on the entry table.
4. `OpenFile` calls `make_os_file(pool_, fd, length, offset, deflated)` for
   streaming access, or constructs a `MemFile` for fully-buffered access.

---

## `DirBackend` — plain OS directory

**Files**: `dir_backend.hpp`, `dir_backend.cpp`

### Fields

| Name | Type | Role |
|------|------|------|
| `root_` | `std::string` | Absolute path to the mounted directory |
| `flags_` | `SearchPathFlags` | Flags from the `AddGameDirectory` call |
| `ci_` | `CIDirectory` | Case-insensitive resolver; may be native or emulated |

### Key operations

- **`OpenFile`**: calls `resolve_path(path)` through `ci_` to get the
  canonical on-disk path, then calls `platform::open_file` and
  `make_os_file(pool_, fd, size)`. Returns `nullptr` on write-mode requests
  targeting a `NoWrite` path, or if the file does not exist.
- **`FindFile`**: calls `ci_.Resolve(subdir, filename)`.
- **`Search`**: calls `ci_.Glob(subdir, pattern, case_insensitive)`.
- **`LoadFile`**: opens + reads into a `std::vector<std::byte>`.
- **`InvalidateDirectory`**: calls `ci_.Invalidate(subdir)` — purges the name
  cache after a write so subsequent `FindFile` sees new entries.

**Write support**: `DirBackend` is the only backend that can service write and
append requests. `OpenFile` with a write mode calls `platform::open_file` with
`platform::OpenMode::WriteOnly | Create | Truncate` (or `Append`), creates
missing intermediate directories via `platform::make_directory`, and returns an
`OsFile` for the output fd.

### Lifecycle / ownership

`CIDirectory ci_` is constructed inline from `root_`; its lazy name-cache is
populated on first `Resolve` / `Glob` call and protected by `CIDirectory`'s own
`std::mutex`.

---

## `PakBackend` — Quake PAK archive

**Files**: `pak_backend.hpp`, `pak_backend.cpp`

### Fields

| Name | Type | Role |
|------|------|------|
| `path_` | `std::string` | Absolute path to the `.pak` file |
| `flags_` | `SearchPathFlags` | Mount flags |
| `entries_` | `vector<Entry>` | Sorted (case-insensitive) list of `{name, offset, size}` |
| `file_time_` | `file_time_type` | Mtime of the PAK file (cached at construction) |
| `valid_` | `bool` | False if the PAK header was malformed or file not found |

`Entry::name` is the full path within the archive (original case from the PAK
directory). Entries are sorted by `CiNameLess` after parsing.

### Key operations

- **Constructor**: opens the PAK, reads the 12-byte header (`PACK` magic,
  offset, size), seeks to the directory, reads all `PakEntry` records, populates
  and sorts `entries_`, caches `file_time_`, closes the fd.
- **`OpenFile`**: calls `find_entry(path)` (binary search via
  `ci_find_by_name`); on hit, opens the PAK file, seeks to `entry.offset`,
  returns `make_os_file(pool_, fd, entry.size, entry.offset)`.
- **`LoadFile`**: same as `OpenFile` but reads the full entry into a vector.
- **`FileTime`**: returns `file_time_` (the PAK's own mtime) regardless of
  which entry is queried.

### Immutability

After the constructor, `entries_` and all other fields are never written.
Concurrent `OpenFile` / `FindFile` / `Search` calls on the same `PakBackend`
instance are safe without any locking.

---

## `WadBackend` — GoldSrc WAD2 / WAD3

**Files**: `wad_backend.hpp`, `wad_backend.cpp`

### Key differences from PakBackend

- WAD2/WAD3 entries (lumps) may be compressed with a GoldSrc-specific
  scheme (type codes `0x43`, `0x40`). The constructor decompresses all entries
  eagerly into `std::vector<std::byte>` payloads stored in `entries_`.
- `OpenFile` returns a `MemFile` wrapping a copy of the decompressed payload —
  no fd is opened per query.
- WAD archives are read-only and have no `Exec` flag by default (they never
  serve native libraries).

### Fields

| Name | Type | Role |
|------|------|------|
| `path_` | `std::string` | Absolute WAD path |
| `flags_` | `SearchPathFlags` | Mount flags |
| `entries_` | `vector<WadEntry>` | `{name, data (vector<byte>)}`, sorted by `CiNameLess` |
| `file_time_` | `file_time_type` | WAD mtime |
| `valid_` | `bool` | False if header malformed |

---

## `ZipBackend` — ZIP / PK3

**Files**: `zip_backend.hpp`, `zip_backend.cpp`

### Key differences

- Uses miniz (`xash3dpp_miniz`) to parse the ZIP central directory.
- Entries may be stored (no compression) or deflated.
- For deflated entries, `OpenFile` opens the ZIP file, seeks to the local file
  header, and returns `make_os_file(pool_, fd, uncompressed_size, data_offset, /*deflated=*/true)`.
  `OsFile` performs incremental zlib inflation on `Read`.
- For stored entries, `real_offset` is the data start and `deflated = false`.

### Fields

| Name | Type | Role |
|------|------|------|
| `path_` | `std::string` | Absolute ZIP path |
| `flags_` | `SearchPathFlags` | Mount flags |
| `entries_` | `vector<ZipEntry>` | `{name, offset, comp_size, uncomp_size, deflated}`, sorted CI |
| `file_time_` | `file_time_type` | ZIP mtime |
| `valid_` | `bool` | |

---

## `Pk3DirBackend` — loose PK3 directory

**Files**: `pk3dir_backend.hpp`, `pk3dir_backend.cpp`

A thin wrapper that delegates entirely to an internal `DirBackend`. Exists so
that `.pk3dir` entries are recognised as a separate archive type in
`k_archive_types` and can have distinct flags or mount-order priority from a
plain `AddGameDirectory` call.

### Lifecycle / ownership

`Pk3DirBackend` owns a `std::unique_ptr<ISearchBackend>` pointing to a
`DirBackend`. Both are allocated from `pool_`.

---

## `AndroidBackend` — Android AAsset

**Files**: `android_backend.hpp`, `android_backend.cpp`  
**Compiled when**: `ANDROID` CMake platform variable is set
(`XASH_ANDROID` preprocessor define).

### Purpose

Provides access to assets bundled in the APK via Android's `AAssetManager` API.
The entry table is populated from `platform::android::list_assets()`, which
calls the JNI-backed `android.content.res.AssetManager.list()`.

### Key differences

- `OpenFile`: calls `platform::android::open_asset_fd(path)` to obtain a file
  descriptor (Android can return a native fd for APK-internal assets). Passes
  the fd to `make_os_file(pool_, fd, len)`.
- `FindFile`: uses a sorted entry table populated at construction, searched via
  `ci_find_by_name`.
- `Create` sets `engine_package = false` for regular game assets; engine-internal
  assets use `engine_package = true` to select the correct `AAssetManager`.
- Has no `Exec` flag; native libraries are never served from APK assets.

### Threading model

The Android JNI global state (`g_jni`, `g_handles` in `platform/android.cpp`)
is written in `android_init_jni()` and read in `get_asset_manager()`. This is
an **unmitigated data race** if `android_init_jni()` can be called concurrently
with `get_asset_manager()`. In practice JNI init runs on the main thread before
any query threads start, but there is no lock or documented contract.

---

## Shared patterns

### `archive_helpers.hpp` templates

All archive backends use two templates from `archive_helpers.hpp`:

```cpp
template<typename T>
const T* ci_find_by_name(const std::vector<T>& entries, std::string_view name);

template<typename T>
std::vector<std::string> archive_search_by_name(
    const std::vector<T>& entries, std::string_view pattern);
```

Both require that `T::name` is a `std::string` and that `entries` is sorted by
`CiNameLess<T>`. `ci_find_by_name` uses `std::lower_bound` with
`xash::utilities::strnicmp`; `archive_search_by_name` uses
`xash::utilities::match_pattern`.

### Pool-aware `operator delete`

All backends inherit `ISearchBackend::operator delete` which calls `mem_free`.
Backend constructors use `pool_new<T>(pool, ...)` to allocate. See
[backend-interface.md](./backend-interface.md) for the full explanation.

## Threading model

All archive backends (`PakBackend`, `WadBackend`, `ZipBackend`, `Pk3DirBackend`)
are **immutable after construction** — their entry tables and metadata are
populated entirely in the constructor and never written afterwards. Concurrent
`OpenFile`, `FindFile`, `LoadFile`, and `Search` calls are safe without any
per-backend locking.

`DirBackend` is also safe for concurrent reads; its `CIDirectory` member handles
its own internal cache locking via `std::mutex`.

The caller (`Filesystem::Impl`) holds `shared_lock(paths_mutex)` during all
read operations, which prevents concurrent `ClearPaths` from destroying a backend
while a query is in flight.

## See also

- [backend-interface.md](./backend-interface.md) — `ISearchBackend` interface these classes implement
- [file-io.md](./file-io.md) — `File` handles returned by `OpenFile`
- [platform-layer.md](./platform-layer.md) — `CIDirectory`, `platform::os_io` used by `DirBackend`
