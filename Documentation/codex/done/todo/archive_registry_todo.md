# Archive Registry TODO

## Purpose

This TODO tracks the filesystem-specific archive registry that will eventually
replace the legacy `g_archives` table without changing mount behavior.

The archive registry is a consumer of the generic ordered registry utility in
`src/include/utilities/registry.hpp`.

## Current Scaffold

- [x] Add archive backend type enum.
  Evidence: `src/include/filesystem/archive_registry.hpp`.
- [x] Add archive format descriptor type.
  Evidence: `src/include/filesystem/archive_registry.hpp`.
- [x] Define archive registry typedef over the generic ordered registry.
  Decision: Use case-insensitive C-string keys for extensions so `PAK` and
  `pak` refer to the same descriptor. Descriptor extensions follow legacy
  `g_archives` style and do not include a leading dot.
  Evidence: `src/include/filesystem/archive_registry.hpp`,
  `tests/filesystem/archive_registry.cpp`.
- [x] Add compile/link smoke tests for the descriptor shape.
  Evidence: `tests/filesystem/archive_registry.cpp`; command `.\waf.bat build`.
- [x] Add default descriptor table for PAK, PK3, PK3DIR, and WAD.
  Evidence: `src/filesystem/archive_registry.cpp`,
  `tests/filesystem/archive_registry.cpp`.
- [x] Preserve legacy scan priority: PAK -> PK3 -> PK3DIR -> WAD.
  Decision: Store scan priority as descriptor metadata. Mount policy still
  owns how that priority is used.
  Evidence: `src/filesystem/archive_registry.cpp`,
  `tests/filesystem/archive_registry.cpp`.
- [x] Add archive registry initialization function.
  Evidence: `RegisterDefaultArchiveFormats` in
  `src/filesystem/archive_registry.cpp`.
- [x] Add duplicate descriptor policy tests for default registration.
  Evidence: `tests/filesystem/archive_registry.cpp`.
- [x] Add filesystem registry snapshot records and JSON/human writers.
  Evidence: `src/include/filesystem/registry_snapshot.hpp`,
  `src/filesystem/registry_snapshot.cpp`,
  `tests/filesystem/registry_snapshot.cpp`.

## Pending Implementation

- [x] Decide where mount factory adapters live so the generic descriptor does
  not expose legacy function-pointer details unnecessarily.
  Decision: Keep factory pointers in the legacy C `fs_archive_t` table for the
  first integration pass. Add a small C-compatible adapter that exposes modern
  descriptor metadata to `filesystem.c`, then map descriptors back to the
  existing legacy factory table by search path type.
  Evidence: `filesystem/archive_registry_adapter.h`,
  `filesystem/archive_registry_adapter.cpp`.
- [x] Compare default registry output against legacy `g_archives` behavior.
  Evidence: `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_wad-archive.exe`, and
  `.\build\filesystem\test_zip-archive.exe` passed after scan ordering was
  routed through the registry adapter.
- [x] Wire `FS_MountArchive_Fullpath` through the registry after descriptor
  parity is proven by tests.
  Evidence: `FS_AddArchive_Fullpath` now uses `FS_ArchiveRegistry_Find` when
  no explicit legacy archive descriptor is supplied; `tests/filesystem/archive-order.c`
  covers direct `MountArchive_Fullpath("direct.PAK", FS_GAMEDIR_PATH)`.
- [x] Route `FS_IsArchiveExtensionSupported` through the registry adapter.
  Evidence: `filesystem/filesystem.c`, `tests/filesystem/no-init.c`.
- [x] Route game-directory archive scan ordering through the registry adapter.
  Evidence: `FS_AddGameDirectory` uses registry enumeration order and maps each
  descriptor to the existing legacy factory table.

## Boundaries

- The archive registry owns archive descriptor lookup and stable enumeration.
- It does not decide game hierarchy mount order.
- It does not validate paths.
- It does not scan directories by itself.
- It does not mount contained WADs directly; it only records the descriptor
  metadata that policy can use later.
