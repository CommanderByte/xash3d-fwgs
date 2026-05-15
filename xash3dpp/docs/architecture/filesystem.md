> This document has been superseded by the directory
> [docs/architecture/filesystem/](./filesystem/README.md).

# Filesystem Module Architecture

> Boundary spec: [docs/boundaries/filesystem-boundary.md](../boundaries/filesystem-boundary.md)  
> Legacy survey: [docs/legacy-survey/filesystem.md](../legacy-survey/filesystem.md)

## Goals

- Replace the legacy C plugin with a **statically-linked C++20 module** inside
  the engine.
- Preserve the first-match, layered-search-path semantics that all existing
  game content depends on.
- Make the backend abstraction explicit via a proper virtual interface instead
  of a hand-rolled vtable.
- Return caller-owned buffers (`std::vector<std::byte>`) from bulk-load
  operations; eliminate the legacy pool/malloc split.
- Be thread-safe for concurrent reads; mount/unmount is an infrequent exclusive
  operation.
- Keep a `VFileSystem009` shim in a separate translation unit for GoldSrc tool
  compatibility.
- **No C compatibility shims.** The filesystem module has no fixed external ABI
  (nothing is exported to game DLLs). No `extern "C"`, no `const char *`
  overloads, no function-pointer tables in the public API.
- **Portable via a `platform/` abstraction layer.** All OS-level I/O (fd
  open/read/write/seek, directory scan, case-sensitivity probe, Android AAsset)
  is declared in `platform/os_io.hpp` and implemented in platform-specific
  translation units. No platform `#ifdef` blocks appear above the `platform/`
  layer.

---

## Source tree layout

**Public headers** (included by other engine subsystems):

```text
xash3dpp/include/xash3dpp/filesystem/
    filesystem.hpp        Filesystem class (pimpl — hides internal types)
    file.hpp              File abstract base class + FsOffset typedef
    gameinfo.hpp          GameInfo redirect shim (canonical: xash3dpp/gameinfo.hpp)
    search_path_flags.hpp SearchPathFlags enum class + bitwise operators
```

`gameinfo_parser.hpp` (pure text transforms) lives at `include/xash3dpp/gameinfo_parser.hpp`
in `namespace xash::` and is compiled as part of `xash3dpp_utilities`. It has
no filesystem dependency.

**Implementation** (internal to the module):

```text
xash3dpp/src/filesystem/
    CMakeLists.txt
    filesystem.cpp            Filesystem::Impl + all method bodies
    file.cpp                  OsFile concrete class (OsFd + optional zlib)
    gameinfo_parser.cpp       GameInfo free functions (parse / serialise / fixup)
    ci_directory.cpp          CIDirectory implementation
    platform/
        os_io.hpp  →  (see include/xash3dpp/private/filesystem/platform/)
        posix.cpp             POSIX (Linux / macOS)
        win32.cpp             Win32
        android.cpp           Android AAsset extensions (XASH_ANDROID only)
    backends/
        dir_backend.cpp
        pak_backend.cpp
        wad_backend.cpp
        zip_backend.cpp
        pk3dir_backend.cpp
        android_backend.cpp
    vfs009/
        vfs009.cpp            VFileSystem009 shim (CMake option, on by default)

xash3dpp/include/xash3dpp/private/filesystem/
    i_search_backend.hpp      ISearchBackend pure virtual interface
    search_path.hpp           SearchPath struct
    archive_registry.hpp      constexpr k_archive_types table
    ci_directory.hpp          CIDirectory class
    os_fd.hpp                 OsFd RAII fd wrapper
    platform/
        os_io.hpp             platform-neutral OS I/O declarations
    backends/
        dir_backend.hpp       plain directory
        pak_backend.hpp       Quake PAK
        wad_backend.hpp       GoldSrc WAD2/WAD3
        zip_backend.hpp       ZIP / PK3
        pk3dir_backend.hpp    pk3dir
        android_backend.hpp   Android AAsset backend
    vfs009/
        vfs009.hpp            VFileSystem009 shim declarations
```

Tests mirror the `src/` layout under `xash3dpp/tests/filesystem/`.

---

## Key abstractions

### `ISearchBackend`

The central polymorphic interface. Every mounted archive type derives from it.

```cpp
class ISearchBackend {
public:
    virtual ~ISearchBackend() = default;

    // Human-readable description (replaces pfnPrintInfo char-buffer pattern).
    virtual std::string     Info() const = 0;

    // Returns nullptr if the file does not exist in this backend.
    virtual std::unique_ptr<File> OpenFile(std::string_view path, std::string_view mode) = 0;

    // Returns nullopt if the file does not exist.
    virtual std::optional<std::filesystem::file_time_type> FileTime(std::string_view path) = 0;

    // Case-insensitive name resolution; returns canonical name or nullopt.
    virtual std::optional<std::string> FindFile(std::string_view path) = 0;

    // Glob search — returns all matching names within this backend.
    virtual std::vector<std::string> Search(std::string_view pattern, bool case_insensitive) = 0;

    // Whole-file load.  Returns empty vector if not found.
    virtual std::vector<std::byte> LoadFile(std::string_view path) = 0;
};
```

