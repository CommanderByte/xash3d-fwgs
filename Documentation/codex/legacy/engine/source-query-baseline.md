# Source Query Baseline

Phase 54 documents the current `engine/server/sv_query.c` behavior before the
payload bytes move behind a modern helper.

## Entry Point

`SV_SourceQuery_HandleConnnectionlessPacket()` keeps the existing dispatch:

- exact `A2S_GOLDSRC_INFO` (`"TSource Engine Query"`) requests build the info
  response;
- `A2S_GOLDSRC_RULES` (`'V'`) requests build the rules response;
- `A2S_GOLDSRC_PLAYERS` (`'U'`) requests build the player response;
- all other connectionless strings are ignored by this helper.

The misspelling in `HandleConnnectionlessPacket` is part of the current C
surface and should remain until a larger compatibility cleanup.

## Info Response

The details response writes this byte order:

1. connectionless header `0xffffffff`;
2. response byte `S2A_GOLDSRC_INFO` (`'I'`);
3. `PROTOCOL_VERSION`;
4. strings: hostname, map name, game folder, game description;
5. app ID short, currently `0`;
6. player count byte, where bots are included in the player total;
7. max-clients byte;
8. bot count byte;
9. server type byte, `'d'` for dedicated or `'l'` for listen;
10. platform byte, `'w'`, `'m'`, or `'l'`;
11. password flag byte;
12. `GI->secure` byte;
13. `XASH_VERSION` string.

Live state still comes from `hostname`, `sv`, `svs`, `svgame`, `GI`,
`SV_GetPlayerCount()`, `SV_HavePassword()`, and `Host_IsDedicated()`.

## Rules Response

Rules responses use header `0xffffffff`, response byte `'E'`, and a patched
little-endian rule count. Only cvars with `FCVAR_SERVER` are exported.

Protected values keep the legacy masking rule:

- non-empty values other than case-insensitive `none` become `"1"`;
- empty values and `none` become `"0"`;
- unprotected values are written as-is.

No packet is sent when the exported cvar count is zero.

## Players Response

Player-list responses use header `0xffffffff`, response byte `'D'`, and a
patched player count byte. They are suppressed when `sv_expose_player_list` is
false or the server has a password.

Only clients with `state >= cs_connected` are included. The row index is the
sequential exported row number, not the original client slot. Fake clients use
duration `-1.0`; other clients use `host.realtime - connection_started`.

No packet is sent when the exported player count is zero.

## Migration Boundary

Phase 54 may move byte construction. It must leave these legacy-owned:

- connectionless request dispatch;
- live server/cvar/client/global reads;
- `NET_SendPacket`;
- the existing public C function names;
- query suppression decisions that depend on live server state.
