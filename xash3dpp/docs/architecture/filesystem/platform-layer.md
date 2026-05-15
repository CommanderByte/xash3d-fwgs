# Platform Layer

> **Defined in**: `xash3dpp/include/xash3dpp/private/filesystem/platform/os_io.hpp`,
> `xash3dpp/include/xash3dpp/private/filesystem/ci_directory.hpp`,
> `xash3dpp/src/filesystem/platform/posix.cpp`,
> `xash3dpp/src/filesystem/platform/win32.cpp`,
> `xash3dpp/src/filesystem/platform/android.cpp`,
> `xash3dpp/src/filesystem/ci_directory.cpp`  
> **Namespace**: `xash::filesystem::platform` / `xash::filesystem`

## Overview

The platform layer has two parts:

1. **`platform/os_io.hpp`** — a pure-declaration header that abstracts all
   OS-level I/O into typed C++ functions. Each platform (POSIX, Win32, Android)
   provides one or two translation units implementing those declarations. No
   `#ifdef` blocks appear in any code *above* this layer.

2. **`CIDirectory`** — a case-insensitive directory resolver that detects
   whether the underlying volume handles case folding natively, and if not,
   emulates it with a per-subdirectory lazy name-cache. It sits just above the
   platform layer (uses `platform::list_directory`) but below the backends
   (used exclusively by `DirBackend`).

---

## `platform/os_io.hpp` — OS I/O declarations

**Namespace**: `xash::filesystem::platform`

### `OpenMode` enum

```cpp
enum class OpenMode : unsigned {
    ReadOnly  = 0,
    WriteOnly = 1,
    ReadWrite = 2,
    Append    = 4,
    Create    = 8,
    Truncate  = 16,
    Memory    = 32,  // Linux memfd_create; stub elsewhere
};
```

Bitwise `|`, `&`, and `any()` are provided as `constexpr` free functions in the
same header.

### File open / close

| Function | Signature | Notes |
|----------|-----------|-------|
| `open_file` | `OsFd(path, OpenMode)` | Returns invalid `OsFd` on failure |
| `open_memfd` | `OsFd(name)` | `memfd_create` on Linux; stub on other platforms |
| `close_fd` | `void(int)` | Called by `OsFd::close()` |

### Read / write / seek

| Function | Signature | Notes |
|----------|-----------|-------|
| `read` | `int64_t(OsFd&, void*, size_t)` | POSIX retries on `EINTR` |
| `write` | `int64_t(OsFd&, const void*, size_t)` | |
| `seek` | `int64_t(OsFd&, int64_t offset, int whence)` | `whence` uses `SEEK_SET` etc. |
| `tell` | `int64_t(OsFd&)` | |
| `flush` | `void(OsFd&)` | `fsync` (POSIX) / `_commit` (Win32) |

### File metadata

| Function | Signature | Notes |
|----------|-----------|-------|
| `file_size` | `optional<int64_t>(path)` | Returns `nullopt` on error |
| `file_time` | `optional<file_time_type>(path)` | Returns `nullopt` on error |

### Directory operations

| Function | Signature | Notes |
|----------|-----------|-------|
| `list_directory` | `vector<string>(path)` | Returns all entries except `.` and `..` |
| `is_case_insensitive` | `bool(path)` | Probes volume; drives `CIDirectory` mode |
| `make_directory` | `bool(path)` | Creates a directory (and parents); returns false on failure |
| `rename_file` | `bool(from, to)` | Atomic rename where the OS supports it |
| `delete_file` | `bool(path)` | |
| `copy_file` | `bool(from, to)` | Byte-level copy fallback |

### Platform-specific implementations

**`posix.cpp`** (Linux / macOS):
- `open_file`: maps `OpenMode` to `open(2)` flags (`O_RDONLY`, `O_WRONLY`, etc.)
- `read` / `write`: loops on `EINTR`
- `is_case_insensitive`: on Linux probes `ioctl(FS_IOC_GETFLAGS)` for
  `FS_CASEFOLD_FL`; on macOS uses `getattrlist` for `VOL_CAP_FMT_CASE_SENSITIVE`
  (inverted)
- `list_directory`: uses POSIX `opendir` / `readdir`

**`win32.cpp`**:
- `open_file`: uses `CreateFileW` with `GENERIC_READ`/`WRITE` + relevant flags
- `is_case_insensitive`: always returns `true` (NTFS is case-insensitive by
  default; the raw Win32 API is case-sensitive but standard usage is not)
