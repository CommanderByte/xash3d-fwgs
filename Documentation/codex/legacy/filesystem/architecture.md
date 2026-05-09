# Legacy Filesystem Architecture

## Purpose

The current filesystem is not just a file abstraction. It is the engine's
virtual filesystem, mod discovery system, archive mounting layer, case
compatibility layer, DLL path resolver, and compatibility adapter for
Valve-style filesystem consumers.

It is implemented as a shared library target named `filesystem_stdio`. The
engine loads that module at runtime and talks to it through a C function table.
The module also exposes `VFileSystem009` for code that expects the older
Valve-style C++ interface.

## Module Map

```mermaid
flowchart TB
    Engine["engine/common/filesystem_engine.c"]
    Module["filesystem_stdio shared library"]
    PublicHeader["filesystem/filesystem.h"]
    InternalHeader["filesystem/filesystem_internal.h"]
    Core["filesystem/filesystem.c"]
    VFS["filesystem/VFileSystem009.cpp"]
    Dir["filesystem/dir.c"]
    Pak["filesystem/pak.c"]
    Wad["filesystem/wad.c"]
    Zip["filesystem/zip.c"]
    Android["filesystem/android.c"]
    PublicLib["public/ helpers"]
    EngineCommon["common/com_strings.h and engine callbacks"]

    Engine -->|"COM_LoadLibrary + GetFSAPI"| Module
    Engine -->|"uses fs_api_t"| PublicHeader
    Module --> Core
    Module --> VFS
    Module --> Dir
    Module --> Pak
    Module --> Wad
    Module --> Zip
    Module --> Android
    Core --> PublicHeader
    Core --> InternalHeader
    Dir --> InternalHeader
    Pak --> InternalHeader
    Wad --> InternalHeader
    Zip --> InternalHeader
    Android --> InternalHeader
    Core --> PublicLib
    Dir --> PublicLib
    Pak --> PublicLib
    Wad --> PublicLib
    Zip --> PublicLib
    Core --> EngineCommon
```

## Exported Interfaces

```mermaid
flowchart LR
    Engine["Engine"]
    GetFSAPI["GetFSAPI(version, api, globals, callbacks)"]
    FsApi["fs_api_t function table"]
    Globals["fs_globals_t / FI"]
    Callbacks["fs_interface_t engine callbacks"]
    CreateInterface["CreateInterface(name, retval)"]
    ValveFS["IFileSystem VFileSystem009"]
    ApiCopy["copied fs_api_t for XashFileSystem004"]

    Engine --> GetFSAPI
    GetFSAPI --> FsApi
    GetFSAPI --> Globals
    Engine --> Callbacks
    Callbacks --> GetFSAPI
    Engine --> CreateInterface
    CreateInterface --> ValveFS
    CreateInterface --> ApiCopy
```

The C API boundary is the primary engine boundary. `VFileSystem009` is a C++
compatibility wrapper that mostly delegates back to C `FS_*` functions.

Keep stable:

- `GET_FS_API`
- `FS_API_VERSION`
- `FS_API_CREATEINTERFACE_TAG`
- `FILESYSTEM_INTERFACE_VERSION`
- `fs_api_t`
- `fs_globals_t`
- `fs_interface_t`
- `search_t` ownership expectations
- `file_t` opacity outside filesystem internals

## Core State

```mermaid
classDiagram
    class fs_globals_t {
        +const gameinfo_t* GameInfo
        +gameinfo_t* games[MAX_MODS]
        +int numgames
    }

    class filesystem_globals {
        +poolhandle_t fs_mempool
        +char fs_rootdir[MAX_SYSPATH]
        +searchpath_t* fs_writepath
        -searchpath_t* fs_searchpaths
        -char fs_basedir[MAX_SYSPATH]
        -char fs_gamedir[MAX_SYSPATH]
        -char fs_rodir[MAX_SYSPATH]
        -string fs_language
        -qboolean fs_ext_path
    }

    class gameinfo_t {
        +char gamefolder[MAX_QPATH]
        +char basedir[MAX_QPATH]
        +char falldir[MAX_QPATH]
        +char startmap[MAX_QPATH]
        +char trainmap[MAX_QPATH]
        +char title[64]
        +char dll_path[MAX_QPATH]
        +char game_dll[MAX_QPATH]
        +qboolean internal_vgui_support
        +qboolean rodir
        +int64_t mtime
    }

    class fs_api_t {
        +InitStdio()
        +ShutdownStdio()
        +Rescan()
        +AddGameDirectory()
        +AddGameHierarchy()
        +Search()
        +Open()
        +LoadFile()
        +FindLibrary()
        +MountArchive_Fullpath()
    }

    filesystem_globals --> fs_globals_t : owns FI
    fs_globals_t --> gameinfo_t : indexes games
    fs_api_t --> filesystem_globals : operates on
```

