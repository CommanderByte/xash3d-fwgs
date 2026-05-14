# Filesystem State TODO

## Purpose

Track the migration of `filesystem.c` global state into a private modern
`FilesystemState` object while preserving the legacy C ABI.

## Current Legacy Responsibilities

- Own root, base, game, read-only, and language path strings.
- Own the ordered search path list and current write path.
- Own direct-path compatibility mode.
- Keep `FI`, `fs_mempool`, and engine callbacks available to legacy entry
  points.

## Migration Order

- [x] Create target-neutral `FilesystemState` scaffold with fixed storage and
  opaque search path pointers.
  Evidence: `src/include/filesystem/filesystem_state.hpp`,
  `src/filesystem/filesystem_state.cpp`,
  `tests/filesystem/filesystem_state.cpp`; command
  `.\waf.bat build --targets=test_filesystem_state`.
- [x] Add read-only state snapshot capture from `filesystem.c` globals.
  Evidence: `src/include/filesystem/compat/filesystem_runtime_adapter.h`,
  `src/filesystem/compat/filesystem_runtime_adapter.cpp`, and
  `FS_SyncStateFromGlobals` in `filesystem/filesystem.c`; the original state
  adapter was folded into the runtime adapter during Phase 26.
- [x] Route `FS_AllowDirectPaths` through a tiny state helper.
  Evidence: `FS_SetDirectPaths` and `FS_DirectPathsEnabled` in
  `filesystem/filesystem.c`.
- [x] Route root/base/game/rodir/language assignment through state setters.
  Evidence: `FS_SetRootDir`, `FS_SetBaseDir`, `FS_SetGameDir`,
  `FS_SetReadOnlyDir`, and `FS_SetLanguage` in `filesystem/filesystem.c`.
- [x] Route search path list head and write path through state accessors.
  Evidence: `FS_SearchPaths`, `FS_SetSearchPaths`, `FS_WritePath`, and
  `FS_SetWritePath` in `filesystem/filesystem.c`.
- [x] Add regression tests after each wiring step.
  Evidence: command `.\waf.bat clean build` passed 33/33 tests; Windows smoke
  test with `+fs_path +quit` exited 0.

## Boundaries

- Do not expose `FilesystemState` through `fs_api_t` or `VFileSystem009`.
- Do not change `searchpath_t` list ordering while migrating ownership.
- Keep `fs_globals_t FI` ABI-compatible until callers no longer depend on it.
- Avoid dynamic allocation in the first state object.
