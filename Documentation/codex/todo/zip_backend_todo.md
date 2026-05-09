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

- [ ] Expand ZIP tests for unsupported/corrupted archive failure cases.
- [ ] Keep stored and deflated fixtures passing before implementation movement.
- [ ] Add `ZipBackend` skeleton after PAK migration is stable.
- [ ] Preserve decompression and `file_t::ztk` behavior exactly.
- [ ] Keep `FS_AddZip_Fullpath` callable from C.

## Boundaries

- Do not change miniz interaction or compressed stream ownership during the
  first bridge.
- Do not treat `pk3dir` as ZIP; it remains a directory backend with ZIP-like
  search semantics.