Most state is process-global inside `filesystem.c`. Modernization should first
wrap this state conceptually, then mechanically, and only later move storage.

## Search Path Object Model

`searchpath_t` is the central legacy object. It combines metadata, backend
storage, a linked-list pointer, and a callback table.

```mermaid
classDiagram
    class searchpath_t {
        +string filename
        +searchpathtype_t type
        +int flags
        +dir_t* dir
        +pack_t* pack
        +wfile_t* wad
        +zip_t* zip
        +android_assets_t* assets
        +searchpath_t* next
        +pfnPrintInfo()
        +pfnClose()
        +pfnOpenFile()
        +pfnFileTime()
        +pfnFindFile()
        +pfnSearch()
        +pfnLoadFile()
    }

    class dir_t {
        +string name
        +int numentries
        +dir_t* entries
    }

    class pack_t {
        +file_t* handle
        +int numfiles
        +dpackfile_t files[]
    }

    class wfile_t {
        +int infotableofs
        +int numlumps
        +poolhandle_t mempool
        +file_t* handle
        +dlumpinfo_t* lumps
        +time_t filetime
    }

    class zip_t {
        +file_t* handle
        +int numfiles
        +zipfile_t files[]
    }

    class android_assets_t {
        +AAssetManager* manager
        +string path
    }

    searchpath_t --> dir_t : SEARCHPATH_PLAIN / PK3DIR
    searchpath_t --> pack_t : SEARCHPATH_PAK
    searchpath_t --> wfile_t : SEARCHPATH_WAD
    searchpath_t --> zip_t : SEARCHPATH_ZIP
    searchpath_t --> android_assets_t : SEARCHPATH_ANDROID_ASSETS
    searchpath_t --> searchpath_t : next
```

This is already a manual object model. The callback table is the current
polymorphic seam.

## Backend Callback Matrix

| Callback | Directory | PAK | WAD | ZIP/PK3 | Android Assets |
| --- | --- | --- | --- | --- | --- |
| Print info | yes | yes | yes | yes | yes |
| Close | yes | yes | yes | yes | yes |
| Open file | yes | yes | yes | yes | yes |
| File time | yes | yes | yes | yes | yes |
| Find file | yes | yes | yes | yes | yes |
| Search | yes | yes | yes | yes | yes |
| Load file | default via open/read | default via open/read | custom lump load | custom ZIP load | custom asset load |

The directory backend is the best first modernization pilot because it has a
clear cache object and already has a case-insensitive behavior test.

## File Handle Model

```mermaid
classDiagram
    class file_t {
        +int handle
        +int ungetc
        +time_t filetime
        +searchpath_t* searchpath
        +fs_offset_t real_length
        +fs_offset_t position
        +fs_offset_t offset
        +uint32_t flags
        +ztoolkit_t* ztk
        +fs_offset_t buff_ind
        +fs_offset_t buff_len
        +byte buff[FILE_BUFF_SIZE]
    }

    class ztoolkit_t {
        +z_stream zstream
        +size_t comp_length
        +size_t in_ind
        +size_t in_len
        +size_t in_position
        +byte input[FILE_BUFF_SIZE]
    }

    file_t --> searchpath_t : source
    file_t --> ztoolkit_t : optional deflate state
```

`file_t` is opaque in the public header but fully defined in
`filesystem_internal.h`. It represents direct disk files, package slices, and
compressed streams.

## Startup Sequence

