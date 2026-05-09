# Filesystem Decluttering TODO

## Purpose

Track the aggressive-but-compatible cleanup pass between handler migration and
logging modernization. The goal is to shrink `filesystem/` toward true public
ABI/export glue while preserving legacy quirks through explicit compatibility
adapters under `src/`.

This phase is allowed to move files and reorganize build inputs more
aggressively than earlier migration phases, but it must not change exported
symbols, public struct layouts, or externally visible filesystem behavior.

## Phase 29 Tasks

- [x] `FS-DECLUTTER-001` Move compatibility adapter sources and headers out of
  the legacy `filesystem/` folder.
  Evidence: adapter sources moved to `src/filesystem/compat/`; adapter headers
  moved to `src/include/filesystem/compat/`; `filesystem/` now contains 17
  files; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
  Notes: these files remain DLL-only compatibility code through
  `filesystem/wscript`; they are not part of the reusable
  `modern_filesystem` static library.

- [x] `FS-DECLUTTER-002` Move private compatibility headers out of
  `filesystem/` once the remaining legacy `.c` bodies include narrower paths.
  Evidence: private headers moved to
  `src/include/filesystem/compat/private/`; `filesystem/` now contains 13
  files; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
  Notes: likely destination is `src/include/filesystem/compat/private/`, but
  keep public `filesystem.h` and `VFileSystem009.h` in place.

- [x] `FS-DECLUTTER-003` Split `filesystem.c` into export/runtime glue and
  focused legacy compatibility bodies.
  Evidence: `src/filesystem/compat/stringlist_legacy.cpp`,
  `src/filesystem/compat/memory_legacy.cpp`, `filesystem/filesystem.c`;
  command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
  Notes: first candidates are stringlist helpers, filesystem state bootstrap,
  direct-path helpers, and file load/write helpers.

- [x] `FS-DECLUTTER-004` Move `VFileSystem009.cpp` into a compatibility source
  location while preserving `VFileSystem009.h` in the public legacy folder.
  Evidence: `src/filesystem/compat/VFileSystem009.cpp`,
  `filesystem/VFileSystem009.h`, `filesystem/wscript`; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
  Notes: the implementation can live beside the facade adapter once the build
  file makes that relationship obvious.

- [x] `FS-DECLUTTER-005` Reassess whether `dir.c`, `pak.c`, `wad.c`, `zip.c`,
  and `android.c` can become smaller backend shims or move under compat.
  Evidence: `src/filesystem/compat/dir.c`,
  `src/filesystem/compat/pak.c`, `src/filesystem/compat/wad.c`,
  `src/filesystem/compat/zip.c`, `src/filesystem/compat/android.c`,
  `filesystem/wscript`; `filesystem/` now contains 7 files; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
  Notes: do this one backend at a time with archive/file behavior tests.

- [x] `FS-DECLUTTER-006` Update the folder migration map after each physical
  move.
  Evidence: `Documentation/codex/modern/filesystem/filesystem-folder-migration-map.md`,
  `Documentation/codex/todo/filesystem_decluttering_todo.md`.

## Rules

- Keep `GetFSAPI` and `CreateInterface` exported.
- Keep `filesystem/filesystem.h`, `filesystem/VFileSystem009.h`, and
  `filesystem/exports.txt` stable.
- Keep compatibility code build-scoped to the filesystem DLL unless it is
  target-neutral and tested as part of `modern_filesystem`.
- Run archive/file-handle/interface smoke tests after every move batch.
