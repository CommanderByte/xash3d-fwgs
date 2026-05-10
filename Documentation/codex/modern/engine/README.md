# Modern Engine Notes

This folder documents intended modern engine internals. It is separate from
`Documentation/codex/legacy/engine/`, which describes the current architecture
and compatibility quirks.

Current implementation notes:

- `basecmd-migration-guide.md`: how the legacy `BaseCmd_*` registry should move
  toward `src/engine/commands/` without changing the public C surface.
- `client-command-dispatch-migration.md`: how Phase 60 routes
  `SV_ExecuteClientCommand()` lookup through target-neutral helpers while
  command handlers and client mutation stay legacy-owned.
- `client-policy-migration.md`: how Phase 85 routes userinfo penalty, rate,
  update interval, and prediction/lag/local-weapon flag decisions through
  target-neutral helpers while info-string mutation stays legacy-owned.
- `command-buffer-migration-guide.md`: how raw `Cbuf_*` buffer mechanics route
  through the private modern command-buffer primitive while dispatch policy
  stays in `cmd.c`.
- `console-logging-migration-guide.md`: how console/logging should be split
  into filters, formatters, and sinks while preserving the public C print
  surface and the rendered console/platform console distinction.
- `connectionless-classifier-migration.md`: how Phase 57 routes
  `SV_ConnectionlessPacket()` command classification through target-neutral
  server helpers while reads and handler effects stay legacy-owned.
- `connection-response-migration.md`: how Phase 59 routes server challenge and
  rejection response text through target-neutral helpers while validation and
  packet sends stay legacy-owned.
- `custom-resource-download-boundary.md`: how custom resource identity,
  download policy, upload queues, resource messages, and consistency checks
  should be split after the Phase 61 audit.
- `game-dll-bridge-boundary.md`: how `sv_game.c` should split into modern
  internals while preserving the game DLL ABI, callback table order, edict
  ownership, and message/session compatibility. The post-audit implementation
  lane is tracked in `Documentation/codex/todo/game_dll_bridge_todo.md`.
- `game-dll-entity-lifecycle-policy.md`: how Phase 95 routes narrow entity
  index, player-slot, and private-data allocation/free planning while leaving
  actual `edict_t` storage and destructor calls legacy-owned.
- `game-dll-string-pool-compatibility.md`: how Phase 94 models
  game-DLL-facing string processing, deduplication, overflow, and
  `string_t` offset behavior without moving the live string base out of
  `sv_game.c`.
- `platform-console-backends.md`: how background console backends should model
  Win32, POSIX, mobile log-only, and null-console capabilities before a broader
  console router exists.
- `public-crt-conversion-guide.md`: how public `Q_atoi*`, `Q_atof`, and
  `Q_atov` route through modern conversion helpers while preserving parsing
  quirks.
- `rendered-console-sink.md`: why the in-game rendered console remains a
  legacy client sink until a later router/client-rendering phase.
- `resource_identity.hpp` / `resource_identity.cpp`: Phase 62's implemented
  target-neutral custom resource identity helpers under `src/engine/server`.
- `server_download_policy.hpp` / `server_download_policy.cpp`: Phase 63's
  implemented `SV_DownloadFile_f()` policy helper; legacy code still owns
  filesystem probes, HPAK reads, fail responses, and netchan fragments.
- `server_consistency_list.hpp` / `server_consistency_list.cpp`: Phase 67's
  implemented consistency-list encoder; legacy code still owns cvars, client
  flags, `resource_t`, and the destination message.
- `server_consistency_policy.hpp` / `server_consistency_policy.cpp`: Phase
  68's implemented consistency setup and response validation policy; legacy
  code still owns file hashing, model bounds probes, message reads, drops, and
  the game DLL consistency callback.
- `server_customization_message.hpp` / `server_customization_message.cpp`:
  Phase 66's implemented customization payload encoder; legacy code still owns
  `svc_customization`, client netchan routing, and customization propagation.
- `server_resource_message.hpp` / `server_resource_message.cpp`: Phase 65's
  implemented resource-row encoder; legacy code still owns command wrappers,
  resource counts, consistency serialization, and netchan delivery.
- `server_upload_queue.hpp` / `server_upload_queue.cpp`: Phase 64's
  implemented upload queue policy helper; legacy code still owns `MSG_*`,
  HPAK probes, upload command emission, allocation, and resource-list mutation.
- `server-migration-guide.md`: how server-side helpers should move into
  `src/engine/server` while `SV_*`, `Log_*`, game DLL callbacks, and protocol
  surfaces remain compatibility boundaries.
- `server-event-log-migration.md`: how Phase 58 routes server event log line
  and stock message formatting through target-neutral helpers while sinks stay
  legacy-owned.
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
