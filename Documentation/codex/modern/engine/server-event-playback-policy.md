# Server Event Playback Policy

Phase 111 adds the first target-neutral helper behind server-side event
playback. The helper deliberately owns decisions, not the event system itself.

## Modern Helper

The policy lives in:

- `src/include/engine/server/server_event_playback_policy.hpp`
- `src/engine/server/server_event_playback_policy.cpp`

It covers these pure decisions:

- reject server playback when `FEV_CLIENT` is set;
- normalize event flags by adding `FEV_SERVER`, clearing `FEV_NOTHOST` and
  `FEV_HOSTONLY` for non-client invokers, and clamping negative delay;
- identify reliable delivery;
- decide whether a recipient should receive an event from plain facts about
  spawned state, edict presence, fake-client status, group pass, visibility
  pass, local-weapons state, current-client match, and invoker-client match;
- select the correct unreliable event queue slot, including `FEV_UPDATE`
  replacement behavior;
- clamp queued event emission count to the legacy `MAX_EVENT_QUEUE / 2 - 1`
  cap.

## Legacy Adapter

The C adapter lives in:

- `engine/server/server_event_playback_policy_adapter.h`
- `engine/server/server_event_playback_policy_adapter.cpp`

The adapter lets `sv_game.c` and `sv_frame.c` keep using legacy structures while
delegating decisions to the modern helper. It does not expose C++ types to the
legacy server sources.

## Routed Legacy Owners

Phase 111 routes only low-risk decision points:

- `sv_game.c`: server/client event admission, flag normalization, reliable
  delivery selection, recipient admission, and unreliable queue slot selection.
- `sv_frame.c`: queued event emission count clamping.

These remain legacy-owned:

- game DLL and PMove callback ABI;
- event precache validation and diagnostics;
- `edict_t`, `sv_client_t`, `sv`, `svs`, and `svgame` ownership;
- PVS/PHS mask construction and visibility checks;
- event argument mutation and event queue mutation;
- `MSG_*` and `MSG_WriteDeltaEvent()` serialization.

## Test Coverage

`tests/engine/server_event_playback_policy.cpp` covers:

- `FEV_CLIENT` server rejection;
- reliable flag detection;
- `FEV_SERVER` addition and delay clamping;
- host-only/not-host clearing for non-client invokers;
- recipient rejection for unspawned, missing-edict, fake-client, failed group,
  and failed visibility cases;
- the absence of a direct HLTV/spectator-specific gate in this path;
- `FEV_NOTHOST` suppression only when local weapons are active and the
  recipient is current or invoker;
- `FEV_HOSTONLY` recipient restriction;
- `FEV_UPDATE` queue-slot replacement behavior;
- first-empty-slot fallback and full-queue drop behavior;
- queued event emit-count clamping.

## Validation

Phase validation passed with:

- focused target: `test_engine_server_event_playback_policy`;
- engine target: `xash`;
- full tests: 116/116;
- smoke command: `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit`;
- first frame: 0.511 seconds.
