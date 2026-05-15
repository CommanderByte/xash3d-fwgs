# OS file I/O

> **Defined in**: `include/xash3dpp/platform/os_fd.hpp`, `include/xash3dpp/platform/os_io.hpp`  
> **Source**: `src/platform/win32/os_io.cpp`, `src/platform/posix/os_io.cpp`, `src/platform/android/os_io.cpp`  
> **Namespace**: `xash::platform`

## Overview

The OS I/O layer provides a RAII file-descriptor wrapper (`OsFd`), a
platform-consistent open-mode flag set (`OpenMode`), and a set of free functions
covering the full POSIX-like file API: open, read, write, seek, stat, directory
listing, and filesystem mutations. An optional Android AAsset bridge is compiled in
on `XASH_ANDROID` builds.

The filesystem subsystem (`xash3dpp_filesystem`) is the primary consumer — it
delegates every OS call to this layer rather than calling `open(2)` / `_wopen`
directly. By isolating system headers in `.cpp` files, the rest of the engine never
sees `<windows.h>` or `<unistd.h>` through transitive includes.

## `OsFd`

**Defined in**: `os_fd.hpp`

`OsFd` owns exactly one OS file descriptor, represented as a CRT `int` (`-1` =
invalid/empty). On Win32 the raw `int` is a CRT descriptor obtained via
`_open_osfhandle`; on POSIX it is a plain POSIX fd.

### Fields

| Name | Type | Role |
|------|------|------|
| `fd_` | `int` | Raw file descriptor; `-1` means no descriptor held |

### Key operations

- **`valid() → bool`**: returns `fd_ >= 0`.
- **`get() → int`**: read-only access to the raw descriptor (e.g. for `fstat`).
- **`release() → int`**: relinquishes ownership without closing. The caller becomes
  responsible for closing the returned descriptor.
- **`close() → void`**: calls `close_fd(fd_)` (defined in the per-platform `os_io.cpp`,
  keeping `<io.h>` / `<unistd.h>` out of the header) and sets `fd_ = -1`. Safe to
  call when `fd_ == -1` (no-op).
- **Move construction / move assignment**: O(1); source becomes invalid (`fd_ = -1`).
  Copying is deleted.

### Lifecycle

A default-constructed `OsFd` holds `fd_ == -1`. The destructor calls `close()`,
which is a no-op when already invalid. Transfer ownership with move construction
or move assignment; `release()` to hand off to code outside this layer.

## `OpenMode`

Bit-flag `enum class` used by `open_file`:

| Value | Meaning |
|-------|---------|
| `ReadOnly` | Open for reading only (value 0) |
| `WriteOnly` | Open for writing only |
| `ReadWrite` | Open for reading and writing |
| `Append` | Writes append to end of file |
| `create` | Create file if it does not exist *(note: lowercase `c` — naming inconsistency in source; all other values are PascalCase)* |
| `Truncate` | Truncate to zero length on open |
| `Memory` | Prefer in-memory backing (`memfd_create` on Linux; temp-file fallback elsewhere) |

`constexpr operator|`, `operator&`, and `any(m)` are provided in the same header.

## File operations

### `open_file(path, mode) → OsFd`

Opens the UTF-8 `path` with the given `OpenMode` flags. Returns an invalid `OsFd`
on any error. Win32 converts `path` to UTF-16 internally before calling `_wopen`.

### `open_memfd(name) → OsFd`

Creates an anonymous in-memory file with a name hint (used in `/proc/fd/…` on
Linux).

| Platform | Mechanism |
|----------|-----------|
| Linux | `memfd_create(name, MFD_CLOEXEC)` |
| Others | A self-deleting temporary file as a fallback |

Returns an invalid `OsFd` on platforms where neither mechanism succeeds.

### `read(fd, buf, size) → int64_t`

Reads up to `size` bytes into `buf`. Returns the number of bytes actually read,
or `-1` on error. No automatic `EINTR` retry — callers that need retry do it
themselves.

### `write(fd, buf, size) → int64_t`

Writes `size` bytes from `buf`. Returns bytes written, or `-1` on error.

### `seek(fd, offset, whence) → int64_t`

POSIX seek semantics: `whence` is `0=SEEK_SET`, `1=SEEK_CUR`, `2=SEEK_END`.
Returns the new file offset, or `-1` on error.

### `tell(fd) → int64_t`

Returns the current file offset, or `-1` on error. Implemented as
`seek(fd, 0, SEEK_CUR)`.

### `flush(fd) → void`

Flushes OS write buffers. POSIX: `fsync(fd.get())`. Win32: `_commit(fd.get())`.

