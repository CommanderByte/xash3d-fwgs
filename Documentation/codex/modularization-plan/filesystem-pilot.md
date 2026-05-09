# Filesystem Pilot Plan

## Subsystem

Name: filesystem module.

Primary owner files:

- `filesystem/filesystem.c`
- `filesystem/filesystem_internal.h`
- `filesystem/filesystem.h`
- `filesystem/dir.c`
- `filesystem/pak.c`
- `filesystem/wad.c`
- `filesystem/zip.c`
- `filesystem/android.c`
- `filesystem/VFileSystem009.cpp`
- `filesystem/VFileSystem009.h`
- `engine/common/filesystem_engine.c`

Legacy architecture overview:

- `Documentation/codex/legacy/filesystem/architecture.md`

Compatibility boundary reference:

- `Documentation/codex/modularization-plan/boundary-filesystem.md`

Build target: `filesystem_stdio`, a shared library built from all
`filesystem/*.c` and `filesystem/*.cpp` files through `filesystem/wscript`.

Existing tests:

- `filesystem/tests/interface.cpp`
- `filesystem/tests/caseinsensitive.c`
- `filesystem/tests/no-init.c`

## Current Responsibilities

The filesystem module currently owns several related but distinct behaviors:

- Loading the `filesystem_stdio` shared library API through `GetFSAPI`.
- Exposing the C `fs_api_t` function table to the engine.
- Exposing Valve-style `VFileSystem009` through `CreateInterface`.
- Managing global search paths, write path, root directory, base directory,
  game directory, read-only directory, and language mount state.
- Discovering game directories and parsing `gameinfo.txt` or `liblist.gam`.
- Mounting plain directories, PAK, WAD, ZIP/PK3, PK3 directory, and Android
  asset search paths.
- Opening, reading, writing, seeking, loading, copying, renaming, deleting, and
  hashing files.
- Emulating case-insensitive lookup on case-sensitive filesystems.
- Enforcing path safety unless direct paths are temporarily allowed.
- Resolving game/client DLL paths for the engine.

This is more than "file IO". It is a virtual filesystem, mod discovery system,
archive manager, compatibility adapter, and DLL path resolver.

## Important Compatibility Boundaries

### C API Boundary

`filesystem/filesystem.h` defines `FS_API_VERSION`, `FS_API_CREATEINTERFACE_TAG`,
`fs_api_t`, `fs_globals_t`, `fs_interface_t`, `FSAPI`, and `GET_FS_API`.

The engine side in `engine/common/filesystem_engine.c` dynamically loads
`filesystem_stdio`, looks up `GetFSAPI`, passes engine callbacks and memory
functions, and stores the returned table in `g_fsapi`.

Do not change these early:

- `FS_API_VERSION`
- `GET_FS_API`
- `fs_api_t` layout
- `fs_globals_t` layout
- `fs_interface_t` layout
- `file_t` opacity from public headers
- `search_t` allocation/free expectations

### Valve-Style Interface Boundary

`filesystem/VFileSystem009.h` defines the `IFileSystem` virtual interface, and
`filesystem/VFileSystem009.cpp` provides `CreateInterface`.

Important compatibility facts:

- `FILESYSTEM_INTERFACE_VERSION` is `VFileSystem009` and is documented as
  "never change this".
- `CreateInterface` can return both `VFileSystem009` and a copied `fs_api_t`
  for `FS_API_CREATEINTERFACE_TAG`.
- The C++ interface is already a compatibility wrapper around C `FS_*`
  functions, not the core implementation.

Treat this as an ABI boundary. Internal C++ types should not leak into this
header or alter vtable order.

### Search Path Backend Boundary

`filesystem_internal.h` defines `searchpath_t`. This is the most useful
internal seam today:

- `filename`
- `type`
- `flags`
- backend union: `dir`, `pack`, `wad`, `zip`, `assets`
- `next`
- callback table: print, close, open, file time, find, search, load

Backends already implement this shape:

