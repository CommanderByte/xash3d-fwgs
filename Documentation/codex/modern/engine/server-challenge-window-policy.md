# Server Challenge Window Policy

Phase 102 narrows the existing challenge migration from Phase 59. Phase 59
already moved challenge and rejection response text formatting into
target-neutral helpers. This phase moves only the challenge time-window math.

## Legacy Baseline

`engine/server/sv_client.c` keeps the challenge hash and connection effects:

- `SV_GetChallenge()` hashes the remote address, `svs.challenge_salt`, and a
  32-bit time window with MD5.
- `SV_SendChallenge()` used `host.realtime / 5`, truncated to `uint32_t`, as
  the issued challenge window.
- `SV_CheckChallenge()` accepts the current window and the previous window.
  At server time zero the previous window intentionally wraps from `0` to
  `UINT32_MAX`, matching the old unsigned `time_window - 1` expression.
- A challenge issued just after a window boundary can remain valid for almost
  two windows. With the five-second legacy window, the maximum lifetime is
  just under ten seconds.

## Modern Boundary

The modern helper owns:

- converting server realtime seconds to the challenge time-window number;
- deriving the accepted current/previous window pair;
- testing whether an issued window belongs to that pair.

The legacy server still owns:

- remote address normalization;
- challenge salt storage;
- MD5 input order and byte representation;
- `Netchan_OutOfBandPrint()` delivery;
- rejection output and connection side effects.

## Route-Through

`engine/server/server_challenge_policy_adapter.*` exposes a tiny C adapter for
the two live calculations used by `sv_client.c`:

- `SV_ChallengePolicy_TimeWindow()`
- `SV_ChallengePolicy_PreviousTimeWindow()`

No packet layout, salt lifetime, or hash bytes moved in this phase. The helper
is deliberately small so future challenge-policy work can reason about the
clock behavior without pulling in `server.h`.

## Test Coverage

`tests/engine/server_challenge_policy.cpp` covers:

- boundary values for `[0, 5)`, `[5, 10)`, and `[10, 15)` windows;
- custom window sizes for fixture coverage;
- defensive invalid-input and overflow clamping for pure helper callers;
- unsigned wrap from current window `0` to previous window `UINT32_MAX`;
- accepting only current and previous challenge windows.
