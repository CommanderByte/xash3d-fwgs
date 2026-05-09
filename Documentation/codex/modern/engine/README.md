# Modern Engine Notes

This folder documents intended modern engine internals. It is separate from
`Documentation/codex/legacy/engine/`, which describes the current architecture
and compatibility quirks.

Current implementation notes:

- `basecmd-migration-guide.md`: how the legacy `BaseCmd_*` registry should move
  toward `src/engine/commands/` without changing the public C surface.
