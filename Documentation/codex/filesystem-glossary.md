# Filesystem Glossary

This glossary defines recurring filesystem terms used in the Codex
documentation. The descriptions reflect the current legacy implementation unless
explicitly marked as future design language.

## Core Terms

### Filesystem Module

The dynamically loaded `filesystem_stdio` library. The engine loads it through
`COM_LoadLibrary`, obtains `GetFSAPI`, and then calls through the returned
`fs_api_t` table.

### Virtual Filesystem

The logical file view built from writable directories, read-only directories,
archives, and special backends. Callers usually ask for paths such as
`maps/c0a0.bsp` or `dlls/hl.dll`, and the filesystem resolves those against its
search path chain.

### Search Path

One mounted source of files. In code this is represented by `searchpath_t`.
Search paths can be plain directories, PAK archives, WAD files, ZIP/PK3
archives, PK3 directory trees, or Android asset roots.

The search path order matters: earlier entries have lookup priority over later
entries.

### Search Path Chain

The linked list rooted at `fs_searchpaths`. `FS_FindFile`, `FS_Search`, and
related read operations walk this chain to find matching content.

### Search Path Flags

Metadata bits on a `searchpath_t`. Important examples include:

- `FS_STATIC_PATH`: path survives `FS_ClearSearchPath`.
- `FS_NOWRITE_PATH`: path should not become the write path.
- `FS_GAMEDIR_PATH`: path belongs to the active game directory.
- `FS_CUSTOM_PATH`: path contains custom/mod or generated data.
- `FS_GAMERODIR_PATH`: path belongs to the game directory but came from rodir.

### Write Path

The directory search path stored in `fs_writepath`. Writes go here instead of
walking the full search path chain. `FS_AddGameDirectory` updates this unless
the mount uses `FS_NOWRITE_PATH`.

### Root Directory

The writable base root passed to `FS_InitStdio` and stored in `fs_rootdir`.
During the current Windows smoke test this is:

```text
C:\git\xash3d-fwgs\run-win32
```

### RoDir

The read-only root directory passed through `XASH3D_RODIR` or `-rodir`, stored
in `fs_rodir`. It lets the engine use assets from a read-only installation while
keeping generated files and local overrides in the writable root.

In the current Windows smoke test, `rodir` points to the Steam Half-Life
installation.

### Game Directory

The active game folder. For Half-Life this is usually `valve`. It is stored in
`fs_gamedir` before game info is selected, and in `FI.GameInfo->gamefolder`
after selection.

### Base Directory

The fallback base game folder stored in `gameinfo_t::basedir`. For normal
Half-Life content this is also usually `valve`. For mods, the active game
folder may differ while `basedir` remains the inherited base content.

### Fallback Directory

The optional extra fallback folder stored in `gameinfo_t::falldir`. It is
mounted by `FS_Rescan` when it differs from the base and active game folders.

### Game Hierarchy

The ordered set of folders mounted for a game. `FS_AddGameHierarchy` is the
legacy function that expands a game folder into rodir content, downloaded
content, base content, custom content, HD/LV/addon/localization folders, and
related archive mounts.

## Archive Terms

### Archive

A mounted packed file source. Current real archive types include PAK, WAD, and
ZIP/PK3. Some directory-like sources, such as PK3 directory trees, use similar
archive dispatch behavior but are not real archive files.

### PAK

The Quake-style `.pak` archive backend implemented in `filesystem/pak.c`.
Entries are read from a linear archive directory and sorted for lookup.

### WAD

The `.wad` archive backend implemented in `filesystem/wad.c`. WAD content is
organized as lumps. Lookup behavior includes type/extension mapping for common
Half-Life and Quake asset types.

### ZIP / PK3

The ZIP archive backend implemented in `filesystem/zip.c`. `.pk3` uses this
same backend. The backend supports stored and deflated entries.

### PK3 Directory

A plain directory with a `.pk3dir` extension. It is backed by the directory
backend but uses `SEARCHPATH_PK3DIR` so it behaves like a directory-shaped PK3
source.

### Android Assets

The Android asset manager backend implemented in `filesystem/android.c`. It
appears as a search path type when building for Android.

## API And Compatibility Terms

### `fs_api_t`

The C function table returned by `GetFSAPI`. This is the primary legacy API the
engine uses to call filesystem operations.

### `GetFSAPI`

The exported C entry point from `filesystem_stdio`. The engine calls it with
`FS_API_VERSION` and receives `fs_api_t` plus `fs_globals_t`.

### `VFileSystem009`

The Valve-style C++ filesystem interface exposed by `CreateInterface`. In this
codebase it is mostly a compatibility wrapper around the C `FS_*` functions.

### `CreateInterface`

The exported factory function that can return `VFileSystem009` or a copied
`fs_api_t` for `FS_API_CREATEINTERFACE_TAG`.

### `search_t`

The public search-result structure returned by `FS_Search`. It owns a packed
allocation containing filename pointers and filename storage. Callers must free
it according to the filesystem API ownership rules.

### `file_t`

An opaque public file handle type. Internally it stores the OS handle, source
search path, package offset, real length, current position, buffering state,
and optional decompression state.

## Behavior Terms

### Loose File

A normal file in a mounted directory. Loose files often override packed archive
content because of search path ordering.

### Archive Precedence

The order in which archive-backed search paths are mounted and searched. This
must remain stable unless a compatibility decision explicitly changes it.

### Direct Paths

A temporary mode controlled by `FS_AllowDirectPaths`. When enabled, path safety
rules are relaxed for special engine workflows such as DLL discovery. This is
compatibility-sensitive and should be tested before refactoring.

### Nasty Path

A path rejected by `FS_CheckNastyPath`, such as empty paths, parent directory
escapes, absolute paths, colon paths, or other non-portable/unsafe forms.

### Case Fixing

The directory backend's behavior of resolving paths case-insensitively on
case-sensitive filesystems. This is handled through the `dir_t` cache and
`FS_FixFileCase`.

### Runtime Smoke Test

A manual or scripted launch that proves the built engine can start with known
assets. For this branch, the key Windows smoke setup uses local `run-win32`
runtime files plus Steam Half-Life assets through `XASH3D_RODIR`.
