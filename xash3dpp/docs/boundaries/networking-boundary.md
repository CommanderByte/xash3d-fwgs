# Networking Boundary Spec

> Legacy sources surveyed:
> `engine/common/net_ws.c` (~1900 lines), `engine/common/net_chan.c` (~1800 lines),
> `engine/common/net_buffer.c` / `.h`, `engine/common/net_encode.c` / `.h`,
> `engine/common/http/net_http_xash.c`, `engine/common/masterlist.c`,
> `engine/common/net_ws.h`, `engine/common/netchan.h`,
> `engine/common/net_ws_private.h`, `common/netadr.h`, `common/net_api.h`.

## Responsibility

The networking module owns every byte that crosses a process boundary over the
network. It breaks into four tightly coupled layers:

| Layer | Files | Concern |
|-------|-------|---------|
| **Transport** (`net_ws`) | `net_ws.c`, `net_ws_private.h` | Raw UDP socket lifecycle, send/recv, loopback ring, IPv4/IPv6 dual-stack, long-packet split/reassembly, lag-loss simulation, non-blocking async DNS |
| **Netchan** (`net_chan`) | `net_chan.c`, `netchan.h` | Reliable-sequenced channel over UDP — fragmentation, flow control, acknowledgement, bzip2/LZSS compression |
| **Message codec** (`net_buffer`) | `net_buffer.c`, `net_buffer.h` | `sizebuf_t` bit/byte stream serialiser: `MSG_WriteByte`, `MSG_ReadLong`, etc. |
| **Delta encoder** (`net_encode`) | `net_encode.c`, `net_encode.h` | Delta-compressed entity/player/event state for the game protocol; owns delta table lifecycle |

Two satellite subsystems live in the same layer of the engine and depend on
Transport:

- **HTTP downloader** (`engine/common/http/net_http_xash.c`) — TCP-based
  resource download; managed via `HTTP_Init / HTTP_Run / HTTP_Shutdown`.
- **Master-server list** (`engine/common/masterlist.c`) — UDP heartbeat/query
  protocol for the in-game server browser.

The networking module does **not** execute any game logic, load game assets, or
manage memory pools for anything other than netchan fragment buffers.

---

## External ABI contracts

### 1. `netadr_t` (SDK struct — `common/netadr.h`)

A packed 20-byte address type used in every inter-DLL call that passes a
network address. Its binary layout is fixed by the GoldSrc protocol and must
not change.

```
#pragma pack(push, 1)
struct netadr_s {        // total 20 bytes
    uint16_t type;       // overlaps with ip6_0[0..1] for IPv6 disambiguation
    uint8_t  ip6_0[2];
    union {
        uint8_t ip6_1[14];
        struct { union { uint8_t ip[4]; uint32_t ip4; }; uint8_t ipx[10]; };
    };
    uint16_t port;
};
#pragma pack(pop)
```

**Invariant**: for non-IPv6 addresses `ip6_0[0]` and `ip6_0[1]` must be zero;
`NET_NetadrType()` and `NET_NetadrSetType()` enforce this.

### 2. `net_api_t` (client DLL interface — `common/net_api.h`)

A function-pointer table populated by the engine and given to the client DLL
via `cl_exportfuncs_t::pNetAPI`. The client DLL calls these at any time during
gameplay.

```c
typedef struct net_api_s {
    void    (*InitNetworking)(void);
    void    (*Status)(net_status_t *status);
    void    (*SendRequest)(int context, int request, int flags, double timeout,
                           netadr_t *remote_address,
                           net_api_response_func_t response);
    void    (*CancelRequest)(int context);
    void    (*CancelAllRequests)(void);
    const char *(*AdrToString)(netadr_t *a);
    int     (*CompareAdr)(netadr_t *a, netadr_t *b);
    int     (*StringToAdr)(char *s, netadr_t *a);
    const char *(*ValueForKey)(const char *s, const char *key);
    void    (*RemoveKey)(char *s, const char *key);
    void    (*SetValueForKey)(char *s, const char *key,
                              const char *value, int maxsize);
} net_api_t;
```

All eleven slots must remain stable. The async `SendRequest` / `CancelRequest`
callbacks exercise `NETAPI_REQUEST_PING/RULES/PLAYERS/DETAILS/SERVERLIST`.

### 3. `Delta_AddEncoder` (server game DLL hook — `common/net_encode.h`)

```c
void Delta_AddEncoder(char *name, pfnDeltaEncode encodeFunc);
```

