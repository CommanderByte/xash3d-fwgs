# Directory Implementation Migration Audit

This note opens Phase 23 and records the current `filesystem/dir.c`
responsibilities before moving implementation bodies into
`src/filesystem/directory_backend.cpp`.

## Current Shape

`DirectoryBackend` already exists as a bridge object. Before this phase, the
modern object only forwarded into legacy callbacks while the real directory
cache, case repair, search, loose-file open, and lookup behavior lived in
`filesystem/dir.c`.

```mermaid
flowchart LR
    SearchPath["searchpath_t\nSEARCHPATH_PLAIN / PK3DIR"]
    Bridge["DirectoryBackend bridge\nfilesystem/dir_backend_adapter.cpp"]
    Backend["DirectoryBackend\nsrc/filesystem/directory_backend.cpp"]
    Legacy["legacy callbacks\nfilesystem/dir.c"]
    Cache["dir_t tree\ncase-repair cache"]

    SearchPath --> Bridge
    Bridge --> Backend
    Backend --> Legacy
    Legacy --> Cache
```

## Responsibility Map

| Current code | Current role | Target owner |
| --- | --- | --- |
| `dir_t` tree | Cached, sorted view of on-disk directory entries. | Modern directory backend helpers for now; C layout remains compatible with `searchpath_t::dir`. |
| `Platform_GetDirectoryCaseSensitivity` | Platform-specific case sensitivity probe. | Legacy adapter callback until platform services are abstracted more broadly. |
| `FS_PopulateDirEntries` | Initial cache population, empty-directory handling, and case-insensitive directory bypass. | `DirectoryBackend` cache helper behind runtime callbacks. |
| `FS_MaybeUpdateDirEntries` | Cache refresh after files appear or directory contents change. | `DirectoryBackend` cache helper behind runtime callbacks. |
| `FS_FixFileCase` | Case-insensitive path repair and create-path compatibility quirks. | `DirectoryBackend` helper, still exported through the C wrapper for write-path callers. |
| `FS_FindFile_DIR_Legacy` | Loose-file lookup and fixed-name output. | `DirectoryBackend` lookup helper. |
| `FS_Search_DIR_Legacy` | Non-recursive directory search, base-path repair, and duplicate suppression. | `DirectoryBackend` search helper. |
| `FS_OpenFile_DIR_Legacy` | Loose-file system open and `file_t::searchpath` assignment. | `DirectoryBackend` open helper with legacy runtime callbacks. |
| `FS_FileTime_DIR_Legacy` | Direct file timestamp lookup. | Legacy adapter for now; trivial enough to move with runtime ownership later. |
| `FS_InitDirectorySearchpath` | Searchpath callback registration, root cache allocation, and bridge creation. | Adapter entry point until `FilesystemRuntime` owns search paths. |

## Behavioral Quirks To Preserve

- Directory entries are sorted case-insensitively for binary lookup.
- Case-insensitive platforms bypass cache repair and copy the remaining path.
- Missing direct-created files can refresh an existing cached directory.
- Create-path mode copies the remaining path when a component is absent.
- Parent-path inputs beginning with `../` bypass cache walking but still check
  existence unless create-path mode is active.
- Directory search repairs only the base path, lists one directory, matches
  case-insensitively, and suppresses exact duplicate strings.
- `.pk3dir` mounts still use directory behavior while being tagged as a PK3
  directory search path.

## Phase 23 Progress

- `FS-DIR-IMPL-001`: Complete. The directory audit is captured in this
  document.
- `FS-DIR-IMPL-002`: Complete. `tests/filesystem/directory_backend.cpp` now
  covers target-neutral cache population, nested case repair, fixed-name
  output, cache refresh after a direct file appears, and directory search.
- `FS-DIR-IMPL-003`: Complete. Directory cache population, recursive cleanup,
  sorted lookup, cache refresh, case repair, loose-file lookup, loose-file
  open, and directory search now live in `src/filesystem/directory_backend.cpp`.
- `FS-DIR-IMPL-004`: Complete for the current migration boundary.
  `filesystem/dir.c` still owns platform case-sensitivity probing,
  `FS_InitDirectorySearchpath`, callback registration, `dir_t` allocation,
  and public C wrappers such as `FS_FixFileCase` until broader runtime/search
  path ownership moves out of the legacy filesystem folder.

## Remaining Blockers

- `dir_t` remains the public-private C layout behind `searchpath_t::dir` so
  existing write-path callers can continue to pass it to `FS_FixFileCase`.
- Platform-specific case-sensitivity probing still belongs to the adapter
  because it depends on build flags and OS headers.
- Full ownership cleanup belongs with the future `FilesystemRuntime` phase,
  where search paths and write path state can move together.
