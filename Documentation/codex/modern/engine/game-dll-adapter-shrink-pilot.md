# Game DLL Adapter Shrink Pilot

Phase 153 shrinks one mechanical game DLL adapter pattern after the Phase 152
map confirmed which boundaries are safe to share.

## Target

The pilot targets scalar adapter conversion in the client-info and output
game DLL adapters:

- legacy C truth values to modern `bool`;
- modern enum class actions back to legacy C `int`;
- explicit layout checks between legacy constants and modern enum ordinals.

This is intentionally smaller than a bridge facade. The adapter entry points
remain the same, and `sv_game.c` continues to branch on the same legacy enum
constants.

## Shared Helper

`engine/server/game_dll_adapter_shared.hpp` provides:

- `FromLegacyBool(int)`;
- `ToLegacyBool(bool)`;
- `ToLegacyEnum(Enum)`.

The helper owns no game DLL state and performs no side effects. It only removes
repeated conversion glue from adapters where tests prove the ordinal mapping is
intentional.

## Adapter Updates

The pilot updates:

- `engine/server/game_dll_client_info_policy_adapter.cpp`;
- `engine/server/game_dll_output_policy_adapter.cpp`.

`game_dll_client_info_policy_adapter.cpp` now uses `static_assert`s to pin each
client-info C constant to its modern enum value before using `ToLegacyEnum()`.
`game_dll_output_policy_adapter.cpp` already had external constant checks and
now routes bool/enum conversions through the shared helper.

## Boundaries Kept Separate

Phase 153 does not move:

- `gEngfuncs` table construction;
- DLL load/unload or API negotiation;
- live client/userinfo mutation;
- output sinks, command buffers, disconnect, or credits behavior;
- edict, message buffer, resource, trace, PMove, or string-pool ownership.

## Validation

Validation results:

- `.\waf.bat build --targets=test_engine_game_dll_adapter_shared,test_engine_game_dll_bridge_domain,test_engine_game_dll_client_info_policy,test_engine_game_dll_output_policy`
  passed 4/4.
- `.\waf.bat build --alltests` passed 138/138.
- `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_game_dll_adapter_shared -SkipFullTests -AllowSmokeNonZeroExit
  -StopRunningXash` passed.
- Smoke reached first frame in 0.512 seconds and stopped with reason
  `command`.