```mermaid
sequenceDiagram
    participant Host as Host startup
    participant EngineFS as engine/common/filesystem_engine.c
    participant Module as filesystem_stdio
    participant Core as filesystem.c
    participant Disk as Disk / Steam assets

    Host->>EngineFS: FS_Init()
    EngineFS->>EngineFS: determine rootdir from XASH3D_BASEDIR / SDL / cwd
    EngineFS->>EngineFS: determine rodir from -rodir / XASH3D_RODIR
    EngineFS->>Module: COM_LoadLibrary("filesystem_stdio")
    EngineFS->>Module: GetFSAPI(FS_API_VERSION, &g_fsapi, &FI, callbacks)
    Module->>Core: FS_InitInterface(callbacks)
    EngineFS->>Core: SetCurrentDirectory(rootdir)
    EngineFS->>Core: InitStdio(rootdir, basedir, gamedir, rodir)
    Core->>Core: initialize fs_mempool and globals
    Core->>Disk: scan rodir game directories
    Core->>Disk: scan writable root game directories
    Core->>Core: parse gameinfo.txt / liblist.gam
    Core-->>EngineFS: success
    EngineFS->>EngineFS: register fs_* commands and cvars
```

## Game Load And Rescan Sequence

```mermaid
sequenceDiagram
    participant Engine as Engine
    participant Core as filesystem.c
    participant Search as fs_searchpaths
    participant Backend as searchpath backend

    Engine->>Core: LoadGameInfo(flags, language)
    Core->>Core: select FI.GameInfo for fs_gamedir
    Core->>Core: ensure writable game dir when selected game came from rodir
    Core->>Core: Rescan(flags, language)
    Core->>Search: ClearSearchPath(non-static only)
    Core->>Core: add extras paks from environment
    Core->>Core: AddGameHierarchy(basedir)
    Core->>Core: AddGameHierarchy(falldir)
    Core->>Core: AddGameHierarchy(gamefolder)
    loop each hierarchy directory
        Core->>Core: AddGameDirectory(dir, flags)
        Core->>Backend: mount archives in PAK -> PK3 -> PK3DIR -> WAD order
        Core->>Backend: mount raw directory last for loose-file priority
        Backend-->>Search: prepend searchpath_t
    end
```

Important behavior: raw directories are mounted after archives but prepended to
the search chain, so loose files get priority over packed files in that game
directory.

## Read Sequence

```mermaid
sequenceDiagram
    participant Caller as Caller
    participant Core as FS_Open / FS_LoadFile
    participant Policy as Path policy
    participant Search as fs_searchpaths
    participant Backend as searchpath_t callbacks
    participant File as file_t

    Caller->>Core: FS_Open(path, "rb", gamedironly)
    Core->>Policy: strip leading slashes and check nasty path
    Policy-->>Core: allowed
    Core->>Search: FS_FindFile(path, gamedironly)
    loop search paths in order
        Search->>Backend: pfnFindFile(path, fixedname)
        alt found
            Backend-->>Search: pack index or zero
            Search-->>Core: searchpath + fixed path
        end
    end
    Core->>Backend: pfnOpenFile(searchpath, fixed path, mode, pack index)
    Backend-->>Core: file_t
    Core-->>Caller: file_t
```

`FS_LoadFile` follows the same search path lookup, then either calls a backend
`pfnLoadFile` override or opens and reads through `file_t`.

## Write Sequence

```mermaid
sequenceDiagram
    participant Caller as Caller
    participant Core as FS_Open
    participant Policy as Path policy
    participant WritePath as fs_writepath directory backend
    participant Disk as Disk

    Caller->>Core: FS_Open(path, "wb" / "ab" / "+", gamedironly)
    Core->>Policy: strip leading slashes and check nasty path
    Policy-->>Core: allowed
    Core->>WritePath: FS_FixFileCase(writepath->dir, path, createpath=true)
    WritePath-->>Core: real disk path
    Core->>Disk: FS_CreatePath(real disk path)
    Core->>Disk: FS_SysOpen(real disk path, mode)
    Disk-->>Caller: file_t
```

Writes do not walk all search paths. They target the current write path.

## DLL Lookup Sequence

```mermaid
sequenceDiagram
    participant Engine as Engine loader
    participant Core as FS_FindLibrary
    participant Search as FS_FindFile
    participant Backend as searchpath backend

    Engine->>Core: FindLibrary(dllname, directpath, dllInfo)
    Core->>Core: AllowDirectPaths(directpath)
    Core->>Core: normalize and lowercase short path
    alt indirect lookup
        Core->>Search: FindFile(short path)
        Search->>Backend: pfnFindFile
    else direct lookup
        Core->>Search: FindFile(dllname)
        Search->>Backend: pfnFindFile or direct root path
    end
    Core->>Core: fill fullPath, shortPath, encryption/custom loader flags
    Core->>Core: AllowDirectPaths(false)
    Core-->>Engine: fs_dllinfo_t
```

