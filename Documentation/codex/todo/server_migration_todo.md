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

## Deferred Server Items

- [ ] Phase 56 candidate: legacy NetAPI info/rules/player string responses in
  `sv_client.c::SV_Info()`, especially the duplicated details path that says it
  should match `SV_SourceQuery_Details`.
- [ ] Server event logging service after console/log sink ownership is clearer.
- [ ] Declarative server command registration after filter/query pilots.
- [ ] Save/restore migration after binary compatibility fixtures exist.
- [ ] Game DLL bridge migration after explicit ABI and licensing review.
- [ ] Physics/world migration after movement and trace fixtures exist.
