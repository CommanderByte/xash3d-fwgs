# Server Migration TODO

This TODO tracks the active server-side engine migration lane opened by Phase
52. Keep it focused on near-term work; broader server rewrites should remain in
the main phase tracker until they are selected.

## Phase 52: Boundary Audit

- [x] Audit `engine/server/` file ownership and coupling.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.
- [x] Identify compatibility surfaces that must remain stable during early
  migration.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.
- [x] Define the first server compatibility-layer shape.
  Evidence: `Documentation/codex/modern/engine/server-migration-guide.md`.
- [x] Rank near-term migration candidates.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.

## Phase 53: Server Filter Pilot

- [x] Capture focused baseline behavior for existing `sv_filter.c` tests and
  command/file surfaces.
  Evidence: `Documentation/codex/legacy/engine/server-filter-baseline.md`.
- [x] Add modern tests for IP filter inclusion, removal, active/expired rules,
  and config/human formatting.
  Evidence: `tests/engine/server_filter.cpp`.
- [x] Add modern tests for ID filter prefix matching and expiration.
  Evidence: `tests/engine/server_filter.cpp`.
- [x] Implement target-neutral filter value types and lists under
  `src/engine/server`.
  Evidence: `src/include/engine/server/server_filter.hpp`,
  `src/engine/server/server_filter.cpp`.
- [x] Add a legacy adapter that supplies current time and translates legacy
  filter records without moving command handlers yet.
  Evidence: `engine/server/server_filter_adapter.h`,
  `engine/server/server_filter_adapter.cpp`.
- [x] Route the smallest safe part of `sv_filter.c` through the adapter.
  Evidence: `engine/server/sv_filter.c`.
- [x] Run focused tests, `.\waf.bat build --alltests`, and a runtime smoke.
  Evidence: `.\waf.bat build --targets=test_engine_server_filter` passed 1/1;
  `.\waf.bat build --alltests` passed 65/65; Windows runtime smoke reached
  first frame in 0.512 seconds and quit by command on May 10 2026.

## Phase 54: Server Query Response Builder

- [x] Capture byte-level baseline payloads for details, rules, and players.
  Evidence: `Documentation/codex/legacy/engine/source-query-baseline.md`.
- [x] Add tests for password-protected player-list suppression and protected
  cvar value masking.
  Evidence: `tests/engine/source_query.cpp`.
- [x] Implement source-query value rows and response builder under
  `src/engine/server`.
  Evidence: `src/include/engine/server/source_query.hpp`,
  `src/engine/server/source_query.cpp`.
- [x] Keep `NET_SendPacket` and live server-state reads in the legacy adapter.
  Evidence: `engine/server/source_query_adapter.h`,
  `engine/server/source_query_adapter.cpp`, `engine/server/sv_query.c`.
- [x] Run focused golden tests, `.\waf.bat build --alltests`, and a runtime
  smoke.
  Evidence: `.\waf.bat build --targets=test_engine_source_query` passed 1/1;
  `.\waf.bat build --alltests` passed 66/66; Windows runtime smoke copied the
  rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.424 seconds, and stopped with reason `command` at
  May 10 2026 14:15:26 local time.

## Phase 55: Server User-Agent And Input Policy

- [x] Capture current `SV_ProcessUserAgent()` behavior.
  Evidence: `Documentation/codex/legacy/engine/user-agent-policy-baseline.md`.
- [x] Implement target-neutral user-agent validation inputs and result codes.
  Evidence: `src/include/engine/server/user_agent_policy.hpp`,
  `src/engine/server/user_agent_policy.cpp`.
- [x] Add tests for valid/invalid UUIDs, banned IDs, missing input-device
  lists, and touch/mouse/joystick/VR disallow cases.
  Evidence: `tests/engine/user_agent_policy.cpp`.
- [x] Route `SV_ProcessUserAgent()` through the modern validator while keeping
  `SV_RejectConnection()`, cvar reads, and `SV_CheckID()` legacy-owned.
  Evidence: `engine/server/user_agent_policy_adapter.h`,
  `engine/server/user_agent_policy_adapter.cpp`, `engine/server/sv_main.c`.
- [x] Run focused tests, `.\waf.bat build --alltests`, and a runtime smoke.
  Evidence: `.\waf.bat build --targets=test_engine_user_agent_policy` passed
  1/1; `.\waf.bat build --alltests` passed 67/67; Windows runtime smoke copied
  the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.426 seconds, and stopped with reason `command` at
  May 10 2026 14:20:42 local time.

