# Filesystem Compatibility Boundary

## Purpose

This document records the filesystem surfaces that must remain stable while the
implementation moves toward modular C++ internals.

The governing rule is:

```text
Modernize inward, preserve outward.
```

For the filesystem, "outward" includes the C `fs_api_t` interface, the
Valve-style `VFileSystem009` interface, legacy search path behavior, file and
search result ownership rules, path safety quirks, and DLL lookup behavior.

## Boundary Summary

| Boundary | Current Owner | Stability Rule |
| --- | --- | --- |
| `GetFSAPI` export | `filesystem/filesystem.c` | Keep name, version behavior, and output contract stable. |
| `fs_api_t` | `filesystem/filesystem.h` | Do not reorder, remove, or change signatures without explicit ABI decision. |
| `fs_globals_t` | `filesystem/filesystem.h` | Preserve game list and current game info visibility. |
| `fs_interface_t` | `filesystem/filesystem.h` | Preserve engine callback shape and optional callback handling. |
| `CreateInterface` | `filesystem/VFileSystem009.cpp` | Keep supported interface names and retval behavior. |
| `VFileSystem009` | `filesystem/VFileSystem009.h` | Keep vtable order and method signatures stable. |
| `searchpath_t` callbacks | `filesystem/filesystem_internal.h` | Treat as the legacy backend adapter contract. |
| `file_t` | `filesystem/filesystem_internal.h` | Preserve public opacity and internal lifecycle behavior. |
| `search_t` | `filesystem/filesystem.h` | Preserve packed allocation and caller-free expectations. |
| Path policy | `filesystem/filesystem.c` | Preserve rejection/direct-path quirks until tests say otherwise. |
| DLL lookup | `filesystem/filesystem.c` | Preserve short path, full path, rodir, and direct-path behavior. |

## `fs_api_t` Boundary

`fs_api_t` is the primary C API table returned from `GetFSAPI`. The engine
stores it in `g_fsapi` and calls through it from
`engine/common/filesystem_engine.c`.

Do not change these without an explicit design decision:

- function pointer order
- function pointer names
- function signatures
- allocator ownership rules
- return value meanings
- `FS_API_VERSION`
- `FS_API_CREATEINTERFACE_TAG`

### API Groups

| Group | Fields | Compatibility Notes |
| --- | --- | --- |
| Lifecycle | `InitStdio`, `ShutdownStdio` | Initialize and release global filesystem state. `InitStdio` scans game directories. |
| Search path utilities | `Rescan`, `ClearSearchPath`, `AllowDirectPaths`, `AddGameDirectory`, `AddGameHierarchy`, `Search`, `SetCurrentDirectory`, `FindLibrary`, `Path_f` | Most mount-order and path-policy compatibility lives here. |
| Game info utilities | `Gamedir`, `LoadGameInfo` | Exposes selected game folder and triggers hierarchy rescan. |
| File operations | `Open`, `Write`, `Read`, `Seek`, `Tell`, `Eof`, `Flush`, `Close`, `Gets`, `UnGetc`, `Getc`, `VPrintf`, `Printf`, `Print`, `FileLength`, `FileCopy` | Must preserve `file_t` behavior for direct files, archives, and compressed streams. |
| Buffer operations | `LoadFile`, `LoadDirectFile`, `WriteFile`, `LoadFileMalloc` | Allocation ownership differs between `LoadFile` and `LoadFileMalloc`. |
| Hashing | `CRC32_File`, `MD5_HashFile` | Public behavior should remain path-resolution compatible. |
| Filesystem operations | `FileExists`, `FileTime`, `FileSize`, `Rename`, `Delete`, `SysFileExists`, `GetDiskPath`, `GetFullDiskPath`, `GetRootDirectory` | Must preserve search ordering, write path behavior, and root path semantics. |
| Archive interface | `MountArchive_Fullpath`, `IsArchiveExtensionSupported`, `GetArchiveByName`, `FindFileInArchive`, `OpenFileFromArchive`, `LoadFileFromArchive` | Important for backend modularization; keep as adapter surface. |
| Game info generation | `MakeGameInfo` | Generates `gameinfo.txt` for current game. |

### `GetFSAPI` Contract

Current behavior:

1. If engine callbacks are provided, `FS_InitInterface` validates
   `FS_API_VERSION` and installs non-null callbacks.
2. `*api` receives a copy of `g_api`.
3. `*globals` receives `&FI`.
4. The function returns `FS_API_VERSION` on success.

