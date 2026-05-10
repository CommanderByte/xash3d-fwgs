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

## Phase 63: Server Download Policy Helper

- [x] Baseline `SV_DownloadFile_f()` decision ordering.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`,
  `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] Implement target-neutral allow/reject/send/logo download policy.
  Evidence: `src/include/engine/server/server_download_policy.hpp`,
  `src/engine/server/server_download_policy.cpp`.
- [x] Add tests for unsafe paths, disabled downloads, precache misses, model
  sidecar downloads, and custom logo names.
  Evidence: `tests/engine/server_download_policy.cpp`.
- [x] Route `SV_DownloadFile_f()` through the helper while keeping filesystem
  probes, HPAK reads, fail messages, and netchan fragments legacy-owned.
  Evidence: `engine/server/server_download_policy_adapter.h`,
  `engine/server/server_download_policy_adapter.cpp`, `engine/server/sv_client.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_download_policy`
  passed 1/1, `.\waf.bat build --alltests` passed 74/74, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.412 seconds before stopping with reason `command` at May 10 2026
  16:47 local time.

## Phase 64: Client Resource Upload Queue Helper

- [x] Baseline `SV_ParseResourceList()`, `SV_EstimateNeededResources()`,
  `SV_BatchUploadRequest()`, and `SV_CheckFile()` ordering.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`,
  `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] Implement target-neutral upload queue decisions for descriptor validity,
  rate limiting, missing decal estimation, upload limits, and batch actions.
  Evidence: `src/include/engine/server/server_upload_queue.hpp`,
  `src/engine/server/server_upload_queue.cpp`.
- [x] Add tests for invalid descriptors, too-frequent updates, missing decals,
  disabled uploads, max-upload rejection, and batch action selection.
  Evidence: `tests/engine/server_upload_queue.cpp`.
- [x] Route legacy upload queue decisions through the helper while keeping
  `MSG_*`, allocation, HPAK probes, upload command emission, and client/list
  mutation legacy-owned.
  Evidence: `engine/server/server_upload_queue_adapter.h`,
  `engine/server/server_upload_queue_adapter.cpp`, `engine/server/sv_client.c`,
  `engine/server/sv_custom.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_upload_queue`
  passed 1/1, `.\waf.bat build --alltests` passed 75/75, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.410 seconds before stopping with reason `command` at May 10 2026
  17:06 local time.

## Phase 65: Resource Message Serialization Helper

- [x] Baseline `SV_SendResource()` and `SV_SendResources()` row and wrapper
  ownership.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Implement a target-neutral resource-row encoder on top of modern
  `NetworkBitBuffer`.
  Evidence: `src/include/engine/server/server_resource_message.hpp`,
  `src/engine/server/server_resource_message.cpp`.
- [x] Add golden tests for plain rows, custom hashes, reserved payloads, signed
  sizes, and overflow.
  Evidence: `tests/engine/server_resource_message.cpp`.
- [x] Route `SV_SendResource()` through the helper while keeping server command
  bytes, resource-location messages, counts, consistency records, and netchan
  delivery legacy-owned.
  Evidence: `engine/server/server_resource_message_adapter.h`,
  `engine/server/server_resource_message_adapter.cpp`, `engine/server/sv_custom.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_resource_message`
  passed 1/1, `.\waf.bat build --alltests` passed 76/76, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.404 seconds before stopping with reason `command` at May 10 2026
  17:18 local time.

## Phase 66: Customization Message Serialization Helper

- [x] Baseline `SV_SendCustomization()` payload and legacy ownership.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Implement a target-neutral customization payload encoder on top of
  modern `NetworkBitBuffer`.
  Evidence: `src/include/engine/server/server_customization_message.hpp`,
  `src/engine/server/server_customization_message.cpp`.
- [x] Add golden tests for custom and non-custom payloads, signed fields, and
  overflow.
  Evidence: `tests/engine/server_customization_message.cpp`.
- [x] Route `SV_SendCustomization()` through the helper while keeping
  `svc_customization`, client selection, and netchan message ownership legacy.
  Evidence: `engine/server/server_customization_message_adapter.h`,
  `engine/server/server_customization_message_adapter.cpp`,
  `engine/server/sv_custom.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_customization_message`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 77/77, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.408 seconds before stopping with reason `command` at May 10 2026
  17:40 local time.