- `dir.c`: plain directory and PK3 directory behavior.
- `pak.c`: PAK archives.
- `wad.c`: WAD archives and lump loading.
- `zip.c`: ZIP/PK3 archives and decompression.
- `android.c`: Android asset manager search paths.

This is the best place to pilot internal C++ modularization because it is
already object-shaped without requiring public ABI changes.

### Game Hierarchy Boundary

`filesystem.c` owns game discovery and hierarchy assembly:

- `FS_InitStdio`
- `FS_ParseGameInfo`
- `FS_LoadGameInfo`
- `FS_Rescan`
- `FS_AddGameHierarchy`
- `FS_AddGameDirectory`

This code is behavior-sensitive. It controls `rodir`, writable directory
fallback, base game fallback, recursive mod dependencies, custom directories,
download directories, HD/LV/addon/localization mounts, and archive precedence.

Do not move this first. Document it and add coverage before changing behavior.

### Path Safety Boundary

`FS_CheckNastyPath`, `FS_AllowDirectPaths`, `FS_FindFile`, `FS_Open`, and
`FS_FindLibrary` are security and compatibility-sensitive.

Important quirks:

- Direct paths bypass normal path rejection.
- Direct path lookup has a special `../` strip workaround.
- Writes go through `fs_writepath` and directory case fixing.
- Reads walk the search path chain in order.
- DLL lookup lowercases short paths and has special direct-path handling.

Treat this as high-risk until tests cover the old behavior.

## Current Internal Coupling

Global state in `filesystem.c`:

- `FI`
- `fs_mempool`
- `fs_rootdir`
- `fs_writepath`
- `fs_searchpaths`
- `fs_basedir`
- `fs_gamedir`
- `fs_rodir`
- `fs_language`
- `fs_ext_path`
- `g_engfuncs`
- `g_api`

The backend files depend on shared internals:

- allocation through `fs_mempool` and `Mem_*` macros
- logging through `Con_*` macros
- file handles through `FS_SysOpen`, `FS_OpenHandle`, `FS_Close`, `FS_Read`,
  `FS_Seek`, `FS_Tell`
- path/string helpers from `public/` and `common/com_strings.h`
- `searchpath_t` callback contracts

The engine side depends on the exported table and has thin wrapper functions in
`engine/common/filesystem_engine.c`.

## Current Backend Model

The current implementation is already a manual vtable. That is good news.

| Backend | File | Search Type | Notes |
| --- | --- | --- | --- |
| Directory | `dir.c` | `SEARCHPATH_PLAIN` | Also handles `SEARCHPATH_PK3DIR`; owns case-fixing cache. |
| PAK | `pak.c` | `SEARCHPATH_PAK` | Reads linear PAK directory, sorted for lookup. |
| WAD | `wad.c` | `SEARCHPATH_WAD` | Handles WAD lumps and extension/type mapping. |
| ZIP/PK3 | `zip.c` | `SEARCHPATH_ZIP` | Reads central directory and supports deflated data. |
| Android assets | `android.c` | `SEARCHPATH_ANDROID_ASSETS` | Android-only asset manager backend. |

Each backend returns a `searchpath_t *` from `FS_Add*_Fullpath`, fills the
backend pointer and callbacks, and lets `filesystem.c` link it into
`fs_searchpaths`.

This suggests an incremental C++ model where backend implementation details can
be wrapped first while the `searchpath_t` callback ABI stays intact.

## Existing Tests And Gaps

Current tests cover a useful but small slice:

- API loading and `CreateInterface` lookup.
- Some no-init behavior.
- Case-insensitive directory lookup for simple create/open/delete flows.

Missing coverage before a serious pilot:

- Search path precedence across plain files, PAK, PK3, WAD, and `pk3dir`.
- `FS_GAMEDIRONLY_SEARCH_FLAGS` filtering.
- `rodir` plus writable root behavior.
- `FS_AddGameHierarchy` order for basedir, falldir, gamefolder, custom,
  downloaded, HD, LV, addon, and localization folders.