Compatibility rules:

- Continue returning a copied table, not a pointer to mutable caller-owned data.
- Continue accepting null callback tables for standalone tests.
- Keep callback installation optional and field-by-field.
- Keep failure behavior for version mismatch.

## `fs_interface_t` Boundary

`fs_interface_t` is the engine-to-filesystem callback table. It supplies:

- console printing
- fatal error reporting
- memory pool allocation/free
- allocation/reallocation/free
- platform native object lookup

Compatibility rules:

- Keep null callback fallback behavior.
- Keep default stubs usable by standalone tests.
- Do not require engine callbacks for `tests/filesystem/no-init.c`.
- Do not let C++ exceptions cross through callback calls.

## `fs_globals_t` Boundary

`fs_globals_t` exposes:

- `const gameinfo_t *GameInfo`
- `gameinfo_t *games[MAX_MODS]`
- `int numgames`

Compatibility rules:

- Keep `FI` visible through `GetFSAPI`.
- Preserve current game discovery semantics.
- Preserve `GameInfo` as the selected current game after `LoadGameInfo`.
- Avoid changing `gameinfo_t` layout as part of filesystem internals work.

## `VFileSystem009` Boundary

`VFileSystem009` is the Valve-style C++ interface exposed by
`CreateInterface`.

Current supported interface names:

- `FILESYSTEM_INTERFACE_VERSION`, currently `VFileSystem009`
- `FS_API_CREATEINTERFACE_TAG`, currently `XashFileSystem004`

Compatibility rules:

- Do not change `FILESYSTEM_INTERFACE_VERSION`.
- Do not reorder methods in `IFileSystem`.
- Do not alter method signatures in `VFileSystem009.h`.
- Keep `CreateInterface("VFileSystem009")` returning the singleton
  `g_VFileSystem009`.
- Keep `CreateInterface("XashFileSystem004")` returning a copied `fs_api_t`.
- Preserve retval behavior: `0` for found, `1` for not found.

### Vtable Groups

The method order in `VFileSystem009.h` is the compatibility contract. It
contains these broad groups:

- mount/search path management
- file removal and directory creation
- file existence and directory checks
- open/close/seek/tell/size/time
- read/write/line/printf/read-buffer operations
- find-first/find-next/find-close search handles
- local path and parser helpers
- warning/resource preload stubs
- pack-file and cache-read helpers
- no-write search path addition

Modern C++ internals should sit behind this wrapper. They should not change this
interface.

## `searchpath_t` Callback Boundary

`searchpath_t` is the current internal backend adapter object. It is not public
engine ABI, but it is the safest migration seam.

Current fields:

- `filename`
- `type`
- `flags`
- backend union: `dir`, `pack`, `wad`, `zip`, `assets`
- `next`
- backend callbacks

Current callback contract:

| Callback | Purpose | Return/Ownership Notes |
| --- | --- | --- |
| `pfnPrintInfo` | Write human-readable mount info into caller buffer. | Used by `FS_Path_f`; must include enough info to preserve debug output. |
| `pfnClose` | Release backend-owned resources. | Called before freeing non-static `searchpath_t`. |
| `pfnOpenFile` | Open a backend file by fixed path/index. | Returns `file_t *` or null; caller closes with `FS_Close`. |
| `pfnFileTime` | Return backend file time. | Existing negative/missing behavior should be preserved. |
| `pfnFindFile` | Resolve a requested path in this backend. | Returns backend index or `-1`; may write fixed-case path. |
| `pfnSearch` | Append matching names to a `stringlist_t`. | Must preserve duplicate filtering expectations with `FS_Search`. |
| `pfnLoadFile` | Optional full-file load override. | Null means use open/read fallback. |

Compatibility rules:

- Keep callbacks available while introducing C++ backend objects.
- Prefer adapting `searchpath_t` to new internals over replacing all callers at
  once.
- Preserve `FS_Path_f` output semantics during refactors.
- Preserve search order by keeping `fs_searchpaths` mutation behavior stable.

## `file_t` Boundary

`file_t` is opaque in `filesystem.h` and defined in `filesystem_internal.h`.
Callers receive `file_t *` from `Open`, `OpenFileFromArchive`, and related
helpers, and release it with `Close`.

Important internal responsibilities:

- OS handle
- single-character ungetc state
- file time
- source `searchpath_t`
- uncompressed file length
- current position
- package offset
- flags
- optional decompression toolkit
- intermediate read buffer
- optional reduced-file-descriptor backup state

