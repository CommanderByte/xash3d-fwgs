# Filesystem Threading Analysis

> Boundary spec: `docs/boundaries/filesystem-boundary.md`

## Ownership model

`Filesystem` is a singleton-like object created at process start.  Its lifecycle
calls (`Init`, `ActivateGame`, `Rescan`, `Shutdown`) are expected to run on the
**main thread only**, before other threads start issuing queries.  After
initialization the object is used concurrently: asset-loading threads, the server
thread, and the client thread all call read-query methods (`Open`, `LoadFile`,
`FileExists`, etc.).

That split — single-threaded mutation, concurrent read — is **partially** enforced
by a `std::shared_mutex paths_mutex` that guards the `search_paths` deque.  The
rest of the module state (active game info, string roots, Android JNI globals) has
**no synchronization** and relies entirely on the caller not mutating it while
queries are in flight.

There is no `assert_main_thread()` guard, no documented precondition in public
headers, and no enforcement via test.

Individual `File`/`OsFile`/`MemFile` handles are returned as `unique_ptr<File>` and
are **caller-owned** (not shared).  No locking is needed inside those classes.

## Safe items

- **`k_archive_types`** (`archive_registry.hpp:36`) — `inline constexpr` array.
  Written at compile time; read-only for the lifetime of the process.  **Safe-RO**.

- **`PakBackend`, `WadBackend`, `ZipBackend` — all instance state** — Populated
  entirely in the constructor (entries sorted, file_time cached, `valid_` set).
  After construction the objects are pushed into `search_paths` and never mutated
  again.  Concurrent read access through a `shared_lock` on `paths_mutex` is safe.
  **Safe-RO** after construction.

- **`DirBackend::root_`, `DirBackend::flags_`** — Written once in the constructor.
  **Safe-RO** after construction.

- **`CIDirectory::cache_`** (`ci_directory.hpp:43`) — Protected by the per-instance
  `std::mutex cache_mutex_` in all three entry points (`Resolve`, `Glob`,
  `Invalidate`).  Concurrent readers, writers, and invalidators are all serialised
  correctly.  **Race-shared, mitigated**.

- **`CIDirectory::root_`, `CIDirectory::mode_`** — Written once in the constructor.
  **Safe-RO** after construction.

- **`OsFile` / `MemFile` instance state** — All mutable fields (`position_`,
  `buf_`, `buf_pos_`, `buf_len_`, `ungetc_`, `zlib_`, `data_`, `pos_`) belong to
  the instance.  Callers always hold a `unique_ptr<File>`; there is no shared
  ownership.  **Safe-TLS** (per-caller-owned).

- **`Filesystem::Impl::search_paths` / `paths_mutex`** — All query methods acquire
  `shared_lock`; all mutation methods (`AddGameDirectory`, `ClearPaths`,
  `MountArchive`) acquire `unique_lock`.  Correct reader-writer pattern.
  **Race-shared, mitigated by `paths_mutex`**.

- **`bytes_as_string`** (`filesystem.cpp:32`) — File-scope static helper function
  (not a variable).  No state.  **Safe-RO**.

- **Platform I/O functions** (`win32.cpp`, `posix.cpp`) — All are pure functions
  of their arguments; no file-scope mutable state.  POSIX versions retry on
  `EINTR`.  **Safe-TLS**.

## Hazards

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `Impl::active_game` | `filesystem.cpp:50` | **Race-shared** | `ActivateGame()` writes the entire `GameInfo` struct without any lock. `Rescan()` and `FindLibrary()` read it (`active_game.dll_path`, `.gamefolder`, `.basedir`, `.falldir`, `.basedir`) without any lock. Concurrent `ActivateGame` + `FindLibrary` is a data race on `std::string` members of `GameInfo`. |
| `Impl::game_loaded` | `filesystem.cpp:51` | **Race-shared** | Written (true/false) in `ActivateGame()` and `Shutdown()` without a lock; read in `Rescan()` and `FindLibrary()` without a lock. Non-atomic `bool` write/read from different threads is a data race. |
| `Impl::gamedir` | `filesystem.cpp:45` | **Race-shared** | Set in `ActivateGame()` (`impl_->gamedir = g.gamefolder`) without a lock; read in `Gamedir()` and (transitively) in any code that calls `Gamedir()` concurrently. |
| `Impl::allow_direct_paths` | `filesystem.cpp:47` | **Race-shared** (latent) | Written in `AllowDirectPaths(bool)` without any lock. Currently unread in the rewrite, but when wired to `Open()` or similar it will race with `AllowDirectPaths` called from a second thread. |
| `Impl::rootdir` / `basedir` / `rodir` | `filesystem.cpp:42–44` | **Race-shared** (weak) | Written in `Init()` only — before other threads start — and never written again. Safe in expected usage but the contract is implicit; there is no `assert_main_thread()` guard. If `Init()` is called a second time from a background thread while queries are in flight the reads in `Rescan()` / `ScanGameDirectories()` / `FindLibrary()` / `GetRootDirectory()` will race. Classified as **Safe-RO** in practice, noted here because the invariant is unenforced. |
| `Rescan()` path-list gap | `filesystem.cpp:94–122` | **Race-shared** (TOCTOU) | `Rescan` calls `ClearPaths()` (acquires + releases `unique_lock`), then reads `active_game` (no lock), then calls `AddGameHierarchy` (acquires `unique_lock` again per `AddGameDirectory`). Between `ClearPaths` and the first `AddGameDirectory` call the `search_paths` deque is empty. A concurrent query thread holding `shared_lock` will find no paths and silently return not-found / nullptr. This is not a data race, but it is an observable TOCTOU window. |
| `FindLibrary` game-state reads before lock | `filesystem.cpp:396–411` | **Race-shared** | `FindLibrary` reads `impl_->game_loaded` and `impl_->active_game` (both unprotected) *before* acquiring `shared_lock` on `paths_mutex`. The shared_lock therefore does not cover these reads. |
| `g_jni` | `platform/android.cpp:48` | **Race-shared** | File-scope `JniState` struct in anonymous namespace. Written (all fields) in `android_init_jni()`. Read in `get_asset_manager()` and `list_assets()`. No lock anywhere. Concurrent calls (e.g. JNI callbacks on different JNI threads) would race. |
| `g_handles[2]` | `platform/android.cpp:51` | **Race-lazy-init** | `get_asset_manager()` checks `h->mgr` and then writes `h->mgr`, `h->engine`, `h->package_name` without any lock or atomic. Classic unsynchronised double-checked locking: two concurrent callers can both observe `mgr == nullptr`, then both write all fields. Writing `h->package_name` (a `std::string`) simultaneously from two threads is undefined behaviour. |

