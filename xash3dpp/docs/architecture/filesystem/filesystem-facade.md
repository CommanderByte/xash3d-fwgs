# Filesystem Facade

> **Defined in**: `xash3dpp/include/xash3dpp/filesystem/filesystem.hpp` /
> `xash3dpp/src/filesystem/filesystem.cpp`  
> **Namespace**: `xash::filesystem`

## Overview

`Filesystem` is the sole public entry point for all virtual-filesystem operations.
It follows the *pimpl* (pointer-to-implementation) idiom: the header exposes
only method signatures and a `std::unique_ptr<Impl>`, hiding every internal type
(`SearchPath`, `ISearchBackend`, mutexes, pool handle) from includers.

The class is neither copyable nor movable. Engine code holds a single instance for
the lifetime of a game session, created before worker threads start.

## `Filesystem` class

### Fields (all in `Filesystem::Impl`, hidden from callers)

| Name | Type | Role |
|------|------|------|
| `rootdir` | `std::string` | Filesystem root — written once in `Init()`, read-only thereafter |
| `basedir` | `std::string` | Base game folder name — init-only |
| `rodir` | `std::string` | Read-only mirror root — init-only |
| `search_paths` | `std::deque<SearchPath>` | Active search stack; protected by `paths_mutex` |
| `paths_mutex` | `std::shared_mutex` | Reader-writer lock for `search_paths` |
| `gamedir` | `std::string` | Active game folder name; protected by `game_mutex` |
| `active_game` | `GameInfo` | Parsed game-info for the selected game; protected by `game_mutex` |
| `game_loaded` | `bool` | True after `ActivateGame()` succeeds; protected by `game_mutex` |
| `game_mutex` | `std::shared_mutex` | Reader-writer lock for game-selection state |
| `pool_` | `xash::memory::PoolHandle` | Memory pool created in `Init()`, destroyed in `Shutdown()` |
| `allow_direct_paths` | `std::atomic<bool>` | Toggles `../` and absolute-path access; lock-free |

### Lifecycle

#### `Init(rootdir, basedir, gamedir, rodir)`

Stores the four root strings and creates the `PoolHandle`:
```
impl_->pool_ = xash::memory::create_pool("filesystem");
```
Returns `false` if pool creation fails. Must be called on the main thread before
any other method and before worker threads start issuing queries.

#### `ActivateGame(gamefolder, mount_flags, language)`

1. Calls `ScanGameDirectories(rootdir)` to find all `GameInfo` candidates.
2. Finds the matching `gamefolder` entry.
3. Under `unique_lock(game_mutex)`: stores `active_game`, `gamedir`,
   `game_loaded = true`.
4. Calls `Rescan(mount_flags, language)` to rebuild search paths.

Returns `false` if the gamefolder is not found.

#### `Rescan(mount_flags, language)`

1. Snapshots `active_game` under `shared_lock(game_mutex)`.
2. Releases the game lock.
3. Calls `ClearPaths()` (exclusive lock on `paths_mutex`).
4. Rebuilds the full hierarchy (`basedir → falldir → gamedir`) by calling
   `AddGameHierarchy` (which calls `AddGameDirectory` for each level).
5. Each `AddGameDirectory` call acquires an exclusive lock on `paths_mutex`.

**Known TOCTOU window**: between `ClearPaths()` and the first `AddGameDirectory`,
`search_paths` is empty. A concurrent reader acquiring a `shared_lock` during
this window will find no paths and silently return not-found / nullptr.

#### `Shutdown()`

1. Clears `search_paths` under `unique_lock(paths_mutex)` — destroys all backends.
2. Resets `active_game` and `game_loaded` under `unique_lock(game_mutex)`.
3. Calls `xash::memory::destroy_pool(pool_)`.

**Pre-condition**: all `unique_ptr<File>` handles returned by `Open` must have
been destroyed before `Shutdown()`. Their `operator delete` calls `mem_free`
which dereferences the pool; the pool must still be alive.

### File I/O operations

#### `Open(path, mode, gamedironly)`

