# Server Service Message Baseline

Phase 73 covers a small set of server-to-client service messages whose payloads
are compact enough to extract without moving broader server state.

## Covered Writers

| Legacy writer | Command | Payload | Legacy-owned context |
| --- | --- | --- | --- |
| `SV_FailDownload()` | `svc_filetxferfailed` (`49`) | failed filename string | empty filename guard, client message destination |
| `SV_BuildReconnect()` | `svc_stufftext` (`9`) | `reconnect\n` string | caller-selected destination buffer |
| `SV_UpdateClientView()` | `svc_setview` (`5`) | 16-bit unsigned view entity | `pViewEntity`/edict selection |
| `SV_TogglePause()` | `svc_setpause` (`24`) | one pause bit | background-server guard, pause state mutation, broadcast text |
| `SV_WriteVoiceCodec()` | `svc_voiceinit` (`52`) | codec string plus 8-bit quality | voice enable gates and `sv_voicequality` cvar |

`pfnSetView()` in `sv_game.c` shares the `svc_setview` payload, but game DLL
edict validation and client lookup remain legacy-owned.

## Compatibility Notes

- Xash protocol pause messages write one bit after `svc_setpause`; GoldSrc and
  Quake protocol readers consume a byte on their own protocol paths.
- `svc_setview` uses `MSG_WriteWord()`, so values are transmitted as the lower
  16 unsigned bits.
- `SV_FailDownload()` still returns without writing anything for an empty or
  null filename.
- `SV_WriteVoiceCodec()` still uses `VOICE_DEFAULT_CODEC` from the legacy voice
  header and `(int)sv_voicequality.value`; the modern helper has a defensive
  default only for null or empty direct calls.
- Legacy routing keeps `MSG_BeginServerCmd()` at the call sites so
  `net_send_debug` command-name output is preserved.

## Modern Boundary

`src/engine/server/server_service_messages.cpp` owns only the target-neutral
byte and bit layout. The C adapter receives legacy `sizebuf_t` storage and
returns the updated cursor/overflow state. Server state, entity lookup, cvars,
client choice, and message ownership remain in the legacy server files.