## Phase 67: Consistency List Serialization Helper

- [x] Baseline `SV_SendConsistencyList()` enable/disable conditions, delta
  encoding, absolute-index fallback, and terminator behavior.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Implement a target-neutral consistency-list encoder for resource-index
  snapshots.
  Evidence: `src/include/engine/server/server_consistency_list.hpp`,
  `src/engine/server/server_consistency_list.cpp`.
- [x] Add golden tests for disabled paths, enabled empty lists, small deltas,
  large deltas, reader roundtrip, and overflow.
  Evidence: `tests/engine/server_consistency_list.cpp`.
- [x] Route `SV_SendConsistencyList()` through the helper while keeping client
  flags, cvars, `resource_t`, and message destination ownership legacy.
  Evidence: `engine/server/server_consistency_list_adapter.h`,
  `engine/server/server_consistency_list_adapter.cpp`,
  `engine/server/sv_custom.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_consistency_list`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 78/78, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.421 seconds before stopping with reason `command` at May 10 2026
  17:45 local time.

## Phase 68: Consistency Resource Policy

- [x] Baseline `SV_TransferConsistencyInfo()` and
  `SV_ParseConsistencyResponse()` exact-file, same-bounds, specified-bounds,
  invalid type, and bad-resource behavior.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Implement target-neutral consistency request/response policy helpers
  that consume adapter-provided MD5 prefixes and bounds snapshots.
  Evidence: `src/include/engine/server/server_consistency_policy.hpp`,
  `src/engine/server/server_consistency_policy.cpp`.
- [x] Add tests for setup gates, MD5-prefix comparison, bounds reservation,
  same/spec bounds validation, invalid force types, and response count checks.
  Evidence: `tests/engine/server_consistency_policy.cpp`.
- [x] Route consistency policy decisions through the helper while keeping file
  hashing, model bounds probes, client drops, messages, and game DLL callbacks
  legacy-owned.
  Evidence: `engine/server/server_consistency_policy_adapter.h`,
  `engine/server/server_consistency_policy_adapter.cpp`,
  `engine/server/sv_custom.c`.
