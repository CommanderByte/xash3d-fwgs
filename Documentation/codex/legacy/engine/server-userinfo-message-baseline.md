# Server Userinfo Message Baseline

`SV_FullClientUpdate()` in `engine/server/sv_client.c` writes the
`svc_updateuserinfo` message that tells clients about one player slot. The
message is part of the server protocol surface and is parsed by
`CL_UpdateUserinfo()` in `engine/client/parse/cl_parse.c`.

## Legacy Ownership

The legacy function still owns the stateful parts of the operation:

- calls `SV_UserinfoChanged()` before serializing;
- derives the client index with pointer arithmetic against `svs.clients`;
- begins the server command with `MSG_BeginServerCmd(..., svc_updateuserinfo)`;
- copies `cl->userinfo` into a bounded local buffer;
- strips prefixed keys with `Info_RemovePrefixedKeys(info, '_')`;
- hashes the complete `cl->hashedcdkey` array with MD5.

Those pieces depend on global server state, legacy info-string rules, or
legacy message diagnostics, so Phase 72 keeps them in `sv_client.c`.

## Payload Layout

After the command byte, the payload layout is:

| Field | Encoding | Notes |
| --- | --- | --- |
| client index | `MAX_CLIENT_BITS` unsigned bits | Currently 5 bits for 32 slots. |
| user ID | signed 32-bit value | Written in the normal message bitstream. |
| active bit | 1 bit | Set when `cl->name[0]` is non-zero. |
| userinfo | NUL-terminated string | Only emitted for active clients, after prefix stripping. |
| hashed CD key digest | 16 bytes | Only emitted for active clients, MD5 of the full `hashedcdkey` storage. |

Unnamed clients still send the client index and user ID followed by an inactive
bit. They do not send a userinfo string or digest.

## Modern Boundary

Phase 72 moves only the payload writer into
`src/engine/server/messaging/server_userinfo_message.cpp`. The C adapter accepts the
legacy buffer pointer, bit capacity, current bit, already-sanitized userinfo,
and already-computed digest bytes. It returns the updated bit cursor and
overflow flag for the legacy `sizebuf_t`.
