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
| `net_splitpacket_max_fragments` | 256 | SplitReassembler slot count (uint8_t packet_id field range) |
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

### Delta encoder — implementation notes (2026-07-04)

The rewrite (`src/networking/delta/`, public API `networking/delta.hpp`)
reproduces `net_encode.c` byte-for-byte on the wire.  Deliberate seam shifts
and hardenings, each behaviour-neutral for well-formed traffic:

- **Command bytes are caller-supplied.** Legacy wrote `svc_deltatable` /
  `svc_deltamovevars` internally via `MSG_BeginServerCmd`; `DeltaTables::
  write_description` / `write_delta_movevars` take the raw command value as a
  parameter (wire-identical; xash3dpp has no `svc_*` enum yet — it arrives
  with the game-protocol layer).
- **`IBaselineResolver`** replaces `MSG_ReadDeltaEntity`'s direct reads of
  `clgame.static_entities` / `cls.packet_entities` / `cl.instanced_baseline`.
  The client subsystem (Chunk 9+) implements the three lookup branches; the
  codec only hands over the signed 7-bit offset and the entity kind.
- **`IDeltaWireFormat` selection axis (Q-14 documentation requirement):** the
  Xash per-field-mark-bit format and the GoldSrc byte-group-mask format are
  sibling implementations behind one private seam, selected by
  `IProtocolDriver::delta_tables()` (`DeltaTableSet`).  A future wire format
  is one new sibling TU plus one factory case in
  `private/networking/delta/wire_format.hpp`; table management and the struct
  codec call sites are untouched by construction.
- **Signed payload encodings differ by wire format** (`SignEncoding` in the
  field codec): GoldSrc message parsing brackets delta payloads in
  `MSG_StartBitWriting`/`MSG_EndBitWriting`, flipping `MSG_Write/ReadSBitLong`
  to sign-bit-first + magnitude; the Xash path is plain two's complement.
  `parse_table_gs` also byte-aligns the cursor afterwards like
  `MSG_EndBitWriting`; alignment for the general GS batch codec is the
  message-parser's responsibility (the legacy brackets live in the caller).
- **`DeltaField*` is the game-DLL token** where legacy passed `delta_s*`.
  HLSDK-conformant DLLs treat it as opaque and mutate only through the
  engine's find/set/unset helpers; a DLL that dereferences `delta_s*`
  directly would misread the layout.  Revisited by the Chunk 6 ABI-shim
  audit before `pfnDeltaAddEncoder` is exported.
- **Wire-reachable hardenings (legacy UB removed, behaviour preserved):**
  `parse_table_field` bounds-checks the 4-bit tableIndex (legacy blind-indexed
  `dt_info[]`); the GS mask loops bound `bits[i>>3]` (legacy indexed a fixed
  `bits[8]` unchecked); `Sys_Error`/`Host_Error` sites return `false` after a
  `core::log` Error per Q-5 (`init` on missing delta.lst, unknown struct,
  missing `{`, bad table index, GS `numFields > maxFields`, bad entity
  numbers).
- **Stats:** `DeltaTables::stats()` exposes Tier-1 `tables_parsed` /
  `structs_encoded` / `structs_decoded` atomics, plus `XASH_STATS`-gated
  changed-field and rollback counters.

## Known Deviations

- `register_encoder` (legacy `Delta_AddEncoder`) returns `bool` instead of
  silently logging; the future `enginefuncs_t` shim discards the result to
  preserve the void ABI signature.
- The movevars fallback re-assertion `numFields = ARRAYSIZE(pm_fields) - 4`
  is an `XASH_ASSERT` consistency check rather than a truncating assignment
  (the rewrite appends exactly 27 fields).
- `write_delta_entity` refuses to write on a bad entity number and returns
  `false` where legacy raised `Host_Error` mid-frame.

---

## Pluggable game protocol per client

The four packet-header magic numbers, the SPLITPACKET / SPLITPACKETGS framing
formats, the netchan reliability scheme, and the delta-encoder field tables
are **wire-frozen for the default GoldSrc-compatible protocol** and must match
the legacy constants byte-for-byte. They are not negotiable for clients that
identify as vanilla GoldSrc or Xash3D protocol 48/49.

The rewrite must however leave the door open to **per-client protocol
selection** — newer (or experimental) client builds may negotiate a different
protocol on connect. This is implemented as a `ProtocolDriver` interface
selected at `Netchan_Setup` time:

```cpp
struct IProtocolDriver {
    virtual void   write_packet_header(MessageBuf& out, const FrameMeta&) noexcept = 0;
    virtual Result<FrameMeta> read_packet_header(MessageBuf& in) noexcept = 0;
    virtual SplitFormat split_format() const noexcept = 0;   // Xash | GoldSrc | NewV2
    virtual DeltaTableSet delta_tables() const noexcept = 0; // identifies field table set
};
```

