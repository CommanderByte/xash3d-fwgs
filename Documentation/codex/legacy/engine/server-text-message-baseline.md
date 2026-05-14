# Server Text Message Baseline

Phase 75 covers the repeated server-side writers for `svc_print` and
`svc_stufftext`. The migration boundary is intentionally small: formatting,
client selection, command validation, and destination buffer ownership remain in
legacy code, while the target-neutral helper owns the byte layout of the text
payload after the legacy command byte is written.

## Protocol Shape

`engine/common/protocol.h` defines:

- `svc_print = 8`
- `svc_stufftext = 9`

Both payloads are NUL-terminated strings written with `MSG_WriteString()`.
There is no length prefix. The command byte is emitted by
`MSG_BeginServerCmd()` before the string payload.

Client parsing in `engine/client/parse/cl_parse.c` reads both commands with
`MSG_ReadString()`. `svc_print` is passed to `Con_Printf()`. `svc_stufftext` is
fed into the client command buffer after the existing legacy filtering and
tracing logic.

## Legacy Writers

`SV_ClientPrintf()` in `engine/server/sv_cmds.c`:

- returns immediately for `FCL_FAKECLIENT`;
- formats into `char string[MAX_SYSPATH]` with `Q_vsnprintf()`;
- writes `svc_print` to `cl->netchan.message`;
- writes the formatted string payload.

`SV_BroadcastPrintf()`:

- formats once into `MAX_SYSPATH`;
- when `sv.state == ss_active`, iterates `svs.clients`;
- skips fake clients, the ignored client, and clients not in `cs_spawned`;
- writes `svc_print` and the formatted string to each selected client;
- echoes to the dedicated server console with `Con_DPrintf()`.

`SV_BroadcastCommand()`:

- returns when `sv.state == ss_dead`;
- formats into `MAX_SYSPATH`;
- writes `svc_stufftext` and the formatted string to `sv.reliable_datagram`.

`pfnClientCommand()` in `engine/server/sv_game.c`:

- returns unless `sv.state == ss_active`;
- resolves the target edict with `SV_ClientFromEdict()`;
- skips missing clients and fake clients;
- formats into `char buffer[MAX_STRING]`;
- calls `SV_IsValidCmd()` before writing;
- writes `svc_stufftext` and the formatted command to the client's reliable
  netchan message only when the command is valid.

Nearby text writers that remain out of this phase include `svc_centerprint`,
music `svc_stufftext` formatting in `SV_StartMusic()`, redirect printing, and
serverdata/setup command strings. Those either have distinct message commands
or broader ownership questions and should be handled by later focused phases.

## Compatibility Notes

- `MAX_SYSPATH` and `MAX_STRING` formatting limits are part of the legacy
  behavior. Phase 75 does not introduce new truncation or validation rules.
- Embedded NUL bytes terminate the serialized string because the legacy writer
  uses C string length semantics.
- Overflow behavior stays with `sizebuf_t`: if the destination buffer cannot
  fit the payload, `bOverflow` is set.
- Fake-client filtering, spawned-client filtering, and ignored-client handling
  are compatibility-sensitive and stay in the legacy call sites.
- `pfnClientCommand()` keeps `SV_IsValidCmd()` in legacy code because command
  safety is a command-system concern, not a payload-serialization concern.
