# Client Session Slots

Phase 125 introduces a small target-neutral helper for the client array scans
that were previously open-coded in `sv_client.c`.

## Modern Helper

`client_session_slots` works from plain slot snapshots:

- slot state;
- private client flags.

It currently answers:

- whether a slot is free;
- whether a slot is connected enough to count as present;
- whether the slot is a fake client;
- player, bot, and connected-slot counts;
- the first reusable free slot;
- whether a population transition should refresh the master heartbeat after a
  connect or drop.

The helper intentionally does not know about `sv_client_t`, `netchan_t`,
`edict_t`, cvars, game DLL callbacks, addresses, qports, or resource lists.

## Legacy Route-Through

`engine/server/client_session_slots_adapter.*` bridges the helper back to C.
`sv_client.c` now uses it for:

- `SV_GetPlayerCount()`;
- `SV_FindEmptySlot()`;
- the first-client/full-server master heartbeat refresh in
  `SV_MaybeNotifyPlayerCountChange()`;
- the empty-server master heartbeat refresh in `SV_DropClient()`.

The route-through keeps all live slot mutation in legacy code. `SV_ConnectClient()`
still owns command parsing, protocol validation, password checks, reconnect
matching, memory allocation, edict setup, netchan setup, response packets, and
userinfo mutation. `SV_DropClient()` still owns disconnect messages, game DLL
disconnect callbacks, frame/resource cleanup, redirect cleanup, netchan cleanup,
and full-client-update emission.

## Follow-Up

The next client/session helpers should only grow where a decision can be
expressed as plain values. Reconnect matching might become a helper later, but
only after address/qport snapshots are separated from `netadr_t`. Full
connect/drop/spawn ownership should wait until fixtures exist for netchan,
edict, and game DLL side effects.
