# Client Session Domain Pilot

Phase 140 groups the target-neutral client/session helpers under
`src/engine/server/client/` and `src/include/engine/server/client/`.
The flat headers under `src/include/engine/server/` remain forwarding headers
so adapters and focused tests can keep their includes during the transition.

## Domain Shape

The client domain currently owns plain-value helpers for:

- connectionless command classification and reject/challenge response text;
- challenge-window admission;
- user-agent UUID/input-device admission policy;
- client session slot counting and first-free-slot selection;
- userinfo penalty, rate/update interval, and private client flag snapshots;
- client command route classification;
- timeout and pause-release policy;
- remote admin authentication and quoted command reconstruction.

These helpers are intentionally narrow. They model decisions that can be
tested without `client_t`, `edict_t`, netchan buffers, cvar storage, or live
command execution.

## Neighbor Domains

Some client-facing helpers deliberately stay outside this folder:

- Transfer/download/upload helpers remain in `resources/` because their
  behavior is resource-list and filesystem-policy heavy.
- Voice relay and userinfo message serialization remain in `messaging/`
  because they are packet payload builders.
- PMove bridge policy remains flat for now because it spans client sessions,
  movement, world traces, and user command execution.
- Source-query and NetAPI response builders remain query/info helpers rather
  than client-session helpers.

## Compatibility Boundaries

The pilot keeps these legacy-owned:

- `SV_ConnectClient`, `SV_DropClient`, `SV_New_f`, `SV_Spawn_f`, and actual
  `client_t` state mutation.
- Netchan sends, redirect state, packet reads, and voice-packet reads.
- Command execution, game DLL callbacks, and cvar-query callback dispatch.
- Resource-list mutation and HPAK/filesystem probes.

## Test Coverage

`tests/engine/client_session_domain.cpp` uses the canonical
`engine/server/client/...` headers and checks that representative admission,
challenge, user-agent, remote-admin, session-slot, userinfo, client-command,
rate, and timeout decisions compose as plain values. Focused tests still own
the detailed edge-case coverage for each helper.
