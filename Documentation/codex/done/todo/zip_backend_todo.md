# ZIP And PK3 Backend TODO

## Purpose

This TODO tracks migration of `filesystem/zip.c`, which handles ZIP/PK3
archives and compressed file reads.

## Current Legacy Responsibilities

- Parse ZIP end-of-central-directory and central directory records.
- Reject empty or unsupported/corrupted archives.
- Support stored and deflated file entries.
- Open files with decompression state and optional full-file load override.
- Implement find, search, file time, print info, close, open, and load.

## Migration Order

- [x] Expand ZIP tests for unsupported/corrupted archive failure cases.
  Evidence: `tests/filesystem/zip-archive.c` covers corrupt ZIP mount
  rejection and unsupported compression load failure.
- [x] Keep stored and deflated fixtures passing before implementation movement.
  Evidence: `tests/filesystem/zip-archive.c`; command
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`.
- [x] Add `ZipBackend` skeleton after PAK migration is stable.
  Evidence: `src/include/filesystem/zip_backend.hpp`,
  `src/filesystem/zip_backend.cpp`, `tests/filesystem/zip_backend.cpp`.
- [x] Preserve decompression and `file_t::ztk` behavior exactly.
  Evidence: legacy `FS_OpenFile_ZIP_Legacy` and `FS_LoadZIPFile_Legacy`
  remain the implementation behind bridge hooks.
- [x] Keep `FS_AddZip_Fullpath` callable from C.
  Evidence: `tests/filesystem/zip-archive.c`.
- [x] Run archive, no-init, and Windows smoke tests after callback forwarding.
  Evidence: `.\waf.bat build`,
  `.\build\src\test_filesystem_zip_backend.exe`,
  `.\build\filesystem\test_zip-archive.exe`,
  `.\build\filesystem\test_wad-archive.exe`,
  `.\build\filesystem\test_no-init.exe`, and Windows `+fs_path +quit` smoke.

## Boundaries

- Do not change miniz interaction or compressed stream ownership during the
  first bridge.
- Do not treat `pk3dir` as ZIP; it remains a directory backend with ZIP-like
  search semantics.
