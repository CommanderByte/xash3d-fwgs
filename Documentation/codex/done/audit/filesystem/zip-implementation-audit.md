# ZIP/PK3 Implementation Migration Audit

This note opens Phase 22 and records the current `filesystem/zip.c`
responsibilities before moving implementation bodies into
`src/filesystem/zip_backend.cpp`.

## Current Shape

`ZipBackend` exists as a bridge object. It owns metadata and forwards through
`ZipBackendOps`, while ZIP/PK3 parsing, lookup, search, file-handle setup, and
load-file behavior still live in `filesystem/zip.c`.

```mermaid
flowchart LR
    SearchPath["searchpath_t\nSEARCHPATH_ZIP"]
    Bridge["ZipBackend bridge\nfilesystem/zip_backend_adapter.cpp"]
    Backend["ZipBackend\nsrc/filesystem/zip_backend.cpp"]
    Legacy["legacy callbacks\nfilesystem/zip.c"]
    Zip["zip_t\nhandle, file table"]

    SearchPath --> Bridge
    Bridge --> Backend
    Backend --> Legacy
    Legacy --> Zip
```

## Responsibility Map

| Current code | Current role | Target owner |
| --- | --- | --- |
| ZIP header structs/constants | On-disk local-header, central-directory, and EOCD layout. | Modern ZIP backend private parser structs. |
| `FS_LoadZip` | Opens archive, validates first local header, finds EOCD, reads central directory, skips empty entries, recalculates data offsets from local headers, and sorts entries. | `ZipBackend` parser/open logic behind runtime callbacks. |
| `FS_SortZip` | Case-insensitive sort for binary lookup. | `ZipBackend` entry table helper. |
| `FS_OpenFile_ZIP_Legacy` | Opens a file handle into the package and sets up deflated-stream state when needed. | `ZipBackend::openFile` policy with legacy callbacks for `file_t` mutation. |
| `FS_LoadZIPFile_Legacy` | Loads stored files directly, deflates compressed files into caller-owned buffers, and rejects unsupported methods. | `ZipBackend::loadFile` helper with runtime callbacks for read/seek/temp allocation/logging. |
| `FS_FindFile_ZIP_Legacy` | Binary lookup, fixed-name output, and entry index result. | `ZipBackend::findFile`. |
| `FS_Search_ZIP_Legacy` | Pattern search, duplicate suppression, and directory-prefix result behavior. | `ZipBackend::search`. |
| `FS_AddZip_Fullpath` | Public legacy mount entry point, callback registration, bridge creation, and mount logging. | Adapter entry point until the broader filesystem runtime owns search paths. |

## Behavioral Quirks To Preserve

- Empty ZIP/PK3 archives fail to mount.
- Archives larger than 4GB fail before parsing.
- Central-directory entries with zero uncompressed size are ignored.
- Unsupported compression methods may mount but fail when opened or loaded.
- File lookup is case-insensitive and returns the sorted entry index.
- Search is not recursive for `*.ext`; nested files require matching the nested
  path such as `folder/*.ext`.
- Search walks upward through path elements, so directory-prefix results can be
  produced when the pattern matches them.
- Deflated file handles rely on legacy `file_t`/`ztoolkit_t` internals until
  file-handle ownership moves into the modern runtime.

## Migration Plan

1. Expand ZIP fixture tests for case-insensitive lookup and search quirks.
2. Move ZIP entry sorting, binary lookup, search, parser, and open
   orchestration into `src/filesystem/zip_backend.cpp`.
3. Move stored/deflated load-file behavior into the modern backend behind
   runtime callbacks for read/seek/temp allocation/logging.
4. Move open-entry policy into the modern backend while keeping deflated
   `file_t` setup as a legacy callback.
5. Keep `filesystem/zip.c` as adapter/ownership glue until searchpath and
   file-handle internals move into modern code.

## Phase 22 Progress

- `FS-ZIP-IMPL-001`: Complete. The audit is captured in this document.
- `FS-ZIP-IMPL-002`: Complete. `tests/filesystem/zip-archive.c` now covers
  case-insensitive lookup, nested stored-file lookup, root-only `*.txt` search,
  explicit nested `folder/*.txt` search, stored loads, deflated loads, and
  unsupported compression rejection.
- `FS-ZIP-IMPL-003`: Complete. ZIP entry sorting, binary lookup,
  central-directory parsing, local-header offset recalculation, and archive
  open orchestration now live in `src/filesystem/zip_backend.cpp`.
- `FS-ZIP-IMPL-004`: Complete. Stored/deflated load-file behavior, open-entry
  policy, and search now live in `src/filesystem/zip_backend.cpp`. Deflated
  `file_t` setup and raw inflate execution remain runtime callbacks so the
  modern backend does not depend directly on legacy `file_t` internals or miniz
  linkage.
- `FS-ZIP-IMPL-005`: Complete for the current migration boundary.
  `filesystem/zip.c` still owns `zip_t`, callback registration, and
  `FS_AddZip_Fullpath` until broader searchpath ownership moves out of the
  legacy filesystem folder.
