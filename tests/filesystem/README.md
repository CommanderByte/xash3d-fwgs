# Filesystem Behavior Test Plan

This folder tracks filesystem behaviors that should be covered before and
during the modular C++ migration.

Compiled legacy filesystem tests live in this folder and are built by
`filesystem/wscript` when `--enable-tests` is set. Modern internal filesystem
utility tests can also live here and be built by `src/wscript` when they do not
need to run beside `filesystem_stdio`.

## Baselines

- [windows-test-baseline.md](windows-test-baseline.md) records the current
  Windows `--enable-tests` result and existing filesystem test coverage.

## Why This Exists

The filesystem has compatibility behavior that is easy to break accidentally:

- search path ordering
- loose file priority
- archive mount order
- `rodir` overlays
- `gamedironly` filtering
- case-insensitive lookup emulation
- direct path exceptions
- game/client DLL lookup
- allocator ownership for loaded file buffers

The migration should add tests for these before changing implementation
structure.

## Behavior Matrix

| Behavior | Current Coverage | Needed Test Location | Notes |
| --- | --- | --- | --- |
| API loads through `GetFSAPI` | `tests/filesystem/interface.cpp` | existing | Expand if API table layout changes. |
| `CreateInterface` lookups | `tests/filesystem/interface.cpp` | existing | Covers known interfaces and missing interface retval. |
| No-init safety | `tests/filesystem/no-init.c` | existing | Keep passing during adapter work. |
| Basic case-insensitive lookup | `tests/filesystem/caseinsensitive.c` | existing | Covers filesystem-created and direct-created files. |
| Nested directory case repair | `tests/filesystem/caseinsensitive.c` | existing | Added before directory backend pilot. |
| Cache refresh after direct file appears | `tests/filesystem/caseinsensitive.c` | existing | Covers direct file in existing cached directory. |
| Write path directory creation | `tests/filesystem/caseinsensitive.c` | existing | Uses `FS_Open(..., "wb", true)`. |
| Path rejection | `tests/filesystem/caseinsensitive.c` | existing | Covers `..`, absolute paths, colon paths. |
| Direct path behavior | `tests/filesystem/directpath.c` | existing | Covers `../` strip behavior when direct paths are enabled and reset. |
| Loose file beats archive file | `tests/filesystem/archive-order.c` | existing | Uses generated PAK fixture. |
| Archive mount idempotency | `tests/filesystem/archive-order.c` | existing | Direct `MountArchive_Fullpath` returns the same mounted archive for duplicate mount. |
| Unsupported archive mount failure | `tests/filesystem/archive-order.c` | existing | Unsupported extension does not mount. |
| Gamefolder beats basedir | `tests/filesystem/hierarchy.c` | existing | Generated `valve` plus `mod` fixture. |
| `gamedironly` filtering | `tests/filesystem/hierarchy.c` | existing | Confirms base-only content is hidden when requested. |
| Falldir/custom/downloaded/HD/LV/addon/localization hierarchy | `tests/filesystem/hierarchy.c` | existing | Freezes high-risk game hierarchy mount behavior before extraction. |
| PAK open/search | `tests/filesystem/archive-order.c` | existing | Generated PAK contains archive-only file. |
| PK3 directory mount behavior | `tests/filesystem/pk3dir.c` | existing | Covers `.pk3dir` directory mounts and loose-file precedence. |
| ZIP stored file | `tests/filesystem/zip-archive.c` | existing | Generated PK3 fixture with manual ZIP records. |
| ZIP deflated file | `tests/filesystem/zip-archive.c` | existing | Uses miniz raw deflate data inside generated PK3. |
| WAD lump lookup | `tests/filesystem/wad-archive.c` | existing | Generated WAD3 fixture with a script lump. |
| WADs mounted from archives | `tests/filesystem/wad-archive.c` | existing | Generated PAK fixture containing a WAD3 file. |
| `rodir` overlay precedence | `tests/filesystem/rodir.c` | existing | Writable root beats rodir; rodir-only content remains visible. |
| `VFileSystem009` compatibility behavior | `tests/filesystem/interface.cpp` | existing | Covers simple method behavior after lookup. |
| `XashFileSystem004` copied table behavior | `tests/filesystem/interface.cpp` | existing | Confirms returned table refreshes after caller mutation. |
| Mount snapshot records and writers | `tests/filesystem/debug_snapshot.cpp` | existing | Built by `src/wscript`; covers modern internal debug snapshot output. |
| Archive registry descriptors | `tests/filesystem/archive_registry.cpp` | existing | Built by `src/wscript`; covers default descriptor metadata and order. |
| Directory backend skeleton | `tests/filesystem/directory_backend.cpp` | existing | Built by `src/wscript`; covers private backend metadata and safe inert defaults. |
| PAK backend skeleton | `tests/filesystem/pak_backend.cpp` | existing | Built by `src/wscript`; covers private backend metadata and safe inert defaults. |
| Registry snapshot records and writers | `tests/filesystem/registry_snapshot.cpp` | existing | Built by `src/wscript`; covers future `fs_registry` output records. |
| DLL lookup | `tests/filesystem/dll-lookup.c` | existing | Focuses on returned `fs_dllinfo_t`, not loading real DLLs. |

## First Unit Tests To Add

1. Search result ordering and `FindFirst`/`FindNext` behavior through `VFileSystem009`.
2. Additional archive edge cases: duplicate names across multiple archives and unsupported compression.
3. Expanded DLL lookup cases for direct path mode and archived libraries.

These give the directory backend pilot a safety net before any C++ adapter work.

## Test Support Helpers

`tests/filesystem/fs_test_common.h` contains the shared test loader helper for
opening `filesystem_stdio`, finding `GetFSAPI`, and retrieving the `fs_api_t`
table. It also contains small fixture helpers for unique test directories,
directory creation/removal, and direct fixture-file writes.

`tests/filesystem/archive-order.c` currently generates its own tiny PAK fixture.
If more archive tests need generated archives, move those helpers into shared
test support instead of duplicating them.

## Fixture Notes

Prefer generated fixtures:

- directory trees created under a temporary test folder
- small text files with unique magic strings
- tiny PAK/ZIP/WAD files generated by helper functions

Avoid:

- Steam install dependencies
- large binary fixtures
- platform-specific absolute paths

## Relationship To Runtime Smoke Tests

Unit tests should not require real Half-Life assets. Runtime smoke tests still
matter for integration and should remain documented under `Documentation/codex`,
especially the Windows `XASH3D_BASEDIR` plus `XASH3D_RODIR` setup.
