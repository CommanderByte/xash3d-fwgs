# Filesystem Boundary Spec

> Legacy survey background: [docs/legacy-survey/filesystem.md](../legacy-survey/filesystem.md)\
> Narrow legacy deep-dive: [docs/legacy-survey/deep-dive-filesystem.md](../legacy-survey/deep-dive-filesystem.md)

> Refreshed 2026-07-06 (as-built pass) — reconciled the spec against the current
> `xash3dpp_filesystem` target: the pimpl `Filesystem` facade + `ISearchBackend`
> polymorphic backends (PAK / WAD / ZIP / DIR / PK3DIR / Android), the
> `std::shared_mutex` reader-writer model, the `CIDirectory` per-backend CI
> cache, and the `xash3dpp_miniz` shared static target. Added the
> `## Extension axes (Q-21)` section and refreshed `## Threading (as-built)` to
> match the nine `compliance-allow(thread-assert)` sites actually in code.

## Responsibility

The filesystem module provides virtual game-asset I/O over a layered stack of
**search paths**. Each path is a backend — plain directory, PAK (Quake), WAD
(GoldSrc), ZIP/PK3, or Android AAsset — and the module multiplexes `Open`,
`Read`, `LoadFile`, `Search`, and related operations across that stack using a
first-match, last-added-wins policy. It also owns game-directory discovery
(scanning for `gameinfo.txt` / `liblist.gam`), game-info parsing and
serialisation, DLL/library path resolution, and case-insensitive filename
emulation on POSIX. It does **not** parse game data formats (BSP, MDL, etc.),
manage network I/O, or execute any game logic.

## External ABI contracts

None — fully internal to the engine.

The filesystem is a **separately-loaded plugin** (DLL/SO). Its only exported
symbol is `GetFSAPI` (signature `FSAPI`, see `filesystem.h`). The engine calls
this at startup to receive the `fs_api_t` function-pointer table and the
`fs_globals_t *` pointer. Everything flows through those two handles.

The `IFileSystem` / `VFileSystem009` C++ vtable (`FILESYSTEM_INTERFACE_VERSION "VFileSystem009"`) is a compatibility shim for mods that call through the
GoldSrc interface. It is **not** a fixed ABI constraint for the rewrite — no
game DLL is allowed to link against it directly; they receive it through the
engine at runtime.

## Interface (what the rest of the engine calls)

All calls go through the `fs_api_t` function-pointer table returned by
`GetFSAPI`. The engine also reads `fs_globals_t::GameInfo` (a `const gameinfo_t *`) directly.

### Lifecycle

| Function | Purpose |
| --- | --- |
| `InitStdio(rootdir, basedir, gamedir, rodir)` | Scan game directories, build game list, add static search paths |
| `ShutdownStdio()` | Release all search paths and the memory pool |
| `Rescan(flags, language)` | Clear non-static paths and rebuild hierarchy for current `GameInfo` |
| `LoadGameInfo(flags, language)` | Select active game from the list; calls `Rescan` |
| `MakeGameInfo()` | Write `gameinfo.txt` for the current game directory |

### Search-path management

| Function | Purpose |
| --- | --- |
| `AddGameDirectory(dir, flags)` | Mount all archives in `dir`, then mount `dir` itself |
| `AddGameHierarchy(dir, flags)` | Recursive helper: handles base/fallback/HD/LV/addon/l10n |
| `ClearSearchPath()` | Remove all non-`FS_STATIC_PATH` entries |
| `AllowDirectPaths(enable)` | Toggle `fs_ext_path` — allow `../` and absolute paths |
| `SetCurrentDirectory(path)` | `chdir` wrapper |
| `GetRootDirectory(buf, size)` | Returns `fs_rootdir` |
| `Path_f()` | Debug-print the active search path (dev console) |
| `MountArchive_Fullpath(path, flags)` | Mount a single archive by absolute path |

### File operations

