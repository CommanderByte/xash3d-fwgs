# Deep Dive: Legacy Networking Core — `net_ws` / `net_chan` / `net_buffer` + OOB, split-packet, LZSS, master query

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. **Scope: the transport / netchan / buffer core**
— the parts the delta encoder does NOT cover. The delta engine (`net_encode.c`,
DT_\* tables, field-codec math, Xash-vs-GoldSrc delta dialects,
`Delta_AddEncoder`, baselines) has its own narrow-and-exact companion,
[deep-dive-delta-encoder.md](deep-dive-delta-encoder.md) — read that for
anything delta-shaped; it is not duplicated here. Wide survey coverage lives in
[engine-common-and-platform.md](engine-common-and-platform.md). Line numbers are
against the working tree on 2026-07-06; behaviour references, not design
constraints. Everything here is **legacy** — read it for the on-wire framing,
the reliability algorithm, and the socket-lifecycle contract, not as a rewrite
blueprint (the rewrite chose a `NetworkContext` pimpl + injected
`IPlatformSockets` + per-`Netchan` design — see §7 As-built mapping).*

Primary sources:

- `engine/common/net_ws.c` (~1900 lines) + `net_ws.h` / `net_ws_private.h` —
  UDP/TCP socket lifecycle, loopback ring, split/reassembly, fakelag, async DNS
- `engine/common/net_chan.c` (~1800 lines) + `netchan.h` — reliable-sequenced
  channel: fragmentation, flow control, acknowledgement, compression
- `engine/common/net_buffer.c` / `.h` — `sizebuf_t` bit/byte serialiser
- `engine/common/common.c` — `LZSS_Compress` / `LZSS_Decompress` /
  `LZSS_IsCompressed` / `LZSS_GetActualSize`
- `engine/common/masterlist.c` — master-server heartbeat/query UDP protocol
- `common/netadr.h` — the packed 20-byte `netadr_t`

______________________________________________________________________

## 0. Headline findings (read first)

1. **All socket I/O is confined to three functions.** `NET_GetPacket`,
   `NET_SendPacket`, and `NET_SendPacketEx` are the *only* places that touch a
   real socket handle. Everything above them (netchan, master list, the game
   protocol) speaks through these. This is a deliberate seam so a future async
   I/O thread can be dropped in — the rewrite preserved it exactly (three-entry
   transport seam through `IPlatformSockets`).
2. **`net_from` is a re-entrancy trap.** The source address of the packet being
   processed is a file-static global (`net_from`), overwritten by every
   `NET_QueuePacket` / `NET_LagPacket`. All legacy packet processing is
   single-threaded because of it. The rewrite **deleted** it (OQ-8) — `from` is
   an out-parameter — which is the single change that makes the NetIO-thread
   split source-compatible later.
3. **There are two independent framing dialects on the wire, chosen per
   connection**, not per packet: Xash native and GoldSrc. They differ in the
   split-packet `packet_id` width (short high/low-byte vs 4-bit nibbles), in
   qport presence, and in munge obfuscation. The receiver picks the dialect
   from `connprotocol_t`. (The *delta* dialect split is a third, orthogonal
   axis — see the delta deep-dive.)
4. **Only the server fragments outgoing UDP.** `NET_SendLong` splits packets on
   the `NS_SERVER` side only; clients never fragment (client→server payloads
   are small by design). There is one global `LONGPACKET net.split` reassembly
   slot — only one fragmented packet can be in flight per process at a time.
5. **The reliable guarantee is a single-buffer resend, not a sliding window.**
   Netchan keeps *one* `reliable_buf`; on a detected drop (the remote acks a
   sequence past the last reliable transmit without the matching reliable bit)
   the *entire* buffer is resent. This is the Quake/GoldSrc reliability scheme —
   simple, and wire-frozen.
6. **The bit codec is the whole protocol's foundation.** `sizebuf_t` +the
   `MSG_*` family serialise every byte on the wire: bit-packed integers,
   1/8-unit fixed-point coords, quantised angles, and the `'%'→'.'` string
   sanitisation. It is a POD struct with free functions and a sticky overflow
   flag; the rewrite turned it into the value-semantic `MessageBuf` class.