### `SearchPathFlags`

Replaces the 27+ `#define FS_*` integer bitmasks from the legacy header.

```cpp
enum class SearchPathFlags : uint32_t {
    None        = 0,
    Static      = 1 << 0,  // survives ClearPaths()
    NoWrite     = 1 << 1,  // never the write target
    GameDir     = 1 << 2,  // part of current game hierarchy
    Exec        = 1 << 3,  // may serve DLL/SO files
    Custom      = 1 << 4,  // user/tool-injected archive
    SkipWads    = 1 << 5,  // do not auto-mount embedded WADs
    // … mount variant flags (HD, LV, Addon, L10n) …
};
// Bitwise operators provided via XASH_DEFINE_ENUM_FLAGS(SearchPathFlags)
```

### `SearchPath`

A mounted entry in the search stack. Owns its backend.

```cpp
struct SearchPath {
    std::unique_ptr<ISearchBackend> backend;
    std::string                     source_path;   // disk path that was mounted
    SearchPathFlags                 flags;
};
```

The active stack is a `std::deque<SearchPath>` stored in the `Filesystem` class.
New mounts are **prepended** (front insertion) so that the most-recently-added
path is searched first. `STATIC_PATH` entries are exempt from `ClearPaths()`.

### `File` (public abstract base)

The type callers hold. `File` is the public interface in
`include/xash3dpp/filesystem/file.hpp`; the concrete `OsFile` class lives
entirely inside `src/filesystem/file.cpp`.

```cpp
// file.hpp — public
using FsOffset = std::int64_t;

class File {
public:
    File()                       = default;
    virtual ~File()              = default;
    File(const File&)            = delete;
    File& operator=(const File&) = delete;

    virtual FsOffset Read(std::span<std::byte> buf)        = 0;
    virtual FsOffset Write(std::span<const std::byte> buf) = 0;
    virtual FsOffset Seek(FsOffset offset, int whence)     = 0;
    virtual FsOffset Tell()   const = 0;
    virtual FsOffset Length() const = 0;  // uncompressed size
    virtual bool     Eof()    const = 0;
    virtual void     Flush()        = 0;

    virtual std::optional<std::string> Gets()       = 0;
    virtual int                        Getc()       = 0;
    virtual void                       UnGetc(int c)= 0;
};
```

`OsFile` (internal) owns an `OsFd` and optional zlib decompression state.
`OsFd` is a value-type RAII wrapper around a native fd (defined in
`src/filesystem/os_fd.hpp`); its `close()` is defined in the platform layer.

### `GameInfo` (public plain-data struct)

In `include/xash3dpp/filesystem/gameinfo.hpp`. All `qboolean` fields become
`bool`; all `char[]` string fields become `std::string`. The struct is **not**
included by `filesystem.hpp` — callers that need game metadata include
`gameinfo.hpp` separately.

```cpp
// filesystem.hpp — no gameinfo.hpp included
std::vector<GameInfo> ScanGameDirectories(std::string_view root) const;
const GameInfo&       GetGameInfo() const;  // valid only after ActivateGame()
```

`ScanGameDirectories` lists subdirectories of `root`, reads `gameinfo.txt` /
`liblist.gam` in each, and calls the public parsing free functions. The host
picks a game from the returned list and calls `ActivateGame(gamefolder, ...)`.  
To write a modified `gameinfo.txt`, the host calls `serialise_gameinfo()` and
passes the result to `Filesystem::WriteFile()`.

### `Filesystem` (public, pimpl)

`Filesystem` in the public header holds only a `std::unique_ptr<Impl>`. The
`Impl` struct in `filesystem.cpp` owns the `std::shared_mutex`, the
`std::deque<SearchPath>`, the `GameInfo` vector, and the write-path pointer.
No internal types (`SearchPath`, `ISearchBackend`, etc.) leak into the public
header.

---

## Archive registry

A `constexpr` table replaces the legacy `g_archives[]`. Each entry names an
extension, points to a factory function, and carries capability flags.

```cpp
struct ArchiveType {
    std::string_view   extension;
    SearchPathFlags    default_flags;
    bool               mounts_wads;   // auto-mount WADs found inside
    bool               allow_exec;    // may serve DLL files

    using Factory = std::unique_ptr<ISearchBackend>(*)(std::string_view path, SearchPathFlags flags);
    Factory factory;
};

constexpr std::array g_archive_types = {
    ArchiveType{ "pak",    EXEC_PATH,   true,  true,  &PakBackend::Create  },
    ArchiveType{ "pk3",    {},          true,  false, &ZipBackend::Create  },
    ArchiveType{ "pk3dir", {},          true,  false, &Pk3DirBackend::Create },
    ArchiveType{ "wad",    {},          false, false, &WadBackend::Create  },
};
```