| Function | Purpose |
| --- | --- |
| `Open(path, mode, gamedironly)` | Streaming open; writes go to `fs_writepath` |
| `Close(file)` | |
| `Read(file, buf, size)` | Handles buffered + zlib-deflated reads transparently |
| `Write(file, data, size)` | |
| `Seek(file, offset, whence)` | Handles seeking in deflated files via re-inflate |
| `Tell(file)` | |
| `Eof(file)` | |
| `Flush(file)` | `fsync`/`_commit` |
| `Gets / Getc / UnGetc / Printf / VPrintf / Print` | Text-mode helpers |
| `FileLength(file)` | Uncompressed size |
| `FileCopy(dst, src, size)` | Buffered copy between two open `file_t` |

### Bulk-load operations

| Function | Purpose |
| --- | --- |
| `LoadFile(path, sizeptr, gamedironly)` | Whole-file into pool memory (null-terminated) |
| `LoadFileMalloc(path, sizeptr, gamedironly)` | Same but via `malloc`; caller frees with `free()` |
| `LoadDirectFile(path, sizeptr)` | Bypass VFS; read from disk path directly |
| `WriteFile(path, data, len)` | Whole-file write |

### Query operations

| Function | Purpose |
| --- | --- |
| `FileExists(path, gamedironly)` | Boolean existence check |
| `FileSize(path, gamedironly)` | |
| `FileTime(path, gamedironly)` | Modification timestamp |
| `SysFileExists(path)` | OS-level existence (no VFS) |
| `GetDiskPath(name, gamedironly)` | Returns on-disk path or `NULL` for packed files |
| `GetFullDiskPath(buf, size, name, gamedironly)` | Like above, to a caller buffer |
| `Search(pattern, caseinsensitive, gamedironly)` | Wildcard glob across all paths; returns `search_t *` |
| `Rename / Delete` | Mutate `fs_writepath` only |
| `CRC32_File / MD5_HashFile` | Hash an open file |

### Archive/library API

| Function | Purpose |
| --- | --- |
| `IsArchiveExtensionSupported(ext, flags)` | Query which extensions the current build knows |
| `GetArchiveByName(name, prev)` | Iterate mounted archives by filename |
| `FindFileInArchive(sp, path, outpath, len)` | Case-insensitive lookup within a specific archive |
| `OpenFileFromArchive(sp, path, mode, ind)` | Open from a specific archive (case-sensitive) |
| `LoadFileFromArchive(sp, path, ind, sizeptr, sys_malloc)` | Bulk-load from specific archive |
| `ArchivePath(file)` | Name of the archive that supplied an open file |
| `FindLibrary(name, directpath, dllinfo)` | Resolve a game DLL name to a full disk path |

### Gameinfo query

| Symbol | Purpose |
| --- | --- |
| `Gamedir()` | Returns `GameInfo->gamefolder` or `fs_gamedir` pre-init |
| `fs_globals_t::GameInfo` | Pointer to the active `gameinfo_t`; read directly by engine |
| `fs_globals_t::games[MAX_MODS]` | Full list of discovered game directories |

## Dependencies (what this module calls)

> **Rewrite note:** the xash3dpp rewrite (`xash3dpp_filesystem`) uses the
> following CMake link dependencies. The legacy table below documents what the
> original `filesystem.c` calls through injected callbacks; it remains for
> reference during the migration audit.

### xash3dpp rewrite dependencies

| Dependency | Visibility | Reason |
| --- | --- | --- |
| `xash3dpp_utilities` | PUBLIC | String/path helpers used in the public API |
| `xash3dpp_memory` | PUBLIC | `PoolHandle`, `pool_alloc`, etc. exposed in public headers |
| `xash3dpp_platform` | PRIVATE | All OS file I/O routed through `xash::platform` (open, seek, stat, directory ops, case-sensitivity check) |
| `xash3dpp_miniz` | PRIVATE | zlib inflate for ZIP/PK3 and deflated WADs |
| `android` + `log` | PRIVATE (Android) | NDK AAsset bridge via `xash3dpp_platform` |

### Legacy (pre-rewrite) injected dependencies

