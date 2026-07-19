# Deep Dive: Legacy `filesystem/` — VFS, Archive Backends, On-Disk Formats & Plugin ABI

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy virtual filesystem plugin at
the repository root (`filesystem/*.c`, `filesystem/filesystem_internal.h`,
`common/wadfile.h`, `engine/filesystem.h`) that the `xash3dpp` `filesystem`
subsystem replaces. Wide survey coverage lives in
[filesystem.md](filesystem.md); this is the narrow-and-exact companion. Line
numbers are against the working tree on that date; behaviour references, not
design constraints. Everything here is **legacy** — read for on-disk formats,
the frozen archive layouts, and the `GetFSAPI` plugin contract, not as a rewrite
blueprint (the rewrite chose a leaner pimpl + `ISearchBackend` design — see
§8).*

Primary sources:

- `filesystem/filesystem.c` — search-path management, file lifecycle, gameinfo
- `filesystem/filesystem_internal.h` — `searchpath_t`, `file_t`, `ztoolkit_t`
- `filesystem/pak.c` — Quake PAK backend + `dpackheader_t` / `dpackfile_t`
- `filesystem/wad.c` + `common/wadfile.h` — WAD2/WAD3 backend + `dwadinfo_t` /
  `dlumpinfo_t` / `TYP_*`
- `filesystem/zip.c` — ZIP/PK3 backend + LFH/CDF/EOCD records
- `filesystem/dir.c` — plain-directory backend + case-insensitivity trie
- `filesystem/android.c` — NDK `AAssetManager` backend
- `filesystem/VFileSystem009.cpp` — GoldSrc `IFileSystem` C++ vtable shim
- `engine/filesystem.h` — `fs_api_t`, `fs_globals_t`, `gameinfo_t`, `GetFSAPI`

**Global assumptions:** the legacy plugin is single-threaded (no locks); all
sizes are host-native; PAK/WAD structs are read raw with no `#pragma pack` (ZIP
records *are* `#pragma pack(1)`). The plugin depends on `engine/` only through
injected `fs_interface_t` callbacks — it never links toward the engine.

______________________________________________________________________

## 1. Search-path model

### 1.1 `searchpath_t` (`filesystem_internal.h:97-125`)

An intrusive singly-linked list; the **head has highest priority** (paths are
*prepended*). Each node is a tagged union of one backend plus a 7-entry
function-pointer vtable:

```c
typedef enum {
    SEARCHPATH_PLAIN = 0,       // plain directory
    SEARCHPATH_PAK,             // Quake PAK
    SEARCHPATH_WAD,             // GoldSrc WAD2/WAD3
    SEARCHPATH_ZIP,             // ZIP / PK3
    SEARCHPATH_PK3DIR,          // a directory that behaves like a ZIP
    SEARCHPATH_ANDROID_ASSETS   // NDK AAsset
} searchpathtype_t;

typedef struct searchpath_s {
    string           filename;             // mounted disk path
    searchpathtype_t type;
    int              flags;                // FS_* path flags (see §1.2)
    union { dir_t *dir; pack_t *pack; wfile_t *wad;
            zip_t *zip; android_assets_t *assets; };
    struct searchpath_s *next;
    void    (*pfnPrintInfo)( searchpath_t*, char *dst, size_t size );
    void    (*pfnClose)    ( searchpath_t* );
    file_t *(*pfnOpenFile) ( searchpath_t*, const char *name, const char *mode, int pack_ind );
    int     (*pfnFileTime) ( searchpath_t*, const char *name );
    int     (*pfnFindFile) ( searchpath_t*, const char *path, char *fixedname, size_t len );
    void    (*pfnSearch)   ( searchpath_t*, stringlist_t*, const char *pattern, int caseinsensitive );
    byte   *(*pfnLoadFile) ( searchpath_t*, const char *path, int pack_ind,
                             fs_offset_t *filesize, void *(*alloc)(size_t), void (*free)(void*) );
} searchpath_t;
```

### 1.2 Path flags (`engine/filesystem.h`)

| Flag | Meaning |
|------|---------|
| `FS_STATIC_PATH` | Never removed by `FS_ClearSearchPath` (root + rodir `./`) |
| `FS_NOWRITE_PATH` | Read-only path; never selected as `fs_writepath` |
| `FS_GAMEDIR_PATH` | Belongs to the active gamedir (honours `gamedironly`) |
| `FS_CUSTOM_PATH` | Added out-of-band (env PAKs, `MountArchive_Fullpath`) |
| `FS_GAMERODIR_PATH` | RoDir-side gamedir path |
| `FS_EXEC_PATH` | May supply DLL/SO files to `FS_FindLibrary` |
| `FS_SKIP_ARCHIVED_WADS` | Suppress WAD auto-mount for this archive |
| `FS_LOAD_PACKED_WAD` | Marks a WAD auto-mounted from inside another archive |

