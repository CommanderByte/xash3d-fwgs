# File I/O

> **Defined in**: `xash3dpp/include/xash3dpp/filesystem/file.hpp`,
> `xash3dpp/include/xash3dpp/private/filesystem/mem_file.hpp`,
> `xash3dpp/include/xash3dpp/platform/os_fd.hpp` (owned by `xash3dpp_platform`),
> `xash3dpp/include/xash3dpp/private/filesystem/os_file_factory.hpp`,
> `xash3dpp/src/filesystem/file.cpp`  
> **Namespace**: `xash::filesystem` (`File`, `OsFile`, `MemFile`); `xash::platform` (`OsFd`)

## Overview

The file-I/O concept covers everything between a backend returning a file handle
and the caller reading data from it. There are three concrete implementations of
`File`: `OsFile` (the common case — backed by a native OS file descriptor with
optional zlib inflate), `MemFile` (fully in-memory, used by `WadBackend`), and
any future implementation.

All file handles are returned as `std::unique_ptr<File>`. The custom `operator
delete` on the `File` base ensures pool-backed allocation is reclaimed correctly
when the `unique_ptr` goes out of scope.

## `File` — abstract streaming handle

**Header**: `filesystem/file.hpp`

### Key operations

| Method | Signature | Notes |
|--------|-----------|-------|
| `Read` | `FsOffset Read(span<byte>)` | Returns bytes read; 0 at end-of-file |
| `Write` | `FsOffset Write(span<const byte>)` | Returns bytes written; -1 if unsupported (MemFile) |
| `Seek` | `FsOffset Seek(FsOffset, SeekOrigin)` | Returns new position; -1 on error |
| `Tell` | `FsOffset Tell() const` | Current logical read position |
| `Length` | `FsOffset Length() const` | Uncompressed (logical) size in bytes |
| `Eof` | `bool Eof() const` | True when `Tell() >= Length()` |
| `Flush` | `void Flush()` | Sync to OS; no-op for read-only and MemFile |
| `Gets` | `optional<string> Gets()` | Read a text line; nullopt at EOF |
| `Getc` | `int Getc()` | Read one byte as int; EOF (-1) at end |
| `UnGetc` | `void UnGetc(int c)` | Push one byte back; one-byte buffer |

`FsOffset` is `std::int64_t`. `SeekOrigin` is a typed enum replacing
`SEEK_SET` / `SEEK_CUR` / `SEEK_END`.

### Pool-aware deallocation

```cpp
static void operator delete(void* p) noexcept           { xash::memory::mem_free(p); }
static void operator delete(void* p, std::size_t) noexcept { xash::memory::mem_free(p); }
```

These overrides are on the base `File` class. C++ deallocation dispatch searches
the *dynamic type* first; since neither `OsFile` nor `MemFile` declares their own
`operator delete`, the base override is always found. This means:

```cpp
std::unique_ptr<File> f = backend->open_file("foo.txt", "rb");
// ... f goes out of scope, calls ~OsFile(), then File::operator delete
// which calls mem_free — correctly reclaims from the pool.
```

### Lifecycle / ownership

`File` objects are non-copyable, non-movable. Callers always hold a
`unique_ptr<File>`. There is no shared ownership; no locking is needed inside
`File` implementations.

---

## `OsFile` — native fd + optional inflate

**Source**: `file.cpp` (class is not exported — defined only in the `.cpp`)

`OsFile` is the standard file handle for every backend except `WadBackend`. It
wraps an `OsFd` (RAII fd), adds a small read-ahead buffer, and supports
transparent zlib inflate for compressed archive entries.

### Fields

| Name | Type | Role |
|------|------|------|
| `fd_` | `OsFd` | Owning native file descriptor |
| `length_` | `FsOffset` | Logical (uncompressed) size |
| `position_` | `FsOffset` | Bytes consumed from raw source (fd or inflate stream) |
| `real_offset_` | `FsOffset` | Byte offset of the entry within an archive (0 for plain files) |
| `deflated_` | `bool` | True if entry is zlib-deflated |
| `zlib_` | `optional<ZlibState>` | Incremental decompressor; present only if `deflated_` |
| `buf_` | `array<byte, limits::filesystem_file_buffer_size>` | Read-ahead buffer (2 KiB) |
| `buf_pos_` | `size_t` | Cursor into `buf_` |
| `buf_len_` | `size_t` | Valid bytes in `buf_` |
| `ungetc_` | `int` | One-byte pushback buffer (`EOF` = empty) |

**Effective position** = `position_ − (buf_len_ − buf_pos_)`. `Tell()` returns
this value; `Eof()` tests it against `length_`.

### Buffered read

`Read` drains `buf_` first, then calls `platform::read` (or `inflate_read`) to
refill. The `limits::filesystem_file_buffer_size`-byte (2 KiB) buffer is optimised for the common pattern of reading small
records (WAD lump metadata, BSP lump headers) without system-call overhead.

