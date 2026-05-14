# Server Sound Message Baseline

Phase 76 covers `SV_BuildSoundMsg()` in `engine/server/sv_game.c`. The safe
migration boundary is the protocol payload shape for `svc_sound` and
`svc_restoresound`; legacy code still owns sample lookup, sentence parsing,
diagnostics, entity lookup, multicast routing, and save/restore extra data.

## Callers

`SV_StartSound()` builds dynamic or static sound messages into `sv.multicast`
and then calls `SV_Multicast()`. It chooses multicast destination from the
input flags, channel, Quake-compatibility mode, client count, and predicted
local-player filtering.

`pfnEmitAmbientSound()` uses `CHAN_STATIC`, optionally marks loading-time
sounds with `SND_SPAWNING`, builds into `sv.multicast`, and multicasts to
`MSG_INIT` or `MSG_ALL`.

`pfnBuildSoundMsg()` is a game-DLL callback wrapper around
`SV_BuildSoundMsg()`.

`SV_RewriteMessage()` can rewrite a GoldSrc `svc_spawnstaticsound` into the
modern sound message format.

`sv_save.c` uses `SND_RESTORE_POSITION` to write `svc_restoresound` into
`sv.signon`, then writes restore-only extra fields after `SV_BuildSoundMsg()`
returns: word index, sample position, and forced end.

## Legacy Build Rules

Before writing the payload, `SV_BuildSoundMsg()`:

- clamps volume to `0..255` after reporting out-of-range input;
- clamps attenuation to `0..4` after reporting out-of-range input;
- clamps channel to `0..7` after reporting out-of-range input;
- clamps pitch to `0..255` after reporting out-of-range input;
- rejects empty or null samples;
- parses `!<number>` as a sentence; indexes above
  `MAX_SOUNDS_NONSENTENCE` also set `SND_SEQUENCE` and subtract that offset;
- parses `#<number>` as a sentence sequence;
- maps samples beginning with `*` to `CHAN_STREAM` before sound-index lookup;
- resolves normal samples through `SV_SoundIndex()` and rejects index zero;
- resolves invalid entity pointers to entity index zero.

The emitted command is:

- `svc_sound` when `SND_RESTORE_POSITION` is not set;
- `svc_restoresound` when `SND_RESTORE_POSITION` is set.

Network flags are based on the caller flags with additional optional-field bits
set when values differ from defaults:

- `SND_VOLUME` when volume is not `255`;
- `SND_ATTENUATION` when attenuation is not `ATTN_NONE`;
- `SND_PITCH` when pitch is not `PITCH_NORM`.

Before serialization, `SND_RESTORE_POSITION`, `SND_FILTER_CLIENT`, and
`SND_SPAWNING` are cleared because they are routing/build-time flags rather
than client payload flags.

## Payload Layout

After the command byte written by `MSG_BeginServerCmd()`, the payload is:

- `flags`: `MAX_SND_FLAGS_BITS` unsigned bits;
- `sound_idx`: `MAX_SOUND_BITS` unsigned bits;
- `chan`: `MAX_SND_CHAN_BITS` unsigned bits;
- optional volume byte when `SND_VOLUME` is set;
- optional attenuation byte when `SND_ATTENUATION` is set, encoded as
  `min(attn * 64, 255)`;
- optional pitch byte when `SND_PITCH` is set;
- `entityIndex`: `MAX_ENTITY_BITS` unsigned bits;
- origin XYZ with `MSG_WriteVec3Coord()`.

Coordinate encoding depends on `ENGINE_WRITE_LARGE_COORD`:

- default mode writes `(int)(value * 8.0f)` as signed 16-bit values;
- large-coordinate mode writes `Q_rint(value)` as signed 16-bit values.

`svc_restoresound` readers expect save/restore to append restore-only data
after this shared payload. Phase 76 does not move those fields.

## Compatibility Notes

- The helper must not perform `SV_SoundIndex()` or sentence lookup; those depend
  on legacy resource tables and diagnostics.
- The helper must not decide multicast destination; that depends on caller
  state, prediction filtering, Quake compatibility, and `SV_Multicast()`.
- Legacy overflow semantics stay with `sizebuf_t`. The adapter reports the new
  bit cursor and overflow state back to the legacy buffer.
- The GoldSrc static-sound rewrite path remains a caller of
  `SV_BuildSoundMsg()`, so it benefits from the shared payload writer without
  changing rewrite policy.