7. **Compression is optional and link-selected.** LZSS (in `common.c`) and
   bzip2 are fed by netchan only when the `NETCHAN_USE_LZSS` / `_BZIP2` flag is
   set; dedicated builds compile them out. LZSS has its own `LZSS` wire magic +
   8-byte header; bzip2 rides netchan's own framing with no magic of its own.

______________________________________________________________________

## 1. Transport — `net_ws.c`

### 1.1 The three-function socket seam

| Function | Role |
|----------|------|
| `NET_GetPacket(sock, &from, data, &len)` | Pull one packet: loopback ring first, then round-robin IPv4/IPv6 `recvfrom`; applies fakelag; reassembles a completed split. Returns `qboolean`. |
| `NET_SendPacket(sock, len, data, to)` | Send one datagram; loopback for in-process addresses, else `sendto`. |
| `NET_SendPacketEx(sock, len, data, to, splitsize)` | Send with optional server-side SPLITPACKET fragmentation (`NET_SendLong`). |

`sock` is `NS_CLIENT` or `NS_SERVER` (`netsrc_t`). The `net_state_t net`
file-static holds `ip_sockets[NS_COUNT]`, `ip6_sockets[NS_COUNT]`, the loopback
rings, the lag queues, the split-reassembly slot, and the state flags. Socket
open/close happens in `NET_Config(multiplayer, changeport)` — open when
`multiplayer` goes true, close on false; a no-op in single-player.

### 1.2 Loopback ring

`NET_SendLoopPacket` writes into `loopbacks[sock ^ 1]` — a send on `NS_CLIENT`
is read by `NS_SERVER` and vice-versa (simulates a real round trip in one
process). Ring depth is `MAX_LOOPBACK = 4` slots per side.

### 1.3 Split-packet / long-packet reassembly

Oversized datagrams are fragmented under the `NET_HEADER_SPLITPACKET` (`-2`)
discriminator. Two framings:

- **Xash `SPLITPACKET`**: `packet_id` is a `short` — high byte = packet_number,
  low byte = packet_count; up to `NET_MAX_FRAGMENTS` (~506).
- **GoldSrc `SPLITPACKETGS`**: `packet_id` is an `unsigned char` — high nibble =
  packet_number, low nibble = packet_count; max `NET_MAX_GOLDSRC_FRAGMENTS` = 5.

`SPLITPACKET_MIN_SIZE = 508` (RFC 791 floor), `SPLITPACKET_MAX_SIZE = 64000`.
There is one global `net.split` slot; a new sequence number resets it.

### 1.4 Fakelag / fakeloss

`NET_AdjustLag` / `NET_LagPacket` implement `net_fakelag` (added latency via a
doubly-linked per-side queue) and `net_fakeloss` (drop counter). Guarded:
non-developer builds silently reset `net_fakelag` to 0, and fakelag requires
`sv_cheats` + developer mode.

### 1.5 Async DNS

`NET_StringToAdrNB` fires **one** background resolver thread on first call and
returns `NET_EAI_AGAIN` until the result is ready; only one hostname resolves
at a time (two mutexes `mutexns` / `mutexres`). The caller polls on subsequent
frames. `NET_StringToAdr` is the blocking variant.

### 1.6 `netadr_t` (packed 20 bytes, `common/netadr.h`)

```c
#pragma pack(push, 1)
struct netadr_s {          // total 20 bytes
    uint16_t type;         // overlaps ip6_0[0..1] for IPv6 disambiguation
    uint8_t  ip6_0[2];
    union { uint8_t ip6_1[14];
            struct { union { uint8_t ip[4]; uint32_t ip4; }; uint8_t ipx[10]; }; };
    uint16_t port;
};
#pragma pack(pop)
```

Invariant: for non-IPv6 addresses `ip6_0[0] == ip6_0[1] == 0`;
`NET_NetadrType` / `NET_NetadrSetType` enforce it. `NET_AdrToString` returns a
**static** char buffer (the classic re-entrancy hazard the rewrite removed).

______________________________________________________________________

## 2. Netchan — `net_chan.c`

### 2.1 The header