Called by the server game DLL at `ServerActivate` time to register custom
entity-state encode callbacks. The function signature and semantics are part of
the GoldSrc game DLL ABI exposed via `enginefuncs_t::pfnDeltaAddEncoder`
(see `engine/eiface.h`). The rewrite must export this under the same name.

### 4. Packet header magic numbers (wire format)

| Constant | Value | Meaning |
|---|---|---|
| `NET_HEADER_OUTOFBANDPACKET` | `-1` (0xFFFFFFFF) | Connectionless packet |
| `NET_HEADER_SPLITPACKET` | `-2` (0xFFFFFFFE) | Fragmented oversized packet |
| `NET_HEADER_COMPRESSEDPACKET` | `-3` (0xFFFFFFFD) | Compressed (unused in practice) |

These are the on-wire discriminators read before the netchan layer is consulted.
Game DLLs and third-party tools rely on them for protocol parsing.

---

## Interface (what the rest of the engine calls)

### Transport — `engine/common/net_ws.h`

#### Lifecycle

| Function | Purpose |
|---|---|
| `NET_Init()` | Register cvars, allocate DNS mutex, set `net.initialized = true` |
| `NET_Shutdown()` | Close all sockets, free lag simulation data, destroy DNS mutex |
| `NET_Config(multiplayer, changeport)` | Open (`multiplayer=true`) or close real sockets; clear loopback; no-op in single-player |
| `NET_IsActive()` | Returns `net.initialized`; used to guard sends before init |

#### Send/Receive (the only packet I/O entry points)

| Function | Purpose |
|---|---|
| `NET_GetPacket(sock, from*, data*, length*)` | Pull one packet: loopback first, then round-robin IPv4/IPv6 socket. Applies fakelag. Returns `qboolean`. |
| `NET_SendPacket(sock, length, data, to)` | Send one datagram; delegates to loopback or real socket |
| `NET_SendPacketEx(sock, length, data, to, splitsize)` | Send with optional server-side SPLITPACKET fragmentation |

All socket calls are **confined to these three functions**. This design
boundary must be preserved in the rewrite to allow transparent future
migration to an async I/O thread.

#### Address utilities

`NET_StringToAdr`, `NET_StringToAdrNB` (non-blocking), `NET_StringToFilterAdr`,
`NET_AdrToString`, `NET_BaseAdrToString`, `NET_CompareAdr`, `NET_CompareBaseAdr`,
`NET_CompareAdrByMask`, `NET_CompareAdrSort`, `NET_IsReservedAdr`,
`NET_GetLocalAddress`, `NET_IsLocalAddress`, `NET_IP6BytesToNetadr`,
`NET_NetadrToIP6Bytes`, `NET_MakeSocketNonBlocking`, `NET_IsSocketValid`,
`NET_IsSocketError`.

### Netchan — `engine/common/netchan.h`

#### Lifecycle

| Function | Purpose |
|---|---|
| `Netchan_Init()` | Register cvars, pick random `qport`, allocate `net_mempool` |
| `Netchan_Shutdown()` | Free `net_mempool` |

#### Channel operations

| Function | Purpose |
|---|---|
| `Netchan_Setup(sock, chan, adr, qport, client, pfnBlockSize, flags)` | Initialise a `netchan_t`; `flags` selects munge/bzip2/LZSS/GoldSrc mode |
| `Netchan_TransmitBits(chan, bits, data)` | Send one frame of reliable + unreliable data |
| `Netchan_Process(chan, msg)` | Receive and reassemble; returns `true` when a complete message is ready |
| `Netchan_OutOfBand(socket, adr, length, data)` | Send a connectionless datagram |
| `Netchan_OutOfBandPrint(socket, adr, fmt, ...)` | Formatted connectionless datagram |
| `Netchan_CanPacket(chan, choke)` | Rate-limiting gate |
| `Netchan_Clear(chan)` | Reset channel state |
| `Netchan_FragSend(chan)` | Flush pending fragment queue |

#### Fragment helpers

`Netchan_CreateFragments`, `Netchan_CreateFileFragments`,
`Netchan_CreateFileFragmentsFromBuffer`, `Netchan_CopyNormalFragments`,
`Netchan_CopyFileFragments`, `Netchan_UpdateProgress`,
`Netchan_IncomingReady`, `Netchan_IsLocal`, `Netchan_ReportFlow`.

### Message codec — `engine/common/net_buffer.h`

