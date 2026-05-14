# Directory Backend TODO

## Purpose

This TODO tracks the first modern filesystem backend pilot.

The goal is to prove the internal C++ backend shape while preserving the
legacy C ABI and existing `searchpath_t` callback behavior.

## Current Scaffold

- [x] Add internal search path backend interface.
  Evidence: `src/include/filesystem/search_path_backend.hpp`,
  `src/filesystem/search_path_backend.cpp`.
- [x] Add internal search path metadata type.
  Evidence: `SearchPathMetadata` in
  `src/include/filesystem/search_path_backend.hpp`.
- [x] Add directory backend skeleton.
  Evidence: `src/include/filesystem/directory_backend.hpp`,
  `src/filesystem/directory_backend.cpp`.
- [x] Add compile-time and behavior-neutral tests for the skeleton.
  Evidence: `tests/filesystem/directory_backend.cpp`; command
  `.\waf.bat build`.

## Pending Adapter Work

- [x] Decide bridge storage strategy for the first live pilot:
  `searchpath_t` field, existing `dir_t` wrapper, or external bridge object.
  Decision: Store a private backend bridge pointer in the existing `dir_t`
  root object. This avoids changing `searchpath_t` layout while keeping
  directory backend ownership tied to directory cache ownership.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.cpp`.
- [x] Keep `FS_AddDir_Fullpath` callable from C.
  Evidence: `filesystem/dir.c`; command `.\waf.bat build`.
- [x] Keep `FS_InitDirectorySearchpath` callable from C.
  Evidence: `filesystem/dir.c`; command `.\waf.bat build`.
- [x] Keep `FS_FixFileCase` callable from C.
  Evidence: `filesystem/dir.c`; command `.\waf.bat build`.
- [x] Forward `FS_PrintInfo_DIR` to `DirectoryBackend::printInfo`.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.cpp`.
- [x] Forward `FS_FileTime_DIR` to `DirectoryBackend::fileTime`.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.cpp`.
- [x] Forward `FS_FindFile_DIR` to `DirectoryBackend::findFile`.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.cpp`.
- [x] Forward `FS_OpenFile_DIR` to `DirectoryBackend::openFile`.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.cpp`.
- [x] Forward `FS_Search_DIR` to `DirectoryBackend::search`.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.cpp`.
- [x] Preserve old `dir_t` cache ownership until tests prove behavior parity.
  Evidence: legacy directory logic remains in `filesystem/dir.c` and is called
  through bridge hooks.
- [x] Run legacy filesystem tests after the first callback is forwarded.
  Evidence: command `.\waf.bat build` passed 15/15 tests.
- [x] Run Windows runtime smoke test after all directory callbacks are
  forwarded.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. The process exited `0`, printed the expected
  search path, and stopped with reason `"command"`.

## Boundaries

- The backend interface is private to modern internals.
- Do not expose C++ backend types through `fs_api_t`, `GetFSAPI`,
  `VFileSystem009`, `search_t`, or `file_t`.
- Do not replace `fs_searchpaths` or search path ordering during the directory
  backend pilot.
- Keep path policy extraction separate from the backend bridge unless a tiny
  helper is required to preserve exact legacy behavior.