`HEADER_BYTES = 8 + MAX_STREAMS * 13 = 34`; `MAX_STREAMS = 2` (a normal data
stream and a file-download stream). The two sequence words `w1` / `w2` carry
the outgoing/incoming sequence numbers and the reliable bits (high bit of `w1`
= reliable-message-present; high bit of `w2` = reliable-ack). GoldSrc also
writes a `qport` word on client→server packets; Xash protocol 49 likewise sends
a qport; GoldSrc-compat mode (`gs_netchan`) omits it on that path.

### 2.2 Reliability

One `reliable_buf` per channel. `Netchan_TransmitBits` assembles: header +
(reliable payload if the last one is unacked or a new one is queued) +
unreliable payload, gated by `Netchan_CanPacket` (the rate/choke gate).
`Netchan_Process` decodes the header, drops stale/duplicate sequences, advances
`incoming_sequence` / `incoming_acknowledged`, and — on a drop — the sender
resends the whole `reliable_buf`. `net_drop` counts drops since last poll.

### 2.3 Fragmentation

`Netchan_CreateFragments` / `Netchan_CreateFileFragments[FromBuffer]` split a
large reliable message into `fragbuf_t` records allocated from `net_mempool`
(a `poolhandle_t`). `Netchan_FragSend` flushes the pending queue;
`Netchan_CopyNormalFragments` / `Netchan_CopyFileFragments` reassemble on the
receive side. File fragments carry a filename; the copy path must reject
path traversal.

### 2.4 Flags

`NETCHAN_USE_MUNGE` (trivial XOR obfuscation for GoldSrc — not security),
`NETCHAN_USE_BZIP2`, `NETCHAN_USE_LZSS` (mutually exclusive), `NETCHAN_GOLDSRC`
(selects GoldSrc wire framing). Compression flags are ignored at link time in
dedicated builds.

______________________________________________________________________

## 3. Message codec — `net_buffer.c`

`sizebuf_t` is a POD: `{ const char *pDebugName; byte *pData; bool bOverflow;
int iCurBit; int nDataBits; }`. All I/O is free functions:

- **Bit layer**: `MSG_StartBitWriting` / `MSG_WriteOneBit` /
  `MSG_WriteUBitLong` / `MSG_WriteSBitLong` (and read duals). Bits pack LSB-first
  within a byte, bytes little-endian.
- **Byte layer**: `MSG_WriteByte/Short/Long/Float/String`, read duals.
  `MSG_WriteString` writes the NUL terminator; `MSG_ReadStringExt` decodes every
  `'%'` as `'.'` (format-specifier defense — wire-parity-sensitive).
- **Quantised reals**: `MSG_WriteCoord` (1/8-unit fixed-point in an int16),
  `MSG_WriteBitAngle` (angle→`num_bits`), vec3 helpers.
- **Overflow**: past-end writes set `bOverflow` and drop; past-end reads set it
  and return 0/empty. No exceptions; callers check `MSG_CheckOverflow`.

The `iAlternateSign` flag (set inside `MSG_StartBitWriting` brackets) flips
signed fields to sign-magnitude — this is the mechanism behind the GoldSrc
"broken signed integers" delta quirk (detailed in the delta deep-dive §3).

______________________________________________________________________

## 4. Out-of-band + packet-header magic

Read *before* netchan is consulted (`common/protocol.h`):

| Constant | Value | Meaning |
|----------|-------|---------|
| `NET_HEADER_OUTOFBANDPACKET` | `-1` (0xFFFFFFFF) | Connectionless (`FF FF FF FF` + ASCII command) |
| `NET_HEADER_SPLITPACKET` | `-2` (0xFFFFFFFE) | Fragmented oversized packet |
| `NET_HEADER_COMPRESSEDPACKET` | `-3` (0xFFFFFFFD) | Compressed (rare) |

`Netchan_OutOfBand` / `Netchan_OutOfBandPrint` frame connectionless datagrams
(connect handshakes, `getchallenge`, `rcon`, master queries). Game DLLs and
third-party tools parse these discriminators directly.

______________________________________________________________________

## 5. LZSS — `common.c`

