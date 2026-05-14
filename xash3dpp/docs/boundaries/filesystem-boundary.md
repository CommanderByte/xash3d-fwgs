# Filesystem Boundary Spec

> Legacy survey background: [docs/legacy-survey/filesystem.md](../legacy-survey/filesystem.md)

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

The `IFileSystem` / `VFileSystem009` C++ vtable (`FILESYSTEM_INTERFACE_VERSION
"VFileSystem009"`) is a compatibility shim for mods that call through the
GoldSrc interface. It is **not** a fixed ABI constraint for the rewrite — no
game DLL is allowed to link against it directly; they receive it through the
engine at runtime.

## Interface (what the rest of the engine calls)

All calls go through the `fs_api_t` function-pointer table returned by
`GetFSAPI`. The engine also reads `fs_globals_t::GameInfo` (a `const
gameinfo_t *`) directly.

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

2. **`VFileSystem009` retention** — **Keep it for now.** Exact placement
   (separate compilation unit, opt-in shim header, etc.) is deferred until
   the engine's own public-API structure is established.

3. **Async I/O** — **Desired, but deferred.** The engine threading model
   must be established first. The synchronous API should be the initial
   target; async can be layered once the job/scheduler design is known.

4. **Memory ownership model** — The legacy split (`LoadFile` → pool memory,
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

5. **Thread safety** — **Required.** Internal mutable state (search-path
   list, write-path pointer, game-info pointer) must be protected. At minimum:
   a shared/exclusive (reader-writer) lock for path queries and file opens,
   with exclusive acquisition for mount/unmount operations. Per-file `Read`/
   `Write`/`Seek` operations should not block global path lookups.

6. **`gameinfo_t` location** — Currently embedded in `filesystem.h` alongside
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

7. **Case-insensitive filename lookup** — The legacy trie (`dir.c`) is
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
