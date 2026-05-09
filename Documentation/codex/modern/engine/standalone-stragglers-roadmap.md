# Standalone Stragglers Roadmap

## Why This Exists

Most modernization phases touch real subsystem ownership. That is useful, but
it can make every step feel like a mini architecture summit. A small
standalone-straggler phase gives us a place to harvest easy wins when they are
already well covered by tests.

## Top Candidate: CRC32 Constants

`public/crclib.c` still owns a static 256-entry CRC32 table. Modern checksum
code already computes the same table entries in `src/utilities/checksum.cpp`.

That makes CRC32 a good next slice because:

- public tests already cover standard CRC32 vectors and block-sequence quirks;
- modern utility tests already exercise checksum behavior;
- the public C ABI can remain untouched;
- the change should be local to `public/crclib.c`, `src/utilities`, or a small
  compat bridge.

The main caution is performance. The old table is constant-time lookup. If we
replace it with dynamic entry generation in hot loops, that could be slower.
The preferred implementation is either:

- share a generated/static table from the modern utility layer; or
- keep a cached table helper behind the modern interface.

Avoid replacing every byte step with repeated polynomial recomputation unless
tests or profiling prove the cost is irrelevant.

## Near-Term Phase Queue

1. Phase 46: low-risk standalone stragglers, starting with CRC32 constants.
2. Phase 47: public CRT micro-seams such as `Q_atoi` or narrow string/parser
   helpers, only after golden tests are explicit.
3. Phase 48: platform user/runtime helper extraction, starting with
   `Sys_GetCurrentUser` if Windows behavior is easy to verify.
4. Phase 49: filesystem engine bridge and logging follow-up, now that the
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
