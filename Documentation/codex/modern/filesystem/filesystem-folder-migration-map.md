# Filesystem Folder Migration Map

## Goal

Move real filesystem behavior into `src/filesystem` while keeping legacy ABI
facades stable until the rest of the engine no longer depends on them.

The desired end state is:

- `src/filesystem/` owns implementation.
- `src/include/filesystem/` owns private modern contracts.
- `filesystem/` contains only public ABI headers, export metadata, build glue,
  the compatibility umbrella, and `filesystem.c` until its exported runtime
  facade can be split safely.
- completed TODO and audit documents move into `Documentation/codex/done/`
  after their implementation evidence is committed.

## Current File Roles

| File | Current Role | Target State |
| --- | --- | --- |
| `filesystem/filesystem.h` | Public C ABI facade. | Keep stable until a deliberate ABI version bump. |
| `filesystem/VFileSystem009.h` | Public Valve-style C++ ABI facade. | Keep stable; do not expose modern types. |
| `src/filesystem/compat/VFileSystem009.cpp` | C++ facade implementation plus wrapper behavior. | Thin wrapper over modern runtime; keep ABI shape. |
| `filesystem/fscallback.h` | Legacy callback convenience header. | Keep until callback users migrate or compatibility facade changes. |
| `filesystem/filesystem_internal.h` | Compatibility umbrella for legacy filesystem private headers. | Keep only for large legacy `.c` bodies until they finish shrinking. |
| `src/include/filesystem/compat/private/filesystem_private_types.h` | Private legacy layouts for `file_t`, `searchpath_t`, `stringlist_t`, and backend type enums. | Keep private; modern code should prefer target-neutral contracts. |
| `src/include/filesystem/compat/private/filesystem_private_globals.h` | Private declarations for legacy global filesystem state. | Shrink as globals move behind `FilesystemRuntime`/query APIs. |
| `src/include/filesystem/compat/private/filesystem_private_memory.h` | Private memory and engine callback macros. | Replace with logging/memory facades as Phase 30 progresses. |
| `src/include/filesystem/compat/private/filesystem_private_api.h` | Private declarations for legacy `FS_*` entry points used by adapters. | Shrink toward `src/filesystem/legacy_adapter.cpp`. |
| `filesystem/filesystem.c` | Runtime orchestration, globals, file handles, search paths, API table. | Shrink into C facade and runtime adapter over `FilesystemRuntime`. |
| `src/filesystem/compat/*.cpp` | DLL-only compatibility adapters for modern helpers/backends and legacy callbacks. | Shrink toward one explicit legacy adapter boundary as legacy `.c` bodies disappear. |
| `src/filesystem/compat/stringlist_legacy.cpp` | Legacy stringlist and directory-listing helper bodies moved out of `filesystem.c`. | Replace with safer target-neutral listing/query helpers later. |
| `src/filesystem/compat/memory_legacy.cpp` | Legacy memory wrapper bodies moved out of `filesystem.c`. | Replace with explicit memory facade once engine allocation policy is isolated. |
| `src/include/filesystem/compat/*.h` | Private compatibility adapter declarations used by the filesystem DLL. | Keep private; do not expose through public ABI headers. |
| `src/filesystem/compat/dir.c` | Directory cache/search/case-fix compatibility body. | Shrink further as `DirectoryBackend` owns more platform behavior. |
| `src/filesystem/compat/pak.c` | PAK compatibility body. | Shrink further as `PakBackend` owns more archive ownership behavior. |
| `src/filesystem/compat/wad.c` | WAD compatibility body. | Shrink further as `WadBackend` owns more archive ownership behavior. |
| `src/filesystem/compat/zip.c` | ZIP/PK3 compatibility body. | Shrink further as `ZipBackend` owns more archive ownership behavior. |
| `src/filesystem/compat/android.c` | Android asset compatibility body under platform guard. | Keep JNI/platform calls isolated while target-neutral Android assets backend grows. |
| `filesystem/exports.txt` | Export list. | Keep until build/export model changes. |
| `filesystem/wscript` | Filesystem DLL build target. | Compile legacy bodies plus DLL-only compat sources from `src/filesystem/compat`. |

## Migration Principles

- Tests move first. Any behavior moved out of a legacy file must have focused
  tests before or in the same commit.
- Keep exported symbols boring. ABI facades stay C-compatible or existing
  `VFileSystem009`-compatible.
- Modern internals must not leak through public headers.
- Prefer one backend body migration at a time.
- After each backend body migration, the legacy `.c` file should visibly shrink.
- A legacy file is only eligible for deletion when no build target, public
  header, or platform facade still depends on it.

## Recommended Order

1. WAD implementation body.
2. PAK implementation body.
3. ZIP/PK3 implementation body.
4. Directory implementation body.
5. Android assets implementation body.
6. `FilesystemRuntime` ownership of state, search paths, and gameinfo.
7. `file_t`/handle operations behind runtime-owned handle objects.
8. `VFileSystem009` wrapper thinning.
9. Build-tree convergence and removal of obsolete legacy sources.
10. Move completed TODO/audit documents into `Documentation/codex/done/`.

WAD comes first because it is self-contained, already has archive-in-archive
coverage, and exercises both lookup and load-file behavior without the ZIP
deflate complexity.
