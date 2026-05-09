# PAK Implementation Migration Audit

This note opens Phase 21 and records the current `filesystem/pak.c`
responsibilities before moving implementation bodies into
`src/filesystem/pak_backend.cpp`.

## Current Shape

`PakBackend` exists as a bridge object. It owns metadata and forwards through
`PakBackendOps`, while the actual PAK parser, lookup, search, open, and close
behavior still lives in `filesystem/pak.c`.

```mermaid
flowchart LR
    SearchPath["searchpath_t\nSEARCHPATH_PAK"]
    Bridge["PakBackend bridge\nfilesystem/pak_backend_adapter.cpp"]
    Backend["PakBackend\nsrc/filesystem/pak_backend.cpp"]
    Legacy["legacy callbacks\nfilesystem/pak.c"]
    Pack["pack_t\nhandle, file table"]

    SearchPath --> Bridge
    Bridge --> Backend
    Backend --> Legacy
    Legacy --> Pack
```

## Responsibility Map

| Current code | Current role | Target owner |
| --- | --- | --- |
| `dpackheader_t`, `dpackfile_t` | On-disk PAK header and directory entry layout. | Modern PAK backend private/public adapter structs. |
| `FS_SortPak` | Case-insensitive sort of directory entries for binary lookup. | `PakBackend` parser/index builder. |
| `FS_LoadPackPAK` | Opens a PAK, validates header and directory size, reads and endian-fixes entries, sorts table, and leaves the handle open. | `PakBackend` open/parser logic behind a runtime boundary. |
| `FS_OpenFile_PAK_Legacy` | Builds a `file_t` handle for an entry by index. | `PakBackend::openFile` helper using a legacy open-handle callback. |
| `FS_FindFile_PAK_Legacy` | Binary lookup, fixed-name output, and entry index result. | `PakBackend::findFile`. |
| `FS_Search_PAK_Legacy` | Pattern search, duplicate suppression, and directory-prefix result behavior. | `PakBackend::search`. |
| `FS_FileTime_PAK_Legacy` | Returns the package handle timestamp. | `PakBackend::fileTime`. |
| `FS_PrintInfo_PAK_Legacy` | Human-readable mounted PAK summary, including parent archive source when applicable. | Adapter for now; can move after parent archive metadata is represented in modern structures. |
| `FS_Close_PAK_Legacy` | Closes the PAK handle and frees `pack_t`. | Adapter ownership glue until searchpath ownership moves out of legacy code. |
| `FS_AddPak_Fullpath` | Public legacy mount entry point, searchpath callback registration, bridge creation, and mount logging. | Adapter entry point until the broader filesystem runtime owns search paths. |
| `FS_CheckForQuakePak` | Opens a PAK and checks for root-level Quake marker files. | Can reuse the modern parser and table lookup once parser ownership moves. |

## Behavioral Quirks To Preserve

- PAK magic is little-endian `PACK`.
- Directory size must be a whole number of 64-byte entries.
- Empty PAKs fail to mount.
- Over-limit PAKs fail to mount.
- File entries are sorted case-insensitively after loading.
- File lookup is case-insensitive and returns the sorted entry index.
- Search walks upward through path elements, so a file can also produce a
  directory-name result if the pattern matches that directory prefix.
- Duplicate suppression in search compares exact strings currently in the
  temporary result list.
- `FS_CheckForQuakePak` ignores entries in subdirectories.

## Migration Plan

1. Expand the PAK fixture tests for case-insensitive lookup and search output.
2. Move PAK entry sorting, binary lookup, and search into
   `src/filesystem/pak_backend.cpp`.
3. Move PAK header/directory parsing and open orchestration behind a runtime
   boundary for file, memory, endian, and logging-adjacent operations.
4. Keep `filesystem/pak.c` as adapter/ownership glue until `searchpath_t`
   ownership moves into the modern runtime.

## Phase 21 Progress

- `FS-PAK-IMPL-001`: Complete. The PAK audit is captured in this document, and
  `tests/filesystem/archive-order.c` now covers case-insensitive lookup,
  nested entry lookup, non-recursive `*.txt` search behavior, and explicit
  nested `folder/*.txt` search behavior.
- `FS-PAK-IMPL-002`: Complete. PAK entry sorting, binary lookup, and
  header/directory parsing now live in `src/filesystem/pak_backend.cpp`.
- `FS-PAK-IMPL-003`: Complete. PAK open orchestration, packed-entry open, and
  search now live in `src/filesystem/pak_backend.cpp` behind runtime callback
  boundaries.
- `FS-PAK-IMPL-004`: Complete for the current migration boundary.
  `filesystem/pak.c` still owns `pack_t`, callback registration,
  `FS_AddPak_Fullpath`, and the `FS_CheckForQuakePak` facade, because those
  are tied to broader searchpath/runtime ownership.