Compatibility rules:

- Keep `file_t` opaque outside filesystem internals.
- Preserve `FS_Close` as the cleanup path.
- Preserve behavior for direct files, package slices, and compressed streams.
- Preserve `FS_Tell`, `FS_Seek`, and `FS_Eof` semantics.
- Do not let backend-specific C++ file types leak through public APIs.

## `search_t` Boundary

`search_t` is returned by `FS_Search`.

Current shape:

- `numfilenames`
- `filenames`
- `filenamesbuffer`

Current allocation behavior:

- `FS_Search` allocates one packed block containing the `search_t`, filename
  pointer array, and filename text storage.
- Callers are expected to free the returned pointer according to the filesystem
  memory ownership convention.
- `VFileSystem009::FindFirst` owns the returned `search_t` inside
  `CSearchState` and frees it in `CSearchState` destruction.

Compatibility rules:

- Preserve packed allocation unless all callers are audited.
- Preserve sorted result behavior.
- Preserve duplicate elimination behavior.
- Preserve null return on no matches or invalid patterns.

## `FS_LoadFile` And `FS_LoadFileMalloc`

These two APIs intentionally differ by allocator ownership.

| API | Allocation Source | Caller Free Expected |
| --- | --- | --- |
| `FS_LoadFile` | filesystem memory path through `FS_CustomAlloc` / `Mem_Malloc` | filesystem memory free path |
| `FS_LoadFileMalloc` | standard `malloc` path | standard `free` |

Internally, both route through `FS_LoadFile_`, which calls
`FS_LoadFileFromArchive` with an allocator choice. Backend `pfnLoadFile`
callbacks receive allocator and free-function pointers and must use them.

Compatibility rules:

- Do not merge allocator behavior.
- Ensure backend custom loaders use the allocator callbacks they are given.
- Preserve trailing nul byte behavior for loaded files where current code adds
  it.
- Preserve null return on missing files or rejected paths.

## `FS_FindLibrary` Boundary

`FS_FindLibrary` resolves game/client DLL paths into `fs_dllinfo_t`.

Important current behavior:

- Temporarily enables direct paths according to the `directpath` argument.
- Strips some relative path forms before lookup.
- Lowercases the short path.
- Searches by `dllInfo->shortPath`.
- If indirect lookup fails, may try a `bin/` prefix.
- Uses `FS_FindFile` and search path metadata to build the full disk path.
- Treats rodir-backed absolute search paths specially.
- Fills `encrypted` and `custom_loader` flags.
- Always resets direct paths to false before returning.

Compatibility rules:

- Preserve direct-path enable/reset behavior.
- Preserve lowercase short path behavior until explicitly changed.
- Preserve `bin/` fallback behavior.
- Preserve rodir full-path handling.
- Preserve encrypted/custom loader detection.
- Add tests before changing any path normalization logic.

## Direct Path And Path Rejection Boundary

`FS_CheckNastyPath` rejects unsafe or non-portable paths when direct paths are
not enabled.

Rejected forms include:

- empty paths
- colon paths
- paths containing `//`
- paths containing `..`
- absolute paths beginning with `/`
- paths containing `/.`

`FS_AllowDirectPaths(true)` bypasses this check for compatibility-sensitive
engine workflows. `FS_FindFile` also has a direct-path workaround that strips a
leading `../` before checking under `fs_rootdir`.

Compatibility rules:

- Preserve current rejection behavior until tests cover it.
- Preserve direct-path bypass behavior until tests cover it.
- Preserve the `../` strip workaround until a design task explicitly changes it.
- Ensure all temporary direct-path use resets back to false.

## Review Checklist

Use this checklist before accepting filesystem implementation changes:

- Did `filesystem.h` change? If yes, was an ABI decision recorded?
- Did `VFileSystem009.h` change? If yes, was vtable compatibility reviewed?
- Did `searchpath_t` callback behavior change?
- Did search path ordering change?
- Did `fs_path` output change?
- Did `rodir` behavior change?
- Did allocator ownership change?
- Did DLL lookup behavior change?
- Did direct-path or path rejection behavior change?
- Were unit tests or smoke-test evidence added for the changed behavior?

## Related Documents

- `Documentation/codex/legacy/filesystem/architecture.md`
- `Documentation/codex/legacy/filesystem/windows-fs-path-baseline.md`
- `Documentation/codex/filesystem-glossary.md`
- `Documentation/codex/modularization-plan/filesystem-pilot.md`
- `tests/filesystem/README.md`
