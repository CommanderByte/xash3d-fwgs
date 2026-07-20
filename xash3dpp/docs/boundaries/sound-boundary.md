# Sound Boundary Spec (Chunk 9)

> Assembled 2026-07-19 from six A1 recon fragments (R9.1 mixer, R9.2 soundlib,
> R9.3 VOX/mouth, R9.4 DSP, R9.5 client-state coupling, R9.6 SNDDMA/SoundAPI
> ABI) by the A2 spec-draft assembler. Legacy C engine is REFERENCE-ONLY.
> Sibling-owned surface (filesystem I/O, networking `svc_sound` wire,
> cmd_cvar registration mechanics, map_loader BSP leaf data, server) is cited,
> not re-derived, per the sibling-scope rule.

> Legacy sources surveyed: `engine/client/sound/{s_main,s_mix,s_vox,s_mouth,
> s_dsp,s_load,s_stream}.c` (~6,322 lines total), `engine/common/soundlib/
> {snd_utils.c,soundlib.h}`, `engine/client/soundlib/{snd_main,snd_wav,
> snd_mp3,snd_ogg_vorbis,snd_ogg_opus,ogg_filestream}.c` + `libmpg/*`,
> `common/sound_api.h`, `engine/platform/{platform.h,sdl2/s_sdl2.c,
> stub/s_stub.c}`, `engine/client/dll_int/cl_game.c`, `engine/client/cl_frame.c`.

______________________________________________________________________

## Responsibility

The sound module owns loading, mixing, spatializing, and outputting all game
audio: the channel-based mixer (dynamic/static/ambient channels, raw/voice
streaming channels), the sentence (VOX) word-sequencer, mouth-animation
amplitude tracking for lip-sync, the room-effects DSP chain (reverb/delay/
amod), the soundlib decode layer (WAV/MP3/OGG/Opus), and the SNDDMA platform
seam that hands a filled buffer to the OS audio device. It does **not** parse
the network `svc_sound` wire format (networking), does not resolve file
search paths (filesystem), does not own BSP leaf ambient data (map_loader),
and does not implement voice codec/network transport (voice, fenced out of
this pack per the A1 brief).

______________________________________________________________________

## External ABI contracts

### SoundAPI override surface (client-DLL sound seam)

`CL_SOUND_INTERFACE_VERSION = 1` (`sound_api.h:37`); the header is explicitly
marked experimental with no backward-compat guarantee. Negotiated once at
client-DLL init via `HUD_GetSoundInterface` (a Xash3D FWGS extension, not
stock GoldSrc — `cl_game.c:110`), called from `S_InitSoundAPI()`
(`cl_game.c:4135`), **not** from `s_main.c`'s own `S_Init`.

- **`sound_api_t`** — engine→client callback table, **5 slots**:
  `CL_GetEntitySpatialization`, `S_GetSfxByHandle`, `S_RawEntSamples`,
  `SND_ForceInitMouth`, `Voice_GetAudioInfo` (`s_main.c:1935-1941`,
  `sound_api.h:170-178`). (Correction to the assignment's assumed count: this
  is the correct, verified count of 5 — matches R9.1/R9.6.)
- **`sound_interface_t`** — client→engine callback table, versioned via a
  leading `int version`, **8 fn slots**: `pfnS_Init`, `pfnS_Shutdown`,
  `pfnS_UpdateSound`, `pfnS_PaintChannels`, `pfnS_UpdateChannel`,
  `pfnS_UpdateRawChannel`, `pfnS_Spatialize`, `pfnS_FreeSound`
  (`sound_api.h:181-194`). `ch == NULL` is the documented "channel freed"
  sentinel for both `Update*` callbacks.
- Fallback: on negotiation failure `S_InitSoundAPI` still probes
  `pfnS_Init(&snd)` and hands the client a raw pointer to the engine's
  `snd_globals_t` global (`s_main.c:1948,1967-1968`) — a legacy-shaped,
  capture-less fallback the rewrite must reproduce byte-for-byte for
  ABI-conformant client DLLs.
- Only `pfnS_PaintChannels` has an engine-side fallback (`S_PaintChannels`);
  every other hook is guarded-with-no-fallback (`s_main.c:1562-1565` vs.
  `1611-1612`, `561-563`, `167-173`, `207-212`; `s_load.c:235-237`).

**`channel_t` / `rawchan_t` layouts — layout-pinned, plumbing deferred.**
Both structs carry `uintptr_t engine_reserved[8]` + `uintptr_t
game_reserved[8]` as frozen ABI padding shared with the game DLL
(`sound_api.h:76-105,107-123`). `rawchan_t::s_rawend` is the **only**
`volatile`-qualified field across the whole ABI — a legacy hint that raw/
voice producers and the mix consumer race on the ring write cursor even in
legacy intent (`sound_api.h:115`). The rewrite pins these layouts exactly
(byte-for-byte field order incl. reserved padding) but does not wire live
game-DLL plumbing through them yet — deferred to the ABI-shim chunk per the
networking-boundary precedent for `DeltaField*`.

**Main-thread-bound note (threading-model §8.4):** `pfnS_PaintChannels` runs
the entire client-overridden mix on whatever thread calls `S_UpdateChannels`
— an ABI-v1 hook with no context/thread declaration. Because the SoundAPI
contract predates any thread-role concept, **the mixer design must not
preclude a main-pumped fallback**: if a client DLL overrides `pfnS_PaintChannels`,
the rewrite must be able to run the whole paint synchronously on T_Main
(never assume T_AudioDecoder is guaranteed available to a legacy-ABI client).

**`svc_sound` stays networking's** — the wire decode that turns a network
packet into a `S_StartSound` call is out of this spec's scope (sibling-scope
rule; networking owns all `svc_*` payloads).

### SNDDMA platform seam — corrected count: 5 SNDDMA + 4 VoiceCapture fns

`platform.h:390-402` declares:

| Group | Functions | Count |
|---|---|---|
| SNDDMA_* (live) | `Init`, `Shutdown`, `BeginPainting`, `Submit`, `Activate` | **5** |
| SNDDMA_* (dead, commented out) | `PrintDeviceName`, `LockSound`, `UnlockSound` | 0 (not live ABI) |
| VoiceCapture_* | `Init`, `Shutdown`, `Activate`, `Lock` | **4** |

(The assignment brief cited "3 VoiceCapture fns"; R9.6 found and verified 4
at `platform.h:399-402` — corrected here.)

`SNDDMA_BeginPainting`/`SNDDMA_Submit` are the SDL2 backend's
`SDL_Lock/UnlockAudioDevice` passthrough (`s_sdl2.c:184-200`) — the only
synchronization primitive preventing the SDL audio callback thread from
reading `snd.buffer` mid-write by the mixer in the legacy model.

