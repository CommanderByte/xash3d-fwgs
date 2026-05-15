# filesystem — Architecture Overview

> **Source**: `xash3dpp/src/filesystem/`\
> **Public API**: `xash3dpp/include/xash3dpp/filesystem/`\
> **Legacy reference**: `filesystem/filesystem.c`, `filesystem/filesystem.h`, `filesystem/pak.c`, `filesystem/zip.c`, `filesystem/wad.c`, `filesystem/dir.c`, `filesystem/VFileSystem009.cpp`

## Purpose

The filesystem module provides a **virtual, layered game-asset I/O system** over a
stack of *search paths*. Callers open, read, search for, and write game assets by
virtual name; the module resolves each name against each active search path in
priority order and returns the first match. It owns game-directory discovery,
game-info parsing, DLL/library path resolution, and case-insensitive filename
emulation on POSIX volumes.

It does **not** parse game-specific data formats (BSP, MDL, SPR, etc.), manage
network I/O, implement resource-pack decompression beyond zlib deflate (handled
transparently inside `OsFile`), or execute any game logic.

## Design goals

- **First-match layered search**: the last-added path has highest priority;
- `clear_paths` + rebuild fully re-orders the stack without restarting the engine.
- **Thread-safe for concurrent reads**: `open`, `load_file`, `file_exists`, and all
  query methods acquire a shared reader lock; mutation methods hold an exclusive
  writer lock. Individual `File` handles are caller-owned (`unique_ptr<File>`) and
  never shared.
- **No C ABI**: the public API uses `std::string_view`, `std::vector<std::byte>`,
  and `unique_ptr<File>`. No `const char *` overloads, no function-pointer tables,
  no `extern "C"`.
- **Pimpl isolation**: the `Filesystem` class header includes no internal types.
  `SearchPath`, `ISearchBackend`, `OsFile`, and `CIDirectory` are all hidden behind
  the pimpl wall.
- **Pool-backed allocations**: `Filesystem::Impl` creates a single `PoolHandle` in
  `init()`. All backends, `OsFile`, and `MemFile` are allocated from that pool.
  `mem_free` is called through a custom `operator delete` on `File` and
  `ISearchBackend`, so `unique_ptr` destruction works correctly through base
  pointers.
- **Portable via `xash3dpp_platform`**: all OS-level I/O is declared in
  `platform/os_io.hpp` (namespace `xash::platform`) and implemented in the
  `xash3dpp_platform` module. The filesystem module consumes it as a private
  dependency; no platform `#ifdef` blocks appear in filesystem source files.
- **No exceptions, no RTTI**: compiled with `/EHs-c-` and `/GR-`. Errors propagate
  as `nullptr` returns or empty `std::optional`/`std::vector`.

## Key invariants

- `Filesystem::init()` must be called **before** any other method and from the
  main thread before worker threads start.
- `Filesystem::shutdown()` must be called after all outstanding `File` handles
  have been destroyed; their `operator delete` calls `mem_free` into the pool that
  `shutdown()` will destroy.
- `clear_paths()` removes all non-`Static` entries atomically under an exclusive
  lock; a concurrent reader holding a `shared_lock` may observe an empty path list
  during the brief rebuild window (TOCTOU — see [filesystem-facade.md](./filesystem-facade.md)).
- The `pool_` field inside `Filesystem::Impl` must outlive every `SearchPath`
  (and therefore every `ISearchBackend`) stored in `search_paths`.
- Archive backends (`PakBackend`, `ZipBackend`, `WadBackend`) are immutable after
  construction; their entry tables are populated in the constructor and never
  modified.
- `DirBackend`'s `CIDirectory` cache is protected by its own `std::mutex`; this is
  the only intra-backend lock.

## Relationship to legacy code

The legacy `filesystem/` plugin is a separately-loaded DLL/SO that exports a
`GetFSAPI` function returning an `fs_api_t` function-pointer table. The rewrite
replaces this with a statically-linked `xash3dpp_filesystem` target and a typed
C++ class (`Filesystem`). Key changes:

- The hand-rolled vtable (`pfnOpenFile`, `pfnFindFile`, etc. in `searchpath_t`) is
  replaced by a proper virtual interface (`ISearchBackend`).
- Caller-provided output buffers (`char *buf, size_t size`) are replaced by
  `std::string` and `std::vector<std::byte>` return values.
- The shared global `file_t` pool is replaced by per-`Filesystem` `PoolHandle`
  allocation.
- The `VFileSystem009` GoldSrc compatibility vtable is preserved as an *optional*
  shim translation unit (`vfs009/vfs009.cpp`) compiled behind the
  `XASH_VFS009_SHIM` CMake option.
- The `gameinfo_parser` free functions (parse / serialise / fixup) live in
  `xash3dpp_utilities` now, with no filesystem dependency.
- Platform OS I/O (`os_io.hpp`, `OsFd`, all per-OS `.cpp` files) has moved to
  the `xash3dpp_platform` module. `xash3dpp_filesystem` links it as a private dep.

## Architecture at a glance

```text
 ┌──────────────────────────────────────────────────────────────┐
 │  Caller (engine / editor / tests)                            │
 │  Filesystem (public pimpl facade)                            │
 └──────────────────┬───────────────────────────────────────────┘
                    │ holds unique_ptr<Impl>
 ┌──────────────────▼───────────────────────────────────────────┐
 │  Filesystem::Impl                                            │
 │    search_paths (deque<SearchPath>)  paths_mutex             │
 │    active_game (GameInfo)            game_mutex              │
 │    pool_  (PoolHandle)                                       │
 └───┬───────────────────────────────────────────────────────── ┘
     │ each SearchPath holds unique_ptr<ISearchBackend>
 ┌───▼──────────────────────────────────────────────────────────┐
 │  ISearchBackend (virtual interface)                          │
 │  ├── DirBackend   (plain directory + CIDirectory cache)      │
 │  ├── PakBackend   (Quake PAK)                                │
 │  ├── WadBackend   (GoldSrc WAD2 / WAD3)                      │
 │  ├── ZipBackend   (ZIP / PK3)                                │
 │  ├── Pk3DirBackend (PK3 directory)                           │
 │  └── AndroidBackend (AAsset — XASH_ANDROID only)            │
 └───────────────────────────────────────────────────────────── ┘
 ┌───────────────────────────────────────────────────────────── ┐
 │  File (abstract streaming handle)                            │
 │  ├── OsFile   (native fd + optional zlib inflate)            │
 │  └── MemFile  (in-memory vector, used by WadBackend)         │
 └───────────────────────────────────────────────────────────── ┘
 ┌───────────────────────────────────────────────────────────── ┐
 │  platform/os_io.hpp (OS I/O declarations)                    │
 │  ├── posix.cpp   (Linux / macOS)                             │
 │  ├── win32.cpp   (Windows)                                   │
 │  └── android.cpp (Android AAsset — XASH_ANDROID only)       │
 └───────────────────────────────────────────────────────────── ┘
 ┌───────────────────────────────────────────────────────────── ┐
 │  vfs009/vfs009.cpp   (IFileSystem009 shim — optional)        │
 └───────────────────────────────────────────────────────────── ┘
```

## Index of concepts

- [index.md](./index.md) — full file/symbol index
- [filesystem-facade.md](./filesystem-facade.md) — `Filesystem` class, pimpl, lifecycle, threading model
- [search-path.md](./search-path.md) — `SearchPath`, `SearchPathFlags`, layered lookup, mount hierarchy
- [backend-interface.md](./backend-interface.md) — `ISearchBackend`, `archive_registry.hpp`, mount order
- [archive-backends.md](./archive-backends.md) — all concrete backends (Dir, PAK, WAD, ZIP, PK3, Android)
- [file-io.md](./file-io.md) — `File`, `OsFile`, `MemFile`, `OsFd`, streaming, zlib, pool-aware dealloc
- [platform-layer.md](./platform-layer.md) — `platform/os_io.hpp`, `CIDirectory`, platform implementations
- [vfs009-shim.md](./vfs009-shim.md) — `VFileSystem009` compatibility shim, CMake option