- Archive mounting idempotency.
- WADs mounted from archives.
- Direct path allow/deny behavior.
- `FS_FindLibrary` behavior for game/client DLL paths.
- `FS_Search` duplicate elimination and directory pseudo-results.
- Case fixing after external filesystem changes.
- `FS_LoadFileMalloc` versus `FS_LoadFile` allocator ownership.

## Proposed Internal Shape

Keep public ABI and exported C functions exactly as they are. Internally, move
toward these implementation concepts:

| Concept | Purpose | First Possible Home |
| --- | --- | --- |
| `FilesystemState` | Owns root dirs, search chain, write path, language, direct-path flag, game list view. | Later wrapper around current globals in `filesystem.c`. |
| `SearchPathOps` | Names the existing callback table contract. | C-compatible helper declarations in `filesystem_internal.h`. |
| `DirectorySearchPath` | Encapsulates `dir_t` cache, case sensitivity probing, case fixing, directory search. | First C++ pilot behind existing `searchpath_t`. |
| `ArchiveRegistry` | Owns supported archive extension table and mount dispatch. | Later extraction from `g_archives` and `FS_AddArchive_Fullpath`. |
| `GameHierarchyBuilder` | Builds mount order from `gameinfo_t`, flags, rodir, and language. | Later, after tests. |
| `PathPolicy` | Centralizes nasty path checks and direct-path rules. | Later, after tests. |
| `FileHandle` internals | RAII for descriptor close, decompression state, package offsets, backup handles. | Later, after backend pilot. |

The first pilot should not introduce all of these. It should prove one small
pattern: C++ implementation object hidden behind unchanged C callbacks.

## Recommended First Pilot: Directory Backend

Why directory backend first:

- `dir.c` already owns a clear private `dir_t` tree.
- It has existing focused coverage through `caseinsensitive.c`.
- It is important enough to reveal real issues, but less format-sensitive than
  PAK/WAD/ZIP parsing.
- It does not require changing public `filesystem.h`.

Initial shape:

1. Add tests that freeze current directory behavior.
2. Rename or split `dir.c` into a C++ implementation file only after tests
   exist.
3. Keep `FS_AddDir_Fullpath`, `FS_InitDirectorySearchpath`, and
   `FS_FixFileCase` callable from C.
4. Hide the case-cache tree behind a private C++ type.
5. Keep `searchpath_t` allocation, fields, and callbacks intact.
6. Use RAII only for local temporary cleanup first, not for externally owned
   `searchpath_t` lifetime.

First code direction:

```cpp
namespace fs
{
class DirectoryCache
{
public:
    explicit DirectoryCache(const char *root);
    ~DirectoryCache();

    bool fixCase(const char *path, char *out, size_t outSize, bool createPath);
    int findFile(const char *path, char *fixedName, size_t fixedNameSize);
    void search(stringlist_t *out, const char *pattern, bool caseInsensitive);

private:
    // Wrap the current dir_t tree first; replace it later only if useful.
};
}
```

The exported C callbacks would remain thin adapters:

```cpp
static int FS_FindFile_DIR(searchpath_t *search, const char *path,
    char *fixedname, size_t len)
{
    return static_cast<fs::DirectoryCache *>(search->dir)
        ->findFile(path, fixedname, len);
}
```

That exact cast is only illustrative. The real implementation may need a
transition wrapper because `searchpath_t::dir` is currently typed as `dir_t *`.

## Test Plan Before Refactor

Add or expand filesystem tests in this order:

1. Directory case fixing:
   - existing file found with wrong case
   - newly created file detected after cache refresh
   - nested directory creation through write path
   - parent path rejected unless direct paths are enabled

2. Search path ordering:
   - loose file overrides archive file
   - later game directory overrides earlier base directory
   - `gamedironly` excludes static/base paths where expected

3. Archive basics:
   - PAK open/search/filetime
   - ZIP stored file open/search
   - ZIP deflated file load
   - WAD lump lookup by extension/type

4. Game hierarchy:
   - basedir plus gamefolder mount order
   - falldir mount order
   - rodir plus writable root precedence
   - custom/downloaded/HD/LV/addon/localization folders

