# Server Frame Datagram Baseline

Phase: 82

Legacy owner: `engine/server/sv_frame.c`

## Scope

This baseline covers the end-of-frame message assembly paths around
`SV_SendClientMessages()`, `SV_UpdateToReliableMessages()`, and
`SV_SendClientDatagram()`.

It deliberately does not move entity snapshot generation, clientdata writing,
netchan transmission, fragment construction, broadcast/drop side effects, or
actual message writes. Those remain legacy-owned.

## Client Datagram Append

`SV_SendClientDatagram()` creates a temporary per-client datagram, writes
`svc_time`, clientdata, and entity deltas, then appends the accumulated
per-client multicast datagram.

Legacy behavior:

- If `cl->datagram` overflowed, print a warning and do not append it.
- Otherwise, append it only when `MSG_GetNumBytesWritten( &cl->datagram ) <
  MSG_GetNumBytesLeft( &msg )`.
- If it would not fit, print the unreliable-datagram warning at most once every
  five seconds using `cl->overflow_warn_time`.
- Clear `cl->datagram` after the decision, even when ignored.
- If the final temporary datagram overflowed, print an error and clear it
  before transmission.
- Always call `Netchan_TransmitBits()` afterward.

The strict `<` comparison is part of the compatibility surface: equal size is
treated as not fitting.

## Resend Userinfo And Movevars

`SV_UpdateToReliableMessages()` scans spawned clients with edicts.

Legacy behavior:

- `FCL_RESEND_USERINFO` sends `SV_FullClientUpdate()` only when
  `next_sendinfotime <= host.realtime` and there is at least
  `strlen(userinfo) + 6` bytes left in `sv.reliable_datagram`.
- A successful userinfo resend clears `FCL_RESEND_USERINFO` and sets
  `next_sendinfotime = host.realtime + 1.0`.
- `FCL_RESEND_MOVEVARS` sends full movevars directly to
  `cl->netchan.message` and clears the flag.

## Server Datagram Overflow Clearing

Before copying accumulated server datagrams to clients:

- If `sv.datagram` overflowed, print `sv.datagram overflowed!` and clear it.
- If `sv.spec_datagram` overflowed, print `sv.spec_datagram overflowed!` and
  clear it.

## Fanout To Clients

Reliable and unreliable accumulated datagrams are considered for all clients
whose state is at least `cs_connected` and who are not fake clients.

Legacy behavior:

- Server reliable datagram:
  - copy to `cl->netchan.message` when written bytes are strictly less than
    bytes left;
  - otherwise call `Netchan_CreateFragments()`.
- Server unreliable datagram:
  - copy to `cl->datagram` when written bytes are strictly less than bytes left;
  - otherwise print `Ignoring unreliable datagram...` and drop that payload for
    the client.
- Spectator datagram:
  - only considered for `FCL_HLTV_PROXY`;
  - copy or ignore with warning using the same strict capacity test.
- Clear `sv.reliable_datagram`, `sv.spec_datagram`, and `sv.datagram` after the
  fanout loop.

## Send Loop

`SV_SendClientMessages()` returns immediately while the server is dead. It then
updates reliable messages and loops over clients.

Legacy behavior:

- Skip clients at or below `cs_zombie` and fake clients.
- `FCL_SKIP_NET_MESSAGE` is a one-frame skip; clear it and continue.
- Local clients are forced to send when `host_limitlocal` is false.
- Spawned clients are marked for send when their next message time is due in
  the current frame, or when it is more than two seconds in the future.
- Reliable netchan overflow clears the reliable message and client datagram,
  broadcasts/logs overflow, drops the client, sets `FCL_SEND_NET_MESSAGE`, and
  clears `netchan.cleartime` so the drop can go out immediately.
- If sending is requested but the client has been silent longer than
  `sv_failuretime`, clear `FCL_SEND_NET_MESSAGE`.
- When sending is still requested:
  - if `Netchan_CanPacket()` rejects, increment `chokecount`;
  - otherwise update `next_messagetime`, clear `FCL_SEND_NET_MESSAGE`, send a
    full datagram for spawned clients, and transmit an empty reliable update
    for non-spawned connected clients.

## Extraction Boundary

Safe to extract:

- Copy versus fragment versus ignore decisions.
- Datagram overflow clear decisions.
- Userinfo/movevars resend readiness.
- Client send-loop gates that only depend on primitive state.

Kept legacy-owned:

- Entity frame and clientdata construction.
- `MSG_WriteBits()`, `MSG_Clear()`, and buffer ownership.
- Netchan send/fragment calls.
- `SV_UpdateUserInfo()`, `SV_FullUpdateMovevars()`, drops, broadcasts, and
  console output.