| Dependency | How injected | Reason |
| --- | --- | --- |
| `_Con_Printf / _Con_DPrintf / _Con_Reportf` | `fs_interface_t` passed to `GetFSAPI` | Logging |
| `_Sys_Error` | same | Fatal errors |
| `_Mem_AllocPool / _Mem_FreePool / _Mem_Alloc / _Mem_Realloc / _Mem_Free` | same | All heap use |
| `_Sys_GetNativeObject` | same | Android `AAssetManager` retrieval |
| `crtlib` (`public/crtlib.h`) | Linked statically | `Q_strncpy`, `COM_ParseFile`, etc. |
| `crclib` (`public/crclib.h`) | Linked statically | CRC32 / MD5 |
| `miniz` (`public/miniz.h`) | Linked statically | zlib inflate for ZIP/PK3 |
| `utflib` (`public/utflib.h`) | Linked statically (Win32 only) | UTF-8 ↔ UTF-16 path conversion |
| `library_suffix` (`public/library_suffix.h`) | Linked statically | Platform `.so`/`.dll`/`.dylib` suffix |
| `common/com_strings.h`, `xash3d_types.h` | Headers only | Types and string constants |
| Android JNI / NDK | Platform only | `AAssetManager`, `JNIEnv` |

The module has **zero reverse dependencies** on `engine/` — all engine services
are provided through the injected `fs_interface_t` callbacks.

## Owned state

| Variable | Type | Description |
| --- | --- | --- |
| `fs_searchpaths` | `searchpath_t *` (static) | Head of the mounted-path linked list |
| `fs_writepath` | `searchpath_t *` | Active writable path (last non-`NOWRITE` dir added) |
| `fs_mempool` | `poolhandle_t` | Filesystem memory pool |
| `fs_rootdir` | `char[MAX_SYSPATH]` | Working directory at startup (`rootdir` arg) |
| `fs_basedir` | `char[MAX_SYSPATH]` (static) | Base game dir (`basedir` arg, e.g. `"valve"`) |
| `fs_gamedir` | `char[MAX_SYSPATH]` (static) | Current game dir (`gamedir` arg) |
| `fs_rodir` | `char[MAX_SYSPATH]` (static) | Read-only overlay dir (`rodir` arg) |
| `fs_language` | `string` (static) | Active locale tag for `_l10n` directories |
| `fs_ext_path` | `qboolean` (static) | When `true`, bypass nasty-path check |
| `FI` | `fs_globals_t` | `GameInfo` pointer + `games[]` array (all discovered mods) |
| `g_engfuncs` | `fs_interface_t` | Injected engine callbacks (initially stdlib stubs) |
| `g_api` | `const fs_api_t` | Constant table of exported function pointers |
| `fs_last_readfile` / `fs_last_zip` | `file_t * / zip_t *` (static, `XASH_REDUCE_FD` only) | LRU fd cache |
| `fs_directpath` | `searchpath_t` (static) | Reusable searchpath for `fs_ext_path` direct lookups |

## Quirks and invariants

- **Search-path priority** — paths are prepended (not appended). The last-added
  path has the highest priority. `FS_ClearSearchPath` skips `FS_STATIC_PATH`
  entries (the root and rodir `./` paths added in `FS_InitStdio`).

- **WAD auto-mount** — when any PAK or ZIP is mounted, every `.wad` file found
  inside it is immediately mounted as its own `SEARCHPATH_WAD` entry. This
  cannot be suppressed for individual archives without `FS_SKIP_ARCHIVED_WADS`.

- **Archive ordering within a gamedir** — `FS_AddGameDirectory` mounts in
  PAK→PK3→WAD order (matching `g_archives[]`), then adds the plain directory
  last (highest priority). Raw loose files and WADs therefore override packed
  ones.

- **Case-insensitive emulation** — `dir.c` builds an in-memory sorted trie
  (`dir_t` tree) of filesystem entries on first lookup. On Linux, if the
  directory already has `FS_CASEFOLD_FL` set (kernel-level case folding), the
  trie is skipped entirely. On Windows the trie is always skipped (native
  case-insensitivity). On Android it is always built (accessing `/storage` via
  ioctl crashes `MediaProviderGoogle`, so `FS_CASEFOLD_FL` detection is
  disabled there).

- **`FS_AllowDirectPaths` must be reset** — callers that enable `fs_ext_path`
  are responsible for immediately disabling it after the operation. Failure
  leaves the global flag set, silently broadening all subsequent path lookups.

