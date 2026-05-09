# Filesystem Folder Migration Map

## Goal

Move real filesystem behavior into `src/filesystem` while keeping legacy ABI
facades stable until the rest of the engine no longer depends on them.

The desired end state is:

- `src/filesystem/` owns implementation.
- `src/include/filesystem/` owns private modern contracts.
- `filesystem/` contains only build/export glue and thin compatibility
  adapters, then shrinks further when callers are ready.
- completed TODO and audit documents move into `Documentation/codex/done/`
  after their implementation evidence is committed.

## Current File Roles

| File | Current Role | Target State |
| --- | --- | --- |
| `filesystem/filesystem.h` | Public C ABI facade. | Keep stable until a deliberate ABI version bump. |
| `filesystem/VFileSystem009.h` | Public Valve-style C++ ABI facade. | Keep stable; do not expose modern types. |
| `filesystem/VFileSystem009.cpp` | C++ facade implementation plus wrapper behavior. | Thin wrapper over modern runtime; keep ABI shape. |
| `filesystem/fscallback.h` | Legacy callback convenience header. | Keep until callback users migrate or compatibility facade changes. |
| `filesystem/filesystem_internal.h` | Large private legacy internals header. | Split/shrink into focused adapter headers. |
| `filesystem/filesystem.c` | Runtime orchestration, globals, file handles, search paths, API table. | Shrink into C facade and runtime adapter over `FilesystemRuntime`. |
| `filesystem/dir.c` | Directory cache/search/case-fix implementation. | Move behavior into `DirectoryBackend`; leave adapter only. |
| `filesystem/pak.c` | PAK parsing/search/open implementation. | Move behavior into `PakBackend`; leave adapter only. |
| `filesystem/wad.c` | WAD parsing/lump lookup/load implementation. | Move behavior into `WadBackend`; leave adapter only. |
| `filesystem/zip.c` | ZIP/PK3 parsing/search/open/deflate setup. | Move behavior into `ZipBackend`; leave adapter only. |
| `filesystem/android.c` | Android asset implementation under platform guard. | Move behavior into `AndroidAssetsBackend`; leave platform adapter only. |
| `filesystem/*_adapter.cpp/.h` | Bridge modern helpers/backends to legacy C callbacks. | Keep while legacy facades exist; remove when no longer needed. |
| `filesystem/exports.txt` | Export list. | Keep until build/export model changes. |
| `filesystem/wscript` | Legacy DLL build target. | Gradually change source list toward modern implementation plus facades. |

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
