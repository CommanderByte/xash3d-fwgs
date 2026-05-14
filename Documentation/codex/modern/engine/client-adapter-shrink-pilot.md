# Client Adapter Shrink Pilot

Phase 155 reduces repeated client/session adapter glue after Phase 154 proves
the helpers compose cleanly as a domain.

## Target

The safe target is scalar adapter conversion:

- legacy C `int` truth values to modern `bool`;
- modern `bool` results to legacy `0`/`1`;
- modern enum class values to legacy C `int` or C enum values;
- bounded `std::size_t` payload lengths back to legacy signed `int` returns.

This is mechanical translation only. It does not own client state, packet
buffers, transfers, movement, or query sends.

## Shared Helper

`engine/server/client_adapter_shared.hpp` provides:

- `FromLegacyBool(int)`;
- `ToLegacyBool(bool)`;
- `ToLegacyEnum(Enum)`;
- `ToLegacySize(std::size_t)`.

The helper lives in the legacy adapter tree because it exists to bridge C
adapter signatures to modern C++ helpers. It is intentionally separate from the
game DLL adapter helper, because the client/session boundary has its own
ownership and validation path.

## Adapter Updates

The pilot routes scalar conversions through the shared helper in:

- `client_command_dispatch_adapter.cpp`;
- `client_policy_adapter.cpp`;
- `client_session_slots_adapter.cpp`;
- `connection_response_adapter.cpp`;
- `connectionless_classifier_adapter.cpp`;
- `netapi_info_adapter.cpp`;
- `remote_admin_command_adapter.cpp`;
- `source_query_adapter.cpp`.

Enum layout dependencies are guarded with `static_assert`s where the legacy C
constants must match modern enum ordinals.

## Boundaries Kept Separate

Phase 155 deliberately avoids:

- a broad `sv_client.c` facade;
- netchan or transfer ownership;
- movement, voice, or download/upload side effects;
- query packet send ownership;
- live client or edict mutation.

## Validation

Focused validation passed 11/11:

- `test_engine_client_adapter_shared`;
- `test_engine_client_session_domain`;
- `test_engine_client_command_dispatch`;
- `test_engine_client_policy`;
- `test_engine_client_session_slots`;
- `test_engine_connection_response`;
- `test_engine_connectionless_classifier`;
- `test_engine_remote_admin_command`;
- `test_engine_server_challenge_policy`;
- `test_engine_source_query`;
- `test_engine_netapi_info`.

Full validation and smoke timing also passed:

- `.\waf.bat build --alltests` passed 139/139.
- `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_client_adapter_shared -SkipFullTests -AllowSmokeNonZeroExit
  -StopRunningXash` passed.
- Smoke reached first frame in 0.501 seconds and stopped with reason
  `command`.
