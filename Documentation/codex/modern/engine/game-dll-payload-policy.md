# Game DLL Payload Policy

Phase 92 extracts game-DLL-facing payload and callback routing decisions while
leaving live engine buffers and entity state in the legacy adapter.

## Legacy Baseline

- `SV_StartSound()` chooses `MSG_INIT`, `MSG_ALL`, or reliable PAS multicast
  based on spawning flags, static channel, Quake compatibility, client count,
  stop-sound overrides, and prediction-filter flags.
- `pfnEmitAmbientSound()` adds `SND_SPAWNING` during server loading, sends
  spawning ambients to `MSG_INIT`, otherwise broadcasts, and lets stop-sound
  override back to `MSG_ALL`.
- `pfnParticleEffect()` silently skips when the unreliable datagram has fewer
  than 16 bytes left, clamps direction components to signed-byte range after
  multiplying by 16, and truncates count/color to bytes.
- `pfnLightStyle()` clamps negative styles to zero, treats styles beyond
  `MAX_LIGHTSTYLES` as fatal, and suppresses world lightstyle writes while a
  loadgame restore is active.
- `pfnStaticDecal()` always routes game DLL static decals through the signon
  buffer with `FDECAL_PERMANENT` and scale `1.0`.
- `pfnMakeStatic()` ignores invalid entities before asking the game DLL to
  capture a baseline; static entity capacity and delta writing remain in the
  existing static-message adapter.

## Modern Boundary

`src/engine/server/game_dll/game_dll_payload_policy.cpp` owns pure decisions for:

- sound multicast destinations and prediction filtering;
- ambient-sound spawning flag propagation;
- particle-buffer admission and component encoding;
- lightstyle set/skip/fatal decisions;
- static-decal default flags and scale;
- `MakeStatic` invalid-entity admission.

`engine/server/game_dll_payload_policy_adapter.cpp` exposes those plans to
`sv_game.c`. The legacy server still owns:

- `sv.multicast`, `sv.signon`, and `sv.datagram`;
- `SV_Multicast()`, `SV_BuildSoundMsg()`, and `SV_CreateDecal()`;
- `svs.static_entities`, baselines, and edict lifetime;
- `SV_SetLightStyle()` and `Host_Error()`;
- resource indices and model lookups.

Existing modern helpers still write the sound and BSP-decal payload bytes; this
phase adds the missing callback-level routing and admission layer above them.
