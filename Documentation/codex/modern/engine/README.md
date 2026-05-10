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
- `connectionless-classifier-migration.md`: how Phase 57 routes
  `SV_ConnectionlessPacket()` command classification through target-neutral
  server helpers while reads and handler effects stay legacy-owned.
- `platform-console-backends.md`: how background console backends should model
  Win32, POSIX, mobile log-only, and null-console capabilities before a broader
  console router exists.
- `public-crt-conversion-guide.md`: how public `Q_atoi*`, `Q_atof`, and
  `Q_atov` route through modern conversion helpers while preserving parsing
  quirks.
- `rendered-console-sink.md`: why the in-game rendered console remains a
  legacy client sink until a later router/client-rendering phase.
- `server-migration-guide.md`: how server-side helpers should move into
  `src/engine/server` while `SV_*`, `Log_*`, game DLL callbacks, and protocol
  surfaces remain compatibility boundaries.
- `server-filter-migration.md`: how Phase 53 routes ID/IP filter policy through
  target-neutral modern server helpers while legacy command/file ownership
  stays in `sv_filter.c`.
- `source-query-migration.md`: how Phase 54 routes GoldSrc source-query payload
  bytes through target-neutral builders while live server state and
  `NET_SendPacket` stay legacy-owned.
- `user-agent-policy-migration.md`: how Phase 55 routes connection user-agent
  validation through target-neutral policy while cvars, ID bans, and rejection
  sends stay legacy-owned.
- `hash-checksum-migration-guide.md`: how public `crclib` compatibility exports
  should delegate into `src/utilities` without making hash/checksum helpers
  engine-owned.
- `info-string-migration-guide.md`: how the legacy `Info_*` API now routes
  through `src/engine/info_string.*` while keeping C callers stable.
- `network-buffer-migration-guide.md`: how the private modern network bit
  primitive should grow behind the legacy `MSG_*` wire-format surface.
- `netapi-info-migration.md`: how Phase 56 routes short and long server
  NetAPI info-string construction through modern builders while request
  parsing and packet sends stay legacy-owned.
- `string-path-migration-guide.md`: how public `crtlib` path helpers route
  through `src/utilities/path.*` while keeping C callers stable.
- `standalone-stragglers-roadmap.md`: how to queue small, well-tested cleanup
  slices such as CRC32 constants without turning them into broad subsystem
  rewrites.
- `system-platform-facade-plan.md`: how `Sys_*` stays as the legacy C facade
  while target-neutral or platform-selected helpers move behind it.