- [x] Run focused tests, full tests, runtime smoke, and record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_consistency_policy`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 79/79, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.409 seconds before stopping with reason `command` at May 10 2026
  17:52 local time.

## Phase 69: Server Resource Catalog Builder

Goal: turn the resource-list construction rules into a target-neutral planner
before touching the global `sv.resources` list directly.

- [x] Baseline `SV_AddResource()`, `SV_CreateResourceList()`,
  `SV_DetermineResourceType()`, and the ordering of generic, sound, model,
  decal, and event resources.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] Implement a modern resource catalog builder that takes snapshots of
  precache entries, flags, indexes, types, and adapter-provided file sizes.
  Evidence: `src/include/engine/server/server_resource_catalog.hpp`,
  `src/engine/server/server_resource_catalog.cpp`.
- [x] Keep filesystem probes, console output, `sv.resources` mutation, and
  `MAX_RESOURCE_LIST` enforcement in the adapter unless the boundary becomes
  clearly cleaner during implementation.
  Evidence: `engine/server/server_resource_catalog_adapter.h`,
  `engine/server/server_resource_catalog_adapter.cpp`,
  `engine/server/sv_init.c`.
- [x] Add tests for empty precache entries, `!` sound sentinels, model wildcard
  resources, signed sizes, index propagation, flags, and stable output order.
  Evidence: `tests/engine/server_resource_catalog.cpp`.
- [x] Route the resource entry planning through the helper and verify focused
  tests, full tests, and `+wait +wait` smoke timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_resource_catalog`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 80/80 tests, and the fresh
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` smoke copied
  `build\engine\xash.dll` into `run-win32`, ran with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.413 seconds, and stopped with reason `command` at
  May 10 2026 18:07 local time.

## Phase 70: Hot Resource Announcement

Goal: separate the single-resource announcement decision from reliable datagram
delivery.

- [ ] Baseline `SV_SendSingleResource()` for models, sounds, generic files,
  file-size lookup, path prefixing, and `svc_resource` emission.
- [ ] Implement a hot-resource announcement planner that builds the exact
  `resource_t` snapshot needed by `SV_SendResource()`.
- [ ] Keep `FS_FileSize()`, reliable datagram ownership, and final
  `SV_SendResource()` delivery legacy-owned.
- [ ] Add tests for model `*` resources, sound path prefixing, empty names,
  generic files, flags, indexes, and size preservation.
- [ ] Route `SV_SendSingleResource()` through the helper and verify focused
  tests, full tests, and `+wait +wait` smoke timing.

## Phase 71: Server Reslist File Policy

Goal: isolate `.res` and `reslist.txt` token classification without moving
legacy file loading or parse ownership yet.

- [ ] Baseline `SV_ReadResourceList()` behavior for safe-download filtering,
  slash normalization, sound classification, generic fallback, and console
  diagnostics.
- [ ] Implement a reslist token classifier that returns normalized path,
  resource type, and intended index route.
- [ ] Keep `FS_LoadFile()`, `COM_ParseFile()`, logging, `SV_SoundIndex()`, and
  `SV_GenericIndex()` legacy-owned.
- [ ] Add tests for empty tokens, unsafe paths, backslash input, `sound/`
  prefixes, supported sound extensions, unsupported sound files, and generic
  fallback.
- [ ] Route classification through the helper and verify focused tests, full
  tests, and `+wait +wait` smoke timing.

## Phase 72: Client Userinfo Update Message

Goal: move the `svc_updateuserinfo` payload shape into modern tested code while
preserving legacy userinfo mutation and hashing.

- [x] Baseline `SV_FullClientUpdate()` for named clients, unnamed clients,
  client indexes, user IDs, sanitized info strings, and hashed CD key payloads.
- [x] Implement a target-neutral update-userinfo encoder that consumes
  adapter-provided sanitized userinfo and digest bytes.
- [x] Keep `SV_UserinfoChanged()`, `Info_RemovePrefixedKeys()`, MD5
  calculation, and destination message ownership legacy-owned unless a smaller
  extraction proves safe.
- [x] Add golden tests for name-present bit behavior, unnamed clients, digest
  emission, exact byte layout, and overflow handling.
- [x] Route serialization through the helper and verify focused tests, full
  tests, and `+wait +wait` smoke timing.

## Phase 73: Small Server Service Messages

Goal: remove a set of tiny service-message writers from legacy code without
creating a broad serverdata rewrite.

- [ ] Baseline `SV_FailDownload()`, `SV_BuildReconnect()`,
  `SV_UpdateClientView()`, `SV_TogglePause()`, and `SV_WriteVoiceCodec()`.
- [ ] Implement target-neutral encoders for `svc_filetxferfailed`,
  `svc_stufftext reconnect`, `svc_setview`, `svc_setpause`, and
  `svc_voiceinit`.
- [ ] Keep cvars, state checks, client selection, and destination buffer
  ownership in legacy code.
- [ ] Add golden tests for command bytes, strings, signed fields, fallback
  codec behavior, and overflow handling.
- [ ] Route the selected writers through adapters and verify focused tests,
  full tests, and `+wait +wait` smoke timing.

## Phase 74: Server Voice Relay Policy

Goal: extract the voice relay decision tree before attempting a larger client
message or audio subsystem migration.

- [ ] Baseline `SV_ParseVoiceData()` for loopback, frame count, size limits,
  voice enable gates, spawned-client gates, physics callback behavior,
  listener masks, and per-recipient datagram-capacity checks.
- [ ] Implement a target-neutral voice relay policy helper and add a
  `svc_voicedata` payload writer only if the boundary stays small.
- [ ] Keep message reads, `SV_Physics()->pfnVoice_SetClientListening()`,
  recipient iteration, and datagram writes legacy-owned.
- [ ] Add tests for oversized packets, disabled voice, loopback to sender,
  listener-mask filtering, single-player suppression, and capacity rejection.
- [ ] Route relay decisions through the helper and verify focused tests, full
  tests, and `+wait +wait` smoke timing.

## Deferred Server Items

- [ ] Server event logging service after console/log sink ownership is clearer.
- [ ] Declarative server command registration after filter/query pilots.
- [ ] Serverdata handshake migration after the smaller service-message writers
  are stable and a baseline fixture exists.
- [ ] Save/restore migration after binary compatibility fixtures exist.
- [ ] Game DLL bridge migration after explicit ABI and licensing review.
- [ ] Physics/world migration after movement and trace fixtures exist.