## Required caller contracts

1. **Lifecycle methods are main-thread-only.**  `Init()`, `ActivateGame()`,
   `Rescan()`, `Shutdown()`, and `AllowDirectPaths()` must only be called from the
   main (engine) thread, and no query method (`Open`, `LoadFile`, `FileExists`,
   `FindLibrary`, …) may be executing concurrently on any other thread when these
   are called.  This contract is currently undocumented and unenforced.

2. **`File` handles are single-owner.**  A `unique_ptr<File>` returned from `Open`
   must not be shared across threads.  All methods on `OsFile`/`MemFile` access
   mutable instance state without any internal lock.

3. **`CIDirectory` is not publicly exposed.**  It is only reachable via a
   `DirBackend` held inside `search_paths` and accessed under `paths_mutex`. Its
   `cache_mutex_` is sufficient for that access pattern.  Do not expose
   `CIDirectory` instances directly to external callers.

4. **Android: `android_init_jni` is one-shot, main-thread-only.**  All JNI setup
   must complete before any thread calls `get_asset_manager()`.  The current code
   has no mechanism to enforce or detect concurrent initialisation.

## Recommendations

Ordered from simplest/safest to deeper redesign:

1. **Add `assert` guards to lifecycle methods.**  Record the main thread ID in
   `Filesystem::Init()` and assert on it in `ActivateGame()`, `Rescan()`,
   `Shutdown()`, and `AllowDirectPaths()`.  This catches threading misuse at
   development time without any runtime overhead in release builds:

   ```cpp
   // In Impl:
   std::thread::id owner_thread;
   
   // In Init():
   impl_->owner_thread = std::this_thread::get_id();
   
   // Helper:
   void assert_owner() const {
       assert(std::this_thread::get_id() == impl_->owner_thread
              && "Lifecycle method called from non-owner thread");
   }
   ```

2. **Protect `active_game` and `game_loaded` with a `std::shared_mutex`.**  A
   second reader-writer lock (`game_mutex`) would let `FindLibrary()` and
   `Gamedir()` take a `shared_lock` while `ActivateGame()` takes a `unique_lock`.
   Alternatively, if lifecycle calls are strictly main-thread-only (recommendation
   1), use `std::atomic<bool>` for `game_loaded` (relaxed store + acquire load) and
   document that `active_game` is only written during init.

3. **Protect `allow_direct_paths` with `paths_mutex` (or a dedicated atomic).**
   The flag is inherently a per-query toggle (legacy: enable, call, disable).  In
   a multi-threaded engine that pattern is racy by design; prefer passing the flag
   as an argument to `Open()` instead of mutating shared state.

4. **Fix the `g_handles` lazy-init race on Android** (`platform/android.cpp`).
   The simplest fix is a `std::once_flag` per handle slot:

   ```cpp
   static std::once_flag g_init_flags[2];

   AssetManagerHandle* get_asset_manager(bool engine_package) noexcept {
       const int idx = engine_package ? 0 : 1;
       std::call_once(g_init_flags[idx], [&] { /* ... populate g_handles[idx] ... */ });
       return g_handles[idx].mgr ? &g_handles[idx] : nullptr;
   }
   ```

5. **Close the `Rescan()` TOCTOU window** by holding the `unique_lock` for the
   entire rescan operation (clear + repopulate), building the new path list into a
   local vector first, then swapping atomically under the lock.  This prevents
   concurrent readers from observing an empty search path mid-rescan.

6. **Long-term: separate the mutable game-state bag from the query-facing state.**
   `Filesystem::Impl` currently mixes lifecycle-only fields (`rootdir`, `basedir`,
   `active_game`, `game_loaded`) with the concurrently accessed `search_paths` and
   `paths_mutex`.  Splitting them into `GameState` (main-thread-only, no lock
   needed) and `QueryState` (shared, `paths_mutex`-protected) makes the ownership
   model explicit in the type system.
