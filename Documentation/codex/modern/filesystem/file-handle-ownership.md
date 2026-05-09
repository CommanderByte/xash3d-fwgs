# File Handle Ownership Notes

## Purpose

These notes freeze the current `file_t` ownership rules before the file-handle
operations move out of `filesystem.c`.

## Plain Handles

`FS_Open` allocates each `file_t` from `fs_mempool`, opens an OS file handle,
sets `ungetc` to `EOF`, captures the real file length with `lseek`, and seeks
back to the start unless append mode is requested.

`FS_Close` is the only owner-side cleanup path. It releases any reduced-FD
backup path, closes the OS handle when present, tears down decompression state,
and frees the `file_t`.

## Archive Handles

Archive entries are opened through `FS_OpenHandle`. The returned `file_t`
stores:

- `searchpath`: owning search path.
- `offset`: archive-local file offset.
- `real_length`: uncompressed visible file length.
- `position`: logical uncompressed cursor.

When `XASH_REDUCE_FD` is disabled, the archive handle is duplicated or reopened.
When `XASH_REDUCE_FD` is enabled, the handle starts closed and is reopened from
`backup_path` by `FS_EnsureOpenFile`.

## Deflated Streams

Deflated ZIP entries set `FILE_DEFLATED` and allocate `ztoolkit_t`. Reads must
go through zlib state in `file->ztk`; direct OS reads only refill compressed
input. `FS_Close` owns `inflateEnd` and frees `file->ztk`.

Compressed seeks are implemented by replaying decompression from the beginning
when seeking backward, then reading and discarding bytes until the requested
logical offset is reached.

## Reduced-FD Caveat

The existing source contains a warning that `XASH_REDUCE_FD` is broken for
compressed files. The migration should keep that caveat visible and avoid
claiming compressed backup-handle behavior is fixed unless a dedicated test is
added on a reduced-FD build.

## Safe First Extraction

`FileHandleOps` owns only target-neutral cursor math: logical position,
EOF checks, seek target resolution, and buffered-seek range checks. It does not
own OS handles, zlib streams, `file_t` allocation, backup paths, or archive
lifetimes.