- The default driver (`GoldSrcProtocolDriver`) is always linked and produces
  bit-exact legacy output.
- Alternative drivers register a new `connprotocol_t` enumerant and a
  matching `IProtocolDriver` factory at `Netchan_Init` time.
- The selection is per-`netchan_t`, not global: a server may speak GoldSrc to
  one client and a newer protocol to another concurrently.
- `Netchan_Setup`'s existing `flags` argument is extended with a
  `protocol_driver` field rather than overloaded with more bit flags.

No alternative driver ships in Chunk 4; the seam exists so that a Chunk 4b
follow-up (or any later feature work) can add one without touching the core
netchan or delta code.

---

## Resolved Decisions

| # | Decision | Resolution and rationale |
|---|----------|--------------------------|
| OQ-1 | **Module decomposition** | **Single `xash3dpp_networking` target** containing transport, netchan, message codec, and delta encoder; each layer in its own `.cpp` translation unit so layer-level unit tests link against the same archive. Mirrors the single `NetworkContext` slot in `EngineContext` (decisions-architecture §Q-2). |
| OQ-2 | **`NetError` codes** | Initial enum: `WouldBlock, SocketInvalid, BindFailed, Overflow, SplitTooLarge, DnsAgain, DnsFailure, BadAddress, BufferTooSmall, NotInitialised`. Public APIs that can fail return `std::expected<T, NetError>` per Q-5; the diagnostic message at the public-API boundary names the code by enumerant. New codes are added per-feature, not pre-emptively. |
| OQ-3 | **`netadr_t` wrapper** | Intra-engine code uses `xash::networking::NetAddress`, a value type that enforces the `ip6_0[0..1] == 0` invariant in its constructors and provides `from_string` / `to_string` / `compare` / `compare_mask`. At every frozen-ABI boundary (game DLL, client DLL `net_api_t`) the wrapper is converted to the packed 20-byte `netadr_t` POD by a small adapter — analogous to the `string_view` rule in Q-8. |
| OQ-4 | **DNS resolver threading** | Retain the one-thread-at-a-time dedicated-resolver model for now (the worker pool / `JobToken` lands later in the host-side work). The single-in-flight contract is preserved exactly; migration to the worker pool is a follow-up that does not change the public `string_to_adr_nb` API. |
| OQ-5 | **HTTP downloader placement** | **Separate `xash3dpp_http` target** with its own boundary spec. HTTP is TCP-based, has its own state machine, rate limits, and a gzip path (miniz) that overlaps only at the dependency level — not the protocol level — with netchan compression. `HTTP_Run` is invoked from the host's per-frame tick, not from `NetworkContext::tick`. Note: a later pass will group small same-layer targets into folders (e.g. `xash3dpp/src/net/{networking,http,master_list}`) — see decisions-architecture Q-11. |
| OQ-6 | **Master server list placement** | **Stays in networking** (`master_list.cpp` inside `xash3dpp_networking`). The list speaks UDP OOB packets that only netchan can frame; the server is its only logical consumer but the I/O lives at the same layer as transport. The networking subsystem exposes a small `IMasterListClient` interface that `server/` configures (`set_lan`, `set_nat`, `heartbeat()`). This follows the satellite-module placement paradigm recorded in decisions-architecture Q-11. |
| OQ-7 | **bzip2 / LZSS in dedicated builds** | Keep conditional compilation. The `XASH_NET_COMPRESSION` CMake option (default ON for client builds, OFF for `XASH_DEDICATED`) selects `compress_bz2.cpp` + `compress_lzss.cpp` vs `compress_null.cpp` at link time — same link-time-selection paradigm as `XASH_GOLDSRC_COMPAT`, zero `#ifdef` in core. |
| OQ-8 | **`net_from` re-entrancy** | The file-static `net_from` global is eliminated. `from` is a `NetAddress*` out-parameter on `NET_GetPacket`, `queue_packet`, `lag_packet`, and threaded through to every layer above. This is mandatory: it is the only design change that makes the threading-model `T_NetIO` migration source-compatible later. |

All eight open questions are now decided. Any further design contention belongs
in a new `OQ-N` entry above this table.

---

## Satellite components

Sub-features of the networking layer evaluated against the
`decisions-architecture.md §Q-11` satellite-placement test. Score is the count
of separation criteria met out of 5; ≥ 2 → separate target.

| Feature | Criteria met | Score | Verdict |
|---------|--------------|-------|---------|
| **HTTP downloader** | (a) independent state machine, (b) different external dep (TCP vs UDP), (c) useful without parent (assets, not netchan) | 3 | **separate** target `xash3dpp_http` |
| **Master-server list** | (d) small interface to parent, only (a) independent state machine fully matches | 1 | **same** target — lives in `xash3dpp_networking` as `master_list.cpp`, exposes `IMasterListClient` |
| **Async DNS resolver** | (d) small interface to parent (string → NetAddress); shares transport's threading model | 0 | **same** target — lives in `dns.cpp` inside `xash3dpp_networking` |
| **Default GoldSrc protocol driver** | always linked; selected per-`netchan_t` via `IProtocolDriver`, not a CMake option | n/a | **same** target — `protocol_driver_goldsrc.cpp` |
| **Compression backends (bzip2 / LZSS)** | link-time-selected by `XASH_NET_COMPRESSION` (Q-7 pattern); not a separation candidate | n/a | **same** target — `compress_{bz2,lzss,null}.cpp` |