**Finding: `s_stub` omits `SNDDMA_Activate`.** Every other backend
(sdl1/sdl2/sdl3/alsa) defines all 5 SNDDMA_* functions; the null/stub backend
(`s_stub.c`, `XASH_SOUND == SOUND_NULL`) defines `Init` (always returns
`false`), `Shutdown`, `BeginPainting`/`Submit` (empty bodies), but **does
not define `SNDDMA_Activate` at all** — confirmed absent by full-file read
and by a repo-wide grep showing definitions only in sdl3/sdl2/sdl1/alsa. Since
`SNDDMA_Init` always fails under the stub, `snd.initialized` never becomes
true and no known caller reaches `Activate` for this backend in practice —
but this is a real asymmetry the rewrite's null-audio-device target (the
ratified "null audio device only" decision) must either preserve (define
`Activate` as a no-op for symmetry) or explicitly accept as dead-code-shaped.
Recorded as SND-OQ-2-adjacent open item, not silently fixed.

### Compat scope (Q-12)

**SoundAPI v1 + GoldSrc mixer behaviour (bit-exact integer kernels).** The
rewrite's compat boundary is:

- **Frozen wire-exact:** the 12 mix-kernel variants' integer arithmetic
  (`>> (x-8)` volume law, nearest/lerp/pitch resample accumulators — R9.1),
  `CLIP16` guard-band clamp (`[SHRT_MIN+8, SHRT_MAX-8]`, not full int16
  range), the DSP delay/reverb/amod fixed-point math (`>>8`, `>>7`, `>>6`
  shifts — R9.4), the VOX word-grammar parser and punctuation-splitting rules
  (R9.3), the mouth amplitude running-average and data-dependent stride
  (`i += 80 + (data & 0x1F)` — R9.3). These must reproduce legacy output
  sample-for-sample for identical input.
- **Frozen ABI:** `CL_SOUND_INTERFACE_VERSION = 1`, the 5+8 function-table
  shapes, `channel_t`/`rawchan_t` layouts including reserved padding.
- **Not frozen:** the internal threading topology (T_Main-only in legacy →
  T_Main/T_AudioDecoder/T_AudioCallback split in xash3dpp), soundlib's
  internal decode-scratch storage shape (as long as decode output is
  byte-identical), and any P-3/P-4 introspection surfaces added on top.

______________________________________________________________________

## Interface (what the rest of the engine calls)

The planned `Sound` class surface, derived from R9.1's `S_*` census.

| Planned method | Legacy source | Purpose |
|---|---|---|
| `start_sound(pos,ent,chan,handle,fvol,attn,pitch,flags)` | `S_StartSound` (`s_main.c:626`) | Primary play entry: alter-then-start for STOP/CHANGE flags, pick dyn/static channel, spatialize, first-audibility drop |
| `restore_sound(...,sample,end,word_index)` | `S_RestoreSound` (`s_main.c:749`) | Save/restore variant; skips alter pre-pass, applies sample/forced_end offsets + sentence word_index |
| `ambient_sound(pos,ent,handle,fvol,attn,pitch,flags)` | `S_AmbientSound` (`s_main.c:872`) | Static-area-only allocation, always spatializes, chipmunk pitch mult |
| `start_local_sound(name,volume,reliable)` | `S_StartLocalSound` (`s_main.c:955`) | Registers + plays on the listener entnum; `reliable`→CHAN_STATIC else CHAN_AUTO |
| `stop_sound(entnum,channel,soundname)` | `S_StopSound` (`s_main.c:1451`, `GAME_EXPORT`) | Resolve name, `S_AlterChannel(...,SND_STOP)` |
| `stop_all_sounds(ambient)` | `S_StopAllSounds` (`s_main.c:1465`) | Reset to `MAX_DYNAMIC_CHANNELS`, free all channels, clear DSP state, zero soundfade |
| `update_frame(rvp)` | `S_UpdateFrame` (`s_main.c:1590`) | Publish listener pose (origin/forward/right/up/entnum) — becomes the `ListenerSnapshot` producer |
| `update_sound()` | `SND_UpdateSound` (`s_main.c:1607`) | Per-frame driver: fade, ambients, respatialize, raw spatialize, stream pump, then update_channels |
| `extra_update()` | `S_ExtraUpdate` (`s_main.c:1577`) | Mid-frame mix pump; guarded on `initialized` |
| `channels_snapshot()` (**P-4 accessor**) | `S_GetCurrentStaticSounds`/`S_GetCurrentDynamicSounds` (`s_main.c:975,1019`) | Typed read of live channel state for save/restore and future debug/MCP introspection — replaces direct `snd.channels[]` array access |
| `master_volume()` | `S_GetMasterVolume` (`s_main.c:115`) | Paint-time gain source: focus-mute, soundfade, menu-gate |
| `paint_channels(endtime)` | `S_PaintChannels` (`s_mix.c:542`) | Public mix entry; client-overridable via `pfnS_PaintChannels` |

### Cvar/command census (restricted flags noted)

Full census: 11 cvars registered in `S_Init` (`s_main.c:1980-1990`: `volume`,
`MP3Volume`, `_snd_mixahead`, `s_show`, `s_lerping`, `ambient_level`,
`ambient_fade`, `snd_mute_losefocus`, `s_test`, `s_samplecount`,
`s_warn_late_precache`) + 13 commands registered in `S_Init`
(`s_main.c:1998-2011`: `play`, `play2`, `playvol`, `stopsound`, `music`
(`CMD_OVERRIDABLE` — HLU SDK collision), `soundlist`, `s_info`, `s_fade`,
`soundfade`, `+voicerecord`, `-voicerecord`, `spk`, `speak`). **All commands
are unrestricted** except `music` (`CMD_OVERRIDABLE`); none carry
`CMD_RESTRICTED`/trust gating in legacy. `dsp_profile` (s_dsp.c) is the one
`Cmd_AddRestrictedCommand` in the whole subsystem.

______________________________________________________________________

## Dependencies (what this module calls)

### Client-state coupling — R9.5's three-group resolution

R9.5 inventoried every `cl.*`/`cls.*`/`refState.*`/`host.*`/`GI->*` read
across the sound tree and resolved it into three transport groups per the
ratified SND-OQ-1 shape (providers read ONLY on T_Main; POD snapshots ship
via the command stream):

1. **Ships-in-command-stream** (`ListenerSnapshot`, `MixGateSnapshot`,
   `RegistrationSnapshot` PODs) — listener pose (group a), ambient/frametime
   (group b), DSP waterlevel (group c), gating booleans (group f),
   registration-time data (group g). All captured on T_Main once per frame
   (or once per registration event) and shipped down the audio command
   stream; the mix/decoder thread never re-reads live `cl.*`/`host.*`.
