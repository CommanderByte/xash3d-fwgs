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

## Phase 59: Server Challenge And Rejection Response Formatter

- [x] Baseline `SV_SendChallenge()`, `SV_RejectConnection()`, and stock
  `SV_ConnectClient()` rejection text.
  Evidence: `Documentation/codex/legacy/engine/connection-response-baseline.md`.
- [x] Document the boundary: response text is target-neutral, while challenge
  generation, address handling, validation, reporting, and packet sends remain
  legacy-owned.
  Evidence: `Documentation/codex/modern/engine/connection-response-migration.md`.
- [x] Add tests for challenge response formatting, the three rejection packet
  bodies, console report text, null safety, and truncation termination.
  Evidence: `tests/engine/connection_response.cpp`.
- [x] Route `SV_SendChallenge()` and `SV_RejectConnection()` through the adapter
  while keeping side effects and validation legacy-owned.
  Evidence: `engine/server/connection_response_adapter.h`,
  `engine/server/connection_response_adapter.cpp`, `engine/server/sv_client.c`.
- [x] Run focused tests, full tests, and a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_connection_response`
  passed 1/1; `.\waf.bat build --alltests` passed 71/71; Windows runtime
  smoke copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.427 seconds, and stopped with reason `command` at
  May 10 2026 14:46:31 local time.

## Phase 60: Server Client Command Dispatch Table

- [x] Baseline `SV_ExecuteClientCommand()` built-in, enttools, fullupdate, and
  game DLL fallback routing.
  Evidence: `Documentation/codex/legacy/engine/client-command-dispatch-baseline.md`.
- [x] Implement a target-neutral command metadata table and first routing
  decision helper.
  Evidence: `src/include/engine/server/client_command_dispatch.hpp`,
  `src/engine/server/client_command_dispatch.cpp`.
- [x] Add tests for exact/case-sensitive lookup, built-ins before active-server
  gating, enttools gates, fullupdate throttling, and game DLL fallback.
  Evidence: `tests/engine/client_command_dispatch.cpp`.
- [x] Route `SV_ExecuteClientCommand()` through the adapter while keeping
  command handlers, logging, game DLL calls, and client mutation legacy-owned.
  Evidence: `engine/server/client_command_dispatch_adapter.h`,
  `engine/server/client_command_dispatch_adapter.cpp`,
  `engine/server/sv_client.c`.
- [x] Run focused tests, full tests, runtime smoke, and manual launch.
  Evidence: `.\waf.bat build --targets=test_engine_client_command_dispatch`
  passed, `.\waf.bat build --alltests` passed 72/72 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.414 seconds before stopping with reason `command` at May 10 2026
  14:55 local time. A visible `run-win32\xash3d.exe -dev 2 -log` game process
  was launched for manual new-game validation.

## Phase 61: Custom Resource And Download Boundary Audit

- [x] Audit custom resource ownership across `engine/common/custom.c`,
  `engine/server/sv_custom.c`, and `SV_DownloadFile_f()`.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Identify the extractable target-neutral resource identity and download
  policy pieces.
  Evidence: `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] Capture baseline notes for custom MD5 names, HPAK temp-file behavior,
  download allow/fail rules, precache checks, and upload queue flow.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Define follow-up migration slices and test fixtures.
  Evidence: `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.

## Phase 62: Custom Resource Identity Helpers

- [x] Baseline resource descriptor comparison, hash/key formatting, and custom
  resource lookup helpers.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`,
  `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] Implement target-neutral resource identity helpers.
  Evidence: `src/include/engine/server/resource_identity.hpp`,
  `src/engine/server/resource_identity.cpp`.
- [x] Add tests for `!MD5` names, safe download names, sound-resource matching,
  resource lookup, type names, and size summaries.
  Evidence: `tests/engine/resource_identity.cpp`.
- [x] Route the smallest safe legacy caller through the helper.
  Evidence: `engine/common/custom_resource_identity_adapter.h`,
  `engine/common/custom_resource_identity_adapter.cpp`,
  `engine/common/custom.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_resource_identity` passed
  1/1, `.\waf.bat build --alltests` passed 73/73, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.427 seconds before stopping with reason `command` at May 10 2026
  15:58 local time.

## Deferred Server Items

- [ ] Phase 63: server download decision policy helper after resource identity
  fixtures exist.
- [ ] Phase 64: client resource upload queue helper after identity/download
  policy fixtures exist.
- [ ] Phase 65: resource message serialization helper using golden row tests.
- [ ] Phase 66: consistency resource policy helper after resource message
  encoding is documented.
- [ ] Server event logging service after console/log sink ownership is clearer.
- [ ] Declarative server command registration after filter/query pilots.
- [ ] Save/restore migration after binary compatibility fixtures exist.
- [ ] Game DLL bridge migration after explicit ABI and licensing review.
- [ ] Physics/world migration after movement and trace fixtures exist.