### 1.3 Archive registry & mount order (`filesystem.c:91-117`)

`FS_AddGameDirectory` mounts archives in `g_archives[]` order, then adds the
plain directory **last** (highest priority). Loose files therefore override
packed ones.

| Order | ext | type | `load_wads` | `real_archive` | `allow_exec` |
|-------|-----|------|:-----------:|:--------------:|:------------:|
| 1 | `pak` | `SEARCHPATH_PAK` | ✔ | ✔ | ✔ |
| 2 | `pk3` | `SEARCHPATH_ZIP` | ✔ | ✔ | — |
| 3 | `pk3dir` | `SEARCHPATH_PK3DIR` | ✔ | — | — |
| 4 | `wad` | `SEARCHPATH_WAD` | — | ✔ | — |
| — | *(plain dir)* | `SEARCHPATH_PLAIN` | — | — | ✔ |

`load_wads = true` ⇒ every `.wad` found inside is auto-mounted as its own
`SEARCHPATH_WAD` (unless `FS_SKIP_ARCHIVED_WADS`). Only PAK and plain
directories set `allow_exec` (→ `FS_EXEC_PATH`), so only they can supply game
DLLs.

______________________________________________________________________

## 2. Open-file handle — `file_t` / `ztoolkit_t` (`filesystem_internal.h:44-75`)

```c
#define FILE_BUFF_SIZE 2048
#define FILE_DEFLATED  BIT(0)

typedef struct {                     // decompression state (only if deflated)
    z_stream zstream;
    size_t   comp_length, in_ind, in_len, in_position;
    byte     input[FILE_BUFF_SIZE];
} ztoolkit_t;

struct file_s {
    int          handle;             // OS fd
    int          ungetc;             // pushed-back char, EOF if none
    time_t       filetime;
    searchpath_t *searchpath;
    fs_offset_t  real_length;        // uncompressed size
    fs_offset_t  position;           // cursor in decompressed stream
    fs_offset_t  offset;             // base offset within the package (0 = external)
    uint32_t     flags;              // FILE_DEFLATED
    ztoolkit_t   *ztk;               // non-NULL ⇒ reads go through inflate
    fs_offset_t  buff_ind, buff_len; // 2 KB read-ahead buffer state
    byte         buff[FILE_BUFF_SIZE];
#ifdef XASH_REDUCE_FD                 // PSVita/NSwitch: ≤1 fd open at a time
    const char *backup_path;
    fs_offset_t backup_position;
    uint        backup_options;
#endif
};
```

Backward seeks in a deflated file re-`inflateInit` from `offset` and discard
forward — there is no random access into DEFLATE.

______________________________________________________________________

## 3. On-disk archive formats

### 3.1 PAK — Quake (`pak.c:41-66`)

```c
#define IDPACKV1HEADER  (('K'<<24)+('C'<<16)+('A'<<8)+'P')  // "PACK" little-endian
#define MAX_FILES_IN_PACK 65536

typedef struct { int ident; int dirofs; int dirlen; } dpackheader_t;   // 12 bytes
typedef struct {
    char name[56];   // NUL-padded path, total entry = 64 bytes
    int  filepos;
    int  filelen;
} dpackfile_t;                                                          // 64 bytes
```

Directory = `dirlen / 64` entries at `dirofs`. Entries are sorted
case-insensitively at load (`FS_SortPak`, `Q_stricmp`) so lookup is binary
search. No compression in PAK.

### 3.2 WAD — GoldSrc/Quake textures (`common/wadfile.h`, `wad.c`)

```c
#define IDWAD2HEADER (('2'<<24)+('D'<<16)+('A'<<8)+'W')  // "WAD2" Quake
#define IDWAD3HEADER (('3'<<24)+('D'<<16)+('A'<<8)+'W')  // "WAD3" Half-Life
#define WAD3_NAMELEN 16
#define MAX_FILES_IN_WAD 65535

typedef struct { int ident; int numlumps; int infotableofs; } dwadinfo_t;  // 12 bytes

typedef struct {
    int         filepos;             // lump offset in WAD
    int         disksize;            // stored size (may be compressed)
    int         size;                // uncompressed size
    signed char type;                // TYP_* (see below)
    signed char attribs;             // ATTR_* (ATTR_READONLY = BIT(0), rest reserved)
    signed char pad0, pad1;
    char        name[16];            // NUL-terminated, 15 significant chars
} dlumpinfo_t;                                                           // 32 bytes
```