5. Interface compatibility:
   - `GetFSAPI` version and table still load
   - `CreateInterface("VFileSystem009")` still works
   - `CreateInterface("XashFileSystem004")` still returns a copied API table

Manual smoke recipe:

1. Build Windows client with SDL2.
2. Build `hlsdk-portable`.
3. Run `scripts/setup-windows-runtime.ps1`.
4. Launch with Steam assets through `XASH3D_RODIR`.
5. Confirm menu/first frame.
6. Run `fs_path` in console and compare search path ordering against baseline.

## Migration Steps

### Step 1: Boundary Notes

Create a `boundary-filesystem.md` note that freezes:

- public C API table fields
- `VFileSystem009` vtable expectations
- `searchpath_t` callback contract
- `file_t` ownership expectations
- `search_t` allocation/free ownership
- allocator behavior for `FS_LoadFile` versus `FS_LoadFileMalloc`

### Step 2: Tests

Add focused tests before moving backend implementation. Prefer small fixture
directories and generated tiny archives instead of depending on real game data.

Important: tests should exercise the public API table where practical, because
the engine consumes the module that way.

### Step 3: Directory Backend Wrapper

Convert the directory backend internals behind stable C entry points:

- keep `FS_AddDir_Fullpath`
- keep `FS_InitDirectorySearchpath`
- keep `FS_FixFileCase`
- keep `searchpath_t` callbacks
- keep `dir_t` compatible until all callers are adapted

Success means only the implementation language changes; observed behavior does
not.

### Step 4: Search Path Helper Cleanup

After the directory backend is stable, extract small helpers around:

- appending a search path
- closing non-static paths
- archive extension dispatch
- WAD-in-archive mounting

Do not introduce a full `FilesystemState` object yet. First reduce the number
of places that mutate the linked list directly.

### Step 5: Archive Backends

Only after directory backend and tests settle:

- consider PAK as the next C++ backend
- then ZIP/PK3, because decompression and ZIP64 details are more sensitive
- leave WAD until texture/lump behavior has specific coverage

### Step 6: Game Hierarchy Builder

Move game hierarchy assembly last within the filesystem pilot. It is compact
enough to want cleanup, but it is also the compatibility heart of mod loading.

## Risks

Risk: accidentally changing search path precedence.
Mitigation: write ordering tests and compare `fs_path` output before and after.

Risk: breaking case-insensitive emulation on Linux or Android.
Mitigation: preserve current probing behavior and test nested case repair.

Risk: leaking C++ types into public SDK headers.
Mitigation: keep C++ declarations in implementation files or private headers
included only from C++ translation units.

Risk: changing memory ownership.
Mitigation: keep `Mem_*`, `FS_LoadFile`, `FS_LoadFileMalloc`, and `search_t`
ownership contracts unchanged until explicitly documented and tested.

Risk: breaking no-init behavior.
Mitigation: keep `filesystem/tests/no-init.c` passing and expand it when new
wrappers are introduced.

Risk: over-abstracting too early.
Mitigation: first pilot wraps one backend only. Larger state objects wait until
tests prove the current behavior.

## Open Questions

- Should backend implementation files use `.cpp` while preserving C entry
  points, or should we create adjacent `*_cpp.cpp` files and leave existing
  `.c` adapter files in place?
- Do we want C++ exceptions disabled for all filesystem C++ files? Waf already
  adds `-fno-exceptions` for non-MSVC C++ in this target.
- Should `searchpath_t` remain the long-term internal backend object, or become
  a C adapter around private C++ backend objects?
- How should tests generate PAK/WAD/ZIP fixtures portably?
- Should `FS_SetCurrentDirectory` remain fatal on failure, or should that be
  addressed separately before deeper refactors?

## Near-Term Backlog

- Add `boundary-filesystem.md`.
- Add filesystem fixture helpers for tests.
- Add search path ordering tests for loose files versus archives.
- Add `rodir` smoke-test documentation using the current Steam asset setup.
- Add a baseline `fs_path` capture from the working Windows runtime.
- Prototype a tiny private C++ helper in the directory backend without changing
  public behavior.
