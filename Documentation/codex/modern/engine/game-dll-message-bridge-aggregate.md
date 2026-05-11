# Game DLL Message Bridge Aggregate

Phase 122 adds the first cross-callback game DLL bridge aggregate. It focuses
on message bridge behavior because the project already has narrow helpers for
message sessions, user-message registration, multicast destination policy, and
server message envelopes.

## Helper

`game_dll_message_bridge` is intentionally small. It owns:

- building a `GameDllMessageBeginRequest` from a registered
  `GameDllUserMessageSlot`;
- building and writing the active-server user-message registration resend
  payload that mirrors `SV_SendUserReg()`.

It does not own live `sv.multicast`, `SV_Multicast()`, client netchannels,
`pfnMessageBegin()`, `pfnMessageEnd()`, `GiveFnptrsToDll()`, edict storage,
string-base storage, or callback table publication.

## Aggregate Coverage

`tests/engine/game_dll_message_bridge.cpp` covers:

- a registered fixed-size user message flowing from registry slot to message
  begin request, write primitives, `GameDllMessageSession::end()`, and
  multicast destination planning;
- a newly registered variable-size user message patching its reserved payload
  size through the message session;
- active-server user-message registration producing the same resend payload
  shape as `SV_SendUserReg()`;
- rejected or inactive registrations not producing a resend payload;
- fixed-size mismatch clearing the message buffer rather than multicasting;
- rewritten system-message admission retaining the legacy command byte while
  tracking the effective replacement message.

## Boundary Decision

This helper is enough for Phase 122, but it is not a full game DLL messaging
facade. The live bridge remains in `sv_game.c` until later phases have broader
fixtures for `svgame.msg`, `sv.multicast`, destination entities, trace/visibility
state, and callback side effects.

Phase 123 can now decide whether a small grouped messaging adapter would make
the current bridge easier to read. If it does, it should start with the message
session and user-message registration adapters, not the concrete callback table
or real DLL loader.