- **Write path semantics** — all file writes and renames target `fs_writepath`
  regardless of which search path found the file for reading. There is only one
  writable directory at a time.

- **`XASH_REDUCE_FD`** — a compile-time platform option (PSVita, NSwitch) that
  keeps at most one native fd open at a time via `FS_EnsureOpenFile`. This is
  silently broken for simultaneously open compressed files.

- **`fs_ext_path` and `../` stripping** — in `FS_FindFile`, when `fs_ext_path`
  is active, the leading `../` is stripped from the search name before
  constructing the native path. This is an acknowledged hack; the correct fix
  would be using `fs_writepath` instead of `fs_rootdir` as the base.

- **gameinfo.txt vs liblist.gam** — if both exist and `liblist.gam` is
  strictly newer (by mtime), the legacy file is converted to `gameinfo.txt`
  in-place. This conversion is only performed on the RwDir, never on the RoDir.

- **Quake gamedir auto-detection** — if a directory has no `gameinfo.txt` /
  `liblist.gam` but contains `pak0.pak` + (`progs.dat` or `quake.rc`), a stub
  `gameinfo_t` is synthesised in memory with Quake-appropriate defaults (no
  file is written).

- **Half-Life `singleplayer_only` hack** — `liblist.gam` for the `valve`
  directory specifies `singleplayer_only` but Half-Life supports multiplayer.
  The parser explicitly overrides this to `GAME_NORMAL` when
  `gamefolder == "valve"`.

- **`hldemo1` inject** — if `demomap` is empty after parsing and the game
  title is `"Half-Life"`, the field is hardcoded to `"hldemo1"`.

- **`XASH3D_EXTRAS_PAK1` / `_PAK2` env vars** — at every `FS_Rescan`, these
  environment variables are read and the named archives are mounted as
  `FS_NOWRITE_PATH | FS_CUSTOM_PATH`. There is no ordering guarantee relative
  to the normal game hierarchy.

- **Executable flag propagation** — only `FS_EXEC_PATH`-flagged search paths
  can supply DLL/SO files to `FS_FindLibrary`. Plain directories and the
  PAK format have this flag; PK3/ZIP/WAD do not. On Android,
  `/data/...` paths receive it automatically; all other paths do too except
  when Android is the target.

- **DLL idiot-path stripping** — `FS_FindLibrary` strips leading `../gamedir/`
  and `../valve/` prefixes inserted by some mod DLLs that embed relative paths
  to themselves in exported strings.

- **`VFileSystem009` stubs** — `GetLocalCopy`, `HintResourceNeed`,
  `WaitForResources`, `GetWaitForResourcesProgress`, `CancelWaitForResources`,
  `LogLevelLoadStarted/Finished`, `IsAppReadyForOfflinePlay`, and `SetVBuf` are
  all no-ops or return stub values. These methods exist to satisfy the vtable
  layout; no tested mod depends on their behaviour.

- **`GetFSAPI` null-interface fallback** — if `fs_interface_t` is `NULL` or
  individual function pointers within it are `NULL`, the module falls back to
  stdlib stubs (`malloc`/`free`/`printf`). This allows limited pre-initialisation
  use (e.g. the `no-init` test).

## Design decisions

1. **Module boundary** — **Integrate into the engine as a static module.**
   No C-ABI plugin boundary in the rewrite. If a standalone tool needs
   filesystem access, add a thin adapter interface at that point.

1. **`VFileSystem009` retention** — **Keep it for now.** Exact placement
   (separate compilation unit, opt-in shim header, etc.) is deferred until
   the engine's own public-API structure is established.

1. **Async I/O** — **Desired, but deferred.** The engine threading model
   must be established first. The synchronous API should be the initial
   target; async can be layered once the job/scheduler design is known.

1. **Memory ownership model** — The legacy split (`LoadFile` → pool memory,
   `LoadFileMalloc` → stdlib memory) exists because the pool allocator
   (`zone.c`, DarkPlaces-derived) uses a 32-bit handle rather than a pointer
   (changed to avoid breaking the reuse of a 32-bit model field on 64-bit
   targets). Pool memory is freed en masse when `Mem_FreePool` is called at
   shutdown, so `LoadFile` callers that forget to `Mem_Free` are silently
   cleaned up; `LoadFileMalloc` exists for callers that need the buffer to
   outlive the FS pool. In the rewrite, returning `std::vector<std::byte>`
   eliminates both variants: the buffer is caller-owned and freed by RAII. The
   injected allocator pattern (`_Mem_AllocPool` etc.) is unnecessary in a
   statically-linked build.

