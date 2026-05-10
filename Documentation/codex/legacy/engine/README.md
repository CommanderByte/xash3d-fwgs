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
- [console-logging-baseline.md](console-logging-baseline.md) documents the
  current print hub, in-game console, platform console, engine log, and server
  event log ownership.
- [hash-checksum-baseline.md](hash-checksum-baseline.md) documents the public
  `crclib` hash/checksum surface, callers, and migration constraints.
- [info-string-baseline.md](info-string-baseline.md) documents the migrated
  `Info_*` compatibility rules, quirks, and test coverage.
- [network-buffer-baseline.md](network-buffer-baseline.md) documents `MSG_*`
  bit/byte buffer behavior, overflow quirks, GoldSrc sign mode, and the first
  safe routing boundary.
- [server-boundary-audit.md](server-boundary-audit.md) audits `engine/server/`
  ownership, compatibility surfaces, and near-term migration candidates.
- [server-filter-baseline.md](server-filter-baseline.md) documents current
  `sv_filter.c` ID/IP filter behavior, command/file surfaces, and Phase 53
  route-through scope.
- [source-query-baseline.md](source-query-baseline.md) documents current
  `sv_query.c` GoldSrc query payloads, suppression rules, and Phase 54
  route-through scope.
- [string-path-baseline.md](string-path-baseline.md) documents shared
  `crtlib` string/path helpers, path quirks, and the Phase 40 migration scope.
- [system-platform-facade-audit.md](system-platform-facade-audit.md) audits
  `system.c`, `system.h`, and the platform source split before Phase 44
  platform-facade cleanup.
- [user-agent-policy-baseline.md](user-agent-policy-baseline.md) documents
  `SV_ProcessUserAgent()` UUID, ban, input-device, and rejection-message
  behavior.
