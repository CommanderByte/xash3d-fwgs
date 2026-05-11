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
- `client-session-boundary-audit.md`: how Phase 124 maps `sv_client.c` into
  admission, session slots, spawn handshake, userinfo, command, transfer,
  voice, cvar-query, remote-admin, and movement ownership before choosing a
  small client-session slot helper as the next seam.
- `client-session-slots.md`: how Phase 125 routes player/bot counts,
  first-free-slot selection, and master heartbeat population decisions through
  plain client-slot snapshots while leaving live connect/drop ownership in
  `sv_client.c`.
- `client-transfer-voice-admin-split.md`: how Phase 126 keeps transfer and
  voice on their existing helpers, defers cvar-query callback ownership, and
  extracts only rcon auth/action plus quoted command reconstruction.
- `server-client-flag-policy.md`: how Phase 109 groups private `FCL_*` flags by
  owner and starts routing fake-client/query-visible predicates without broad
  macro replacement.
- `server-event-playback-boundary.md`: how Phase 110 splits game DLL event
  playback into compatibility-owned live state and target-neutral policy
  candidates for event admission, recipient decisions, and queue-slot planning.
- `server-event-playback-policy.md`: how Phase 111 routes event playback
  admission, flag normalization, recipient decisions, queue-slot planning, and
  queued emit-count clamping through target-neutral helpers.
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
- `game-dll-bridge-submodule-plan.md`: how Phase 121 turns the completed game
  DLL bridge lane into a future `src/engine/server/game_dll/` submodule layout,
  which cross-callback tests should exist first, and which `sv_game.c` regions
  remain too coupled to move.
- `game-dll-changelevel-save-boundary.md`: how Phase 97 models changelevel
  admission, landmark truncation, smooth/classic queuing, and `.HL3`
  entity-patch intent while keeping runtime save/load callbacks legacy-owned.
- `game-dll-entity-lifecycle-policy.md`: how Phase 95 routes narrow entity
  index, player-slot, and private-data allocation/free planning while leaving
  actual `edict_t` storage and destructor calls legacy-owned.
- `game-dll-entity-parse-boundary.md`: how Phase 96 models map entity
  key-value parsing, `angle` rewrite, custom entity fallback, and spawn
  rejection decisions without moving live `pfnKeyValue()`/`pfnSpawn()`
  ordering out of `sv_game.c`.
- `game-dll-load-unload-boundary.md`: how Phase 100 models fake-symbol DLL
  load/unload decisions, API fallback, optional interface behavior, and
  cleanup intent while keeping real library lifetime and ABI publication
  legacy-owned.
- `game-dll-message-bridge-aggregate.md`: how Phase 122 adds the first
  cross-callback game DLL bridge aggregate around user-message registration,
  message-session begin/write/end, resend payloads, multicast destination
  policy, and rewrite admission.
- `game-dll-message-bridge-adapter-pilot.md`: how Phase 123 groups the
  message-session and user-message registry adapter implementations without
  changing the split public C headers, callback table, or live `sv_game.c`
  ownership.
- `game-dll-movement-fake-client-boundary.md`: how Phase 99 models
  movement callback admission, yaw/pitch stepping, walkmove routing,
  maxspeed clamping, and fake-client command snapshots while leaving
  physics and player command execution legacy-owned.
- `milestone-100-server-modernization-audit.md`: how the modern server helper
  layer looks after the game DLL bridge lane, and why server constants and
  constraints are the next low-risk migration target.
- `model-visibility-service-boundary.md`: how Phase 113 audits BSP model,
  hull, PVS/PAS, trace, physics, and game DLL visibility ownership before any
  runtime route-through.
- `post-106-migration-audit.md`: how the server constants lane changes the
  next roadmap, why group filtering, map validation flags, client flags, event
  playback, and read-only cvar snapshots are better next enablers than another
  broad server sweep.
- `server-cpp-ownership-consolidation.md`: how Phase 114 classifies the modern
  server helper surface as reusable concepts, behavior owners, or temporary
  facades, and where the old `sv_*.c` grouping should eventually split.
