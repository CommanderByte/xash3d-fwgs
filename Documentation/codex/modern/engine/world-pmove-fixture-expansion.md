# World And PMove Fixture Expansion

Phase 156 strengthened the fixture layer before moving any more live world or
PMove ownership out of legacy code.

## Scope

The world trace fixture now models the safe, value-only setup decision for
`SV_Move`: encoded move type unpacking, transparent-entity ignore bits,
Quake-compatible monsterclip suppression, and missile bounds selection.

The PMove fixture now includes a setup/finish comparison plan that can compare
safe fields across `SV_SetupPMove` and `SV_FinishPMove`-style flows without
requiring live `playermove_t`, physent storage, callback replay, or edict
state.

## Boundary

These fixtures intentionally do not own:

- hull traversal or BSP/edict collision state
- portal clipping
- PMove callback dispatch
- physent, visent, or moveent population
- touch replay
- game DLL callback invocation
- global trace mutation

That keeps the fixtures useful for small policy extractions without pretending
they are an executable replacement for the legacy runtime.

## Validation

- `.\waf.bat build --targets=test_engine_server_world_trace_policy,test_engine_world_trace_fixtures,test_engine_pmove_usercmd_fixtures,test_engine_server_world_link_policy,test_engine_server_pmove_bridge_policy,test_engine_server_movement_constraints` passed.
- `.\waf.bat build --alltests` passed 140/140 tests.
- `.\scripts\run-phase-validation.ps1 -FocusedTarget test_engine_server_world_trace_policy -SkipFullTests -AllowSmokeNonZeroExit -StopRunningXash` passed; first frame was 0.497 seconds.