1. **Thread safety** — **Required.** Internal mutable state (search-path
   list, write-path pointer, game-info pointer) must be protected. At minimum:
   a shared/exclusive (reader-writer) lock for path queries and file opens,
   with exclusive acquisition for mount/unmount operations. Per-file `Read`/
   `Write`/`Seek` operations should not block global path lookups.

1. **`gameinfo_t` location** — Currently embedded in `filesystem.h` alongside
   the I/O API, coupling game-config concerns into the I/O layer. Two options:

   - **Shared header**: promote to `xash3dpp/include/gameinfo.h`. The FS
     module populates it and the engine reads it directly. Simple, but
     `gameinfo_t` layout becomes a visible contract for all translation units
     that include it.
   - **Query API**: hide the struct behind a typed query (`GameInfo()` returns
     a value type or `const GameInfo &`). Consumers only see the struct if they
     include the gameinfo header explicitly. Cleaner coupling; slightly more
     boilerplate.

   The query-API approach is preferred for the rewrite because it keeps the FS
   public interface focused on I/O operations and makes the gameinfo dependency
   opt-in. **Decision needed before defining the FS public header.**

1. **Case-insensitive filename lookup** — The legacy trie (`dir.c`) is
   per-searchpath, lazily built per-directory, and O(log n) via binary search
   over sorted names. Three options for the rewrite:

   - **Keep lazy per-directory** — current behaviour; adds latency only on
     first access, cheap for rarely-touched directories.
   - **Eager at mount time** — scan all directories when a search path is
     added; simpler lifetime, predictable first-access cost, but adds mount
     latency for large game directories.
   - **Skip when unnecessary** — detect at startup whether the underlying
     volume is already case-insensitive (Windows, macOS, Linux `CASEFOLD_FL`)
     and skip the trie entirely. Requires a reliable probe per mount point.

   The third option is the most performant on the common platforms but requires
   careful per-mountpoint detection (the legacy code only checks per-directory
   inode flags on Linux). A reasonable middle ground: skip the trie on
   case-insensitive volumes detected at mount time; fall back to lazy trie
   otherwise. **Decision needed before implementing the directory backend.**

## Fixed Limits

All filesystem-wide fixed sizes are declared as `inline constexpr` values in
`xash3dpp/limits.hpp` (sibling `xash::limits` namespace). Each constant is
guarded by an `XASH_LIMIT_*` macro for downstream overrides.

| Constant | Default | Purpose |
|----------|---------|---------|
| `filesystem_file_buffer_size` | 2048 | `OsFile` read-ahead I/O buffer |
| `filesystem_zlib_inflate_buf` | 65536 | `ZlibState` raw-input chunk buffer |
| `filesystem_search_path_max` | 256 | Informal upper bound for mounted search paths |
| `pak_max_files` | 65536 | Maximum entries in a PAK archive |
| `wad_max_lumps` | 65535 | Maximum lumps in a WAD3 archive |
| `zip_max_files` | 65535 | Maximum entries in a ZIP archive (uint16 EOCD field) |
| `zip_filename_max` | 4096 | Maximum bytes in a ZIP central-directory path |
| `zip_eocd_scan_max` | 65535 | Maximum tail bytes scanned for the ZIP EOCD record |

Hot-path containers (`PakBackend::entries_`, `WadBackend::entries_`,
`ZipBackend::entries_`) carry `// @pre-reserved:` annotations referring to the
relevant limit. `Filesystem::Impl::search_paths` is `std::deque` (no `reserve`)
and is annotated as informally bounded.

## Observability / Stats

`Filesystem` exposes a `FilesystemStats` snapshot via
`stats() const noexcept`. Fields:

| Field | Type | Meaning |
|-------|------|---------|
| `game_loaded` | `bool` | `true` after a successful `activate_game()` |
| `search_path_count` | `std::size_t` | Number of search paths currently mounted |

