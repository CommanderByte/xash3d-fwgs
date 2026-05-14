# Path Policy TODO

## Purpose

Track extraction of path rejection, direct-path compatibility, write-path
resolution, and full/relative path conversion from `filesystem.c`.

## Migration Order

- [x] Add target-neutral `PathPolicy` helpers for path rejection.
  Evidence: `src/include/filesystem/path_policy.hpp`,
  `src/filesystem/path_policy.cpp`, `tests/filesystem/path_policy.cpp`.
- [x] Freeze direct-path `../` compatibility behavior with focused tests.
  Evidence: `tests/filesystem/path_policy.cpp`.
- [x] Route `FS_CheckNastyPath` through `PathPolicy`.
  Evidence: `filesystem/path_policy_adapter.h`,
  `filesystem/path_policy_adapter.cpp`, `filesystem/filesystem.c`.
- [x] Route direct-path relative stripping through `PathPolicy`.
  Evidence: `FS_PathPolicy_StripDirectRelativePrefix` usage in
  `filesystem/filesystem.c`.
- [x] Route write-mode mutation detection through a dedicated policy helper.
  Evidence: `FS_PathPolicy_IsWriteMode` usage in `filesystem/filesystem.c`.
- [x] Run regression tests after path policy extraction.
  Evidence: command `.\waf.bat clean build` passed 34/34 tests; Windows smoke
  test with `+fs_path +quit` exited 0.

## Boundaries

- Direct-path mode is a compatibility escape hatch, not general path
  normalization.
- Write operations must continue to use `fs_writepath`.
- Keep platform-specific wide-path conversion outside the generic policy.