### Zlib inflate

When `deflated_ == true`:
- The constructor seeks `fd_` to `real_offset_` and initialises a `ZlibState`
  (`mz_inflateInit2` with raw deflate window, -15 bits).
- `inflate_read` feeds compressed data from `fd_` into miniz's `mz_inflate` in
  64 KiB chunks, writing decompressed bytes to the caller's buffer.
- `Seek` in inflate mode: seeking forward resets and re-inflates from
  `real_offset_` to the target position (O(n) cost). Seeking backward is
  therefore O(n) in the worst case.

### `Flush`

Calls `platform::flush(fd_)` (`fsync` on POSIX, `_commit` on Win32).

### Factory: `make_os_file`

```cpp
std::unique_ptr<File> make_os_file(
    xash::memory::PoolHandle pool,
    OsFd     fd,
    FsOffset length,
    FsOffset real_offset = 0,
    bool     deflated    = false);
```

Defined in `file.cpp`. Allocates `OsFile` via `pool_new<OsFile>(pool, ...)` and
returns it as `unique_ptr<File>`. Backends call this instead of constructing
`OsFile` directly, because `OsFile` is not declared in any header.

---

## `MemFile` — in-memory vector file

**Header**: `private/filesystem/mem_file.hpp`

`MemFile` wraps a `std::vector<std::byte>` and implements the full `File`
interface over it. Used by `WadBackend`, which decompresses all lumps eagerly at
construction time and serves them without further I/O.

### Fields

| Name | Type | Role |
|------|------|------|
| `data_` | `vector<byte>` | Owned byte buffer |
| `len_` | `FsOffset` | `data_.size()`, cached for convenience |
| `pos_` | `FsOffset` | Read cursor |
| `ungetc_` | `int` | One-byte pushback (EOF = empty) |

### Key differences from `OsFile`

- `Write` always returns -1 (read-only).
- `Seek` is O(1); `Seek(SeekOrigin::End, offset)` is `len_ + offset`.
- No buffering layer — `Read` copies directly from `data_`.
- Not pool-allocated: `WadBackend` constructs `MemFile` with a moved vector;
  the `MemFile` object itself is created with `pool_new<MemFile>(pool_, ...)`.
  Its `operator delete` falls back to the base `File` override → `mem_free`.

---

## `OsFd` — RAII file descriptor

**Header**: `platform/os_fd.hpp` (namespace `xash::platform`; owned by `xash3dpp_platform`)

A move-only wrapper around a raw `int` fd. Calls `platform::close_fd(fd_)` in
its destructor. Prevents fd leaks on early-return paths that previously required
careful `goto cleanup` logic in the legacy C code.

| Operation | Behaviour |
|-----------|----------|
| Default-constructed | `fd_ = -1` (invalid) |
| `OsFd(int fd)` | Takes ownership |
| Move | Transfers fd; source becomes invalid |
| `get()` | Returns raw fd (no transfer) |
| `valid()` | True if `fd_ >= 0` |
| `release()` | Detaches and returns raw fd (caller owns it) |
| `close()` | Calls `platform::close_fd`; sets `fd_ = -1` |

`OsFd::close()` is implemented in `src/platform/posix/os_io.cpp` and
`src/platform/win32/os_io.cpp` (calls `::close` / `::CloseHandle` respectively).

---

## `mode_flags` helper

Defined inline in `os_file_factory.hpp`. Maps a C-style `fopen` mode string to
`platform::OpenMode`:

| Mode string | Resulting `OpenMode` |
|-------------|---------------------|
| `"r"`, `"rb"` | `ReadOnly` |
| `"r+"`, `"r+b"` | `ReadWrite` |
| `"w"`, `"wb"` | `WriteOnly | Create | Truncate` |
| `"w+"`, `"w+b"` | `ReadWrite | Create | Truncate` |
| `"a"`, `"ab"` | `WriteOnly | Create | Append` |
| `"a+"`, `"a+b"` | `ReadWrite | Create | Append` |

---

## Threading model

`File` handles are **caller-owned** (`unique_ptr<File>`). No shared ownership
exists; all fields of `OsFile` and `MemFile` are private to one caller.
No locking is needed inside `Read`, `Write`, `Seek`, or any other method.

If a caller passes the same `File` pointer to two threads simultaneously, that is
a programmer error — there is no protection against it.

## Error handling

- `Read` / `Write` return `-1` on I/O error (delegated from `platform::read`).
- `Seek` returns `-1` if the target position is out of range.
- `make_os_file` returns `nullptr` if `pool_new` fails (OOM).
- No exceptions are thrown.

## See also

- [backend-interface.md](./backend-interface.md) — backends that produce `unique_ptr<File>`
- [archive-backends.md](./archive-backends.md) — how backends call `make_os_file` and `MemFile`
- [platform-layer.md](./platform-layer.md) — `platform::read`, `platform::seek`, `platform::close_fd`