The optional netchan compressor. Wire format: a **`LZSS` magic** word +an
8-byte header carrying the uncompressed size, then a command-byte-driven stream
(`window_size = 4096`, `lookahead = 16`, `lookshift = 4`). `LZSS_IsCompressed`
tests the magic; `LZSS_GetActualSize` reads the declared size;
`LZSS_Compress` refuses (returns "no gain") when compression does not save
space, and the caller sends uncompressed. The magic, header, and algorithm
parameters are part of the protocol contract — frozen.

______________________________________________________________________

## 6. Master-server list — `masterlist.c`

UDP OOB protocol for the in-game server browser and heartbeat. A server sends a
heartbeat (`FF FF FF FF 'q' '\n'`) to each configured master on an interval and
a shutdown notice (`FF FF FF FF 'b' '\n'`) on exit; the browser queries masters
for the server list. LAN-only servers skip the send. This is a **satellite** of
the transport layer (Q-11 score 1 → same target) — it owns no sockets of its
own, it frames OOB packets that only the transport layer can send.

______________________________________________________________________

## 7. As-built mapping (legacy → `xash3dpp/networking`)

| Legacy (C) | xash3dpp (C++23) | Notes |
|------------|------------------|-------|
| `net_state_t net` file-static | `NetworkContext::Impl` members | no networking file-scope mutable state (P-3) |
| `net_from` global | `NetAddress&` out-param on `get_packet` | **eliminated** (OQ-8) — the G-2 NetIO precondition |
| `NET_GetPacket`/`SendPacket`/`SendPacketEx` | `NetworkContext::get_packet`/`send_packet` (+`SocketKind`) | three-fn socket seam preserved; real I/O via injected `IPlatformSockets` |
| raw `ip_sockets[]`/`ip6_sockets[]` | `std::array<platform::OsSocket,2> os_sockets` | RAII handles; opened by `config(true)` |
| `loopbacks[sock^1]` ring | `LoopbackTransport` | same `sock^1` cross-wiring; `MAX_LOOPBACK = 4` |
| `net.split` global + `split_flags[]` | `std::array<SplitReassembler,2>` | per-side; `net_splitpacket_max_fragments = 256` (uint8 packet_id range) |
| lag doubly-linked queues | `std::array<LagQueue,2>` | drop policy is **caller-supplied** — no networking RNG |
| `NET_AdrToString` static buffer | `to_string(NetAddress, std::span<char>)` | **caller-owned** buffer; static-buf hazard removed |
| `netadr_t` intra-engine | `NetAddress` value type (invariant-enforcing) | packed `netadr_t` only at frozen-ABI edges (OQ-3) |
| `netchan_t` + `net_mempool` global | `Netchan` (one per peer) + `PoolHandle("networking")` | pool owned by `NetworkContext`; channel borrows it |
| `NETCHAN_GOLDSRC`/qport `#ifdef` | `IProtocolDriver` (GoldSrc 48 / Xash 49) | per-channel wire selection; zero `#ifdef` in netchan (Q-14) |
| `sizebuf_t` + `MSG_*` free fns | `MessageBuf` value class | caller-owned buffer, sticky overflow, no globals |
| `iAlternateSign` global flag | `SignEncoding` explicit parameter | see delta deep-dive §3 |
| `LZSS_*` in `common.c` | `networking::lzss::{compress,decompress,…}` | `LZSS` magic + params frozen; value-semantic |
| bzip2 path | `networking::bz2::*` (link-selected, OQ-7) | real backend **deferred** until `3rdparty/bzip2` in CMake |
| `masterlist.c` | `master_list.cpp` + `IMasterListClient`/`Config` | satellite in-target (OQ-6); `create_master_list_client` factory |
| async DNS `nsthread` | *(deferred)* | OQ-4 one-thread model not yet ported (no `dns.cpp`) |
| HTTP downloader | *(separate target, decided-not-built)* | `xash3dpp_http` (OQ-5, Q-11) |

**Threading posture** is documented in
[../threading-analysis/networking-threading.md](../threading-analysis/networking-threading.md)
and the boundary's `## Extension axes (Q-21)` flip table — the whole transport
stack is `T_NetIO` single-thread-by-contract (55 `compliance-allow(thread-assert)`
sites, 0 runtime asserts until the G-2 split). **Delta encoding** is out of
scope here — see [deep-dive-delta-encoder.md](deep-dive-delta-encoder.md).
