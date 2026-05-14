# Game DLL Message Bridge Adapter Pilot

Phase 123 groups the two smallest game DLL messaging adapter implementations
without changing the C surface used by `sv_game.c`.

## What Moved

`engine/server/game_dll_message_bridge_adapter.cpp` now owns the adapter glue
for:

- message-session byte normalization, write-size accounting, destination
  clamping, rewrite admission, and entity-index validation;
- user-message registration planning, duplicate lookup projection, stored-size
  planning, and active-server resend intent.

The public C headers remain split:

- `engine/server/game_dll_message_session_adapter.h`
- `engine/server/game_dll_user_message_registry_adapter.h`

This keeps call sites explicit while removing an unnecessary build-file split
between two closely related message bridge policies.

## What Stayed Legacy-Owned

The pilot does not move callback table publication, `GiveFnptrsToDll()`, real
game DLL load/unload, `svgame.msg` storage, `sv.multicast`, `SV_Multicast()`,
netchan writes, or the live callback functions out of `sv_game.c`.

That boundary is intentional. The grouped adapter should make the messaging
bridge easier to find, but it must not hide the compatibility-sensitive ABI
details that game DLLs still depend on.

## Follow-Up Rule

Future grouped adapters should use the same threshold: group implementation
files only when the grouped concept is already covered by focused tests and
the public C entry points stay stable. If grouping would require renaming C
functions, reordering callback tables, or moving live engine state, keep it as
a later behavior-owner phase instead.
