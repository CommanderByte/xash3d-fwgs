# Modern Engine Notes

This folder documents intended modern engine internals. It is separate from
`Documentation/codex/legacy/engine/`, which describes the current architecture
and compatibility quirks.

Current implementation notes:

- `basecmd-migration-guide.md`: how the legacy `BaseCmd_*` registry should move
  toward `src/engine/commands/` without changing the public C surface.
- `command-buffer-migration-guide.md`: how raw `Cbuf_*` buffer mechanics route
  through the private modern command-buffer primitive while dispatch policy
  stays in `cmd.c`.
- `console-logging-migration-guide.md`: how console/logging should be split
  into filters, formatters, and sinks while preserving the public C print
  surface and the rendered console/platform console distinction.
- `platform-console-backends.md`: how background console backends should model
  Win32, POSIX, mobile log-only, and null-console capabilities before a broader
  console router exists.
- `rendered-console-sink.md`: why the in-game rendered console remains a
  legacy client sink until a later router/client-rendering phase.
- `hash-checksum-migration-guide.md`: how public `crclib` compatibility exports
  should delegate into `src/utilities` without making hash/checksum helpers
  engine-owned.
- `info-string-migration-guide.md`: how the legacy `Info_*` API now routes
  through `src/engine/info_string.*` while keeping C callers stable.
- `network-buffer-migration-guide.md`: how the private modern network bit
  primitive should grow behind the legacy `MSG_*` wire-format surface.
- `string-path-migration-guide.md`: how public `crtlib` path helpers route
  through `src/utilities/path.*` while keeping C callers stable.
- `system-platform-facade-plan.md`: how `Sys_*` stays as the legacy C facade
  while target-neutral or platform-selected helpers move behind it.