All inline / static functions operating on `sizebuf_t`. Key primitives:

`MSG_Init`, `MSG_Clear`, `MSG_StartBitWriting`, `MSG_EndBitWriting`,
`MSG_WriteOneBit`, `MSG_WriteByte`, `MSG_WriteShort`, `MSG_WriteLong`,
`MSG_WriteFloat`, `MSG_WriteString`, `MSG_ReadByte`, `MSG_ReadShort`,
`MSG_ReadLong`, `MSG_ReadFloat`, `MSG_ReadString`, `MSG_Overflow`,
`MSG_SeekToBit`, `MSG_GetNumBytesWritten`, `MSG_ExciseBits`.

### Delta encoder — `engine/common/net_encode.h`

#### Lifecycle

| Function | Purpose |
|---|---|
| `Delta_Init()` | Allocate and populate delta tables for all built-in types; called at server/client activation |
| `Delta_InitClient()` | Client-side variant (reads tables received from server) |
| `Delta_Shutdown()` | Free delta tables |

#### Table management

`Delta_AddEncoder` (game DLL hook), `Delta_FindField`, `Delta_SetField`,
`Delta_UnsetField`, `Delta_SetFieldByIndex`, `Delta_UnsetFieldByIndex`,
`Delta_WriteDescriptionToClient`, `Delta_ParseTableField`,
`Delta_ParseTableField_GS`.

#### Wire encode/decode for each game struct

`MSG_WriteDeltaUsercmd`, `MSG_ReadDeltaUsercmd`,
`MSG_WriteDeltaEvent`, `MSG_ReadDeltaEvent`,
`MSG_WriteDeltaMovevars`, `MSG_ReadDeltaMovevars`,
`MSG_WriteClientData`, `MSG_ReadClientData`,
`MSG_WriteWeaponData`, `MSG_ReadWeaponData`,
`MSG_WriteDeltaEntity`, `MSG_ReadDeltaEntity`,
`Delta_TestBaseline`, `Delta_ReadGSFields`, `Delta_WriteGSFields`.

### HTTP downloader — `engine/common/net_ws.h`

`HTTP_Init`, `HTTP_Shutdown`, `HTTP_Run` (called every frame from `host.c`),
`HTTP_AddDownload`, `HTTP_AddCustomServer`, `HTTP_ClearCustomServers`,
`HTTP_ResetProcessState`.

---

## Dependencies

| Dependency | Used for |
|---|---|
| Platform sockets (Winsock / POSIX) | UDP and TCP I/O — abstracted via `net_ws_private.h` |
| `memory/` subsystem | `Mem_AllocPool` / `Mem_Calloc` / `Mem_Free` for `net_mempool`; `Z_Malloc` for lag simulation queue |
| `cmd_cvar/` subsystem | Cvar registration (`net_fakelag`, `net_fakeloss`, `net_showpackets`, `net_qport`, `net_clockwindow`, `http_timeout`, etc.) |
| `console/` (Con_Printf, Con_DPrintf) | Diagnostic output |
| `host/` (`host.realtime`) | Lag simulation timestamps and bandwidth computations |
| SDL2 / pthread / Win32 threads | Background DNS resolver thread (one thread, created on demand) |
| bzip2 | Netchan payload compression (client-only build; disabled for dedicated) |
| LZSS (miniz) | Alternative netchan compression; also used by HTTP downloader for gzip'd assets |
| `filesystem/` | HTTP downloader writes downloaded files; fragment file transfers write via `FS_WriteFile` |
| `client/` (`CL_GetSplitSize`, `CL_Protocol`, `CL_IsPlaybackDemo`) | Client-specific split-packet size and protocol selection |
| `server/` (`sv_cheats`) | Fakelag requires `sv_cheats` + developer mode; guards in `NET_AdjustLag` |

---

## Owned State

### Transport (`net_state_t net` — file-static)

| Field | Description |
|---|---|
| `loopbacks[NS_COUNT]` | Two ring buffers (4 slots each) for in-process loopback packets |
| `lagdata[NS_COUNT]` | Doubly-linked lag simulation queues |
| `losscount[NS_COUNT]` | Per-socket packet loss counter for `net_fakeloss` |
| `fakelag` | Converged copy of `net_fakelag.value` |
| `split` | `LONGPACKET` reassembly state (global, one in-flight split packet at a time) |
| `split_flags[NET_MAX_FRAGMENTS]` | Per-fragment sequence tracking |
| `sequence_number` | Outgoing split-packet sequence counter |
| `ip_sockets[NS_COUNT]` | IPv4 socket handles for client and server sockets |
| `ip6_sockets[NS_COUNT]` | IPv6 socket handles |
| `rr_state[NS_COUNT]` | Round-robin toggle (IPv4 vs IPv6 for each socket) |
| `initialized`, `configured`, `allow_ip`, `allow_ip6` | State flags |