- `list_directory`: uses `FindFirstFileW` / `FindNextFileW`

**`android.cpp`** (`XASH_ANDROID` only):
- Provides `list_assets(path)` — calls JNI
  `android.content.res.AssetManager.list()` via `g_jni.env`
- Provides `open_asset_fd(path)` — calls `AAsset_openFileDescriptor`
- Provides `android_init_jni(env, context, asset_manager)` — stores JNI
  globals; called once from JNI `onLoad`

**Android JNI hazard**: `g_jni` and `g_handles` in `android.cpp` are file-scope
globals written in `android_init_jni()` and read in `get_asset_manager()` and
`list_assets()`. There is no lock. Concurrent JNI calls from different threads
would race on `g_handles[i].mgr` (classic unsynchronised double-checked
lazy-init). Safe in expected usage (JNI init runs before any query threads start)
but the contract is implicit.

---

## `CIDirectory` — case-insensitive directory resolver

**Header**: `private/filesystem/ci_directory.hpp`  
**Source**: `ci_directory.cpp`  
**Namespace**: `xash::filesystem`

### Purpose

On case-sensitive filesystems (most Linux), looking up `"VALVE/pak0.PAK"` when
the file is named `"valve/pak0.pak"` requires a case-insensitive match that the
OS will not perform automatically. `CIDirectory` detects at construction time
whether the underlying volume already handles this (Windows, macOS, Linux with
`CASEFOLD_FL`), and only activates a software emulation path when needed.

### Fields

| Name | Type | Role |
|------|------|------|
| `mode_` | `Mode` (enum) | `Native` or `Emulated` |
| `root_` | `std::string` | Absolute path to the root of this directory tree |
| `cache_` | `unordered_map<string, vector<string>>` | Map of `subdir → sorted entry names` (Emulated only) |
| `cache_mutex_` | `std::mutex` | Guards `cache_` |

### Key operations

#### `Resolve(subdir, name) → optional<string>`

In `Native` mode: checks existence of `root_/subdir/name` with
`platform::file_size`; returns `name` unchanged if found, `nullopt` otherwise.

In `Emulated` mode:
1. Calls `get_or_populate(root_/subdir)` — builds a sorted entry list if not
   cached.
2. Binary-searches the list case-insensitively for `name`.
3. Returns the exact on-disk spelling, or `nullopt`.

#### `Glob(subdir, pattern, case_insensitive) → vector<string>`

In `Native` mode: calls `platform::list_directory(root_/subdir)` and filters by
`xash::utilities::match_pattern`.

In `Emulated` mode: uses the same lazy-populated cache; applies the same filter.

#### `Invalidate(subdir)`

Acquires `cache_mutex_` and erases the entry for `root_/subdir` from `cache_`.
Called by `DirBackend::InvalidateDirectory` after a write.

#### `get_or_populate(dir)` (private)

Acquires `cache_mutex_`, checks if `cache_[dir]` exists, populates it from
`platform::list_directory` if not, sorts alphabetically, and returns a const
reference.

### Threading model

All three public methods (`Resolve`, `Glob`, `Invalidate`) acquire
`cache_mutex_` before touching `cache_`. Concurrent readers, writers, and
invalidators on the same `CIDirectory` instance are fully serialised.

`mode_` and `root_` are written once in the constructor and read-only thereafter;
they do not require locking.

### Lifecycle / ownership

`CIDirectory` is a value member of `DirBackend` (constructed inline, not
heap-allocated separately). It is alive as long as the `DirBackend` is alive.

---

## Threading model

Platform I/O functions (`posix.cpp`, `win32.cpp`) are **pure functions** of their
arguments: they carry no file-scope mutable state and are safe to call
concurrently from any thread.

`CIDirectory` is safe for concurrent use through its `cache_mutex_`.

`android.cpp` JNI globals have an unmitigated race as described above.

## See also

- [file-io.md](./file-io.md) — `OsFile`, `OsFd` that use `platform::read` / `platform::seek`
- [archive-backends.md](./archive-backends.md) — `DirBackend` that owns a `CIDirectory`
- [docs/threading-analysis/filesystem-threading.md](../../threading-analysis/filesystem-threading.md) — Android JNI hazard detail
