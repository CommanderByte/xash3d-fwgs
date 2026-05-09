# Legacy Engine Notes

This folder documents the current engine architecture before modernization.
It focuses on real ownership, coupling, global state, and migration risk.

## Documents

- [common-audit.md](common-audit.md) audits `engine/` with a deeper pass over
  `engine/common/`.
- [command-cvar-baseline.md](command-cvar-baseline.md) documents the current
  command/cvar ownership, lifecycle, tests, and migration seams.
- [command-buffer-baseline.md](command-buffer-baseline.md) documents the
  command-buffer queue, splitter, comment, quote, insertion, and migration
  compatibility rules.
- [hash-checksum-baseline.md](hash-checksum-baseline.md) documents the public
  `crclib` hash/checksum surface, callers, and migration constraints.
- [info-string-baseline.md](info-string-baseline.md) documents the migrated
  `Info_*` compatibility rules, quirks, and test coverage.
- [string-path-baseline.md](string-path-baseline.md) documents shared
  `crtlib` string/path helpers, path quirks, and the Phase 40 migration scope.
