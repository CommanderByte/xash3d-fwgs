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

- [ ] Freeze direct `MountArchive_Fullpath` PAK behavior with tests.
- [ ] Add a `PakBackend` skeleton mirroring `ISearchPathBackend`.
- [ ] Add a C adapter beside `pak.c`, similar to the directory backend bridge.
- [ ] Forward one callback at a time through the adapter.
- [ ] Keep `FS_AddPak_Fullpath` callable from C.
- [ ] Keep PAK directory allocation and `pack_t` ownership legacy-compatible
  until all tests pass.
- [ ] Run archive, no-init, and Windows smoke tests after callback forwarding.

## Boundaries

- Do not change PAK file format parsing semantics during the first bridge.
- Do not expose C++ types through `filesystem_internal.h` unless hidden behind
  opaque pointers.
- Do not change WAD auto-mounting from PAK archives in this backend step.