Lump-type codes (`wadfile.h:48-57`): `TYP_NONE 0`, `TYP_LABEL 1`,
`TYP_PALETTE 64`, `TYP_DDSTEX 65`, `TYP_GFXPIC 66`, `TYP_MIPTEX 67`,
`TYP_SCRIPT 68`, `TYP_COLORMAP2 69`, `TYP_QFONT 70`; `TYP_ANY -1` = accept any.
Extension→type mapping table `wad_types[]` (`wad.c:71-79`): pal/dds/lmp/fnt/mip/
txt. Lump names are matched by (name, type); `W_FindLump` disambiguates same-name
lumps of different types.

### 3.3 ZIP / PK3 (`zip.c:31-100`, all records `#pragma pack(1)`)

```c
#define ZIP_HEADER_LF   0x04034b50   // "PK\3\4"  local file header
#define ZIP_HEADER_CDF  0x02014b50   // "PK\1\2"  central-directory file header
#define ZIP_HEADER_EOCD 0x06054b50   // "PK\5\6"  end-of-central-directory
#define ZIP_COMPRESSION_NO_COMPRESSION 0
#define ZIP_COMPRESSION_DEFLATED       8
#define ZIP_ZIP64 0xffffffff         // sentinel: a ZIP64 field lives in the extra area

typedef struct {                     // EOCD (22 bytes + comment)
    uint16_t disk_number, start_disk_number;
    uint16_t number_central_directory_record, total_central_directory_record;
    uint32_t size_of_central_directory, central_directory_offset;
    uint16_t commentary_len;
} zip_header_eocd_t;
```

Load path: scan the tail for the EOCD signature, walk the central directory
(CDF records) to build the TOC, then read each file's local header to find the
payload offset. Only stored (0) and DEFLATE (8) are supported. ZIP64 is only
partially handled (the `0xffffffff` sentinel is recognised).

______________________________________________________________________

## 4. Plugin ABI — `GetFSAPI` (`engine/filesystem.h`)

The filesystem is a **separately-loaded DLL/SO**; its only export is
`GetFSAPI` (type `FSAPI`):

```c
int GetFSAPI( int version, fs_api_t *api, fs_globals_t **globals, fs_interface_t *engfuncs );
```

- **`fs_api_t`** — ~40 function pointers (Init/Shutdown, AddGameDirectory,
  Open/Read/Write/Seek, LoadFile, Search, FindLibrary, …). Full catalogue in
  [../boundaries/filesystem-boundary.md](../boundaries/filesystem-boundary.md).
- **`fs_globals_t`** — `GameInfo` (`const gameinfo_t *`) + `games[MAX_MODS]`;
  read directly by the engine.
- **`fs_interface_t`** — engine callbacks injected *into* the plugin
  (`_Con_Printf`, `_Sys_Error`, `_Mem_AllocPool`/`_Mem_Alloc`/`_Mem_Free`/…,
  `_Sys_GetNativeObject`). NULL entries fall back to stdlib stubs.

`FILESYSTEM_INTERFACE_VERSION "VFileSystem009"` — the GoldSrc `IFileSystem` C++
vtable in `VFileSystem009.cpp` is a compat shim mirroring the C API; its
resource-hint/wait/log methods (`HintResourceNeed`, `WaitForResources`,
`LogLevelLoadStarted`, …) are no-op stubs.

______________________________________________________________________

## 5. Quirk catalogue (behavioural reference)

The authoritative quirk list — WAD auto-mount, archive ordering,
case-insensitive trie, `fs_ext_path` `../` stripping, `gameinfo.txt` vs
`liblist.gam` mtime conversion, Quake gamedir auto-detection, the `valve`
`singleplayer_only` override, the `hldemo1` inject, `XASH3D_EXTRAS_PAK1/_PAK2`
env mounts, `FS_EXEC_PATH` propagation, DLL idiot-path stripping,
`XASH_REDUCE_FD` — is maintained **once** in
[../boundaries/filesystem-boundary.md](../boundaries/filesystem-boundary.md)
§Quirks and invariants. This deep-dive does not duplicate it; the boundary spec
is the source of truth and stays in sync with the as-built behaviour.

______________________________________________________________________

## 6. Injected dependencies (legacy)

`crtlib` (`Q_strncpy`, `COM_ParseFile`), `crclib` (CRC32/MD5), `miniz`
(inflate), `utflib` (UTF-8↔UTF-16, Win32), `library_suffix`
(`.so`/`.dll`/`.dylib`). All heap use, logging, fatal errors and the Android
`AAssetManager` handle arrive through `fs_interface_t`.

______________________________________________________________________

## 7. Gameinfo discovery (`filesystem.c`, `gameinfo.c`)

`FS_InitStdio` scans `rootdir`/`rodir` for game directories, each identified by
`gameinfo.txt` (native) or `liblist.gam` (GoldSrc legacy). A `gameinfo_t` per
mod is stored in `FI.games[]`; the active one is `FI.GameInfo`. If both config
files exist and `liblist.gam` is newer, it is converted to `gameinfo.txt`
in-place on the RwDir only. A `pak0.pak` + (`progs.dat` | `quake.rc`) directory
with no config synthesises a Quake-default `gameinfo_t` in memory.