## Phase 56: Legacy NetAPI Query String Builder

- [x] Baseline short `SV_Info()` and long `SV_BuildNetAnswer()` behavior.
  Evidence: `Documentation/codex/legacy/engine/netapi-info-baseline.md`.
- [x] Implement a sibling NetAPI info-string builder, reusing Phase 54 protected
  value masking where appropriate.
  Evidence: `src/include/engine/server/netapi_info.hpp`,
  `src/engine/server/netapi_info.cpp`.
- [x] Add tests for protected cvar masking, errors, details, players, ping, and
  short server-info key order.
  Evidence: `tests/engine/netapi_info.cpp`.
- [x] Route string construction through modern helpers while keeping
  `Netchan_OutOfBandPrint()` and live state reads legacy-owned.
  Evidence: `engine/server/netapi_info_adapter.h`,
  `engine/server/netapi_info_adapter.cpp`, `engine/server/sv_client.c`.
- [x] Run focused tests, `.\waf.bat build --alltests`, and a runtime smoke.
  Evidence: `.\waf.bat build --targets=test_engine_netapi_info` passed 1/1;
  `.\waf.bat build --alltests` passed 68/68; Windows runtime smoke copied the
  rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.437 seconds, and stopped with reason `command` at
  May 10 2026 14:29:55 local time.

## Phase 57: Server Connectionless Command Classifier

- [x] Baseline `SV_ConnectionlessPacket()` parsing and dispatch order.
  Evidence: `Documentation/codex/legacy/engine/connectionless-packet-baseline.md`.
- [x] Implement a target-neutral classifier for full-line and first-token
  command decisions.
  Evidence: `src/include/engine/server/connectionless_classifier.hpp`,
  `src/engine/server/connectionless_classifier.cpp`.
- [x] Add tests for source-query exact match, loose `U`/`V` first-character
  routing, ping/ack aliases, master-server commands, uninitialized `rcon`, and
  game-DLL fallback.
  Evidence: `tests/engine/connectionless_classifier.cpp`.
- [x] Route `SV_ConnectionlessPacket()` through the adapter while keeping
  message reads, logging, handlers, and packet sends legacy-owned.
  Evidence: `engine/server/connectionless_classifier_adapter.h`,
  `engine/server/connectionless_classifier_adapter.cpp`,
  `engine/server/sv_client.c`.
- [x] Run focused tests, full tests, and a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_connectionless_classifier`
  passed 1/1; `.\waf.bat build --alltests` passed 69/69; Windows runtime
  smoke copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.436 seconds, and stopped with reason `command` at
  May 10 2026 14:37:01 local time.

## Phase 58: Server Event Log Formatter

- [x] Baseline `sv_log.c` timestamp prefix, stock messages, and sink ownership.
  Evidence: `Documentation/codex/legacy/engine/server-event-log-baseline.md`.
- [x] Implement target-neutral server event log formatting helpers.
  Evidence: `src/include/engine/server/server_event_log.hpp`,
  `src/engine/server/server_event_log.cpp`.
- [x] Add tests for timestamped lines, cvar rows, start/end rows, log-file
  start/close rows, and truncation termination.
  Evidence: `tests/engine/server_event_log.cpp`.
- [x] Route `sv_log.c` stock message and timestamp line formatting through the
  adapter while keeping files, console echo, UDP, time, cvars, and commands
  legacy-owned.
  Evidence: `engine/server/server_event_log_adapter.h`,
  `engine/server/server_event_log_adapter.cpp`, `engine/server/sv_log.c`.
- [x] Run focused tests, full tests, and a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_server_event_log`
  passed 1/1; `.\waf.bat build --alltests` passed 70/70; Windows runtime
  smoke copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.422 seconds, and stopped with reason `command` at
  May 10 2026 14:42:16 local time.

## Deferred Server Items

- [ ] Phase 59: challenge and rejection response formatting in `sv_client.c`,
  keeping challenge generation and packet sends legacy-owned.
- [ ] Phase 60: client command dispatch table lookup in `sv_client.c`, keeping
  command handlers and client mutation legacy-owned.
- [ ] Server event logging service after console/log sink ownership is clearer.
- [ ] Declarative server command registration after filter/query pilots.
- [ ] Save/restore migration after binary compatibility fixtures exist.
- [ ] Game DLL bridge migration after explicit ABI and licensing review.
- [ ] Physics/world migration after movement and trace fixtures exist.
