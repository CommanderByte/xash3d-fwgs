# Server Connectionless Packet Baseline

This note captures the dispatch behavior of `SV_ConnectionlessPacket()` before
Phase 57 routing. It covers classification only; packet reads, command
tokenization, handlers, game DLL callbacks, and network sends remain legacy
owned.

## Entry Behavior

`SV_ConnectionlessPacket()` handles packets that start with the connectionless
`-1` marker. The current order is:

1. Drop banned IPs through `SV_CheckIP()`.
2. Clear the message, skip the four-byte marker, read one string line, and
   tokenize it with `Cmd_TokenizeString()`.
3. Use `Cmd_Argv(0)` as the first token (`pcmd`) and keep the original line as
   `args`.
4. Optionally report out-of-band packets through `sv_log_outofband`.
5. If the server is not initialized, only `rcon` is accepted.
6. If the sender is a master server, only master challenge (`s`) and NAT
   connect (`c`) requests are accepted; all other master packets are ignored.
7. Otherwise dispatch public server queries and connection commands.

## Public Dispatch Order

For initialized non-master packets, the dispatch order is:

1. GoldSrc source-query requests:
   - exact full line `TSource Engine Query`
   - any first token beginning with `U`
   - any first token beginning with `V`
2. `netinfo`
3. `info`
4. `bandwidth`
5. `getchallenge`
6. `connect`
7. `ping`
8. GoldSrc ping `i`
9. `rcon`
10. acknowledgements: `ack` and GoldSrc ack `j`
11. game DLL connectionless callback, then optional bad-packet logging

The source-query check intentionally uses both `args` and `pcmd`. The exact
full-line check exists because `TSource Engine Query` contains spaces, while
the loose `U`/`V` first-character checks preserve legacy behavior where strings
such as `Unrelated` and `Verbose` still enter the source-query handler.

## Migration Boundary

Phase 57 may move the target-neutral classification table into
`src/engine/server`, but these remain legacy-owned:

- `MSG_*` reads and `Cmd_TokenizeString()`
- `SV_CheckIP()`, `NET_IsMasterAdr()`, and all handler calls
- `Con_Reportf()`, `Con_DPrintf()`, and packet sends
- the game DLL `pfnConnectionlessPacket()` fallback
