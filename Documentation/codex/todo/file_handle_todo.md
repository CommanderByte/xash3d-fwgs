# File Handle TODO

## Purpose

Track eventual migration of `file_t` operations from `filesystem.c` into a
safer internal file-handle layer.

## Migration Order

- [x] Add test coverage for seek/tell/read/write edge cases before extraction.
- [x] Document decompression and backup-handle ownership rules.
- [x] Add target-neutral file handle operation helpers around `file_t`.
- [x] Route close, read, write, seek, tell, eof, flush, getc, and gets in
  small batches.
  Note: tell/eof/seek cursor math now routes through `FileHandleOps`; handle
  length, character, and line helpers now route through
  `file_handle_ops_adapter`; handle lifetime, raw I/O, decompression, and
  reduced-FD reopen behavior remain legacy-owned.
- [ ] Consider a later RAII wrapper only behind legacy C entry points.

Evidence: `.\waf.bat build --targets=test_filesystem_runtime,test_filesystem_file_handle_ops,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive`
passed 6/6 tests on 2026-05-09; `.\waf.bat build` passed 25/25 tests.

## Boundaries

- `file_t` layout is part of legacy internals and must not change casually.
- Preserve packed-file offsets and zlib streaming behavior.
- Preserve `XASH_REDUCE_FD` backup-handle behavior.