- `game-dll-string-pool-compatibility.md`: how Phase 94 models
  game-DLL-facing string processing, deduplication, overflow, and
  `string_t` offset behavior without moving the live string base out of
  `sv_game.c`.
- `game-dll-visibility-trace-boundary.md`: how Phase 98 models trace and
  visibility callback admission, fallback, and result-code decisions while
  deferring collision, BSP, PVS/PAS, and leaf ownership to a later world phase.
- `platform-console-backends.md`: how background console backends should model
  Win32, POSIX, mobile log-only, and null-console capabilities before a broader
  console router exists.
- `public-crt-conversion-guide.md`: how public `Q_atoi*`, `Q_atof`, and
  `Q_atov` route through modern conversion helpers while preserving parsing
  quirks.
- `rendered-console-sink.md`: why the in-game rendered console remains a
  legacy client sink until a later router/client-rendering phase.
- `resource-transfer-consolidation-audit.md`: how Phase 115 groups startup
  resource catalogs, downloads, uploads, customizations, consistency checks,
  hot resources, reslists, and game DLL resource callbacks into a future
  resource-transfer domain.
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
- `server-constants-constraints.md`: how Phase 101 mirrors server-only limits,
  flags, and private constants into a typed modern contract without replacing
  layout-sensitive legacy macros.
- `server-challenge-window-policy.md`: how Phase 102 routes challenge
  time-window calculation through a target-neutral helper while keeping
  address hashing, salts, packets, and rejection effects legacy-owned.
- `server-lifecycle-limits-policy.md`: how Phase 103 routes server maxclient,
  update-backup, packet-entity capacity, game-entity count, and spawn settling
  calculations through target-neutral helpers while keeping allocation and
  activation side effects legacy-owned.
- `server-map-validation-policy.md`: how Phase 108 centralizes
  `SV_MapIsValid()` flag interpretation, changelevel landmark compatibility,
  save/load admission, and game DLL existence behavior while map probing and
  entity parsing stay legacy-owned.
- `server-operator-command-boundary.md`: how Phase 128 keeps
  `sv_cmds.c` command registration, console output, filesystem probes,
  save/load effects, and info-string mutation legacy-owned while routing
  `kick`, `serverinfo`, and `localinfo` argument policy through a modern helper.
- `server-movement-constraints.md`: how Phase 104 names server monster
  movement modes and fly-move clip-plane constraints separately from trace,
  walkmove, and `pm_shared` constants.
- `server-visibility-constraints.md`: how Phase 105 routes entity leaf and
  portal viewentity capacity policy through target-neutral helpers while
  keeping BSP traversal, PVS/PAS, and packet ownership legacy-owned.
- `server-route-through-review.md`: how Phase 106 reviews the Phase 101-105
  constants lane, confirms no extra broad route-through should be made, and
  lists the next behavior-owner phases.
- `server-runtime-configuration-boundary.md`: how Phase 127 keeps `sv_main.c`
  cvar registration, movevars, packet reads, frame ordering, master heartbeats,
  and shutdown legacy-owned while routing client timeout decisions through a
  plain modern policy helper.
- `server-event-log-migration.md`: how Phase 58 routes server event log line
  and stock message formatting through target-neutral helpers while sinks stay
  legacy-owned.
- `server-filter-migration.md`: how Phase 53 routes ID/IP filter policy through
  target-neutral modern server helpers while legacy command/file ownership
  stays in `sv_filter.c`.
- `server-frame-snapshot-boundary.md`: how Phase 129 audits `sv_frame.c`
  packet entity selection, baseline deltas, events, pings, clientdata,
  datagrams, and inactive-client handling before selecting a narrow
  packet-entity delta cursor as the first possible Phase 130 helper.
- `server-group-filter-policy.md`: how Phase 107 routes repeated
  `GROUP_OP_AND` / `GROUP_OP_NAND` entity and active-mask predicates through a
  target-neutral helper while collision, trace, multicast, and save ownership
  stay legacy-owned.
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
- `read-only-cvar-snapshot.md`: how Phase 112 introduces a plain read-only cvar
  value snapshot for modern policy helpers without moving cvar registry,
  mutation, callback, or archive ownership.
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