`AddGameDirectory` iterates `g_archive_types` in order for each directory it
scans, then appends the directory itself. This preserves the legacy
PAK→PK3→WAD→dir mount order.

Each backend stores its file table as a `std::vector` of a plain entry struct
(e.g. `std::vector<DPackEntry>` in `PakBackend`, `std::vector<ZipEntry>` in
`ZipBackend`). The legacy flexible-array-member / separate-allocation pattern
is eliminated (modernization finding M-8).

---

## Platform abstraction

All OS-level I/O is funnelled through free functions declared in
`src/filesystem/platform/os_io.hpp` and implemented in platform-specific
translation units. No `#ifdef _WIN32` / `#ifdef __ANDROID__` blocks appear
above this layer.

| Function | Purpose |
| --- | --- |
| `open_file(path, mode) → OsFd` | Open a file for reading or writing |
| `open_memfd(name) → OsFd` | Anonymous in-memory file (Linux `memfd_create`; stub elsewhere) |
| `read / write / seek / tell / flush` | Thin wrappers over OS primitives |
| `close_fd(int)` | Called by `OsFd` destructor |
| `file_size / file_time` | Metadata without opening the file |
| `list_directory(path)` | Return all entries (no `.` / `..`) |
| `is_case_insensitive(path)` | Probe: returns `true` on Windows, macOS, Linux dirs with `CASEFOLD_FL` |
| `make_directory / rename_file / delete_file` | Write-path mutations |

Android-specific extensions (AAsset manager retrieval, asset listing via JNI)
live in `platform/android.cpp` and are consumed only by `backends/android_backend.cpp`.

---

## Threading model

The `Filesystem` class holds a `mutable std::shared_mutex` protecting the
`deque<SearchPath>`.

| Operation | Lock mode |
| --- | --- |
| `Open` / `FindFile` / `FileExists` / `Search` | `shared_lock` (read) |
| `AddGameDirectory` / `ClearPaths` / `Rescan` | `unique_lock` (write) |
| `Read` / `Write` / `Seek` on an open `File` | **none** — `File` is independently owned by the caller |
| `GetGameInfo()` | no lock — `GameInfo` is only mutated during `LoadGameInfo`, which holds a write lock |

Concurrent reads across multiple files from different threads are safe without
any global lock contention. Mount/unmount is infrequent and takes the write lock
for the duration of the path rebuild.

---

## Case-insensitive directory backend

`CIDirectory` is a separate, reusable class used by `DirBackend` and
`Pk3DirBackend`. At construction it probes the volume:

1. **Windows / macOS**: volume is inherently case-insensitive — no trie needed.
   `Resolve(name)` delegates directly to the OS.
2. **Linux, `FS_CASEFOLD_FL` present** on the root directory: same as above.
3. **Otherwise**: build a sorted name vector per directory on first access
   (lazy), binary-search for case-insensitive match.

The probe result is stored as an enum `CaseSensitivity { Native, Emulated }` so
backends can skip the trie unconditionally on non-case-sensitive builds.

```cpp
class CIDirectory {
public:
    explicit CIDirectory(std::string_view root_path);

    // Returns the canonical on-disk name for `name`, or nullopt.
    std::optional<std::string> Resolve(std::string_view name);
    std::vector<std::string>   Glob(std::string_view pattern, bool ci);

private:
    enum class Mode { Native, Emulated };
    Mode                                           mode_;
    std::string                                    root_;
    // Only used in Emulated mode:
    std::unordered_map<std::string /*dir*/, std::vector<std::string>> cache_;
    mutable std::mutex                             cache_mutex_;
};
```

---

## VFileSystem009 shim

`vfs009.cpp` is a **separate optional translation unit** (controlled by a CMake
option, on by default). It defines a class `VFileSystem009` that implements the
`IFileSystem` abstract interface from `VFileSystem009.h` by delegating all
non-stub calls to a `Filesystem&` reference.

The `CreateInterface("VFileSystem009")` entry point returns a singleton instance
of this class. No vtable slot ordering is changed from the legacy definition.
Stub-only methods (`GetLocalCopy`, `HintResourceNeed`, etc.) remain no-ops.

---

## What is explicitly **not** in this module

- BSP/MDL/SPR/WAD texture format parsing — those are renderer concerns.
- Network or HTTP asset fetching.
- Memory pools — callers own their buffers via `std::vector` / RAII.
- `cvar` / console registration — mount flags are passed in by the engine
  at `Rescan` time.
- Game logic of any kind.