`stats_` lives in the pimpl and is updated under the same locks that guard the
state it reflects (`paths_mutex` for `search_path_count`, `game_mutex` for
`game_loaded`). Always-on; cost is one assignment per mutation.

## Threading (as-built)

> Refreshed 2026-07-06 (as-built pass).

Unlike the pure-function utility subsystems, the filesystem owns real mutable
state and is **thread-safe by lock**, not by thread-confinement. There are
**zero `assert_thread_role` calls** in the subsystem; every thread-role match is
a documented `compliance-allow(thread-assert)` (nine sites: seven on the
`Filesystem` facade in `filesystem.cpp`, two on backends). The concurrency
primitives are:

- **Two `mutable std::shared_mutex` on `Filesystem::Impl`** (`filesystem.cpp`):
  `paths_mutex` guards the `std::deque<SearchPath>` list; `game_mutex` guards
  the `GameInfo active_game` / `gamedir` / `game_loaded` selection state.
- **One `mutable std::mutex cache_mutex_` per `CIDirectory`**
  (`ci_directory.hpp`) protecting the lazy per-subdirectory name cache
  (`std::unordered_map<std::string, std::vector<std::string>>`). This is
  per-backend-instance, not global — no magic static.
- **One `std::atomic<bool> allow_direct_paths`** on the pimpl — lock-free
  toggle for absolute/`../` path permission.
- **No Meyers / magic statics**: the only `static` in the subsystem are
  compile-time `static constexpr` format/magic constants inside the backends
  (`k_IDPACK`, `k_WAD2`, `k_WAD3`, `k_SIG_*`, `k_MAX_LUMPS`, …).

### Thread-role classification (analyse-threading)

| Site | Guard | Class | Rationale |
|------|-------|-------|-----------|
| `init` / `shutdown` | none (writes `pool_`/`rootdir`/`basedir`/`rodir` unguarded) | **Main-by-contract** | Called before workers start / after they join (README key invariants). A real `assert_thread_role(Main)` would `XASH_FATAL` the fs test harness (which registers no `ThreadRole`) — deferred |
| `activate_game` / `rescan` / `add_game_directory` / `add_game_hierarchy` / `clear_paths` / `mount_archive` | `unique_lock` (`paths_mutex` and/or `game_mutex`) | **Any-thread, exclusive** | Path-mutating; writer lock serialises them against readers |
| `open` / `load_file` / `file_exists` / `file_size` / `file_time` / `disk_path` / `search` / `write_file` / `rename` / `remove` / `find_library` | `shared_lock` (`paths_mutex`; `find_library` also snapshots under `game_mutex`) | **Any-thread, concurrent** | Read-only path traversal; concurrent readers safe |
| `load_direct_file` | none | **Any-thread, stateless** | `const`, VFS-bypassing disk read touching no shared state |
| `CIDirectory::resolve` / `glob` / `invalidate` / `get_or_populate` | `lock_guard` (`cache_mutex_`) | **Any-thread, per-instance** | Serialises lazy cache population inside one backend |
| `ISearchBackend` subclasses (PAK/WAD/ZIP/DIR/PK3DIR/Android) | immutable after construction | **Safe-RO** | Entry tables are built once at mount and never mutated; only `CIDirectory`-backed `DirBackend`/`Pk3DirBackend` carry the per-instance mutex above |

**Adjudication (unchanged 6B S4).** All nine mutator-name matches are
`compliance-allow(thread-assert)` rather than `assert_thread_role(Main)`.
`init`/`shutdown` are main-thread-by-contract but a runtime assert would
`XASH_FATAL` the filesystem test harness (which registers no `ThreadRole`, and
is outside the S4 hardening carve); the remaining seven facade paths and the
two backend paths are lock-guarded any-thread paths where a `Main` assert would
contradict the documented concurrent-read / exclusive-write model. Enforcing the
`init`/`shutdown` asserts is a follow-up that must also register
`ThreadRole::Main` in the four `tests/filesystem/*.cpp` harnesses.

