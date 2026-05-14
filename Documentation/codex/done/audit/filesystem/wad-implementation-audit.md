# WAD Implementation Migration Audit

This note opens Phase 20 and records the current `filesystem/wad.c`
responsibilities before moving implementation bodies into
`src/filesystem/wad_backend.cpp`.

## Current Shape

`WadBackend` is already present, but it is still a bridge. The modern object
owns `SearchPathMetadata` and forwards through `WadBackendOps` into legacy C
callbacks. The real WAD behavior still lives in `filesystem/wad.c`.

```mermaid
flowchart LR
    SearchPath["searchpath_t\nSEARCHPATH_WAD"]
    Bridge["WadBackend bridge\nfilesystem/wad_backend_adapter.cpp"]
    Backend["WadBackend\nsrc/filesystem/wad_backend.cpp"]
    Legacy["legacy callbacks\nfilesystem/wad.c"]
    WFile["wfile_t\nheader, handle, lumps"]

    SearchPath --> Bridge
    Bridge --> Backend
    Backend --> Legacy
    Legacy --> WFile
```

## Responsibility Map

| Current code | Current role | Target owner |
| --- | --- | --- |
| `wadtype_t`, `wad_types` | Extension to WAD lump type mapping. | `WadBackend` private helpers or a small WAD metadata helper. |
| `W_TypeFromExt` | Converts lookup/search pattern extensions to lump types; `*` and no extension mean any type, unknown extensions reject. | `WadBackend` lookup policy. |
| `W_ExtFromType` | Converts lump type to a synthetic search-result extension. | `WadBackend` search-result formatting. |
| `W_FindLump` | Binary search over sorted lump table, with type-aware tie handling. | `WadBackend` lump index. |
| `W_AddFileToWad` | Sorted insert into lump table and duplicate warning. | `WadBackend` parser/index builder. |
| `W_Open` | Opens raw or packed WADs, validates headers, reads and endian-fixes the lump table, normalizes names, special-cases `conchars`, and leaves the handle open. | `WadBackend` construction/open logic once file and allocation dependencies are injected cleanly. |
| `FS_CloseWAD` | Releases WAD memory pool, closes the open file handle, and frees `wfile_t`. | `WadBackend` destructor/close path, with adapter cleanup until ownership moves fully. |
| `FS_PrintInfo_WAD_Legacy` | Human-readable mounted WAD summary, including archive source if the WAD was packed. | `WadBackend::printInfo`. |
| `FS_FindFile_WAD_Legacy` | File lookup, optional `wadname/lump.ext` restriction, fixed-name output, and lump index result. | `WadBackend::findFile`. |
| `FS_Search_WAD_Legacy` | Pattern search, optional WAD folder restriction, type filtering, duplicate suppression, and synthetic `wad/lump.ext` results. | `WadBackend::search`. |
| `W_ReadLump_Legacy` | Reads a lump by index through caller-provided allocation callbacks while preserving the WAD file position. | `WadBackend::loadFile`. |
| `FS_AddWad_Fullpath` | Public legacy mount entry point, searchpath callback registration, bridge creation, and mount logging. | Adapter entry point until the entire filesystem mount API moves into modern code. |

## Behavioral Quirks To Preserve

- `.wad` headers may be WAD2 or WAD3.
- Empty WADs fail to mount, but over-limit WADs only warn and keep loading.
- Lump names are normalized to lower-case fixed 16-byte names.
- Quake-style `*` in lump names is rewritten to `!`.
- `conchars` with legacy type `68` is rewritten to `TYP_GFXPIC`.
- Lookups with no extension or `*` match any WAD lump type.
- Unknown extensions reject immediately.
- `wadname/lump.ext` restricts lookup to a mounted WAD with that basename.
- Search results synthesize extensions from lump type and may include a
  `wadname/` prefix for WAD-restricted searches.
- Unrestricted WAD search currently returns synthetic paths with a leading
  slash, for example `/probe.txt`.
- Packed WADs are opened through the virtual filesystem by basename when
  `FS_LOAD_PACKED_WAD` is set.

## Dependencies Blocking A Clean Move

`W_Open` and related paths still depend on the legacy runtime surface:

- file APIs: `FS_Open`, `FS_SysOpen`, `FS_Read`, `FS_Seek`, `FS_Tell`,
  `FS_Close`, `FS_SysFileTime`
- memory APIs: `Mem_Calloc`, `Mem_Malloc`, `Mem_Free`, `Mem_AllocPool`,
  `Mem_FreePool`, `fs_mempool`
- path/string helpers: `COM_FileExtension`, `COM_FileWithoutPath`,
  `COM_FileBase`, `COM_DefaultExtension`, `COM_ExtractFilePath`,
  `Q_stricmp`, `Q_strnlwr`, `matchpattern`
- result helpers: `stringlistappend`
- diagnostics: `Con_Reportf`

The first implementation move should therefore keep an adapter seam for these
dependencies rather than making `WadBackend` reach directly into globals in
every method.

## Migration Plan

1. Add tests for path-restricted WAD lookup, case-insensitive lookup, wildcard
   lookup, unknown-extension rejection, and WAD search result formatting.
2. Move pure WAD type helpers and lookup/search behavior into
   `src/filesystem/wad_backend.cpp`, initially operating on the existing
   `wfile_t` data supplied by the adapter.
3. Move parser/index-building behavior into `WadBackend` with an internal
   runtime dependency object for file, memory, path, and logging calls.
4. Move lump reading and close ownership into `WadBackend`.
5. Shrink `filesystem/wad.c` to `FS_AddWad_Fullpath` plus C ABI adapter glue,
   or document any remaining global ownership blocker.

## Phase 20 Progress

- `FS-WAD-IMPL-001`: Complete. Responsibility mapping is captured in this
  document.
- `FS-WAD-IMPL-002`: Complete. `tests/filesystem/wad-archive.c` now covers
  WAD-name-restricted lookup, case-insensitive lookup, extensionless lookup,
  unknown-extension rejection, unrestricted WAD search output, and
  WAD-restricted search output.
- `FS-WAD-IMPL-003A`: Complete. WAD extension/type mapping, sorted lump
  insertion, and binary lump lookup live in `src/filesystem/wad_backend.cpp`.
  The legacy C file now reaches those helpers through
  `filesystem/wad_backend_adapter.cpp`.
- `FS-WAD-IMPL-003B`: Complete. Header parsing and lump-table normalization
  now live in `src/filesystem/wad_backend.cpp` behind a runtime boundary for
  read, seek, allocation, free, and duplicate-lump reporting.
- `FS-WAD-IMPL-004`: Complete. WAD open orchestration, file lookup, search, and
  lump reading now live in `src/filesystem/wad_backend.cpp`. The legacy file
  still supplies runtime callbacks for `FS_*`, `Mem_*`, `stringlist_t`,
  wildcard matching, and `Con_Reportf`.
- `FS-WAD-IMPL-005`: Complete for the current migration boundary.
  `filesystem/wad.c` is now adapter and ownership glue. It still owns
  `wfile_t`, callback registration, and the `FS_AddWad_Fullpath` entry point
  until broader searchpath ownership moves out of the legacy filesystem folder.