### `close_fd(raw_fd) → void`

Closes a raw integer fd. This is the function called by `OsFd::close()` — it is
kept here so `<io.h>` / `<unistd.h>` stay confined to the `.cpp`. Do not call
directly unless you have obtained a raw fd from `OsFd::release()`.

## File metadata

### `file_size(path) → optional<int64_t>`

Returns the file size in bytes, or `nullopt` if the path does not exist or cannot
be stat'd. Win32: `_stati64`. POSIX: `stat` / `lstat`.

### `file_time(path) → optional<filesystem::file_time_type>`

Returns the last-write time as a `std::filesystem::file_time_type`, or `nullopt`
on failure.

## Directory operations

### `list_directory(path) → vector<string>`

Returns the names (not full paths) of all entries under `path`, excluding `.` and
`..`. Returns an empty vector on error or an empty directory.

Win32: `FindFirstFileW` / `FindNextFileW` loop.  
POSIX: `opendir` / `readdir` loop.

### `is_case_insensitive(path) → bool`

Returns `true` if the volume containing `path` performs case-insensitive name
comparisons natively:

| Platform | Behaviour |
|----------|-----------|
| Win32 | Always `true` (NTFS default) |
| macOS | Always `true` (HFS+/APFS default) |
| Linux | Checks `FS_CASEFOLD_FL` on the inode (kernel ≥ 5.2); `false` if the flag is absent or the syscall is unsupported |
| Other POSIX | Always `false` (conservative default) |

The filesystem layer uses this result to decide whether to skip its in-process
case-insensitive trie search when resolving game paths.

### `make_directory(path) → bool`

Creates the directory at `path`. Returns `true` if the directory was created or
already exists. Does **not** create intermediate directories.

### `rename_file(from, to) → bool`, `delete_file(path) → bool`

Thin wrappers around the OS rename/unlink calls. Return `true` on success.

## Android AAsset bridge

Compiled in only when `XASH_ANDROID` is defined.

| Symbol | Description |
|--------|-------------|
| `AssetManagerHandle` | Opaque struct wrapping `AAssetManager *`; full definition in `android/os_io.cpp` |
| `android_init_jni(env, activity, cls)` | Must be called once at `JNI_OnLoad` before any asset operations; stores JNI references in statics |
| `get_asset_manager(engine_package)` | Obtain the `AssetManagerHandle` via JNI; returns `nullptr` before `android_init_jni` is called |
| `list_assets(mgr, path)` | Names of asset entries directly under `path`; non-recursive; excludes `.` and `..` |
| `asset_exists(mgr, path)` | `true` if the asset exists in the APK assets tree |
| `open_asset(mgr, path)` | Copies the asset into an anonymous in-memory `OsFd`; returned fd is positioned at offset 0 |

The JNI state is stored in statics inside `android/os_io.cpp` — written once at
`JNI_OnLoad`, read-only thereafter.

## Threading model

| Operation | Thread safety |
|-----------|--------------|
| `open_file`, `read`, `write`, `seek`, `tell`, `flush`, `close_fd` | Safe from any thread; no shared mutable state |
| `file_size`, `file_time`, `list_directory` | Safe from any thread |
| `is_case_insensitive`, `make_directory`, `rename_file`, `delete_file` | Safe from any thread |
| `android_init_jni` | Must be called from the JNI-attach thread before any other asset operations |
| `get_asset_manager`, `list_assets`, `asset_exists`, `open_asset` | Safe from any thread after `android_init_jni` returns |

## Error handling

All functions return invalid `OsFd`, `-1`, `false`, `nullopt`, or empty containers
on failure — no exceptions. `open_memfd` returning an invalid `OsFd` is not an
error on platforms that do not support it; callers must check `fd.valid()`.

## Edge cases and invariants

- `OsFd::close()` called on an invalid descriptor (`fd_ == -1`) is a no-op.
- `OsFd` is non-copyable to enforce exclusive ownership. Move-constructing from an
  `OsFd` sets the source's `fd_` to `-1`.
- `OpenMode::create` uses a lowercase `c` (source inconsistency; all other values
  are PascalCase). Future values will follow PascalCase.
- `open_file` on an empty path returns an invalid `OsFd`.
- `list_directory` on a non-existent path returns an empty vector rather than an
  error — callers that need to distinguish "empty" from "does not exist" must call
  `file_size` on the directory path first.
- `file_size` uses `stat`-based byte count, matching what `seek(SEEK_END)` returns.

## See also

- [system-utils.md](./system-utils.md) — `LibHandle` (dynlib) in the same namespace
