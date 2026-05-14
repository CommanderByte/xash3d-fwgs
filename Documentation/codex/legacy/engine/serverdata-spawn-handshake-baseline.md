# Serverdata And Spawn Handshake Baseline

Phase: 81

Legacy owner: `engine/server/sv_client.c`

## Scope

This baseline covers the first server-to-client setup messages and the command
gates around `new`, `spawn`, and `begin`.

It deliberately does not cover packet fragmentation, game DLL callbacks, full
userinfo mutation, delta table descriptions, movevars deltas, lightstyle
iteration, or resource registration internals.

## `SV_SendServerdata()`

`SV_SendServerdata()` writes an optional developer/multiplayer print followed by
`svc_serverdata`.

The print is sent when either developer mode is enabled or `svs.maxclients > 1`.
Its text is:

```text

^3BUILD <build> SERVER (<progs crc> CRC)
Server #<spawncount>
```

The fixed `svc_serverdata` payload is written in this order:

1. `PROTOCOL_VERSION` as long.
2. `svs.spawncount` as long.
3. `sv.worldmapCRC` as long.
4. Client index as byte.
5. `svs.maxclients` as byte.
6. `GI->max_edicts` as word.
7. `MAX_MODELS` as word.
8. `sv.name` as NUL-terminated string.
9. Map message from `svgame.edicts->v.message` as NUL-terminated string.
10. `sv.background` as one bit.
11. `GI->gamefolder` as NUL-terminated string.
12. `host.features` as long.
13. Four player hull min/max triples as signed bytes, interleaved min then max
    per axis.

After those fixed fields, legacy code writes delta table descriptions, full
movevars, user-message registration, and lightstyles.

## `SV_New_f()`

`SV_New_f()` only accepts clients in `cs_connected`. It writes serverdata,
lets the game DLL reject via `pfnClientConnect()`, stuffs full serverinfo, sends
full update-userinfo rows for already spawned players, clears `lastcmd`, creates
fragments, and sends them.

Game DLL callbacks, rejection/drop, player enumeration, and fragmentation stay
legacy-owned.

## `SV_Spawn_f()`

`SV_Spawn_f()` only accepts clients in `cs_connected`. If the client-provided
spawncount does not match `svs.spawncount`, it reruns `SV_New_f()` to refresh
the connection data.

When spawncount matches, it calls `SV_PutClientInServer()`, moves the client to
`cs_spawning`, and sends pause notification when the server is paused.

## `SV_PutClientInServer()`

After game state setup, legacy sets `FCL_RESEND_USERINFO` and
`FCL_RESEND_MOVEVARS`, resets timing fields, and for real clients sends:

1. The accumulated `sv.signon` buffer.
2. `svc_setview`.
3. `svc_signonnum` with value `1`.

If this spawn message overflows, single-player calls `Host_Error()` while
multiplayer drops the client.

## `SV_Begin_f()`

`SV_Begin_f()` only accepts clients in `cs_spawning`; it then marks the client
`cs_spawned` and stores `host.realtime` as `connecttime`.

## Extraction Boundary

Safe to extract:

- Fixed serverdata payload serialization.
- Serverdata print decision and formatting.
- `new`/`spawn`/`begin` state gates.
- Spawncount mismatch routing.
- Resend flag constants.
- `svc_signonnum` payload.
- Overflow response classification.

Kept legacy-owned:

- Game DLL callbacks and rejection text.
- Client state mutation.
- `SV_PutClientInServer()` entity setup.
- Delta table descriptions, movevars, user-message registration, and
  lightstyles.
- `sv.signon` ownership and message fragmentation.
- `Host_Error()`, `SV_DropClient()`, and netchan operations.
