# Standalone Stragglers Roadmap

## Why This Exists

Most modernization phases touch real subsystem ownership. That is useful, but
it can make every step feel like a mini architecture summit. A small
standalone-straggler phase gives us a place to harvest easy wins when they are
already well covered by tests.

## Top Candidate: CRC32 Constants

Phase 46 moved the CRC32 table source behind `src/utilities/checksum.cpp`.
`public/crclib.c` keeps the public `CRC32_*` C functions and their unrolled
loops, but now reads the shared table through the private `Xash_Crc32Table()`
C adapter.

That makes CRC32 a good next slice because:

- public tests already cover standard CRC32 vectors and block-sequence quirks;
- modern utility tests already exercise checksum behavior;
- the public C ABI can remain untouched;
- the change should be local to `public/crclib.c`, `src/utilities`, or a small
  compat bridge.

The main caution was performance. The old table was constant-time lookup, so
the implementation uses a cached modern table and gives C callers a table
pointer. Public CRC loops do not recompute the polynomial per byte.

## Near-Term Phase Queue

1. Phase 46: low-risk standalone stragglers, starting with CRC32 constants.
   Completed with public ABI preserved.
2. Phase 47: public CRT micro-seams such as `Q_atoi` or narrow string/parser
   helpers, only after golden tests are explicit.
3. Phase 48: platform user/runtime helper extraction, starting with
   `Sys_GetCurrentUser` if Windows behavior is easy to verify.
4. Phase 50: filesystem engine bridge and logging follow-up, now that the
   system console phase exists.

## Not Yet

These are intentionally not low-hanging fruit:

- memory pools and allocation families;
- fatal error and restart paths;
- rendered in-game console routing;
- broad parser rewrites;
- renderer/model loading internals.

Those can still be good phases, just not the sleepy “one more tiny thing”
category.
