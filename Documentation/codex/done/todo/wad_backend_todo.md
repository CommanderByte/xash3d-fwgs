# WAD Backend TODO

## Purpose

This TODO tracks the eventual WAD backend migration. WAD should follow PAK
because WAD lump lookup has more compatibility-specific behavior.

## Current Legacy Responsibilities

- Load WAD2/WAD3 headers and lump tables.
- Reject bad, empty, oversized, or corrupted WAD files.
- Map lump names and type/extension behavior for lookup and load.
- Support WADs packed inside other archives through `FS_LOAD_PACKED_WAD`.
- Implement find, search, file time, print info, close, open, and lump load.

## Migration Order

- [x] Expand WAD fixture tests before implementation movement.
  Evidence: `tests/filesystem/wad-archive.c` covers raw WAD and packed WAD
  loading from PAK.
- [x] Add a `WadBackend` skeleton after PAK proves the bridge pattern.
  Evidence: `src/include/filesystem/wad_backend.hpp`,
  `src/filesystem/wad_backend.cpp`, `tests/filesystem/wad_backend.cpp`.
- [x] Preserve packed-WAD paths such as `pak0.pak/inside.wad`.
  Evidence: `tests/filesystem/wad-archive.c`; command
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`.
- [x] Forward callbacks one at a time through a C adapter.
  Evidence: `filesystem/wad.c`, `filesystem/wad_backend_adapter.h`,
  `filesystem/wad_backend_adapter.cpp`.
- [x] Keep `FS_AddWad_Fullpath` callable from C.
  Evidence: `tests/filesystem/wad-archive.c`.
- [x] Run archive, no-init, and Windows smoke tests after callback forwarding.
  Evidence: `.\waf.bat build`,
  `.\build\src\test_filesystem_wad_backend.exe`,
  `.\build\filesystem\test_wad-archive.exe`,
  `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_no-init.exe`, and Windows `+fs_path +quit` smoke.

## Boundaries

- Do not change lump extension/type compatibility during the first bridge.
- Do not merge WAD behavior into generic archive behavior prematurely.