2. **Provider-computed origins** (`IEntitySpatialProvider`, group d) — the
   `CL_GetEntitySpatialization`/`CL_GetMovieSpatialization` seam resolves a
   channel origin from client entity state; must execute ONLY on T_Main
   (entity array is mutated by netchan parse on T_Main — off-Main deref
   races the parse) and pushes resolved origins as channel-param updates
   down the command stream.
3. **Atomic mouth write-back** (`IMouthSink`, group e) — the mix computes
   `mouthopen` for voice/stream/raw channels and must marshal the value back
   to T_Main (or an atomic per-entity slot) rather than mutating
   `cl_entity_t.mouth` directly from the mix thread. This is the **only**
   write direction from sound back into client state.

See Threading section for the full per-field hazard table.

### Filesystem — soundlib I/O

Per the sibling-scope rule, soundlib routes all file access through
filesystem's owned surface: `FS_Open`/`FS_Read`/`FS_FileLength`/`FS_Close`
for header peeks, `FS_LoadFile`/`FS_Seek`/`FS_Tell`/`FS_Eof` for full loads
and streaming chunk scans (`snd_utils.c:91,96`, `snd_wav.c:146`). soundlib
owns its own memory pool (`host.soundpool`, `Mem_AllocPool`/`Mem_FreePool`,
`snd_utils.c:70,82`) and depends on `Platform_DoubleTime()` for resample
timing (`snd_utils.c:373,407` — platform's time surface, not audio, per
sibling-scope rule) and the shared CRC32 utility for the known-broken-WAV
allowlist (`snd_wav.c:358-370`).

______________________________________________________________________

## Owned state

The `snd` global (`snd_globals_t`, `s_main.c:39`, `sound_api.h:132-160`) —
mark what becomes `Sound`-class members:

| Legacy field | Becomes |
|---|---|
| `backend_name`, `buffer`, `format`, `initialized`, `samples`, `samplepos` | `Sound::dma_` (device-facing state; SNDDMA-owned) |
| `paintedtime`, `soundtime` | `Sound::mix_clock_` (mix-thread-private under new model) |
| `origin`, `forward`, `right`, `up`, `entnum` | replaced by `ListenerSnapshot` (P-2 published snapshot) — no longer a live-read member |
| `channels[MAX_CHANNELS]`, `total_channels`, `max_channels` (`* const`) | `Sound::channels_` — mutation crosses the P-1 MPSC audio command queue |
| `raw_channels[MAX_RAW_CHANNELS]`, `max_raw_channels` (`* const`) | `Sound::raw_channels_` — same MPSC boundary |
| `ambient_sfx[NUM_AMBIENTS]`, `have_ambient_sfx` | `Sound::ambient_state_` (registration-time, folded from `RegistrationSnapshot`) |

Other file-scope singletons and their disposition:

