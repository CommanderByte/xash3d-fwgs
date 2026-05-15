# filesystem — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `filesystem/filesystem.hpp` | `xash::filesystem` | `Filesystem`, `SearchResult` |
| `filesystem/file.hpp` | `xash::filesystem` | `File`, `FsOffset`, `SeekOrigin` |
| `filesystem/search_path_flags.hpp` | `xash::filesystem` | `SearchPathFlags` (enum class + operators) |
| `filesystem/gameinfo.hpp` | `xash::filesystem` | redirect shim — includes `xash/gameinfo.hpp` |

`GameInfo` is defined in `include/xash3dpp/gameinfo.hpp` (namespace `xash::`)
and is compiled as part of `xash3dpp_utilities`. The `filesystem/gameinfo.hpp`
shim re-exports it via `using GameInfo = ::xash::GameInfo` for backward
compatibility.

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/filesystem/i_search_backend.hpp` | `ISearchBackend` virtual interface; `is_write_mode` helper |
| `private/filesystem/search_path.hpp` | `SearchPath` aggregate (`backend`, `source_path`, `flags`) |
| `private/filesystem/archive_registry.hpp` | `ArchiveType`, `BackendFactory`, `k_archive_types` constexpr table |
| `private/filesystem/archive_helpers.hpp` | `ci_find_by_name`, `archive_search_by_name` templates; `CiNameLess` |
| `private/filesystem/os_file_factory.hpp` | `make_os_file()`, `mode_flags()` |
| `private/filesystem/mem_file.hpp` | `MemFile` fully-in-memory `File` implementation |
| `private/filesystem/ci_directory.hpp` | `CIDirectory` — case-insensitive directory resolver |

> **Note**: `OsFd` and the OS I/O declarations live in `xash3dpp_platform`:
> `platform/os_fd.hpp` and `platform/os_io.hpp` (namespace `xash::platform`).
> They are consumed here as a private dependency; see [platform-layer.md](./platform-layer.md).
| `private/filesystem/backends/dir_backend.hpp` | `DirBackend` — plain OS directory |
| `private/filesystem/backends/pak_backend.hpp` | `PakBackend` — Quake PAK archive |
| `private/filesystem/backends/wad_backend.hpp` | `WadBackend` — GoldSrc WAD2/WAD3 |
| `private/filesystem/backends/zip_backend.hpp` | `ZipBackend` — ZIP / PK3 archive |
| `private/filesystem/backends/pk3dir_backend.hpp` | `Pk3DirBackend` — loose PK3 directory |
| `private/filesystem/backends/android_backend.hpp` | `AndroidBackend` — AAsset (XASH_ANDROID only) |
| `private/filesystem/vfs009/vfs009.hpp` | `vfs009::create_vfs009_interface()` |

## Source files

| File | Responsibility |
|------|---------------|
| `src/filesystem/filesystem.cpp` | `Filesystem::Impl`, all method bodies, path helpers |
| `src/filesystem/file.cpp` | `OsFile` concrete class; `make_os_file()`; `File::operator delete` |
| `src/filesystem/ci_directory.cpp` | `CIDirectory` implementation (native-vs-emulated probe, lazy cache) |
| `src/filesystem/backends/dir_backend.cpp` | `DirBackend` |
| `src/filesystem/backends/pak_backend.cpp` | `PakBackend` (PAK header parse, sorted entry table) |
| `src/filesystem/backends/wad_backend.cpp` | `WadBackend` (WAD2/WAD3 lump table, `MemFile` serving) |
| `src/filesystem/backends/zip_backend.cpp` | `ZipBackend` (miniz central directory, optional deflate via `OsFile`) |
| `src/filesystem/backends/pk3dir_backend.cpp` | `Pk3DirBackend` (delegates to `DirBackend`) |
| `src/filesystem/backends/android_backend.cpp` | `AndroidBackend` (AAsset fd extraction) |
| `src/filesystem/vfs009/vfs009.cpp` | `IFileSystem009` shim (compiled when `XASH_VFS009_SHIM=ON`) |

> OS I/O platform TUs (`posix/os_io.cpp`, `win32/os_io.cpp`, `android/os_io.cpp`) now live
> in `src/platform/` under `xash3dpp_platform`, not in this module.

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `Filesystem` | class (pimpl) | `filesystem/filesystem.hpp` | Public API surface |
| `Filesystem::Impl` | struct | `filesystem.cpp` | All mutable state; holds `search_paths`, mutexes, `pool_` |
| `File` | abstract class | `filesystem/file.hpp` | Streaming file handle; returned as `unique_ptr<File>` |
| `OsFile` | class (final) | `file.cpp` (anonymous) | Native fd + optional zlib inflate; pool-allocated |
| `MemFile` | class (final) | `private/filesystem/mem_file.hpp` | Vector-backed in-memory file; used by `WadBackend` |
| `OsFd` | class | `platform/os_fd.hpp` (`xash::platform`) | RAII wrapper for raw int fd; owned by `xash3dpp_platform` |
| `ISearchBackend` | abstract class | `private/filesystem/i_search_backend.hpp` | Polymorphic backend interface; pool-allocated |
| `SearchPath` | struct | `private/filesystem/search_path.hpp` | One mounted path entry (`backend` + `source_path` + `flags`) |
| `SearchPathFlags` | enum class | `filesystem/search_path_flags.hpp` | Bitmask for path metadata and mount options |
| `ArchiveType` | struct | `private/filesystem/archive_registry.hpp` | Static descriptor for one archive format |
| `CIDirectory` | class | `private/filesystem/ci_directory.hpp` | Case-insensitive directory resolver; lazy cache |
| `DirBackend` | class (final) | `private/filesystem/backends/dir_backend.hpp` | Plain OS directory backend |
| `PakBackend` | class (final) | `private/filesystem/backends/pak_backend.hpp` | Quake PAK archive backend |
| `WadBackend` | class (final) | `private/filesystem/backends/wad_backend.hpp` | GoldSrc WAD2/WAD3 backend |
| `ZipBackend` | class (final) | `private/filesystem/backends/zip_backend.hpp` | ZIP/PK3 backend (miniz) |
| `Pk3DirBackend` | class (final) | `private/filesystem/backends/pk3dir_backend.hpp` | Loose PK3 directory backend |
| `AndroidBackend` | class (final) | `private/filesystem/backends/android_backend.hpp` | Android AAsset backend |
| `SearchResult` | struct | `filesystem/filesystem.hpp` | Result of `Filesystem::Search()` |
| `FsOffset` | typedef | `filesystem/file.hpp` | `std::int64_t` — file offset / size |
| `SeekOrigin` | enum class | `filesystem/file.hpp` | Typed `SEEK_SET`/`SEEK_CUR`/`SEEK_END` |
| `platform::OpenMode` | enum class | `platform/os_io.hpp` (`xash::platform`) | OS open-flags bitmask; owned by `xash3dpp_platform` |
| `BackendFactory` | function-ptr typedef | `private/filesystem/archive_registry.hpp` | `unique_ptr<ISearchBackend>(*)(pool, path, flags)` |

## Free functions

| Function | Defined in | Purpose |
|----------|-----------|---------|
| `load_direct_file(disk_path)` | `filesystem.cpp` | Bypass VFS — read a file directly by its absolute disk path |
| `rename(from, to)` | `filesystem.cpp` | Rename a file via the highest-priority writable `DirBackend` |
| `remove(path)` | `filesystem.cpp` | Delete a file |
| `crc32_file(path)` | `filesystem.cpp` | CRC-32 checksum of a file; returns `nullopt` on failure |
| `md5_file(path)` | `filesystem.cpp` | MD5 digest of a file; returns 16-byte array or `nullopt` |
| `find_library(name)` | `filesystem.cpp` | Resolve a game library name to an absolute disk path |
| `make_os_file()` | `file.cpp` | Allocate and return a pool-backed `OsFile` |
| `mode_flags()` | `os_file_factory.hpp` | Map fopen mode string → `platform::OpenMode` |
| `is_write_mode()` | `i_search_backend.hpp` | Return true if mode string implies write/append |
| `ci_find_by_name()` | `archive_helpers.hpp` | Binary search entry by case-insensitive name |
| `archive_search_by_name()` | `archive_helpers.hpp` | Glob search across sorted entry vector |
| `vfs009::create_vfs009_interface()` | `vfs009/vfs009.cpp` | Return `IFileSystem009*` wrapping a `Filesystem` |
| `platform::open_file()` | `posix.cpp` / `win32.cpp` | Open a native OS file |
| `platform::list_directory()` | `posix.cpp` / `win32.cpp` | Enumerate a directory |
| `platform::is_case_insensitive()` | `posix.cpp` / `win32.cpp` | Probe volume case sensitivity |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_filesystem` | STATIC | `xash3dpp_utilities`, `xash3dpp_memory` | `xash3dpp_platform`, `xash3dpp_miniz`; `android`, `log` (Android only) |

### CMake options

| Option | Default | Effect |
|--------|---------|--------|
| `XASH_VFS009_SHIM` | `ON` | Compile `vfs009/vfs009.cpp`; adds `XASH_VFS009_SHIM` define |
| (ANDROID platform) | — | Adds `backends/android_backend.cpp` + `platform/android.cpp`; links `android`, `log`; adds `XASH_ANDROID` define |
