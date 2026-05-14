# NetAPI Info Baseline

Phase 56 documents the legacy info-string response builders in
`engine/server/sv_client.c`.

## Short `A2A_INFO`

`SV_Info(netadr_t from, int protocolVersion)` responds to short broadcast
server-info scans.

The function ignores requests when:

- `svs.maxclients == 1`;
- `svs.initialized` is false.

When the requested protocol does not match `PROTOCOL_VERSION`, it sends:

```text
<hostname>: wrong version
```

including the newline in the actual response string.

When the protocol matches, it builds an info string with this key order:

1. `p`: protocol version;
2. `map`: current map name;
3. `dm`: deathmatch flag;
4. `team`: teamplay flag;
5. `coop`: coop flag;
6. `numcl`: player count from `SV_GetPlayerCount()`, excluding bots;
7. `maxcl`: `svs.maxclients`;
8. `gamedir`: `GI->gamefolder`;
9. `password`: `1` when the server has a password, otherwise `0`;
10. `host`: hostname, written last and truncated to fit the 512-byte response.

The hostname truncation uses the legacy `Q_strncpy(temp, hostname, remaining)`
pattern, so the copied hostname is effectively limited to `remaining - 1`
characters.

## Long `A2A_NETINFO`

`SV_BuildNetAnswer(netadr_t from)` responds to the longer NetAPI query path. It
reads request `version`, `context`, and `type` from `Cmd_Argv(1..3)` and sends:

```text
netinfo <context> <type> <info-string>
```

The function ignores requests when:

- `svs.maxclients == 1`;
- `svs.initialized` is false.

## NetAPI Request Behavior

When the protocol version is unsupported, the response info string is:

```text
\neterror\protocol
```

Known request types:

- `NETAPI_REQUEST_PING`: empty info string;
- `NETAPI_REQUEST_RULES`: all `FCVAR_SERVER` cvars, protected values masked as
  `"1"` or `"0"`, then `rules=<count>`;
- `NETAPI_REQUEST_PLAYERS`: `\neterror\forbidden` if player lists are hidden
  or the server has a password, otherwise sequential `pNname`, `pNfrags`, and
  `pNtime` entries followed by `players=<count>`;
- `NETAPI_REQUEST_DETAILS`: `hostname`, `gamedir`, `current`, `max`, and `map`.

Unknown request types return:

```text
\neterror\undefined
```

The details response manually counts clients with `state >= cs_connected`. The
old comment says it should match `SV_SourceQuery_Details`, but this path uses
an info string and does not include all source-query fields.

## Migration Boundary

Phase 56 may move string construction. It must leave these legacy-owned:

- connectionless packet parsing and `Cmd_Argv()` reads;
- live server/cvar/client/global reads;
- `Netchan_OutOfBandPrint()`;
- single-player/uninitialized suppression;
- public static function placement inside `sv_client.c`.
