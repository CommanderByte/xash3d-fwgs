# PAK Backend TODO

## Purpose

This TODO tracks the migration of `filesystem/pak.c` behind the private modern
search path backend interface while preserving the legacy `searchpath_t`
callbacks and public filesystem ABI.

## Current Legacy Responsibilities

- Load PAK headers and directory entries from disk.
- Reject bad, empty, oversized, or corrupted PAK files.
- Sort directory entries for lookup.
- Open package slices through `FS_OpenHandle`.
- Implement find, search, file time, print info, close, and Quake PAK checks.
- Provide `FS_AddPak_Fullpath` as the C mount factory.

## Migration Order

- [x] Freeze direct `MountArchive_Fullpath` PAK behavior with tests.
  Evidence: `tests/filesystem/archive-order.c`.
- [x] Add a `PakBackend` skeleton mirroring `ISearchPathBackend`.
  Evidence: `src/include/filesystem/pak_backend.hpp`,
  `src/filesystem/pak_backend.cpp`, `tests/filesystem/pak_backend.cpp`.
- [x] Add a C adapter beside `pak.c`, similar to the directory backend bridge.
  Evidence: `filesystem/pak_backend_adapter.h`,
  `filesystem/pak_backend_adapter.cpp`.
- [x] Forward one callback at a time through the adapter.
  Evidence: `filesystem/pak.c`; command
  `.\waf.bat build --targets=test_filesystem_pak_backend,test_archive-order,test_wad-archive`.
- [x] Keep `FS_AddPak_Fullpath` callable from C.
  Evidence: `tests/filesystem/archive-order.c`.
- [x] Keep PAK directory allocation and `pack_t` ownership legacy-compatible
  until all tests pass.
  Evidence: `pack_t` still owns the loaded PAK directory and now stores only an
  opaque backend bridge pointer.
- [x] Run archive, no-init, and Windows smoke tests after callback forwarding.
  Evidence: `.\waf.bat build`,
  `.\build\src\test_filesystem_pak_backend.exe`,
  `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_wad-archive.exe`,
  `.\build\filesystem\test_no-init.exe`, and Windows `+fs_path +quit` smoke.

## Boundaries

- Do not change PAK file format parsing semantics during the first bridge.
- Do not expose C++ types through `filesystem_internal.h` unless hidden behind
  opaque pointers.
- Do not change WAD auto-mounting from PAK archives in this backend step.
