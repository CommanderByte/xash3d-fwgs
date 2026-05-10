# Server Multicast Baseline

Phase: 78

Legacy owner: `engine/server/sv_game.c`

Primary function: `SV_Multicast()`

## Responsibilities

`SV_Multicast()` flushes the temporary `sv.multicast` buffer to one or more
legacy destination buffers, then usually clears `sv.multicast`.

It currently owns four separate concerns:

1. Destination decoding from `MSG_*` constants.
2. PVS/PHS visibility mask selection.
3. Per-client recipient filtering.
4. Final writes into signon, reliable, unreliable, or spectator buffers.

The phase 78 extraction must only move the destination and recipient policy.
`sv.multicast`, mask generation, `MSG_WriteBits()`, and buffer ownership stay in
legacy code.

## Destination Modes

| Destination | Legacy behavior |
| --- | --- |
| `MSG_BROADCAST` | Send unreliable data to all eligible clients. |
| `MSG_ALL` | Send reliable data to all eligible clients. |
| `MSG_INIT` while loading | Copy to `sv.signon`, clear `sv.multicast`, return `1`. |
| `MSG_INIT` outside loading | Fall through to reliable `MSG_ALL` behavior. |
| `MSG_PVS` | Require an origin, build a PVS mask, send unreliable data to visible clients. |
| `MSG_PVS_R` | Same as `MSG_PVS`, but reliable. |
| `MSG_PAS` | Require an origin, build a FatPVS/PHS-style mask, send unreliable data to audible clients. |
| `MSG_PAS_R` | Same as `MSG_PAS`, but reliable. |
| `MSG_ONE` | Require a valid player edict, send reliable data to that client slot only. |
| `MSG_ONE_UNRELIABLE` | Same target rules as `MSG_ONE`, but unreliable. |
| `MSG_SPEC` | Send reliable data to HLTV proxy clients through `sv.spec_datagram`. |

Missing origins for PVS/PAS return `0` without clearing `sv.multicast`. Invalid
single-client edicts or indexes also return `0` without clearing. Invalid
destination constants call `Host_Error()`.

## Recipient Filtering

The client loop rejects recipients in this order:

1. `cs_free` and `cs_zombie` clients.
2. Non-spawned clients for unreliable data or user messages. Reliable non-user
   messages may still reach connected/spawning clients.
3. Non-HLTV clients when destination is `MSG_SPEC`.
4. Missing edicts and fake clients.
5. The current predicted client when the caller asks to filter predicted step
   sounds.
6. Group filter mismatches for `GROUP_OP_AND` and `GROUP_OP_NAND`.
7. Failed PVS/PHS visibility checks.

Accepted recipients write to `cl->datagram`, `cl->netchan.message`, or
`sv.spec_datagram` depending on unreliable, reliable, or spectator routing.

## Visibility

Null masks mean visible by GoldSrc rules. Non-null masks are checked against the
client view origin or active view entity, then against portal camera view
entities. PHS destinations use `Mod_FatPVS()` with `FATPHS_RADIUS`; PVS
destinations use `Mod_GetPVSForPoint()`.

The modern policy helper must not generate or inspect BSP visibility data. It
receives the adapter-computed visibility result as an input.

## User Message Rewrite

Game DLL message construction writes into `sv.multicast` through
`pfnMessageBegin()` and `pfnMessageEnd()`. Before `SV_Multicast()` runs,
`pfnMessageEnd()` may rewrite compatibility messages such as
`svc_goldsrc_spawnstaticsound` into modern `svc_sound` data.

Rewrite validation, user-message size checks, tracing, and `sv.multicast`
payload storage remain outside the phase 78 policy helper. The helper only sees
the final `usermessage` boolean that controls spawned-client filtering.

## Extraction Boundary

Safe to extract:

- Destination classification: reliable, single-client, spectator, visibility
  mode, signon write, early abort, or host error.
- Recipient classification from adapter-provided booleans.

Not safe to extract in this phase:

- `sv.multicast` lifetime.
- `sv.signon`, `cl->datagram`, `cl->netchan.message`, and `sv.spec_datagram`
  writes.
- `Mod_FatPVS()`, `Mod_GetPVSForPoint()`, and portal-camera visibility.
- Entity index lookup for `MSG_ONE`.
- `Host_Error()` and console diagnostics.