Acquires `shared_lock(paths_mutex)`, iterates `search_paths` in reverse order
(highest priority first — deque back), delegates to `backend->OpenFile()` until
a non-null `unique_ptr<File>` is returned.

Write mode (`"w"`, `"wb"`, etc.) is routed to the first non-`NoWrite` path
before the iteration. The returned `unique_ptr<File>` is an `OsFile` or
`MemFile` allocated from `pool_`.

#### `LoadFile(path, gamedironly)`

Acquires `shared_lock(paths_mutex)`, iterates backends, calls
`backend->LoadFile()`. Returns `std::vector<std::byte>` (empty on failure).

#### `WriteFile(path, data)`

Routes to the highest-priority writable `DirBackend`. Creates intermediate
directories as needed. Returns `false` on failure.

### Game-info operations

#### `ScanGameDirectories(root)`

Enumerates subdirectories of `root`, attempts to parse `gameinfo.txt` or
`liblist.gam` in each, returns all successfully parsed `GameInfo` values.
Uses `xash::utilities::parse_gameinfo()` from `xash3dpp_utilities`.

#### `GetGameInfo()`

Returns `active_game` under `shared_lock(game_mutex)`. Valid only after
`ActivateGame()`.

### Search-path management

| Method | Lock held | Effect |
|--------|-----------|--------|
| `AddGameDirectory(dir, flags)` | `unique_lock(paths_mutex)` | Mount all archives + dir backend |
| `AddGameHierarchy(dir, flags)` | (calls AddGameDirectory per level) | Mount basedir → falldir → gamedir |
| `ClearPaths()` | `unique_lock(paths_mutex)` | Remove all non-`Static` entries |
| `AllowDirectPaths(enable)` | none (atomic store) | Toggle `allow_direct_paths` |
| `MountArchive(path, flags)` | `unique_lock(paths_mutex)` | Mount one archive by absolute path |

## Threading model

| State | Protection | Notes |
|-------|-----------|-------|
| `search_paths` | `std::shared_mutex paths_mutex` | Shared for reads; exclusive for mutations |
| `active_game`, `gamedir`, `game_loaded` | `std::shared_mutex game_mutex` | Shared for reads; exclusive for `ActivateGame`/`Shutdown` |
| `allow_direct_paths` | `std::atomic<bool>` | Lock-free; caller must restore to `false` after use |
| `rootdir`, `basedir`, `rodir` | None (init-only) | Safe because written once before threads start; contract is implicit |
| `pool_` | None | `Init()` and `Shutdown()` run single-threaded by contract |

**Outstanding hazard — `Rescan` TOCTOU**: `ClearPaths` and the subsequent
`AddGameDirectory` calls each take and release the exclusive lock independently.
Between them the path list is momentarily empty. Concurrent readers will silently
find no assets. This is not a data race but is an observable correctness gap under
active asset loading during a game switch. See the threading analysis for a proposed
fix (atomic swap of a fully-built path list).

**Outstanding hazard — `game_mutex` scope in `FindLibrary`**: `FindLibrary` reads
`game_loaded` and `active_game` *before* acquiring `shared_lock(paths_mutex)`. The
game state reads are protected by `game_mutex`; the path-list reads are protected
by `paths_mutex`; they are separate locks and there is no combined critical section.
This is correct as long as `ActivateGame()` and `FindLibrary()` are never called
concurrently, which is the expected usage.

## Error handling

All methods signal failure via return values:
- `bool` for `Init`, `ActivateGame`, `WriteFile`, `Rename`, `Delete`, `MountArchive`
- `nullptr` / empty `vector` / empty `optional` for file and query operations
- No exceptions are thrown.

## See also

- [search-path.md](./search-path.md) — `SearchPath`, `SearchPathFlags`, path hierarchy
- [backend-interface.md](./backend-interface.md) — `ISearchBackend` interface
- [file-io.md](./file-io.md) — `File`, `OsFile`, `MemFile` handles
- [docs/threading-analysis/filesystem-threading.md](../../threading-analysis/filesystem-threading.md) — full hazard inventory
