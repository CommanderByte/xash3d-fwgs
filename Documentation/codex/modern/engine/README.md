# Modern Engine Notes

This folder documents intended modern engine internals. It is separate from
`Documentation/codex/legacy/engine/`, which describes the current architecture
and compatibility quirks.

Current implementation notes:

- `basecmd-migration-guide.md`: how the legacy `BaseCmd_*` registry should move
  toward `src/engine/commands/` without changing the public C surface.
- `hash-checksum-migration-guide.md`: how public `crclib` compatibility exports
  should delegate into `src/utilities` without making hash/checksum helpers
  engine-owned.
- `info-string-migration-guide.md`: how the legacy `Info_*` API now routes
  through `src/engine/info_string.*` while keeping C callers stable.