______________________________________________________________________

## 8. As-built mapping (`filesystem/` legacy → `xash3dpp/…/filesystem`)

| Legacy construct | Site | `xash3dpp` as-built | Notes |
|------------------|------|---------------------|-------|
| `GetFSAPI` / `fs_api_t` plugin table | `filesystem.h` | **Eliminated** — `class Filesystem` (pimpl) linked statically | Design decision 1: no C-ABI plugin |
| `searchpath_t` + 7 raw fn-ptrs (union) | `filesystem_internal.h` | `struct SearchPath { unique_ptr<ISearchBackend>; string; SearchPathFlags; }` + `ISearchBackend` virtual class | `i_search_backend.hpp` — replaces the hand-rolled vtable |
| `fs_searchpaths` (global linked list) | `filesystem.c` | `std::deque<SearchPath>` on `Filesystem::Impl`, guarded by `paths_mutex` | Globals eliminated (P-3) |
| `fs_writepath`, `fs_rootdir`, `FI` globals | `filesystem.c` | `Impl::{rootdir,basedir,rodir,gamedir,active_game}` | No file-scope mutable state |
| `file_t` + `ztoolkit_t` | `filesystem_internal.h` | `class File` (abstract) + `OsFile` / `MemFile`; `ZlibState` (RAII `mz_inflateEnd`) | `file.hpp`, `file.cpp`, `mem_file.hpp` |
| `int whence` (SEEK_*) | `FS_Seek` | `enum class SeekOrigin { Begin, Current, End }` | modernization M-2 (done) |
| `pack_t` + `dpackfile_t[]` | `pak.c` | `PakBackend` + sorted `std::vector<Entry>` | `pak_backend.{hpp,cpp}`; `k_IDPACK` via `bit_cast` |
| `wfile_t` + `dlumpinfo_t[]` | `wad.c` | `WadBackend` + `std::vector<Entry>` | `wad_backend.{hpp,cpp}`; `k_WAD2`/`k_WAD3` |
| `zip_t` + CDF/EOCD walk | `zip.c` | `ZipBackend` (inflate via `xash3dpp_miniz`) | `zip_backend.{hpp,cpp}` |
| `dir_t` case-insensitivity trie | `dir.c` | `CIDirectory` (Native vs Emulated mode; lazy per-subdir sorted cache under `cache_mutex_`) | `ci_directory.{hpp,cpp}` |
| `SEARCHPATH_PK3DIR` | `dir.c` | `Pk3DirBackend` (delegates to inner dir) | `pk3dir_backend.{hpp,cpp}` |
| `android_assets_t` (NDK) | `android.c` | `AndroidBackend` | `android_backend.cpp` |
| `g_archives[]` mount order | `filesystem.c:91` | `ArchiveType` + `constexpr k_archive_types` table (+ `BackendFactory` fn-ptr) | `archive_registry.hpp` |
| `FS_FindFile_PAK/ZIP` (`Q_strnicmp` binary search) | `pak.c`/`zip.c` | `ci_find_by_name<T>` / `CiNameLess<T>` | `archive_helpers.hpp` — M-7 (`string_view` over-read) **resolved 2026-07-19** via bounded `utilities::ci_compare` |
| `FS_LoadFile` (pool) / `FS_LoadFileMalloc` (malloc) | `filesystem.c` | single `std::vector<std::byte> load_file()` (RAII, caller-owned) | Design decision 4: both variants collapse |
| `fs_globals_t::GameInfo` / `gameinfo_t` | `filesystem.h` | `xash::GameInfo` value; `get_game_info()` returns by value | `gameinfo.hpp` |
| injected `_Mem_*` / `miniz` (static link) | `fs_interface_t` | `xash3dpp_memory` (PUBLIC) + `xash3dpp_miniz` (PRIVATE, shared with `content`) | miniz = shared `../public/miniz.c` static target, **not** owned by utilities |
| `crtlib` string/path helpers | `public/` | `xash3dpp_utilities` (PUBLIC) | `path_join`, `ci_less`, `strnicmp`, `match_pattern` |
| all OS file I/O (`open`, `seek`, `stat`, dir ops, UTF-16) | inline in `filesystem.c`/`dir.c`/win32 | `xash3dpp_platform` (PRIVATE) | extracted out of filesystem entirely (see modernization H-2/L-4 relocation) |
| `VFileSystem009` C++ shim | `VFileSystem009.cpp` | `vfs009/vfs009.cpp` (compat shim, retained) | Design decision 2: keep for now |