| Legacy global | Type | Becomes |
|---|---|---|
| `roombuffer`, `paintbuffer` (`s_mix.c:20`) | `portable_samplepair_t[PAINTBUFFER_SIZE+1]` static | Mix-worker-local scratch (T_AudioDecoder), never file-scope shared |
| `soundfade` struct (`s_main.c:24-32`) | anonymous struct singleton | `Sound::fade_state_`, written on T_Main by fade commands, read at paint-time — crosses via `MixGateSnapshot`/gain param |
| `sndpool`, `snd_fade_sequence` (`s_main.c:34-35`) | poolhandle_t, qboolean | `Sound` owns a pool handle member (P-7 note: legacy is a raw handle, not RAII — port-time gap, see soundlib's identical note) |
| `S_GetSoundtime`'s `buffers`/`oldsamplepos` (`s_main.c:1505`) | function-local statics | Mix-clock-reconstruction state, owned by whichever thread reads device `samplepos` (T_AudioDecoder) |
| `idsp_room` (`s_dsp.c:172`, the ONLY non-static DSP global) | int | `Sound`'s DSP-state member; selection computed on T_Main (reads `cl.local.waterlevel`), only the derived index crosses to the mix thread |
| `sound` (`sndlib_t`, `snd_utils.c:19`) | file-scope global | soundlib's decode-scratch struct — Race-shared today (single-caller assumption); must become per-call/per-thread-local if decode moves to T_AudioDecoder |
| `iff_data`/`iff_dataPtr`/`iff_end`/`iff_lastChunk`/`iff_chunkLen` (`snd_wav.c:20-24`) | file-scope statics | WAV one-shot-parse cursor — same Race-shared class as `sound`; the streaming path (`StreamFindNextChunk`) is already reentrant/context-carrying by contrast |
| `s_sentenceImmediateName` (`s_load.c:33`) | single-slot `string` | VOX immediate-sentence (`'!'`-prefixed via `speak`) handoff — single-in-flight quirk, see Quirks |
| `rgpszrawsentence[CVOXFILESENTENCEMAX]`, `cszrawsentences` (`s_vox.c`) | static array + count | VOX sentence table — becomes `Sound`-owned state per R9.3's P-3 recommendation, not a further file-static |

______________________________________________________________________

## Quirks and invariants

**Kernel arithmetic law.** All 12 macro-generated mix kernels
(`S_MakeMix*`) share one volume law: `pbuf[i].{left,right} += (sample *
volume[{0,1}]) >> (x-8)` where `x` ∈ {8,16} is source bit width
(`s_mix.c:22-31,107-110`). Dispatch order in `S_MixAudio`: flat (rate=1,
frac=0) → lerp (if `s_lerping` cvar on) → pitch, then mono/stereo ×
8/16-bit branch (`s_mix.c:120-173`). Must reproduce bit-exact.

**Paint order.** Per mix block: `S_ClearBuffers` → `S_MixNormalChannelsToRoombuffer`
→ `+= S_MixRawChannels` → `SX_RoomFX` (skipped in menu) →
`S_MixBufferWithGain` (skipped in menu iff room_channels==0) →
`S_TransferPaintBuffer` → advance `paintedtime` (`s_mix.c:555-570`). This
exact 7-step order, including the menu-gate asymmetry (voice/bg-track bypass
DSP by painting directly into `paintbuffer`, `s_mix.c:434-449`), is a
load-bearing invariant.

**`0x40000000` reset.** `S_GetSoundtime`'s 32-bit overflow guard: on DMA
wrap, if `snd.paintedtime > 0x40000000`, reset `buffers=0`,
`paintedtime=fullsamples`, and hard-`S_StopAllSounds(true)`
(`s_main.c:1519-1525`). Must be preserved verbatim in the mix-clock port.

**VOX single-immediate-slot + `FreeWord`.** `s_sentenceImmediateName`
(`s_load.c:33`) is one process-wide slot, not per-channel/per-call: a second
`S_RegisterSound("!...")` before the first handle resolves overwrites the
first — the `SENTENCE_INDEX` handle has no real per-call identity. R9.3
flags this as a live P-3 door concern (race/collision hazard), not just
style — the rewrite should replace the single-slot handoff with an explicit
return value/parameter where the call site allows. `VOX_FreeWord` (per its
own inline TODO) unconditionally zeroes `ch->sample`/`ch->forced_end`/clears
`FL_CHAN_FINISHED`/nulls `ch->data` on EVERY call, even on out-of-range/null
paths — preserved-but-flagged as suspect (`s_vox.c:170-188`).

**DSP off-by-one — `idsp_room == 29` vs 28-max table (decided, S9.5).**
`MAX_ROOM_TYPES = ARRAYSIZE(rgsxpre)` = 29, but the clamp
`bound(0, idsp_room, MAX_ROOM_TYPES)` is **inclusive**, permitting
`idsp_room == 29` — out of bounds for a 29-element array (valid indices
0-28; `s_dsp.c:21,809,822`). This only manifests if `room_type`/
`roomwater_type` cvars are hand-set to exactly 29. **Resolved 2026-07-20
(SND-OQ-6): REPRODUCE-WITH-DEFINED-BEHAVIOUR.** Both preset tables are
padded to a 30th row (index 29, `k_max_room_types`) with an all-zero
`RoomPreset`; the clamp bound itself (`k_max_room_types = 29`) is kept
textually distinct from the padded storage size (`k_room_preset_count = 30`)
so `bound(0, idsp_room, MAX_ROOM_TYPES)` stays byte-for-byte identical to
legacy — index 29 is still reachable, the off-by-one is preserved, not
fixed. A zeroed row (rather than any attempt to "recover" the legacy OOB
content) was chosen because that content is whatever bytes happen to follow
the static array in a given compiled binary's data segment — build-,
compiler- and optimisation-level-dependent, not a reproducible or
meaningful value; the task brief's own fallback ("if the padded-row
contents cannot be made meaningful, a zeroed row with a comment is the
defined superset") applies directly. The zeroed row is behaviourally
indistinguishable from preset 0 ("off") for every field that affects
observable mix output: `room_size == 0` and `room_delay == 0` both disable
their respective delay lines exactly like preset 0's own values, and the
two fields where the zeroed row numerically differs from preset 0
(`room_rvblp`/`room_dlylp`, 1.0/2.0 in preset 0 vs 0.0/0.0 in the sentinel)
are read only inside an ACTIVE delay line's lowpass branch, which never
runs once its governing size/delay is 0. Implemented in
`xash3dpp/include/xash3dpp/private/sound/dsp.hpp` (`k_room_presets_release`/
`k_room_presets_hlalpha052`, file-header rationale) + `src/sound/dsp.cpp`;
pinned by `tests/sound/test_sound_dsp.cpp`'s `test_snd_oq6_sentinel_row_content`
(raw content) and `test_snd_oq6_index29_behaves_like_off` (behavioural
equivalence to room 0). Decision also recorded in
`decisions-architecture.md` §3a.

**Menu/key_dest gating baked into the mix.** Multiple independent gates key
off `cls.key_dest`/`cl.paused`/`cl.background`/`Host_IsSinglePlayerGame()`
inside the mix loop itself (`s_mix.c:333-349,562-566`, R9.5 group f) — not
factored into a single policy layer. The rewrite's `MixGateSnapshot` (R9.5)
consolidates these into one per-frame POD; the mixer must apply them in the
same branch order to preserve behavior (e.g. console+local-sound plays even
when otherwise gated).

**`S_ExtraUpdate` disappearance under the new topology (policy-prescribed
quirk row citing §3.4).** Legacy `S_ExtraUpdate` is a T_Main mid-frame mix
pump ("don't let sound skip if going slow") with exactly two call sites,
both T_Main (`CL_ExtraUpdate`, `cl_view.c:575`). Under the ratified
`T_Main → MPSC → T_AudioDecoder → SPSC ring → T_AudioCallback` topology
(threading-model §3.4), the mix no longer runs synchronously on T_Main, so
`S_ExtraUpdate`'s "pump the mix mid-frame" purpose is structurally obsolete
— the audio thread paints continuously off its own ring, not on T_Main's
demand. **Policy: `S_ExtraUpdate` is intentionally dropped**, not ported;
its two call sites become no-ops (or are removed) in the client/screen-draw
tail. This is a deliberate parity deviation recorded here per Q-12, not an
oversight — flag for the client-boundary spec (Chunk 12) since one call site
lives in `cl_view.c`.

**Additional preserved quirks (bit-exact, brief):** ambient channels are
volume-ramped not started/stopped (`s_main.c:1084-1137`); `play2` command is
registered but never removed in `S_Shutdown` (`s_main.c:2046-2058` vs.
`1998-2011` — 13 added, 12 removed, a leaked registration across restart);
`CLIP16` clamps to `[SHRT_MIN+8, SHRT_MAX-8]`, not full int16 range
(`sound.h:40`); soundfade is skipped entirely in menu
(`s_main.c:127-132`); WAV loader's hardcoded 3-entry CRC32 allowlist
silently zeroes known-broken HL1/Q1 files (`snd_wav.c:347-370`);
`Sound_GetApproxWavePlayLen`'s `filesize - 128` is an acknowledged
GoldSrc-inherited magic number (`snd_utils.c:102-103`).

______________________________________________________________________

## Satellite components

Q-11 test applied per `decisions-architecture.md §Q-11` (5 criteria; score
≥2 → separate target).

| Candidate | Criteria met | Score | Verdict |
|---|---|---|---|
| **soundlib (core: snd_utils/snd_main/snd_wav)** | shares parent's format-table/decode-scratch architecture; no independent external dep beyond filesystem/memory already used elsewhere | 0-1 | **same target** — stays inside sound as the decode layer; the format v-table pattern is itself the plugin point for mp3/ogg/opus below |
| **mp3 / libmpg** (vendored, unlinked, ~8,843 lines: 456 snd_mp3.c + ~8,387 libmpg) | (a) independent internal state machine (frame/layer parsing, Huffman tables, synthesis filterbank — large, self-contained); consumer surface is narrow (5 stream fns + 1 load fn, matches the format-table shape); **NOT** an externally *linked* dependency (compiles as engine source, no mpg123 entry in `engine/wscript`'s client libs) | **ambiguous — score not resolved here** | **adjudication needed.** R9.2 explicitly flags this: whether "vendored source, no external link" counts as failing or passing the Q-11 "pulls in a different external dependency the parent does not need" criterion is not resolved by the criterion's own wording. libmpg is a dependency in every practical sense (huge, independently-versioned upstream, GPL provenance) but is not a *linked* library the way vorbis/opus are. **Recommendation for adjudication:** treat as **separate target** (`xash3dpp_mp3` or similar) on the size/independent-state-machine criteria alone, matching the networking-boundary precedent of `xash3dpp_http` being split out for a similarly self-contained protocol stack — but this is a call for the orchestrator, not a fact this fragment can settle |
| **ogg / opus** (thin wrappers, ~741 lines total over `vorbis`/`vorbisfile`/`opus`/`opusfile`, all externally linked per `engine/wscript`) | (b) different external dependency the parent does not need (real linked libs); (c) decoder state machine lives outside the tree | 2 | **separate target** — clean Q-11 pass; matches the HTTP-downloader precedent (external TCP dep) in the networking boundary spec |
| **voice** | fenced out of this pack entirely per the A1 brief (`voice.c`/`voice.h` are its own recon scope, not covered by R9.1-R9.6) | n/a | **not scored here** — defer to a dedicated voice fragment/pack |

______________________________________________________________________

## Extension axes (Q-21)

Axis set re-read at draft time from `extension-goals.md` §2/§3 (G-1..G-5,
P-1..P-8 — unchanged from the A0 fact base and every A1 fragment's read).

| Goal / primitive | Applies? | Required seam or door |
|---|---|---|
| **P-1** main-thread service inbox (MPSC) | **Yes — headline, first production instance** | Chunk 9 is named in `extension-goals.md` §4 as "First production MPSC queue (audio commands) — validates the P-1 queue family." **This validates the MPSC primitive itself, NOT the Main-inbox drain contract** — the audio MPSC is T_Main → T_AudioDecoder (producer-to-worker), not the debug/MCP inbox shape (worker-to-Main) that G-1/G-3 will need later; the two are structurally similar (typed MPSC, drained at a defined point) but not the same queue instance or drain-site contract. Play/alter/stop (`S_StartSound`/`S_AlterChannel`/`S_StopSound`) are the mutation surface that crosses this queue. The reverse-direction mouth write-back (group e, `IMouthSink`) is a second, distinct marshal-to-Main need this chunk also exercises. |
| **P-2** published-snapshot reads | **Yes** | `ListenerSnapshot`/`MixGateSnapshot`/`RegistrationSnapshot` (R9.5) are the P-2 immutable-publish shape for the audio consumer; `SoundStats` (see below) is the any-thread counter surface. |
| **P-2 (SoundStats)** whole-struct atomics + snapshot reads | **Yes** | A `SoundStats` Tier-1 struct (mirroring `NetworkingStats`'s pattern from the networking boundary) publishes mix-block counters (active channels, dropped-sound count, DSP room index) as relaxed atomics; any-thread readers (future debug thread, G-3) snapshot-read it without touching live channel/mix state. |
| **P-3** context-first entry points | **Yes — door-debt candidate** | The mixer is built entirely on file-scope mutable state (the `snd` global, mix scratch buffers, `S_GetSoundtime` statics — R9.1). Every new mixer entry point must carry context (a `Sound&`/`MixContext&`) instead of reaching `snd`; the ABI-frozen `channels`/`raw_channels` pointers inside `snd_globals_t` are the documented P-3 exception class (forced by the SoundAPI v1 contract). soundlib's `sound` global and VOX's `rgpszrawsentence` table are the same pattern at smaller scale — see Owned state disposition above. |
| **P-4** typed introspection (`channels_snapshot()`) | **Yes** | `channels_snapshot()` (Interface section) replaces direct `snd.channels[]`/`S_GetCurrentStaticSounds`/`S_GetCurrentDynamicSounds` array pokes as the typed read surface for save/restore today and debug/MCP introspection later. The `s_show` console dump (`s_main.c:1640`) is the existing informal precedent this formalizes. |
| **P-5** narrowest-state signatures | Partial — door-debt | Leaf mixers already take a narrow `channel_t*` (R9.1), but `S_MixNormalChannelsToRoombuffer`/`S_PaintChannels` reach `snd`/`cl`/`cls`/`clgame` globals directly — the whole-runtime reach P-5 targets for new signatures. R9.5's provider interfaces (`IEntitySpatialProvider`, `IMouthSink`) are the P-5-conformant replacement shape: each takes only the sub-aggregate it touches. |
| **P-6** services are satellites | See Satellite components | ogg/opus pass Q-11 cleanly (separate target); mp3/libmpg is the ambiguous case flagged for adjudication; voice is fenced out of this pack. |
| **G-3** dedicated debug thread — ring occupancy read pattern | **Door-keep** | A future debug thread reading raw-channel ring occupancy (`s_rawend` vs. read cursor) must consume it via the SPSC ring's own occupancy accessor, never a bare `volatile` peek — the legacy `volatile uint s_rawend` (`sound_api.h:115`) is a single-writer/single-reader same-thread guard in legacy, not a real cross-thread contract; the xash3dpp SPSC ring replaces it with a proper lock-free occupancy primitive (R9.6). |
| **G-5** scripting runtime — commands via `cmd_add` = scripted-scenario surface | **Door-keep, no new work owed now** | All 13 sound commands (`play`, `speak`, `s_fade`, etc.) are registered via the ordinary `Cmd_AddCommand`/`Cmd_AddCommandWithFlags` path (`s_main.c:1998-2011`) and are unrestricted except `music`. This is already G-5's "script surface v0 = `cmd_add`/`cbuf_*`/`cvar_*`" — a scripted test scenario can drive sound via the same commands a player would type; no sound-specific scripting affordance is needed, the existing command surface already qualifies. |
| **G-1** (MCP service) | Consumer via P-4/P-2 | Reads served from `SoundStats` (P-2) + `channels_snapshot()` (P-4); no new backdoor. |
| **G-2** (game ABI v2) | Indirect — noted, not gating | The `pfnS_Spatialize`/`pfnGetSoundInterface` clgame hooks are the GoldSrc client-DLL sound seam; a v2 sound interface would carry context. Not scoped for this chunk. |
| **G-4** (expanded in-game debugging) | Consumer via P-4 | Extends `channels_snapshot()`/`SoundStats`; nothing sound-specific owed beyond P-4 conformance. |
| **P-7** (pool-owned RAII) | **Door-debt, noted not fixed** | `host.soundpool`/`sndpool` are raw `Mem_AllocPool` handles with manual `Mem_FreePool`, not RAII-owned pool classes per P-7's `create_<thing>`/`PoolHandle` idiom (R9.2, R9.1). Legacy predates P-7; the rewrite should adopt the RAII idiom for the `Sound`/soundlib pool ownership at port time. |
| **P-8** (annotation discipline) | Applies to all new code | Every confined type (mix worker, decoder, `Sound` class) carries `@thread-safety`; see Threading section for the per-field hazard classification that seeds the annotation set. |

______________________________________________________________________

### Adjudicated VOX deviations (S9.4 parity audit, 2026-07-20)

- **Degenerate trailing-whitespace sentence entry NOT reproduced**: legacy
  `VOX_ReadSentenceFile_` appends one empty `""/""` table entry when
  sentences.txt ends in trailing whitespace or a blank final line (it
  dereferences the FS trailing NUL past the content, s_vox.c:537-566). The
  rewrite's bounds guard stops cleanly — one fewer entry on such files. The
  entry is reachable only via an empty-name lookup or its exact numeric
  index and never shifts real indices; adjudicated document-not-reproduce
  (reject-gracefully family).
- **Negative-overflow numeric sentence handle**: an all-digit handle
  overflowing int32 negative makes legacy read `rgpszrawsentence[negative]`
  (UB, s_vox.c:272-275); the rewrite falls through to the name scan →
  no-sentence. UB-only input, hardening.

### Adjudicated entry-surface deviations (S9.6 parity audit, 2026-07-20)

- **`register_sound` is ALWAYS lazy**: legacy eager-decodes at register
  time when no registration sequence is active (`if( !s_registering )
  S_LoadSound( sfx )`, s_load.c:333); the rewrite never models
  registration sequences and defers every decode to first play. A live
  channel always loads its source at play time, so no channel observes a
  cache legacy would have populated — the only observable losses are the
  late-precache warning timing and eager decode-error reporting, both
  outside the audited surface. Sanctioned (audit F-4).
- **`stop_sound` on an unresolvable name returns early** where legacy
  dereferences the null `S_FindName` result (s_main.c:1456 → :520, UB).
  Reject-gracefully family; for resolvable names both engines create the
  fresh sfx slot side effect identically (audit uncertainty #5).
- **Channel display-name truncation not reproduced**: legacy stores
  `ch->name` in a `char[16]` (Q_strncpy truncation, s_main.c:700 /
  sound_api.h:78); the rewrite keeps the full name. Affects only
  `channels_snapshot` display today; the Chunk-8 save wiring must decide
  whether serialized channel names re-truncate to 15 chars (audit F-7,
  owner recorded in the campaign deferred list).

### Adjudicated topology deviations (S9.7b gate, 2026-07-20)

- **Decoded audio is retained for the registry's lifetime**: legacy
  `VOX_FreeWord` calls `FS_FreeSound( word->sfx->cache )` (s_vox.c:185-186)
  to bound memory; `SfxRegistry::release()` is a deliberate no-op. The
  four-dimension gate confirmed (parity F-1/F-2, concurrency CONC-1) that
  freeing a cache entry from T_AudioDecoder during word retirement destroys
  audio still borrowed by other live channels AND by `AudioCommand::source`
  pointers in flight on the queue — a cross-thread use-after-free the epoch
  fence does not cover, because the fence orders commands rather than
  protecting buffers. Retention makes the address-stability invariant
  unconditional (`slots_.reserve(sound_max_sfx)` + the MAX_SFX refusal
  mirror legacy's fixed `s_knownSfx[MAX_SFX]`, so the slot vector never
  reallocates and nothing destroys an entry while the registry lives).
  Cost: bounded by MAX_SFX distinct sounds, which is exactly the capacity
  the registry is already sized for. Reclaiming it would require
  refcounting every borrow or a per-block re-resolve under the mutex on the
  decoder hot path — both worse trades than the retention.
- **`dsp_profile` is refused while the topology runs**: the command mutates
  decoder-owned delay lines from T_Main. Legacy has no thread to race
  (s_dsp.c:250 registers it unconditionally), so this is a rewrite-only
  restriction on a debug stress tool, not a behavioural parity question.
- **Master volume partially wired**: `s_volume` and the `snd_mute_losefocus`
  focus gate are applied per legacy `S_GetMasterVolume`'s term order; the
  soundfade term is NOT (no `S_UpdateSoundFade` exists yet) and the
  `MixGateSnapshot` producer has no source for `host.status`/`cls.key_dest`/
  `cl.paused`/`cl.background`, so the focus branch is live but dormant.
  Both are marked in code and owned by the client chunk.

## Threading

### Ratified topology (§3.4) and SND-OQ-1 resolved shape

Legacy: the entire sound tree — mixer, VOX, mouth, DSP, soundlib decode —
runs on **T_Main**. `SND_UpdateSound` runs once per main-loop iteration;
`S_ExtraUpdate` runs mid-frame from `CL_ExtraUpdate` (also T_Main) and the
screen-draw tail. No sound state is touched off the main thread in legacy;
the SDL/DMA callback only consumes the already-filled `snd.buffer` under the
`SNDDMA_BeginPainting`/`Submit` lock bracket.

xash3dpp target (threading-model §3.4, ratified): **T_Main → MPSC →
T_AudioDecoder → SPSC ring → T_AudioCallback**. SND-OQ-1 is resolved:
**providers read ONLY on T_Main**; a POD `ListenerSnapshot` ships via the
command stream. Per R9.5, the full read/write inventory resolves to three
crossing shapes:

1. **Ships-in-command-stream** (T_Main → T_AudioDecoder, one-way):
   `ListenerSnapshot`, `MixGateSnapshot`, `RegistrationSnapshot` — see
   Dependencies section field tables.
2. **Provider-computed, T_Main-only** (`IEntitySpatialProvider`): entity
   origin resolution never leaves T_Main; only resolved origins cross.
3. **Atomic write-back, T_AudioDecoder → T_Main** (`IMouthSink`): the sole
   reverse-direction data flow; `mouthopen` crosses via marshal or atomic
   slot, `sndavg`/`sndcount` accumulators stay mix-private.

### Hazard classification (analyse-threading Step-2 classes)

| Legacy state | Class | Legacy thread | xash3dpp thread | Note |
|---|---|---|---|---|
| `roombuffer`/`paintbuffer` (`s_mix.c:20`) | Race-static-buf | T_Main | T_AudioDecoder (mix worker) | Must become worker-local, not file-scope shared |
| `S_GetSoundtime`'s `buffers`/`oldsamplepos` (`s_main.c:1505`) | Race-static-buf | T_Main | T_AudioDecoder | DMA-wrap clock reconstruction moves with the device-samplepos reader |
| `soundfade`/`snd_fade_sequence`/`sndpool` (`s_main.c:24,34-35`) | Race-shared | T_Main | write T_Main, read T_AudioDecoder | Control→mix crossing; must marshal via `MixGateSnapshot`'s gain param, not a live read |
| `snd.{paintedtime,soundtime,channels,total_channels}` | Race-shared | T_Main | mix-thread-private / MPSC-crossed | `paintedtime` becomes mix-thread-private; channel mutations cross the P-1 MPSC |
| `snd.{origin,forward,right,up,entnum}` (listener pose) | Race-shared (producer at `S_UpdateFrame`) | T_Main | becomes `ListenerSnapshot` POD | Exact publish point for the P-2 snapshot |
| `S_StartSound`/`S_RestoreSound`/`S_StopSound`/`S_AlterChannel` mutation entry points | Race-shared | T_Main | crosses MPSC (P-1) | First production MPSC per extension-goals §4 |
| `rawchan_t::s_rawend` (`sound_api.h:115`) | Race-shared (legacy: `volatile`-as-compiler-guard, single-writer/reader same-thread) | T_Main | SPSC ring occupancy cursor | Legacy `volatile` was never a real cross-thread contract; the SPSC ring replaces it |
| `sound` (`sndlib_t`, `snd_utils.c:19`) | Race-shared | T_Main (single-caller assumption, unverified concurrency) | T_AudioDecoder (decode work) | Port-time hazard to close: must become per-call/per-thread-local if concurrent decode requests overlap |
| `iff_data`/`iff_dataPtr`/`iff_end`/`iff_lastChunk`/`iff_chunkLen` (`snd_wav.c:20-24`) | Race-shared | T_Main | T_AudioDecoder | Same class as `sound`; the streaming path (`StreamFindNextChunk`) is already reentrant by contrast — confirms the race is specific to the one-shot load path |
| `sound.tempbuffer` (`snd_utils.c:76`) | Race-shared | T_Main | T_AudioDecoder | Two independent call paths (resample, WAV MPEG-in-container shortcut) share one static scratch slot — a same-thread reentrancy hazard even before considering multithreading |
| `idsp_room` + all s_dsp.c file-scope statics (`ptable`, `sxamodl/r`, delay lines, etc.) | Race-static-buf | T_Main | selection on T_Main, processing on T_AudioDecoder | Selection reads `cl.local.waterlevel` (must become a `ListenerSnapshot` field, per R9.4/R9.5); if processing splits from selection without a snapshot it becomes Race-shared |
| `SDL_SoundCallback`'s access to `snd.samples`/`snd.samplepos`/`snd.buffer` | Race-shared, lock-mitigated in legacy | SDL's own audio callback thread vs. T_Main | maps to T_AudioCallback (consumer) vs. T_AudioDecoder (producer) | The `SNDDMA_BeginPainting`/`Submit` lock is the pattern the SPSC ring replaces with lock-free exchange |
| `s_sentenceImmediateName` (`s_load.c:33`) | Race-shared (single-slot, documented collision hazard even in legacy) | T_Main | T_Main (unless VOX registration also crosses) | See Quirks — a live P-3 door concern, not just a threading note |
| `clgame.soundFuncs` (SoundAPI table) | Safe-RO post-init / Race-lazy-init during init window | T_Main | T_Main only | Cold-path client-DLL bootstrap; audio pipeline threads never touch it |

### As-built topology (S9.7b, 2026-07-20)

The target posture above is no longer aspirational — sound is the first
multi-threaded subsystem in the tree. As built:

| Piece | Detail |
|---|---|
| Command stream | `core::MpscQueue<AudioCommand, 128, 128>` — 128 normal + 128 reserved (SND-OQ-3). Strictly FIFO; the reserve relaxes ADMISSION, never order |
| Decoder thread | `xash-audio-decoder`, `ThreadRole::AudioDecoder`, priority High, spawned via `platform::spawn_thread`. Drains the queue, applies commands, paints one block, writes the ring |
| PCM ring | `core::SpscRing<int16_t, 16384>` — interleaved stereo device-format frames, no float stage anywhere between the mix kernels and the device (SND-OQ-5) |
| Callback | `RingFillSource::fill()` on `ThreadRole::AudioCallback`. Wait-free: no lock, no allocation, no logging. Empty ring ⇒ zero-fill + the always-on underrun counter (§5.2 exception) |
| Pump | `TopologyParams::internal_pump` is DERIVED from `IAudioDevice::drives_own_callback()` (S9.8, resolving gate finding CONC-6 — a self-driving device must not also get an internal pump, which would put a second reader on the single-consumer ring): `NullDevice` gets the internal pump thread, `SinkDevice` declares self-driving and is pumped explicitly by its owner; a real SDL backend will declare self-driving and pull from the OS callback thread |
| Witness (S9.8) | `test_sound_witness` drives a 25-frame console-scripted sequence (`cbuf_add_text`/`cbuf_execute` — the deliberate G-5 stage-a rehearsal) through channel alloc, VOX, DSP, and the ring into `SinkDevice` with `SoundInitParams::external_decoder` (no decoder thread; the test calls `Sound::decoder_step()` from an AudioDecoder-registered worker) — byte-identical PCM pinned across runs AND arches (FNV `0xDF1088E7D0D531CF`, backed by full `memcmp`) |

Every hazard row in the table above whose "xash3dpp thread" column names
T_AudioDecoder is now **realised and enforced**, not planned: the
paintbuffer/roombuffer scratch, the soundtime reconstruction statics,
`paintedtime`, the channel array, and the s_dsp delay-line state are all
decoder-owned, and the mutation entry points cross the MPSC.

One shared-mutable remains, deliberately: **`SfxRegistry`** is T_Main-owned
but reachable from T_AudioDecoder through `LockedSfxResolver` (a mutex) for
VOX word-advance resolution, which happens inside the paint and cannot be
hoisted without defeating S9.6's lazy-resolution fix. Plain-channel source
resolution IS hoisted to T_Main and ships as a borrowed pointer.
T_AudioCallback never touches the registry. Decode runs OUTSIDE the mutex
(lock → observe → unlock → decode → re-lock → double-checked install) so
T_Main cannot stall the High-priority decoder behind file I/O.

### Assert/annotation duty

Per P-8, every type confined to T_AudioDecoder or T_AudioCallback carries an
`@thread-safety` annotation naming its owning role, following the
networking-boundary precedent. The mixer's `compliance-allow(thread-assert)`
exemption is **retired**: `Mixer::paint_channels()` asserts
`ThreadRole::AudioDecoder` for real. The as-built assert map:

| Site | Asserted role |
|---|---|
| Every `Sound::*` public entry; `AudioTopology::start/stop/submit/flush/channel_snapshot`; `MouthSlots::drain` | `Main` |
| `AudioTopology::decoder_step()`, `Mixer::paint_channels()`, `MouthSlots::set_mouth_open()` | `AudioDecoder` |
| `RingFillSource::fill()` | `AudioCallback` |
| Mix kernels, resample primitives, `apply_command` | none — hot path, reached only through an asserting site |

`VoxSystem` and `RoomDsp` keep a `compliance-allow(thread-assert)` for a
reason worth stating precisely: their real invariant is confinement to
*whichever thread owns the channel array*, which is T_AudioDecoder with the
topology running and T_Main without it. That role is CONDITIONAL, so no
single per-call assert can express it — enforcement lives at the entry
points that know the mode. The one genuine hole this exposed was
`RoomDsp::profile()` reached from the T_Main `dsp_profile` console command
while the decoder paints through the same delay lines; it is now refused
while the topology runs.

**Consequence for the §External ABI main-pumped fallback**: because the
paint asserts `AudioDecoder`, an ABI-v1 client override driving
`pfnS_PaintChannels` synchronously must run on a thread REGISTERED as
`AudioDecoder`. The role is a declaration of what the thread is doing, not a
requirement that it be the spawned decoder — but the constraint is real and
binds the future shim (and S9.8's witness).

______________________________________________________________________

## Open questions

| ID | Question | Recommended shape | Blocks classification |
|---|---|---|---|
| **SND-OQ-1** | Provider contract for cross-thread listener/entity/gate data | **Resolved** (ratified, not open): providers read ONLY on T_Main; POD snapshots (`ListenerSnapshot`, `MixGateSnapshot`, `RegistrationSnapshot`) ship via the command stream; `IEntitySpatialProvider`/`IMouthSink` are the two provider/sink interfaces (R9.5). Recorded here for completeness — implementation must conform, not re-litigate. | **blocks-scaffold** — the `Sound` class's public entry points and the audio command struct shapes cannot be scaffolded until these POD layouts are finalized |
| **SND-OQ-2** | Quiesce/drain + wavdata epoch + shutdown order | ✅ **RESOLVED 2026-07-20 (S9.7b)** — see the decision register row for the ratified text. Fence: `AudioTopology::flush()` submits a reserved-lane `FlushEpoch`; the decoder acks on POP, and MPSC FIFO order makes the ack proof that every earlier command was applied. `flush()` is `[[nodiscard]] bool` — `false` means NOT quiesced. Ownership: `AudioData` is T_Main/`SfxRegistry`-owned, borrowed by the decoder; cache entries are retained for the registry lifetime (see the entry-surface deviations) so a borrow can never dangle. Shutdown: stop accepting → device deactivate + detach → wait out any in-flight callback → join pump → join decoder (final drain) → destroy ring/queue. The `s_stub`-omits-`Activate` sub-item is discharged: `NullDevice::set_active` is a no-op, so the topology drives every backend symmetrically. | resolved |
| **SND-OQ-3** | Queue-full policy for the audio MPSC | ✅ **DECIDED 2026-07-19 (B3), AMENDED 2026-07-20 (S9.7b)** — reserved-capacity fast lane: STOP/CHANGE, `AlterChannel`, `StopAllSounds`, `FlushEpoch` and the per-frame control message can never be refused by a START-full queue. START overflow spins bounded, then drops and counts it in `SoundStats::dropped_sounds`. The original "blocks ... asserted in debug" wording is withdrawn — an assert on producer saturation turns a stalled decoder into an abort and makes the guarantee untestable; an unbounded block would stall the frame. Legacy has no queue (synchronous calls), so nothing here is a parity question. | resolved |
| **SND-OQ-4** | Music streaming: fence `s_stream` out vs. codec vends `IAudioStream` | R9.2's evidence: soundlib already has two parallel per-format v-tables — `loadwavfmt_t` (1 fn, one-shot decode) and `streamfmt_t` (5 fns: open/read/seek/tell/close) — the stream table is strictly wider, confirming streaming is architecturally distinct from one-shot load in legacy already. Two shapes to choose between: (a) fence `s_stream.c`'s background-track logic out of the `Sound` class entirely as its own small satellite (parallels the mp3/ogg Q-11 split), with the codec-vended `stream_t` staying soundlib's concern; or (b) formalize an `IAudioStream` interface at the `Sound`/soundlib boundary that every decoder (WAV/mp3/ogg/opus) implements uniformly, replacing the `streamfmt_t` v-table. (b) is more P-4/P-5-conformant (typed interface vs. raw fn-pointer table) but is new design, not a straight port. | **blocks-scaffold** — determines whether `Sound` links against soundlib's stream v-table directly or against a new interface |
| **SND-OQ-5** | Ring payload int16 — byte-identical rationale | The legacy DMA ring buffer is always 16-bit stereo (`sound.h:29-31`: `SOUND_DMA_SPEED=44100`, hardcoded 2-channel/16-bit output — R9.1 Owned state). The SPSC ring's payload type should stay `int16_t` (interleaved stereo) to keep `S_TransferPaintBuffer`'s reinterpret-as-flat-int-stream logic and `CLIP16`'s clamp semantics unchanged — a float or wider intermediate type in the ring would require a second clamp/convert stage that risks non-bit-exact output vs. legacy. Recommendation: ring payload = `int16_t[2]` (interleaved), matching `S_WriteLinearBlastStereo16`'s existing output shape exactly; do not introduce a float mixing stage in the ring itself (the mix kernels already produce clamped int32 accumulator values before the final `CLIP16` narrow). | **blocks-scaffold** — the SPSC ring's item type and the mix kernel output contract are the same decision |
| **SND-OQ-6** (DSP off-by-one) | `idsp_room == 29` reproduce-vs-clamp | **Resolved 2026-07-20 (S9.5):** reproduce-with-defined-behaviour — both preset tables padded to a 30th, all-zero sentinel row (index 29); see Quirks section above for the full rationale (behaviourally identical to preset 0 "off") and the decisions-architecture.md §3a entry. | closed — DSP port correctness sign-off unblocked |

______________________________________________________________________

## Uncertainties carried forward from A1 (not resolved at draft time)

- Q-11 final verdict for mp3/libmpg (vendored, unlinked) — flagged above,
  needs orchestrator adjudication.
- Whether the `sound`/`iff_*` file-scope statics' single-caller assumption
  is exercised concurrently anywhere in legacy beyond the files R9.2 read —
  not verified.
- `S_UpdateSoundFade`'s piecewise curve (in-fade ramp vs. hold-extension) —
  R9.1 flags this as needing a behavioral trace before port, confidence med.
- Whether mouth `sndavg`/`sndcount` accumulators must cross the T_AudioDecoder
  boundary or can stay mix-private (only `mouthopen` committed) depends on
  the chosen decode-block cadence — a design call, not derivable from
  single-threaded legacy (R9.5).
