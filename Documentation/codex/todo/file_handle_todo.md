# File Handle TODO

## Purpose

Track eventual migration of `file_t` operations from `filesystem.c` into a
safer internal file-handle layer.

## Migration Order

- [x] Add test coverage for seek/tell/read/write edge cases before extraction.
- [x] Document decompression and backup-handle ownership rules.
- [x] Add target-neutral file handle operation helpers around `file_t`.
- [ ] Route close, read, write, seek, tell, eof, flush, getc, and gets in
  small batches.
  Note: tell/eof/seek cursor math now routes through `FileHandleOps`; handle
  lifetime, I/O, decompression, and line helpers remain legacy-owned.
- [ ] Consider a later RAII wrapper only behind legacy C entry points.

## Boundaries

- `file_t` layout is part of legacy internals and must not change casually.
- Preserve packed-file offsets and zlib streaming behavior.
- Preserve `XASH_REDUCE_FD` backup-handle behavior.