### DNS resolver (`nsthread` — file-static)

A single background thread resolves one hostname at a time. Protected by two
mutexes (`mutexns`, `mutexres`). Results are polled by `NET_StringToAdrNB`.

### Netchan globals

| Name | Type | Description |
|---|---|---|
| `net_mempool` | `poolhandle_t` | Pool for all `fragbuf_t` and `fragbufwaiting_t` allocations |
| `net_from` | `netadr_t` | Source address of the packet currently being processed |
| `net_message` | `sizebuf_t` | Message buffer pointing at `net_message_buffer` |
| `net_message_buffer[NET_MAX_MESSAGE]` | `byte[]` | ~196 KiB static receive buffer |
| `net_drop` | `int` | Number of dropped packets since last check (polled by client/server) |

### Delta encoder

`delta_info_t` table array (file-static in `net_encode.c`) describing all
built-in delta types. The `bInitialized` flag per table is mutated at
`Delta_Init` / `Delta_Shutdown`.

---

## Capacity Limits

| Constant | Value | Meaning |
|---|---|---|
| `MAX_DATAGRAM` | 16384 bytes | Maximum unreliable UDP payload |
| `MAX_MULTICAST` | 8192 bytes | Maximum multicast payload |
| `MAX_INIT_MSG` / `NET_MAX_PAYLOAD` | 196608 / 32768 bytes (normal / low-memory) | Maximum netchan message |
| `NET_MAX_FRAGMENT` | 65535 bytes | Maximum single fragment (16384 on low-memory-2) |
| `NET_MAX_MESSAGE` | `NET_MAX_PAYLOAD + HEADER_BYTES` padded to 16 | Wire receive buffer |
| `MAX_LOOPBACK` | 4 | Loopback ring buffer slots per socket |
| `NET_MAX_FRAGMENTS` | ~506 | Maximum split-packet fragment count (standard protocol) |
| `NET_MAX_GOLDSRC_FRAGMENTS` | 5 | Maximum split-packet fragment count (GoldSrc protocol) |
| `SPLITPACKET_MIN_SIZE` | 508 bytes | Minimum split fragment body (RFC 791) |
| `SPLITPACKET_MAX_SIZE` | 64000 bytes | Maximum split fragment total |
| `MAX_RELIABLE_PAYLOAD` | 1400 bytes | Largest fragment/reliable packet on the wire |
| `HEADER_BYTES` | 8 + `MAX_STREAMS * 13` = 34 bytes | Netchan packet header overhead |
| `MAX_STREAMS` | 2 | Normal data stream + file download stream |

---

## Quirks and Invariants

### Transport

1. **Loopback cross-wiring**: `NET_SendLoopPacket` writes to
   `loopbacks[sock ^ 1]`, so a send on `NS_CLIENT` is read by `NS_SERVER` and
   vice versa. This is intentional (simulates a real round-trip within one
   process).

2. **Split-packet direction asymmetry**: `NET_SendLong` only fragments packets
   from `NS_SERVER` side. Clients never fragment outgoing UDP (client → server
   payloads are small by design).

3. **Global split-packet reassembly**: There is one `LONGPACKET net.split`
   struct. Only one fragmented packet can be in-flight at a time per process.
   A new sequence number resets the state.

4. **GoldSrc vs Xash SPLITPACKET format**:
   - GoldSrc (`SPLITPACKETGS`): `packet_id` is `unsigned char`; 4-bit
     packet_number in high nibble, 4-bit packet_count in low nibble; max 5
     fragments.
   - Xash3D standard (`SPLITPACKET`): `packet_id` is `short`; high byte =
     packet_number, low byte = packet_count; up to ~506 fragments.
   The receiver detects the format via `connprotocol_t proto` argument.

5. **Round-robin IPv4/IPv6**: `NET_GetPacket` alternates between IPv4 and IPv6
   sockets per call to prevent one protocol starving the other.

6. **Non-blocking DNS**: `NET_StringToAdrNB` fires a background thread the
   first call, then returns `NET_EAI_AGAIN` until the result is ready. Only
   one hostname can be in-flight at a time. The caller must poll on subsequent
   game frames.

