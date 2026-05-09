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

- [ ] Decide where mount factory adapters live so the generic descriptor does
  not expose legacy function-pointer details unnecessarily.
- [ ] Compare default registry output against legacy `g_archives` behavior.
- [ ] Wire `FS_MountArchive_Fullpath` through the registry after descriptor
  parity is proven by tests.

## Boundaries

- The archive registry owns archive descriptor lookup and stable enumeration.
- It does not decide game hierarchy mount order.
- It does not validate paths.
- It does not scan directories by itself.
- It does not mount contained WADs directly; it only records the descriptor
  metadata that policy can use later.