---

## Rewrite — public injectable interfaces

The following injectable interfaces were moved from the private include tree to
the public include tree (structural compliance fix, detail-audit):

| Public header | Interfaces |
|---|---|
| `include/xash3dpp/networking/master_list.hpp` | `IMasterListConfig`, `IMasterListClient` |
| `include/xash3dpp/networking/protocol_driver.hpp` | `IProtocolDriver`, `IProtocolDriverRegistry`, `SplitFormat`, `DeltaTableSet`, `FrameMeta` |

Callers implementing these interfaces (e.g. the server layer for `IMasterListConfig`)
should include from the public path directly. The private-tree headers are now
redirect stubs.

## Threading

See [docs/threading-analysis/networking-threading.md](../threading-analysis/networking-threading.md) for the full hazard inventory and caller-contract checklist.

**TL;DR:** the entire transport stack (NetworkContext, Netchan, PacketPool, LagQueue, SplitReassembler, MasterListClient) is confined to the `T_NetIO` thread role. Only `NetworkContext::stats()` Tier-1 atomic counters are safe to read from other threads. There are no internal mutexes; single-thread access is the caller's contract.

### QN/Q-22 annotation adjudication (6B S6 retrofit)

The thread-role assert recommended by the threading analysis (Rec. #1) is
**deliberately not wired in yet**: the `T_NetIO` thread is not split from
`ThreadRole::Main` (a G-2 future), so `assert_thread_role(ThreadRole::Main)`
would contradict the documented `T_NetIO`-ready design, and asserting
`ThreadRole::NetIO` would fatal today's main-thread execution and the
role-less networking tests. All 55 flagged transport-stack mutators therefore
carry `compliance-allow(thread-assert)` at their definition, with a reason
drawn from the thread model:

- **Transport stack** (`NetworkContext`, `Netchan`, `LoopbackTransport`,
  `SplitReassembler`, `DeltaTables`): *T_NetIO single-thread caller contract —
  no internal sync; role unasserted until the NetIO split (G-2)*.
- **Stateless codecs / wire transforms** (delta codec, field codec, table
  wire, compat shims, OOB framing, LZSS/compress helpers): *pure transform,
  no thread affinity*.
- **Const wire-format / protocol-driver singletons**: *Safe-RO by
  construction*.
- **`MessageBuf`**: *thread-agnostic value type over a caller-owned buffer*.

When the NetIO thread is introduced (G-2), flip these to
`assert_thread_role(ThreadRole::NetIO)` and strike Rec. #1 in the threading
analysis in the same commit.

**QO literal classification (finish_check item 2):** the four literals the
limits scan cannot auto-classify are all frozen structural/algorithm
constants — not tunable limits or cvars: the IPv6 address length (`v6[16]`),
the LZSS sliding-window size (`window_size = 4096`) and its hash-bucket count
(`buckets[256]`), and the GoldSrc delta-descriptor name field (`fieldName[32]`,
part of the `sizeof(goldsrc_delta_t) == 56` ABI assert). They stay inline as
wire/ABI-frozen values.

## Source folder layout

`src/networking/` is divided into three functional sub-layers. Private headers
mirror the same structure under `include/xash3dpp/private/networking/`.

| Subfolder | Layer | Contents |
|-----------|-------|---------|
| `codec/` | Layer 2 (codec) | `compress_lzss.cpp`, `compress_bz2.cpp`, `compress_null.cpp`, `compressed_packet.cpp` |
| `wire/` | Layer 2 (wire) | `compat_goldsrc.cpp`, `compat_xash.cpp`, `oob_packet.cpp`, `protocol_driver_goldsrc.cpp` |
| `transport/` | Layer 1 | `lag_queue.cpp`, `loopback_transport.cpp`, `packet_pool.cpp`, `split_reassembler.cpp` |
| *(top-level)* | Layer 0 / Layer 3 | `address.cpp`, `message_buf.cpp`, `context.cpp`, `netchan.cpp`, `master_list.cpp` |

Tests mirror the same structure under `tests/networking/{codec,wire,transport}/`.

> **Lesson learned (Chunk 8 retrofit)**: subfolder layout should be planned
> at subsystem scaffold time (see `scaffold-subsystem.prompt.md` Step 3), before
> any `.cpp` files are written. Retrofitting subfolders after 40+ files are
> referenced in CMake is error-prone and requires renaming both source and header
> paths simultaneously.