7. **Fakelag requires `sv_cheats` + developer mode**: In non-developer builds
   `NET_AdjustLag` silently resets `net_fakelag` to 0. This guards against
   accidental lag simulation on production servers.

8. **`net_from` is not re-entrant**: It is a global overwritten by every call
   to `NET_QueuePacket` and `NET_LagPacket`. All packet processing must be
   single-threaded.

### Netchan

9. **Reliable retransmit on drop**: If the remote acknowledges a sequence
   higher than the last reliable transmit without the matching reliable bit,
   the entire `reliable_buf` is resent. This is the GoldSrc/Quake reliable
   guarantee.

10. **bzip2 only in non-dedicated builds**: `NETCHAN_USE_BZIP2` (and
    `NETCHAN_USE_LZSS`) are ignored for dedicated server builds at link time
    (`#if !XASH_DEDICATED`).

11. **GoldSrc netchan flag** (`NETCHAN_GOLDSRC`): Switches to GoldSrc
    wire-packet format (different fragment ID encoding and munge XOR
    obfuscation).

12. **Munge obfuscation** (`NETCHAN_USE_MUNGE`): A trivial XOR scramble applied
    for GoldSrc compatibility. Not a security feature; just obfuscation.

### Delta Encoding

13. **Delta tables are re-initialised on map change**: `Delta_Init` is called
    from both `sv_game.c::SV_InitGameProgs` and `sv_init.c::SV_SpawnServer`.
    The client re-initialises via `Delta_InitClient` after receiving the server's
    table description (`Delta_WriteDescriptionToClient` / `Delta_ParseTableField`).

14. **Game DLL can override field encode/decode** via `Delta_AddEncoder`. The
    registered callback receives the raw field array and mutates bits in-place.
    This hook is part of the `enginefuncs_t` ABI (`pfnDeltaAddEncoder`).

15. **`DT_ENTITY_STATE_PLAYER_T` is a strict superset of `DT_ENTITY_STATE_T`**:
    Player entities are encoded with extra fields. The `type` argument to
    `MSG_WriteDeltaEntity` / `MSG_ReadDeltaEntity` selects which table to use.

---

## Open Questions

| # | Question | Context |
|---|---|---|
| OQ-1 | **Module decomposition**: Should Transport, Netchan, Message Codec, and Delta Encoder be four independent CMake targets, or a single `xash3dpp_networking`? | Codec and Delta have no platform dependencies; separating them would allow unit-testing without sockets. |
| OQ-2 | **`std::expected<T, NetError>` at Chunk 2**: `decisions-architecture.md §Q-5` defers this to Chunk 2. `NET_GetPacket` / `NET_SendPacket` are the entry points; what set of `NetError` codes is needed? | Candidate codes: `WouldBlock`, `SocketInvalid`, `Overflow`, `SplitTooLarge`, `DnsAgain`, `DnsFailure`. |
| OQ-3 | **`netadr_t` as a C++ value type**: The ABI layout is fixed, but should the rewrite wrap it in a `NetAddress` type with constructors/comparators, or keep the C struct and adapt at the ABI boundary? | A wrapping type can enforce the `ip6_0` zeroing invariant internally. |
| OQ-4 | **DNS resolver threading**: Use the xash3dpp worker-thread pool (if one exists) or retain the dedicated-thread-per-query model? | The legacy model fires at most one thread at a time; pooling would allow concurrent multi-server list queries. |
| OQ-5 | **HTTP downloader placement**: Fold HTTP into the networking module, or treat it as a separate `xash3dpp_http` target with its own boundary spec? | HTTP uses TCP, not UDP; it has its own state machine, rate limits, and compression path (miniz). Separation is cleaner. |
| OQ-6 | **Master server list placement**: `masterlist.c` depends on netchan and server state (`sv_lan`, `sv_nat`). Should it be part of the server subsystem or the networking module? | Legacy places it in `engine/common`; the server is its only consumer. |
| OQ-7 | **bzip2 / LZSS in dedicated builds**: The rewrite should decide whether to conditionally compile compression or always include it as a no-op. | Keeping conditional compilation preserves code-size benefit on dedicated servers. |
| OQ-8 | **Single-threaded constraint on `net_from`**: Future async I/O would require `net_from` to become a per-call output parameter or a thread-local. This should be decided at Chunk 2 design time before any implementation. | A clean API would pass `from` through the call stack rather than storing it globally. |