**Extension-goals alignment.** `extension-goals.md` §G-3 states *"the filesystem
is already safe for off-main readers"* — the `shared_lock` read paths above are
that guarantee, and are why the off-main I/O-thread door (see Extension axes
P-1) is realistic. The one caveat is `init`/`shutdown`: they remain
main-thread-by-contract and must stay outside any worker's reach until the
`ThreadRole::Main` assert is enforced.

## Constant classification (QO)

The literals `limits_scan` flags in filesystem source are **wire/disk-frozen
or algorithm-frozen constants**, not tunable capacities (limits.hpp) nor
behavioural knobs (cvars); per QO they stay as in-code constants next to the
format/algorithm that defines them:

| Literal | Site | Classification |
|---------|------|----------------|
| `56` | `pak_backend.cpp` `char name[56]` | PAK on-disk directory-entry name field (frozen file format) |
| `16` | `wad_backend.cpp` `char name[16]`, `normalise_name` (16-byte name array by ref) | WAD3 lump-name field, NUL-padded/15 significant chars (frozen file format) |
| `16` | `filesystem.{hpp,cpp}` `std::array<std::byte,16>` `md5_file` | MD5 digest size, 128 bits (frozen algorithm output) |

The tunable structural capacities (`pak_max_files`, `wad_max_lumps`,
`zip_max_files`, `zip_filename_max`, `zip_eocd_scan_max`,
`filesystem_search_path_max`) already live in `limits.hpp` (see Fixed Limits).

## Q-11 satellite verdict

Not a separate-target satellite: the archive-format backends (PAK, ZIP, WAD,
DIR, PK3DIR, Android asset) share the VFS's format concern, implement the small
`ISearchBackend` interface the parent already exposes, live or die with the
filesystem, and are useless standalone — 0 of the five "separate" criteria
(Q-11). They correctly compile into `xash3dpp_filesystem`; `ISearchBackend` is
intrinsic format dispatch, not a compat/policy seam.

## Q-4 FilesystemInitParams — deferred (2026-07-06, 6B S4)

`Filesystem::init` still takes positional args
(`rootdir, basedir, gamedir, rodir`). Converting to a `FilesystemInitParams`
struct would ripple past the two host call sites
(`src/host/engine_context.cpp`, `src/host/host.cpp`) into **24 test call sites**
across `tests/filesystem/test_file.cpp` (1) and `test_filesystem.cpp` (23) —
beyond the S4 carve (host reach is limited to the two sanctioned lines; broad
test edits are not sanctioned). **Deferred with owner `6B-S8-host`**, to be done
alongside the host-params consolidation that already owns those call sites.

______________________________________________________________________

## Extension axes (Q-21)

> Added 2026-07-06 (as-built pass); axis set completed 2026-07-19
> (consolidation audit). Evaluated against
> [docs/design/extension-goals.md](../design/extension-goals.md) (G-1..G-5,
> P-1..P-8). The filesystem is explicitly named in §G-3 as *"already safe for
> off-main readers"* — so the north-star question here is not *whether* the
> off-main I/O door exists, but *keeping it open* as the subsystem grows.