This flow is compatibility-sensitive because game and client DLL loading
depends on exact search path and rodir behavior.

## Directory Backend Sequence

```mermaid
sequenceDiagram
    participant Core as filesystem.c
    participant DirBackend as dir.c
    participant Cache as dir_t tree
    participant Disk as Disk directory

    Core->>DirBackend: FS_AddDir_Fullpath(path, flags)
    DirBackend->>DirBackend: FS_InitDirectorySearchpath(search, path, flags)
    DirBackend->>Disk: listdirectory(path)
    DirBackend->>Cache: populate sorted entries
    Core->>DirBackend: pfnFindFile(relative path)
    DirBackend->>Cache: walk cached entries case-insensitively
    alt cache misses but directory may have changed
        DirBackend->>Disk: rescan affected directory
        DirBackend->>Cache: merge entries
    end
    DirBackend-->>Core: fixed-case path or not found
```

This backend is a good first C++ pilot because `dir_t` is already a private
object graph with clear lifecycle and behavior.

## Archive Mount Sequence

```mermaid
flowchart TD
    AddGameDirectory["FS_AddGameDirectory(dir, flags)"]
    List["listdirectory(dir)"]
    ArchiveOrder["Archive order: pak -> pk3 -> pk3dir -> wad"]
    MountArchive["FS_AddArchive_Fullpath(archive, fullpath, flags)"]
    AlreadyMounted{"same type and filename already mounted?"}
    BackendAdd["FS_AddPak/Wad/Zip/Dir_Fullpath"]
    Prepend["prepend searchpath_t to fs_searchpaths"]
    LoadWads{"archive load_wads and not FS_SKIP_ARCHIVED_WADS?"}
    SearchWads["search archive for *.wad"]
    AddPackedWad["FS_AddWad_Fullpath(fullpath, FS_LOAD_PACKED_WAD)"]
    AddDirectory["mount raw directory last"]
    MountDirectory["FS_AddArchive_Fullpath(directory backend, dir, flags)"]

    AddGameDirectory --> List
    List --> ArchiveOrder
    ArchiveOrder --> MountArchive
    MountArchive --> AlreadyMounted
    AlreadyMounted -- yes --> AddDirectory
    AlreadyMounted -- no --> BackendAdd
    BackendAdd --> Prepend
    Prepend --> LoadWads
    LoadWads -- yes --> SearchWads
    SearchWads --> AddPackedWad
    AddPackedWad --> Prepend
    LoadWads -- no --> AddDirectory
    AddDirectory --> MountDirectory
    MountDirectory --> Prepend
```

## Behavior Hotspots

These areas should be documented and tested before modernization changes them:

- `FS_AddGameHierarchy` mount order and recursive basedir handling.
- `rodir` overlay behavior between read-only Steam assets and writable root.
- Loose-file precedence over archives.
- PAK, PK3, PK3DIR, WAD archive mount order.
- WADs discovered inside PAK/PK3 archives.
- `gamedironly` filtering through `FS_GAMEDIRONLY_SEARCH_FLAGS`.
- Direct path escape behavior and the special `../` strip.
- Case-insensitive directory cache refresh.
- `FS_FindLibrary` short path casing and full path resolution.
- `FS_LoadFile` versus `FS_LoadFileMalloc` allocator ownership.
- No-init behavior tested by `tests/filesystem/no-init.c`.

## Modernization Reading Notes

The most promising legacy boundaries are:

- `searchpath_t` callback table as the first adapter seam.
- `dir_t` cache as the first private object to wrap.
- `g_archives` and `FS_AddArchive_Fullpath` as a future archive registry.
- `FS_AddGameHierarchy` as a future game hierarchy builder, after tests.
- `FS_CheckNastyPath` and `FS_AllowDirectPaths` as a future path policy, after
  tests.

The least safe early moves are:

- changing `fs_api_t`
- changing `VFileSystem009`
- moving `gameinfo_t` parsing or hierarchy order
- changing allocator ownership
- changing DLL lookup behavior
