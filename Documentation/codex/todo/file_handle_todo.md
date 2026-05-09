# File Handle TODO

## Purpose

Track eventual migration of `file_t` operations from `filesystem.c` into a
safer internal file-handle layer.

## Migration Order

- [ ] Add test coverage for seek/tell/read/write edge cases before extraction.
- [ ] Document decompression and backup-handle ownership rules.
- [ ] Add target-neutral file handle operation helpers around `file_t`.
- [ ] Route close, read, write, seek, tell, eof, flush, getc, and gets in
  small batches.
- [ ] Consider a later RAII wrapper only behind legacy C entry points.

## Boundaries

- `file_t` layout is part of legacy internals and must not change casually.
- Preserve packed-file offsets and zlib streaming behavior.
- Preserve `XASH_REDUCE_FD` backup-handle behavior.