| Goal / primitive | Applies? | Verdict / seam to keep |
|------------------|----------|------------------------|
| **P-3** context-first, no new file-scope state | **Yes — headline (already done)** | All mutable state lives on `Filesystem::Impl` (pimpl); the legacy `fs_searchpaths` / `fs_writepath` / `FI` globals were **eliminated**. Backends receive their pool + root as constructor context. Keep it: no new `static` mutable state, ever — the only `static` allowed is `constexpr` format magic |
| **P-1** off-main worker / I/O thread | **Yes — door-keep (headline door)** | The `shared_lock(paths_mutex)` read paths (`open`/`load_file`/`file_exists`/`search`/…) are already concurrent-safe, so a future asset-streaming or prefetch job can call them from a worker with no lock retrofit. **Door-keep rules:** (a) never add per-`File` shared mutable state — `File` handles stay caller-owned and single-consumer; (b) keep `init`/`shutdown` main-thread-by-contract and out of any worker's reach until the `ThreadRole::Main` assert lands; (c) mount/unmount stays exclusive-write, so a worker must not mutate paths. Async I/O is **desired-but-deferred** (Design decision 3) — this is the door, built when a consumer schedules it (no gold-plating) |
| **P-2** published-snapshot reads | **Partial — already shaped** | `get_game_info()` / `stats()` return **by-value snapshots** taken under the lock, so an off-main reader never holds a live-mutable `GameInfo` ref across a thread boundary. `FilesystemStats` is a value snapshot. Keep returning values, not internal refs, for any new introspection |
| **P-4** typed introspection | **Yes — partial surface exists; keep growing it typed** | `FilesystemStats { game_loaded, search_path_count }` is the seed of the typed surface G-1/G-3/G-4 will consume. A future *"what is mounted"* query (search-path list, per-backend kind, source path, flags, open-handle census) should be a typed value-returning API — **not** an `extern` poke into `Impl::search_paths`. The `ISearchBackend::info()` string is debug-only; a structured `SearchPathView` is the P-4 upgrade path |
| **P-5** narrowest-state signatures | **Yes — already done** | Backend methods take `(std::string_view path, mode)`, not a runtime aggregate; the facade owns the deque and hands each backend only what it needs |
| **P-6** services are satellites | **N/A for the core; door noted** | The filesystem itself integrates as a static module (Design decision 1 — no C-ABI plugin). The archive backends are **not** satellites (Q-11 verdict below: 0/5 criteria). A future *modern-compression* or *network-mount* backend could be a satellite target, but none is planned |
| **P-7** pool-owned RAII lifecycle | **Yes — already conforms** | `File` and every `ISearchBackend` follow the pool-owned idiom: `create_<thing>` factories (`create_pak`/`create_os_file`/…), `pool_new` allocation, dual `operator delete` overloads (`file.hpp:39-40`, `i_search_backend.hpp:25-26`), no class-scoped `operator new`. Door rule: new pool-owned types keep this exact shape. *(An earlier revision of this row answered the retired "over-aligned allocation" question — that concern now lives with the HB-7 shared aligned-allocation door in `implementation-plan.md`; the buffer answer there remains true: ≥8-byte pool alignment suffices here.)* |
| **P-8** annotation discipline | **Yes — satisfied (denominatored)** | 2026-07-19 `annotation-coverage` scan: lifetime / thread_safety / safety / pre_reserved / thread_assert all at 100% for filesystem, with denominators. Keep future coverage statements denominatored, not raw counts. |
| **G-1** in-engine MCP service | Consumer door via P-4 | MCP inspects mounts/files through the typed introspection surface (see P-4): `stats()`, `get_game_info()`, the future `SearchPathView`. No filesystem-side service code — G-1 composes existing typed queries. |
| **G-2** game ABI v2 | N/A | No game-DLL-facing surface. `VFileSystem009` is an engine-side compat shim, not a frozen game ABI (see External ABI contracts) |
| **G-3** dedicated debug thread | **Yes — named consumer (see P-1)** | `extension-goals.md` §G-3 names the filesystem *"already safe for off-main readers"*; the `shared_lock` read paths are that mechanism. Door rules identical to P-1: no per-`File` shared state, mount/unmount stays exclusive-write. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Mounted-path / open-handle overlays read the same typed surface; nothing filesystem-side beyond keeping introspection typed and value-returning. |
| **G-5** scripting runtime | **Door-keep (consumer, not provider)** | `extension-goals.md` §G-5 lists `Filesystem::load_file` / `search` as part of *"script surface v0"*. Keep those two methods P-4-conformant (value-returning, `string_view` in) so the script binding can call them directly with no new shim |

**miniz / ZIP note.** ZIP/PK3 inflate and deflated-WAD reads go through the
**shared `xash3dpp_miniz` static target** (built from `../public/miniz.c`),
linked **PRIVATE** by both `filesystem` and `content`. It is **not** owned by
`utilities`. Any future compression-backend door (zstd/lz4, per the legacy
survey's modernization list) is a *new backend*, not a change to this shared
target.

**Q-12 compat.** GoldSrc/Quake filesystem quirks (WAD auto-mount, `liblist.gam`
conversion, Quake gamedir auto-detection, the `valve` `singleplayer_only`
override) are behavioural constants living next to the code that needs them (see
Quirks). No `ICompatPolicy` seam is warranted yet — unlike `content`, these are
format-detection heuristics, not swappable policy.
