# Server Voice Relay Baseline

`SV_ParseVoiceData()` in `engine/server/sv_client.c` handles `clc_voicedata`
from a client and relays `svc_voicedata` packets to eligible recipients.

## Incoming Packet

The server reads, in order:

| Field | Legacy read | Notes |
| --- | --- | --- |
| loopback | `MSG_ReadByte()` | Non-zero requests local playback. |
| frames | `MSG_ReadByte()` | Forwarded unchanged. |
| size | `MSG_ReadShort()` assigned to `uint` | Negative wire values become huge unsigned values and fail the size guard. |
| payload | `MSG_ReadBytes()` | Read before voice-enabled/spawned gates. |

Payloads larger than the local 4096-byte receive buffer are logged as invalid
and drop the sending client before payload bytes are read.

## Relay Gates

After reading a valid-size payload, legacy behavior is:

- return when `sv_voiceenable` is false;
- return when the sending client is not `cs_spawned`;
- call `SV_Physics()->pfnVoiceData()` when available, and return when it
  reports that it handled the packet;
- suppress relay in single-player unless `sv_voice_singleplayer` is enabled.

The game DLL physics callback remains legacy-owned because it is an ABI surface
and may inspect or consume the payload.

## Recipient Rules

The sender is always considered a local recipient. Other recipients must be at
least `cs_connected` and their bit must be present in the sender's `listeners`
mask. The legacy datagram-capacity check uses `payload_size + 6` before the
local no-loopback path collapses its outgoing payload to zero bytes. Phase 74
preserves that ordering as a compatibility quirk.

The outgoing Xash `svc_voicedata` payload is:

| Field | Legacy write | Notes |
| --- | --- | --- |
| sender index | `MSG_WriteByte()` | Zero-based client index. |
| frames | `MSG_WriteByte()` | Forwarded unchanged. |
| length | `MSG_WriteShort()` | May be zero for local no-loopback ack. |
| payload | `MSG_WriteBytes()` | Omitted when length is zero. |

`MSG_BeginServerCmd(..., svc_voicedata)`, recipient iteration, and destination
datagram ownership remain in legacy code.
