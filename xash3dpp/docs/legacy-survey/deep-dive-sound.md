# Deep Dive: Sound (Chunk 9) — DRAFT

> Recon method: 14-agent A1 fan-out (this pack: 6 agents — R9.1 mixer, R9.2
> soundlib, R9.3 VOX/mouth, R9.4 DSP, R9.5 client-state coupling, R9.6
> SNDDMA/SoundAPI ABI), assembled 2026-07-19 by the A2 spec-draft assembler.
> Legacy C engine at the repo root is REFERENCE-ONLY. This document merges
> all six fragments verbatim (claim tables + citations preserved) into one
> structured recon doc; nothing is re-derived. Sibling-owned surface
> (filesystem, networking `svc_sound`, map_loader BSP leaf data, cmd_cvar
> registration mechanics, server) is cited, not re-derived, per the
> sibling-scope rule.

______________________________________________________________________

## Table of contents

1. [Mixer core — channel model, paint pipeline, mix kernels](#1-mixer-core-s_mainc--s_mixc)
2. [Soundlib — decode layer, format tables, satellite decoders](#2-soundlib)
3. [VOX sentences + mouth animation](#3-vox-sentences-s_voxc--mouth-animation-s_mouthc)
4. [DSP — room effects (reverb/delay/amod)](#4-dsp--s_dspc)
5. [Client-state coupling inventory](#5-sound-tree-client-state-coupling-inventory)
6. [SNDDMA + SoundAPI ABI](#6-snddma--soundapi-abi)

______________________________________________________________________

## 1. Mixer core (`s_main.c` + `s_mix.c`)

Source: `engine/client/sound/{s_main.c,s_mix.c}` (2,071 + ~572 lines).
Scope: channel model, paint pipeline, 12 mix-kernel variants, soundtime/
paintedtime clock, soundfade, `S_ExtraUpdate`, full sound cvar+command
census.

### 1.1 Interface

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| s_main.c:626 | `S_StartSound(pos,ent,chan,handle,fvol,attn,pitch,flags)` primary play entry: alter-then-start for STOP/CHANGE flags, pick dyn/static channel, spatialize, first-audibility drop | `void S_StartSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )` | high |
| s_main.c:749 | `S_RestoreSound(...,sample,end,wordIndex)` = save/restore variant; skips alter pre-pass, applies sample/forced_end offsets and restores sentence word_index | `target_chan->sample = sample; target_chan->forced_end = end;` | high |
| s_main.c:872 | `S_AmbientSound(...)` allocates only in static area, always spatializes, chipmunk pitch mult | `void S_AmbientSound( const vec3_t pos, int ent, sound_t handle, float fvol, float attn, int pitch, int flags )` | high |
| s_main.c:955 | `S_StartLocalSound(name,volume,reliable)` plays on `snd.entnum`; `reliable`→CHAN_STATIC else CHAN_AUTO, always ATTN_NONE/PITCH_NORM | `if( reliable ) channel = CHAN_STATIC; ... S_StartSound( NULL, snd.entnum, channel, sfxHandle, volume, ATTN_NONE, PITCH_NORM, flags );` | high |
| s_main.c:1451 | `S_StopSound(entnum,channel,soundname)` is `GAME_EXPORT`; resolves name via `S_FindName`, `S_AlterChannel(...,SND_STOP)` | `void GAME_EXPORT S_StopSound( int entnum, int channel, const char *soundname )` | high |
| s_main.c:1465 | `S_StopAllSounds(ambient)` resets `total_channels=MAX_DYNAMIC_CHANNELS`, frees every channel, `SX_ClearState`, zeroes channel array, `S_ClearBuffer`, zeroes soundfade | `snd.total_channels = MAX_DYNAMIC_CHANNELS; // no statics` | high |
| s_main.c:1590 | `S_UpdateFrame(rvp)` publishes listener pose into `snd`; gated on RF_DRAW_WORLD set / RF_ONLY_CLIENTDRAW clear | `VectorCopy( rvp->vieworigin, snd.origin ); AngleVectors( rvp->viewangles, snd.forward, snd.right, snd.up ); snd.entnum = rvp->viewentity;` | high |
| s_main.c:1607 | `SND_UpdateSound()` per-frame driver: client override, soundfade, idle-raw-free, ambients, respatialize, spatialize raw, s_show debug, stream bg track, `S_UpdateChannels` | `void SND_UpdateSound( void )` | high |
| s_main.c:1577 | `S_ExtraUpdate()` mid-frame mix pump; "Don't let sound skip if going slow" | `static void S_ExtraUpdate( void ){ if( !snd.initialized ) return; S_UpdateChannels (); }` | high |
| ref_common.c:242 / cl_view.c:575 | `S_ExtraUpdate` has exactly two call sites, both T_Main: `CL_ExtraUpdate` (after IN_Accumulate) and screen-draw tail | `static void CL_ExtraUpdate( void ){ clgame.dllFuncs.IN_Accumulate(); S_ExtraUpdate(); }` | high |
| s_main.c:975 / 1019 | `S_GetCurrentStaticSounds`/`S_GetCurrentDynamicSounds(pout,size)` serialize live channels into `soundlist_t[]` for save/restore | `if( ch->entchannel == CHAN_STATIC && looped && !Host_IsQuakeCompatible()) continue;` | high |
| s_main.c:115 | `S_GetMasterVolume()` = paint-time gain: 0 when HOST_NOFOCUS + snd_mute_losefocus; else `s_volume.value * (1-soundfade%)` | `return s_volume.value * scale;` | high |
| s_mix.c:542 | `S_PaintChannels(endtime)` = public mix entry (client-overridable seam) | `void S_PaintChannels( int endtime )` | high |
| s_mix.c:534 | `S_ClearBuffers(num_samples)` zeroes both static mix buffers for `(num_samples+1)` pairs | `const size_t num_bytes = ( num_samples + 1 ) * sizeof( portable_samplepair_t ); memset( roombuffer, 0, num_bytes ); memset( paintbuffer, 0, num_bytes );` | high |

### 1.2 External ABI contracts (client-DLL sound seam)

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| sound_api.h:37 | `CL_SOUND_INTERFACE_VERSION 1`; experimental / no backward-compat guarantee | `#define CL_SOUND_INTERFACE_VERSION\t1` | high |
| s_main.c:1935 | `gSoundAPI` = 5 engine→client callbacks: `CL_GetEntitySpatialization`, `S_GetSfxByHandle`, `S_RawEntSamples`, `SND_ForceInitMouth`, `Voice_GetAudioInfo` | `static const sound_api_t gSoundAPI = { CL_GetEntitySpatialization, S_GetSfxByHandle, S_RawEntSamples, SND_ForceInitMouth, Voice_GetAudioInfo, };` | high |
| s_main.c:1948 | `S_InitSoundAPI` negotiates `pfnGetSoundInterface`; on failure falls back to legacy `pfnS_Init(&snd)` | `if( clgame.dllFuncs.pfnGetSoundInterface( CL_SOUND_INTERFACE_VERSION, &gSoundAPI, &clgame.soundFuncs ))` | high |
| s_main.c:1562 / sound_api.h:189 | Client can fully replace the mixer via `pfnS_PaintChannels` | `if( clgame.soundFuncs.pfnS_PaintChannels ) clgame.soundFuncs.pfnS_PaintChannels( endtime ); else S_PaintChannels( endtime );` | high |
| s_main.c:559 | `pfnS_Spatialize(ch)` short-circuits `SND_Spatialize` | `if( clgame.soundFuncs.pfnS_Spatialize ){ clgame.soundFuncs.pfnS_Spatialize( ch ); return; }` | high |
| s_main.c:167 / 207 | Channel/raw-channel mutation notifications: `pfnS_UpdateChannel`/`pfnS_UpdateRawChannel`, `ch==NULL` ⇒ freed | `if( !clgame.soundFuncs.pfnS_UpdateChannel ) return; clgame.soundFuncs.pfnS_UpdateChannel( ch_idx, ch, handle );` | high |
| s_main.c:1611 | Per-frame client hook `pfnS_UpdateSound()` | `if( clgame.soundFuncs.pfnS_UpdateSound ) clgame.soundFuncs.pfnS_UpdateSound();` | high |
| sound_api.h:76-105 | `channel_t` carries `uintptr_t engine_reserved[8]` + `game_reserved[8]` — frozen ABI padding | `uintptr_t engine_reserved[8]; uintptr_t game_reserved[8];` | high |

### 1.3 Cvar census (all in `S_Init`, s_main.c:1980-1990)

| file:line | name | default | flags |
|---|---|---|---|
| s_main.c:47 | `volume` (`s_volume`) | "0.7" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:48 | `MP3Volume` (`s_musicvolume`) | "1.0" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:49 | `_snd_mixahead` (`s_mixahead`) | "0.12" | FCVAR_FILTERABLE |
| s_main.c:50 | `s_show` | "0" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:51 | `s_lerping` | "0" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:52 | `ambient_level` (`s_ambient_level`) | "0.3" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:53 | `ambient_fade` (`s_ambient_fade`) | "1000" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:54 | `snd_mute_losefocus` | "1" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:55 | `s_test` | "0" | 0 |
| s_main.c:56 | `s_samplecount` | "0" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |
| s_main.c:57 | `s_warn_late_precache` | "0" | FCVAR_ARCHIVE\|FCVAR_FILTERABLE |

Note: `s_musicvolume`/`s_lerping`/`s_test`/`s_samplecount`/
`s_warn_late_precache`/`snd_mute_losefocus` are `extern` (sound.h) and
consumed by sibling TUs (s_stream, s_load, s_mix); the rest are file-static
to s_main.c.

### 1.4 Command census (`S_Init` s_main.c:1998-2011; removed `S_Shutdown` 2046-2058)

| file:line | name | handler | restricted? |
|---|---|---|---|
| s_main.c:1998 | `play` | `S_Play_f` | no |
| s_main.c:1999 | `play2` | `S_Play2_f` | no (nehahra) |
| s_main.c:2000 | `playvol` | `S_PlayVol_f` | no |
| s_main.c:2001 | `stopsound` | `S_StopSound_f` | no |
| s_main.c:2003 | `music` | `S_Music_f` | `CMD_OVERRIDABLE` (HLU SDK collision) |
| s_main.c:2004 | `soundlist` | `S_SoundList_f` | no |
| s_main.c:2005 | `s_info` | `S_SoundInfo_f` | no |
| s_main.c:2006 | `s_fade` | `S_Fade_f` | no |
| s_main.c:2007 | `soundfade` | `S_SoundFade_f` | no (goldsrc-compat) |
| s_main.c:2008 | `+voicerecord` | `S_VoiceRecordStart_f` | no |
| s_main.c:2009 | `-voicerecord` | `S_VoiceRecordStop_f` | no |
| s_main.c:2010 | `spk` | `S_SayReliable_f` | no |
| s_main.c:2011 | `speak` | `S_Say_f` | no |

**Asymmetry quirk**: `play2` registered but NOT removed in `S_Shutdown`
(13 added, 12 removed).

### 1.5 Owned state

| legacy file:line | claim | verbatim evidence | confidence |
|---|---|---|---|
| s_main.c:39 / sound_api.h:132-160 | `snd` global (`snd_globals_t`) = master mixer state: dma buffer/format, initialized, samples/samplepos, paintedtime/soundtime, listener pose, channels+total_channels, raw_channels, ambient_sfx | `snd_globals_t snd = { .channels = (channel_t[MAX_CHANNELS]){}, .max_channels = MAX_CHANNELS, ... };` | high |
| sound_api.h:153,156 | `channels`/`raw_channels` are `* const` inside `snd` — array pointers fixed at init | `channel_t *const channels; ... rawchan_t **const raw_channels;` | high |
| s_mix.c:20 | Mix scratch `roombuffer`/`paintbuffer` are file-static `portable_samplepair_t[PAINTBUFFER_SIZE+1]` (1025 pairs; +1 = padding for `S_ClearBuffers`) | `static portable_samplepair_t roombuffer[(PAINTBUFFER_SIZE+1)], paintbuffer[(PAINTBUFFER_SIZE+1)];` | high |
| s_main.c:24-32 | `soundfade` file-scope anon-struct singleton; zeroed by `S_StopAllSounds` | `struct { int start_percent; int percent; double start_time; int out_seconds; int hold_time; int in_seconds; } soundfade;` | high |
| s_main.c:34-35 | `sndpool`, `snd_fade_sequence` file-scope globals | `poolhandle_t sndpool; qboolean snd_fade_sequence = false;` | high |
| s_main.c:1505 | `S_GetSoundtime` statics `buffers`/`oldsamplepos` — DMA wrap counter/last sample position | `static int buffers, oldsamplepos;` | high |
| sound_api.h:76-105 | Per-channel `channel_t` model: name[16], sfx, origin, dist_mult, entchannel, flags, entnum, master_vol/leftvol/rightvol (0-255 shorts), basePitch, word_index, inauduble_free_time, sample/forced_end (double), data, words | `double sample; double forced_end; wavdata_t *data; voxword_t *words;` | high |
| sound_api.h:107-123 | `rawchan_t`: entnum, vols, dist_mult, origin, `volatile uint s_rawend`, oldtime, max_samples, flexible rawsamples[]; heap-alloc'd per-slot (MAX_RAW_SAMPLES=16384) | `volatile uint s_rawend; ... size_t max_samples; portable_samplepair_t rawsamples[];` | high |
| sound.h:44-45 | Channel array partition: [0,NUM_AMBIENTS)=ambients, [NUM_AMBIENTS,MAX_DYNAMIC_CHANNELS)=dynamic (4..63), [MAX_DYNAMIC_CHANNELS,total)=static up to MAX_CHANNELS=320 | `#define MAX_DYNAMIC_CHANNELS (60 + NUM_AMBIENTS)  #define MAX_CHANNELS (256 + MAX_DYNAMIC_CHANNELS)` | high |
| sound.h:29-31 | Format constants: SOUND_DMA_SPEED=SOUND_44k=44100, PAINTBUFFER_SIZE=1024, output always 16-bit stereo | `#define SOUND_DMA_SPEED SOUND_44k  #define PAINTBUFFER_SIZE 1024` | high |

### 1.6 Quirks and invariants

**Channel selection** (`SND_PickDynamicChannel`/`SND_PickStaticChannel`/`SND_GetChannelTimeLeft`):

| file:line | claim | evidence |
|---|---|---|
| s_main.c:342-359 | `SND_PickDynamicChannel` scans only [NUM_AMBIENTS,MAX_DYNAMIC_CHANNELS). CHAN_STREAM request already streaming ⇒ NULL, `*ignore=true` (silent no-op) | `if( channel == CHAN_STREAM && SND_FStreamIsPlaying( sfx )){ if( ignore ) *ignore = true; return NULL; }` |
| s_main.c:367-368 | Protection 1: a slot playing CHAN_STREAM is never overridden | `if( ch->sfx && ( ch->entchannel == CHAN_STREAM )) continue;` |
| s_main.c:370-375 | Same-entity+same-entchannel forces immediate victim, EXCEPT CHAN_AUTO(0) never same-entity-overrides | `if( channel != CHAN_AUTO && ch->entnum == entnum && ( ch->entchannel == channel \|\| channel == -1 )){ first_to_die = ch_idx; break; }` |
| s_main.c:378-379 | Protection 2: monster sound may not evict a client's sound | `if( ch->sfx && S_IsClient( ch->entnum ) && !S_IsClient( entnum )) continue;` |
| s_main.c:382-388 | Victim heuristic: least `SND_GetChannelTimeLeft` wins (empty slot=0 wins) | `timeleft = SND_GetChannelTimeLeft( ch ); if( timeleft < life_left ){ life_left = timeleft; first_to_die = ch_idx; }` |
| s_main.c:394-409 | Anti-restart: chosen victim = looped same ent+chan+sfx ⇒ NULL, `*ignore=true` (don't restart) | `if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx == sfx ){ if( ignore ) *ignore = true; return NULL; }` |
| s_main.c:284-331 | `SND_GetChannelTimeLeft` self-documented "needs to be removed after whole sound subsystem rewrite" | `// TODO: this function needs to be removed after whole sound subsystem rewrite` |
| s_main.c:428-462 | `SND_PickStaticChannel` scans [MAX_DYNAMIC_CHANNELS,total): reuse empty/exact-match slot; else grow up to max_channels; at cap logs error | `if( snd.total_channels == snd.max_channels ){ Con_DPrintf( S_ERROR "%s: no free channels\n", __func__ ); return NULL; }` |
| s_main.c:434-441 | Static match test = exact VectorCompare(pos,origin) AND same sfx pointer | `if( VectorCompare( pos, snd.channels[i].origin ) && snd.channels[i].sfx == sfx ) break;` |

**Alter/stop paths** (`S_AlterChannel`/`S_MaybeAlterChannel`):

| file:line | claim | evidence |
|---|---|---|
| s_main.c:514-528 | Scans [NUM_AMBIENTS,total) (ambients excluded), returns on FIRST match; sentence name ('!') matches `sfx=NULL` | `qboolean is_sentence = S_TestSoundChar( sfx->name, '!' ); ...` |
| s_main.c:464-500 | Match gates: live sfx, entnum, entchannel match; sfx==NULL requires `ch->words`; else exact `ch->sfx==sfx` | `if( ch->entnum != entnum ) return false; ... if( !ch->words ) return false;` |
| s_main.c:487-499 | Flag order: SND_STOP ⇒ free+return true (wins); CHANGE_PITCH/CHANGE_VOL can both apply | `if( FBitSet( flags, SND_STOP )){ S_FreeChannel( ch ); return true; } if( FBitSet( flags, SND_CHANGE_PITCH )) ch->basePitch = pitch;` |
| s_main.c:641-649 | Alter pre-pass runs only for STOP\|CHANGE_VOL\|CHANGE_PITCH; STOP returns even on miss | `if( flags & ( SND_STOP\|SND_CHANGE_VOL\|SND_CHANGE_PITCH )){ if( S_AlterChannel(...)) return; if( flags & SND_STOP ) return; }` |

**Start-sound invariants:**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:638-639 | Volume clamp `bound(0,fvol*255,255)`; pitch floor `pitch<=1 → PITCH_NORM` ("Invasion issues") | `vol = bound( 0, fvol * 255, 255 ); if( pitch <= 1 ) pitch = PITCH_NORM;` |
| s_main.c:651 | Null pos defaults to `refState.vieworg` | `if( !pos ) pos = refState.vieworg;` |
| s_main.c:653-654 | CHAN_STREAM implicitly forces SND_STOP_LOOPING | `if( chan == CHAN_STREAM ) SetBits( flags, SND_STOP_LOOPING );` |
| s_main.c:670 | Picked channel fully memset(0) before fill | `memset( target_chan, 0, sizeof( *target_chan ));` |
| s_main.c:674-681 | ent==0 ⇒ FL_CHAN_STATIC_SOUND; !SND_STOP_LOOPING ⇒ FL_CHAN_USE_LOOP; dist_mult=attn/SND_CLIP_DISTANCE | `if( ent == 0 ) SetBits( target_chan->flags, FL_CHAN_STATIC_SOUND );` |
| s_main.c:719-734 | First-audibility drop: left=right=0 ⇒ free & drop unless looping/CHAN_STREAM | `if( !target_chan->leftvol && !target_chan->rightvol ){ ... }` |
| s_main.c:692-703 | Sentence detect: '!' name ⇒ `VOX_LoadSound` | `if( S_TestSoundChar( sfx->name, '!' )){ ... VOX_LoadSound(...)}` |
| s_main.c:929 | `S_AmbientSound` pitch × chipmunk factor `(sys_timescale+1)/2` | `pitch *= (sys_timescale.value + 1) / 2;` |

**Paint pipeline order** (`S_PaintChannels`):

| file:line | claim | evidence |
|---|---|---|
| s_mix.c:544 | Gain fixed once per call: `S_GetMasterVolume()*256` | `int gain = S_GetMasterVolume() * 256;` |
| s_mix.c:546-553 | Outer loop advances paintedtime in ≤PAINTBUFFER_SIZE blocks | `while( snd.paintedtime < endtime ){ ... }` |
| s_mix.c:555-570 | Order: ClearBuffers → MixNormalToRoom → += MixRaw → SX_RoomFX(!menu) → MixBufferWithGain(room>0\|\|!menu) → TransferPaintBuffer → advance paintedtime | `S_ClearBuffers( num_samples ); int room_channels = S_MixNormalChannelsToRoombuffer( roombuffer, end ); room_channels += S_MixRawChannels( end );` |
| s_mix.c:562-566 | Menu gating: skip SX_RoomFX in menu; gain-mix only if room_channels>0 in menu | `if( cls.key_dest != key_menu ) SX_RoomFX( roombuffer, num_samples );` |
| s_mix.c:308-324 | `S_MixNormalChannelsToRoombuffer` early-outs on num_samples≤0 or background+console; chipmunk pitch mult applied | `if( cl.background && cls.key_dest == key_console ) return num_mixed_channels;` |
| s_mix.c:333-349 | Per-channel gate: console+local plays; (menu\|\|paused)+non-local+SP skip; !menu+!ingame+non-static skip | `else if(( cls.key_dest == key_menu \|\| cl.paused ) && !FBitSet( ch->flags, FL_CHAN_LOCAL_SOUND ) && sp ){ continue; }` |
| s_mix.c:351-374 | Load-on-mix `S_LoadSound`; inaudible non-looping starts 0.1s free timer | `ch->inauduble_free_time = host.realtime + MAX_CHANNEL_INAUDIBLE_TIME;` |
| s_mix.c:376-389 | Mouth driven here for CHAN_VOICE/STREAM: `SND_MoveMouth8/16` | `if( ch->entchannel == CHAN_VOICE \|\| ch->entchannel == CHAN_STREAM ){ ...}` |
| s_mix.c:395-408 | Sentence channels mix via `VOX_MixChannelToBuffer`, free on SENTENCE_FINISHED; plain via `S_MixChannelToBuffer`, free on FINISHED | `if( ch->words ){ VOX_MixChannelToBuffer(...); ... }` |

**Raw/streaming mix** (`S_MixRawChannels`):

| file:line | claim | evidence |
|---|---|---|
| s_mix.c:418-419 | Raw channels skipped entirely while `cl.paused` | `if( cl.paused ) return 0;` |
| s_mix.c:434-449 | Voice + background-track paint DIRECTLY into paintbuffer (no DSP); others into roombuffer | `if( is_voice \|\| ch->entnum == S_RAW_SOUND_BACKGROUNDTRACK ){ ... pbuf = paintbuffer; } else { pbuf = roombuffer; num_room_channels++; }` |
| s_mix.c:451-458 | Ring copy: `stop=min(end,s_rawend)`, `mask=max_samples-1` | `pbuf[i].left += ( ch->rawsamples[j & mask].left * ch->leftvol ) >> 8;` |
| s_mix.c:460-468 | Raw mouth sync fed from current ring position | `int pos = snd.paintedtime & ( ch->max_samples - 1 );` |

**Gain mix/clip/transfer:**

| file:line | claim | evidence |
|---|---|---|
| s_mix.c:474-491 | `gain==256` fast path plain add (exact unity); else `dst += (src*gain)>>8` | `if( gain == 256 ){ dst[i].left += src[i].left; ...}` |
| s_mix.c:494-501 | `S_WriteLinearBlastStereo16` clips via CLIP16 = `bound(SHRT_MIN+8,x,SHRT_MAX-8)` | `snd_out[i+0] = CLIP16( snd_p[i+0] );` |
| s_mix.c:503-532 | `S_TransferPaintBuffer` reinterprets as flat int stream into DMA ring, `sampleMask=(samples>>1)-1` | `const int sampleMask = ((snd.samples >> 1) - 1);` |

**The 12 mix-kernel variants:**

| file:line | claim | evidence |
|---|---|---|
| s_mix.c:22-31,107-110 | Common volume law across all 12: `pbuf[i].{l,r} += (sample*volume[{0,1}]) >> (x-8)` | `pbuf[i].left += ( data[i] * volume[0] ) >> ( x - 8 );` |
| s_mix.c:22-42 | Flat (rate==1,frac==0): Mono8/16, Stereo8/16, no fractional index | `pbuf[i].left += ( data[i * 2 + 0] * volume[0] ) >> ( x - 8 );` |
| s_mix.c:44-57 | MonoPitch8/16: nearest-neighbour resample via accumulator | `offset_frac += rate_scale; sample_idx += (uint)offset_frac; offset_frac -= (uint)offset_frac;` |
| s_mix.c:59-72 | StereoPitch8/16: same accumulator, stereo stride ×2 | `sample_idx += (uint)offset_frac << 1;` |
| s_mix.c:74-88 | MonoLerp8/16: linear interp with 1-sample lookahead | `int s = (int)( data[sample_idx] * ( 1.0 - offset_frac ) + data[sample_idx + 1] * offset_frac );` |
| s_mix.c:90-105 | StereoLerp8/16: per-channel lerp, lookahead of one stereo frame | `int sl = (int)( data[sample_idx + 0] * ( 1.0 - offset_frac ) + data[sample_idx + 2] * offset_frac );` |
| s_mix.c:120-173 | `S_MixAudio` dispatch: flat if rate=1&&frac=0; else lerp if lerp; else pitch | `if( Q_equal( rate_scale, 1.0 ) && Q_equal( offset_frac, 0.0 )){ ... }` |

**Per-channel resample driver:**

| file:line | claim | evidence |
|---|---|---|
| s_mix.c:200-205 | Volumes re-clamped at mix; `rate = pitch*sfx.cache.rate/out_rate` | `double rate = pitch * chan->sfx->cache->rate / (double)out_rate;` |
| s_mix.c:208-214 | Timecompress≥100 ⇒ FINISHED, mix nothing; else scales advance | `if( timecompress >= 100 ){ SetBits( chan->flags, FL_CHAN_FINISHED ); return 0; }` |
| s_mix.c:221 | Lerp lookahead = `s_lerping.value?1:0`, off by default | `const int lookahead = s_lerping.value ? 1 : 0;` |
| s_mix.c:226-233 | Request math: `end_sample=sample+rate*num_samples*timecompress_rate` | `int request_num_samples = (int)(ceil( end_sample ) - floor( chan->sample )) + lookahead;` |
| s_mix.c:239-261 | out_count≤0 ⇒ fall back nearest, 1 sample (loop-wrap guard) | `if( out_count <= 0 ){ lerp = false; out_count = 1; }` |
| s_mix.c:264-273 | Advance `chan->sample += out_count*rate*timecompress_rate`; retrieve-nothing ⇒ FINISHED | `chan->sample += out_count * rate * timecompress_rate;` |
| s_mix.c:175-195 | `S_AdjustNumSamples`: forced_end truncation for save/restore | `if( end_sample >= chan->forced_end ){ SetBits( chan->flags, FL_CHAN_FINISHED ); return floor(( chan->forced_end - chan->sample ) / ( rate * timecompress_rate )); }` |
| s_mix.c:279-306 | `VOX_MixChannelToBuffer` word-advance loop | `if( FBitSet( chan->flags, FL_CHAN_FINISHED )){ VOX_FreeWord( chan ); chan->word_index++; VOX_LoadWord( chan ); ...}` |

**Sample-clock/timing:**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:1503-1531 | `S_GetSoundtime` reconstructs monotone clock: wrap-counted `buffers*fullsamples+samplepos/2` | `return ( buffers * fullsamples + samplepos / 2 );` |
| s_main.c:1519-1525 | 32-bit overflow guard: `paintedtime>0x40000000` ⇒ hard reset + `S_StopAllSounds(true)` | `if( snd.paintedtime > 0x40000000 ){ buffers = 0; snd.paintedtime = fullsamples; S_StopAllSounds( true ); }` |
| s_main.c:1544-1553 | `S_UpdateChannels`: `endtime=soundtime+mixahead*44100`, capped to `samples>>1` | `if((int)(endtime - snd.soundtime) > samps ) endtime = snd.soundtime + samps;` |
| s_main.c:1555-1560 | 4-sample alignment invariant, rounded DOWN | `if(( endtime - snd.paintedtime ) & 0x3 ){ endtime -= ( endtime - snd.paintedtime ) & 0x3; }` |
| s_main.c:1539-1541 | Brackets everything in `SNDDMA_BeginPainting`/`Submit`; bails if `!snd.buffer` | `SNDDMA_BeginPainting(); if( !snd.buffer ) return; ... SNDDMA_Submit();` |
| s_mix.c:536-539 | ClearBuffers zeroes `(num_samples+1)` pairs — the +1 is the lerp lookahead slot | `const size_t num_bytes = ( num_samples + 1 ) * sizeof( portable_samplepair_t );` |

**Soundfade math:**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:142-150 | `S_SoundFade` just records params, stamps start_time | `soundfade.start_time = host.realtime;` |
| s_main.c:220-256 | Piecewise curve over the fade window (confidence med) | `soundfade.percent = soundfade.start_percent * ( f / soundfade.out_seconds );` |
| s_main.c:240-250 | Fade-sequence teardown: percent≥100 ⇒ hard stop + clear flag | `if( soundfade.percent >= 100 ){ S_StopAllSounds( false ); S_StopBackgroundTrack(); snd_fade_sequence = false; }` |
| s_main.c:119-134 | Master volume applies fade only outside menu; focus-mute returns 0 first | `if( cls.key_dest != key_menu && soundfade.percent != 0 ){ ... }` |
| s_main.c:1839-1850 | `s_fade` cmd ⇒ SoundFade(100,1,hold,0), hold∈[1,60] default 5 | `S_SoundFade( 100, 1, hold_time, 0 ); snd_fade_sequence = true;` |
| s_main.c:1858-1888 | `soundfade` cmd (goldsrc-compat) parses percent/hold/out/in with clamps | `fade_percent = bound( 0, fade_percent, 100 );` |

**Misc structural:**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:1470 | `S_StopAllSounds` shrinks total_channels back to MAX_DYNAMIC_CHANNELS | `snd.total_channels = MAX_DYNAMIC_CHANNELS;` |
| s_main.c:2046-2058 vs 1998-2011 | `play2` leaked command registration across restart | (no `play2` in removal list) |
| s_main.c:1036-1037 | Static looped sounds never serialized unless Quake-compat | `if( ch->entchannel == CHAN_STATIC && looped && !Host_IsQuakeCompatible()) continue;` |
| s_main.c:1084-1137 | Ambient channels volume-ramped, not started/stopped | `chan->master_vol += round( cl_clientframetime() * s_ambient_fade.value );` |
| s_main.c:597-601 | ATTN_NONE un-panned unless BUGCOMP_SPATIALIZE_SOUND_WITH_ATTN_NONE | `if( !FBitSet( host.bugcomp, BUGCOMP_SPATIALIZE_SOUND_WITH_ATTN_NONE )){ ...}` |
| s_main.c:539-552 | `S_SpatializeChannel` panning: `rvol=master*(1-dist)*(1+dot)`, `lvol=master*(1-dist)*(1-dot)` | `float rvol = round( master_vol * scale );` |

### 1.7 Threading

Legacy: entire mixer on **T_Main**. `SND_UpdateSound` per main-loop
iteration; `S_ExtraUpdate` mid-frame (also T_Main) from `CL_ExtraUpdate` and
the screen-draw tail. No off-main mixer state touch in legacy; SDL/DMA
callback only consumes `snd.buffer` under `SNDDMA_BeginPainting`/`Submit`.

xash3dpp target (threading-model §3.4): T_Main → MPSC → T_AudioDecoder →
SPSC ring → T_AudioCallback; SND-OQ-1 resolved so providers read ONLY on
T_Main and a POD `ListenerSnapshot` ships via the command stream.

| legacy file:line | shared-state observation | class | evidence | confidence |
|---|---|---|---|---|
| s_mix.c:20 | `roombuffer`/`paintbuffer` file-static; would live on T_AudioDecoder | Race-static-buf | `static portable_samplepair_t roombuffer[(PAINTBUFFER_SIZE+1)], paintbuffer[(PAINTBUFFER_SIZE+1)];` | high |
| s_main.c:1505 | `S_GetSoundtime` statics carry DMA-wrap clock; belongs to audio/decoder thread | Race-static-buf | `static int buffers, oldsamplepos;` | high |
| s_main.c:24 / 34-35 | `soundfade`/`snd_fade_sequence`/`sndpool`; control→mix crossing needs marshal | Race-shared | `struct {...} soundfade; poolhandle_t sndpool;` | high |
| sound_api.h:132 / s_main.c:39 | `snd` global's paintedtime/channels mutated during paint; listener fields → `ListenerSnapshot` | Race-shared | `int paintedtime; vec3_t origin; ...` | high |
| s_main.c:1590 | `S_UpdateFrame` = exact publish point for `ListenerSnapshot` producer | Race-shared (producer) | `VectorCopy( rvp->vieworigin, snd.origin ); ...` | high |
| s_main.c:626/1451/514 | Mutation entry points must cross MPSC (P-1, Chunk 9 "first production MPSC") | Race-shared | `void S_StartSound(...); void GAME_EXPORT S_StopSound(...);` | high |
| sound_api.h:115 | `rawchan_t::s_rawend` already `volatile` — hint of producer/consumer race even in legacy intent | Race-shared | `volatile uint s_rawend;` | high |
| s_main.c:1200-1203 | `S_FindRawChannel` lazily heap-allocates raw channel slots | Race-lazy-init | `snd.raw_channels[best] = Mem_Calloc( sndpool, sizeof( *ch ) + sizeof( portable_samplepair_t ) * raw_samples );` | high |
| s_main.c:1562 | Client mixer override runs on whatever thread calls it — G-2 capture-less-slot concern | Race-shared | `if( clgame.soundFuncs.pfnS_PaintChannels ) clgame.soundFuncs.pfnS_PaintChannels( endtime );` | med |

### 1.8 Extension axes (Q-21) — mixer-specific

| file:line | axis | claim | confidence |
|---|---|---|---|
| s_main.c:39, s_mix.c:20, s_main.c:1505 | P-3 | Mixer built on file-scope state (`snd`, scratch buffers, soundtime statics); ABI-frozen channel pointers are the exception class | high |
| s_main.c:626/514/1451 | P-1 | Play/alter/stop = the MPSC mutation surface (validates the queue family) | high |
| s_main.c:1590 | P-2 | Listener pose write = natural `ListenerSnapshot` publish point | high |
| s_main.c:975/1019/1640 | P-4 | `S_GetCurrentStaticSounds`/`s_show` = existing introspection precedent | med |
| s_mix.c:197/308 | P-5 | Leaf mixers already narrow (`channel_t*`); `S_MixNormalChannelsToRoombuffer`/`S_PaintChannels` reach whole-runtime globals | med |
| s_main.c:1935/1562, sound_api.h:181-194 | G-2 | `sound_interface_t` is a capture-less v1 ABI — exactly what G-2 replaces | med |

Uncertainties (mixer): `S_UpdateSoundFade` piecewise curve's in-fade term
unclear (med confidence, needs behavioral trace); `S_MixChannelToBuffer`
loop-wrap edge math not simulated; `s_test`/`s_samplecount`/
`s_warn_late_precache` consumers not traced; `play2` non-removal intent
undeterminable from source; final xash3dpp thread set cited from A0
factbase, not re-derived.

______________________________________________________________________

## 2. Soundlib

Source: `engine/common/soundlib/{snd_utils.c,soundlib.h}`,
`engine/client/soundlib/{snd_main.c,snd_wav.c}` + decoder satellites
(`snd_mp3.c`/`libmpg/*`, `snd_ogg_vorbis.c`, `snd_ogg_opus.c`,
`ogg_filestream.*`).

### 2.1 Interface

| file:line | claim | evidence | confidence |
|---|---|---|---|
| snd_main.c:57 | `FS_LoadSound` single load entry: resolves ext, tries DEFAULT_SOUNDPATH then bare path; `#`-prefixed skips FS | `wavdata_t *FS_LoadSound( const char *filename, const byte *buffer, size_t size )` | high |
| snd_main.c:96,108 | Two-path resolution: `DEFAULT_SOUNDPATH "%s.%s"` then bare `"%s.%s"` | `Q_snprintf( path, sizeof( path ), DEFAULT_SOUNDPATH "%s.%s", loadname, format->ext );` | high |
| snd_main.c:30-48 | `SoundPack()` copies file-scope `sound` struct into heap `wavdata_t`; sole producer of `wavdata_t*` | `static MALLOC_LIKE( FS_FreeSound, 1 ) wavdata_t *SoundPack( void ) { wavdata_t *pack = Mem_Malloc( host.soundpool, sizeof( *pack ) + sound.size );` | high |
| snd_main.c:147 | `FS_FreeSound` thin null-safe `Mem_Free` wrapper | `void FS_FreeSound( wavdata_t *pack ) { if( !pack ) return; Mem_Free( pack ); }` | high |
| snd_main.c:160-215 | `FS_OpenStream` resolves ext, tries `"%s.%s"` only, retries once under `media/` on failure | `if( Q_strncmp( filename, "media/", ...)) { ... stream = FS_OpenStream( loadname ); }` | high |
| snd_main.c:224-278 | Stream read/seek/tell/free = pure v-table dispatch, null-guarded | `if( !stream \|\| !stream->format \|\| !stream->format->readfunc ) return 0;` | high |
| soundlib.h:24-39 | Two parallel v-tables: `loadwavfmt_t` (1 fn) vs `streamfmt_t` (5 fn: open/read/setpos/getpos/free) — SND-OQ-4 scope | `typedef struct loadwavfmt_s { const char *ext; qboolean (*loadfunc)(...); } loadwavfmt_t;` | high |
| snd_utils.c:28-65 | Format registration: two static NULL-terminated arrays wired at `Sound_Init`; XASH_DEDICATED keeps only `.ext` rows | `#ifndef XASH_DEDICATED\n{ "wav", Sound_LoadWAV }, ...\n#else\n{ "wav" }, { "mp3" }, ...` | high |
| snd_utils.c:447-459 | `Sound_SupportedFileFormat` linear scan by extension, case-insensitive | `for( format = sound.loadformats; format && format->ext; format++ ) { if( !Q_stricmp( format->ext, fileext )) return true; }` | high |
| snd_utils.c:419-445 | `Sound_Process` post-load transform: resample only when SOUND_RESAMPLE + a nonzero target; reallocates whole `wavdata_t` | `if( likely( FBitSet( flags, SOUND_RESAMPLE ) && ( width > 0 \|\| rate > 0 \|\| channels > 0 ))) { ... }` | high |
| snd_utils.c:358-417 | `Sound_ResampleInternal` no-op if identical; picks 1 of 3 conversion paths by rate comparison | `if( inrate == outrate && inwidth == outwidth && inchannels == outchannels ) return false;` | high |
| snd_utils.c:85-120 | `Sound_GetApproxWavePlayLen` standalone fast-path, GAME_EXPORT, bypasses format table entirely | `uint GAME_EXPORT Sound_GetApproxWavePlayLen( const char *filepath )` ... `size_t filesize = FS_FileLength( f ) - 128;` | high |
| snd_wav.c:177 | `Sound_LoadWAV` pure in-memory parser, no FS calls; writes into file-scope `sound` | `qboolean Sound_LoadWAV( const char *name, const byte *buffer, fs_offset_t filesize )` | high |
| snd_wav.c:380-477 | `Stream_OpenWAV` DOES open own `file_t`, unlike `Sound_LoadWAV` — asymmetric FS ownership | `file = FS_Open( filename, "rb", false ); ... stream->file = file;` | high |

### 2.2 Dependencies

| file:line | claim | evidence | confidence |
|---|---|---|---|
| snd_utils.c:91,96 | Depends on filesystem's Open/Read/FileLength/Close + LoadFile/Seek/Tell/Eof — filesystem-owned per sibling-scope rule | `file_t *f = FS_Open( name, "rb", false ); if( !f ) return 0;` | high |
| snd_utils.c:70,82 | Owns dedicated pool `host.soundpool`, alloc'd Sound_Init, freed+leak-checked Sound_Shutdown | `host.soundpool = Mem_AllocPool( "SoundLib Pool" );` | high |
| snd_wav.c:193,227,239 | Depends on Con_DPrintf/Reportf/Printf for every error path — errors logged, not just returned | `Con_DPrintf( S_ERROR "%s: %s missing 'RIFF/WAVE' chunks\n", __func__, name );` | high |
| snd_utils.c:373,407 | Depends on `Platform_DoubleTime()` for resample timing/warning threshold | `t1 = Platform_DoubleTime();` | high |
| snd_wav.c:358-370 | Uses shared CRC32 utility to fingerprint known-broken silence WAVs (3-entry allowlist) | `uint32_t crc; CRC32_Init( &crc ); ...` | high |
| snd_wav.c:300-314 | WAV↔MP3 cross-dep: fmt=85 (MPEG-in-WAV) hands off to `Sound_LoadMPG` directly, bypassing normal dispatch | `return Sound_LoadMPG( name, buffer + hdr_size, filesize - hdr_size );` | high |

### 2.3 Satellite components (Q-11 evidence — verdict deferred to boundary spec)

| file:line | claim | evidence | confidence |
|---|---|---|---|
| snd_mp3.c (456) + libmpg/*.{c,h} (~8,387; total mp3≈8,843, correcting A0's "~8.4k" libmpg-only estimate) | Vendored, in-tree mpg123 fork — no external link; `engine/wscript` lists bzip2/vorbis/opus but no mpg123 | `libs += ['bzip2', 'MultiEmulator', 'opus', 'opusfile', 'vorbis', 'vorbisfile']` (mpg123 absent) | high |
| libmpg/{frame,layer3,parse,reader,synth}.c | mp3 has its own large internal state machine (frame/layer parsing, Huffman, synthesis filterbank) | layer3.c=1,597; parse.c=1,083; reader.c=905; frame.c=751 lines | high |
| snd_mp3.c | xash-side consumer surface is small: 5 Stream_*MPG fns + Sound_LoadMPG, matching loadwavfmt/streamfmt shapes | (soundlib.h:119,131-135) | high |
| snd_ogg_vorbis.c (305) + snd_ogg_opus.c (335) + ogg_filestream (60+41) ≈741 lines | Thin wrapper over external libs (vorbis/vorbisfile/opus/opusfile all linked) | `libs += [..., 'opus', 'opusfile', 'vorbis', 'vorbisfile']` | high |
| soundlib.h:119-145 | ogg/opus/mp3 expose identical narrow shape (1 load fn + 5 stream fns each) | (declarations) | high |
| snd_utils.c:28-42,51-65 | All three registered as rows in the same two format tables — architecture already treats them as same-target format plugins | `{ "mp3", Sound_LoadMPG }, { "ogg", Sound_LoadOggVorbis }, { "opus", Sound_LoadOggOpus },` | high |
| n/a | Whether "vendored, unlinked" passes/fails the Q-11 "different external dependency" criterion is unresolved by the criterion's own wording — **ambiguity carried to boundary spec for adjudication** | (analysis) | low |

### 2.4 Threading

| file:line | claim | class | confidence |
|---|---|---|---|
| snd_utils.c:19; soundlib.h:114 | `sndlib_t sound` — one file-scope mutable global, written by every loader with zero sync. Legacy: effectively single-caller. xash3dpp: decode slated for T_AudioDecoder — a **port-time hazard to close** | Race-shared | high |
| snd_wav.c:20-24 | `iff_data`/`iff_dataPtr`/`iff_end`/`iff_lastChunk`/`iff_chunkLen` — WAV chunk-scan cursor, no reentrancy guard | Race-shared | high |
| snd_wav.c:139-170 | `StreamFindNextChunk` (streaming path) takes explicit `file_t*`/out-param — already reentrant, unlike the one-shot path | (contrast, confirms scope of the race) | high |
| snd_utils.c:76,384-438 | `sound.tempbuffer` reused across `Sound_ResampleInternal` AND `Sound_LoadWAV`'s MPEG shortcut — two independent call paths share one scratch slot | Race-shared / same-thread reentrancy hazard | med |

### 2.5 Quirks and invariants

| file:line | claim | evidence | confidence |
|---|---|---|---|
| snd_wav.c:210-221 | `fmt` must be exactly 1 (PCM) or 85 (MPEG-in-WAV); else hard fail — no float-PCM/extensible support | `if( fmt != 1 ) { if( fmt != 85 ) {... return false; } else { mpeg_stream = true; } }` | high |
| snd_wav.c:224-241 | Only mono/stereo, 8/16-bit accepted | `if( sound.channels != 1 && sound.channels != 2 ) {... return false; }` | high |
| snd_wav.c:246-262 | `cue ` chunk parse is a non-conformant heuristic for one authoring tool | `// this is not a proper parse, but it works with CoolEdit...` | high |
| snd_wav.c:281-296 | Loop sample count exceeding data chunk ⇒ hard fail | `if( sound.samples ) { if( samples < sound.samples ) {... return false; } }` | high |
| snd_wav.c:331-345 | 8-bit PCM unsigned→signed conversion in-place; 16-bit only byteswapped | `*pData = (byte)((int)((byte)*pData) - 128 );` | high |
| snd_wav.c:104-109 | Truncation warnings suppressed for allowlisted chunk types (CoolEdit pad-byte non-conformance) | `// otherwise this warning becomes misleading...` | high |
| snd_utils.c:102-103 | `filesize-128` is an acknowledged GoldSrc-inherited magic number | `// magic number from GoldSrc, seems to be header size` | high |
| snd_utils.c:375-376 | Resample ratio comment documents only "normal" 0.5/1/2 ratios though arithmetic supports arbitrary | `stepscale = (double)inrate / outrate;\t// this is usually 0.5, 1, or 2` | high |
| snd_main.c:83-84,134 | `#`-prefix sentinel: skip FS resolution, decode buffer directly; suppresses "couldn't load" warning | `if( filename[0] == '#' && buffer && size ) goto load_internal;` | high |
| snd_wav.c:347-370 | Hardcoded CRC32 allowlist of 3 known-broken HL1/Q1 WAVs, silently zeroed | `0x14a36f29, // common/null.wav (HL1/Q1)` | high |

### 2.6 Extension axes (Q-21)

| file:line | claim | confidence |
|---|---|---|
| snd_utils.c:19,28-65 | `sound` global + format tables are exactly the P-3 "file-scope mutable state/context-less slots" target pattern | high |
| soundlib.h:24-39 | The two v-tables are themselves a plugin pattern; tangential to G-5/G-1 (no typed introspection surface today) | med |
| n/a | No G-1/G-2/G-3/G-4 touch found — reasoned "none" | high |
| snd_utils.c:70,82; snd_wav.c | P-6 (mp3/ogg/opus Q-11 scoring) and P-7 (raw pool handle, not RAII) are the live axes | high |

### 2.7 Uncertainties

- Q-11 verdict for mp3/libmpg not resolved (ambiguous criterion reading).
- Whether the `sound`/`iff_*` single-caller assumption is exercised
  concurrently anywhere beyond snd_main/snd_wav/snd_utils not verified.
- Resample interpolation math read but not independently re-derived/verified.
- `snd_mp3.c`'s own internal logic not read line-by-line, only its size/shape.

______________________________________________________________________

## 3. VOX sentences (`s_vox.c`) + mouth animation (`s_mouth.c`)

Source: `engine/client/sound/{s_vox.c,s_mouth.c}`, struct defs in
`common/sound_api.h` (`voxword_t`, `channel_t`), `common/cl_entity.h`
(`mouth_t`), and the two call sites in `s_mix.c` driving word-advance and
mouth writes.

### 3.1 Interface

| file:line | claim | evidence | confidence |
|---|---|---|---|
| sound.h:157-163 | Public VOX entry points: Init, Shutdown, SetChanVol, LoadSound, ModifyPitch, LoadWord, FreeWord | `void VOX_LoadSound( channel_t *pchan, const char *psz );` | high |
| s_mouth.c:36,62,113,89,103 | Mouth interface: MoveMouth8/16, MoveMouthRaw, ForceInitMouth, ForceCloseMouth | `void SND_MoveMouth8( mouth_t *mouth, int pos, const wavdata_t *sc, int count, qboolean use_loop )` | high |
| sound.h:131-145 | `SND_InitMouth`/`SND_CloseMouth` static-inline gate wrappers keyed on CHAN_VOICE/STREAM + entnum>0 | `static inline void SND_CloseMouth( const channel_t *ch ) { if( ch->entchannel == CHAN_VOICE \|\| ch->entchannel == CHAN_STREAM ) {...} }` | high |
| s_mix.c:279 | `VOX_MixChannelToBuffer` file-static, sole word-advance driver | `static int VOX_MixChannelToBuffer( portable_samplepair_t *pbuf, channel_t *chan, int num_samples, int out_rate, double pitch )` | high |
| s_vox.c:257-298 | Parser helpers file-static; only `VOX_LoadSound` is public | `static const char *VOX_LookupString( const char *pszin )` | high |

### 3.2 Quirks and invariants

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_vox.c:25,27-28 | `CVOXFILESENTENCEMAX`=4096; parse silently stops (`break`) at cap, no error | `#define CVOXFILESENTENCEMAX 4096` | high |
| sound.h:38 | `CVOXWORDMAX`=64 per sentence | `#define CVOXWORDMAX    64` | high |
| s_vox.c:523-568 | sentences.txt format: whitespace-delimited name/value; lines starting `/` = comments | `if( *p != '/' ) { name = p; ... }` | high |
| s_vox.c:557-566 | Storage: one alloc per sentence, `name\0value\0` back-to-back in one buffer | `int size = strlen( name ) + strlen( value ) + 2;` | high |
| s_vox.c:590-598 | `VOX_Shutdown` frees entries but does NOT NULL pointers — stale-pointer hazard until next reload | `void VOX_Shutdown( void ) { ... for( i = 0; i < cszrawsentences; i++ ) Mem_Free( rgpszrawsentence[i] ); cszrawsentences = 0; }` | high |
| s_vox.c:262-266 | `#`-prefix = immediate sentence, bypasses table lookup entirely | `if( *pszin == '#' ) { return pszin + 1; }` | high |
| s_vox.c:270-276 | Numeric index lookup; out-of-range resets to -1, falls through to (failing) name search rather than fast-fail | `if( Q_isdigit( pszin )) { i = Q_atoi( pszin ); if( i >= cszrawsentences ) i = -1; }` | high |
| s_vox.c:300-352 | Word-split grammar: space/./,/(...) delimiters; mid-sentence `.`/`,` synthesizes `_period`/`_comma` pseudo-words, end-of-string punctuation does not | `if(( *psz == '.' \|\| *psz == ',' ) && psz[1] != '\n' && psz[1] != '\r' && psz[1] != '\0' ) { ... }` | high |
| s_vox.c:223-231,238-241 | `VOX_GetDirectory` HACKHACK: leading `/` silently skipped; default dir "vox/" | `// HACKHACK: some modders send strings like "/fvox/_period four"` | high |
| s_vox.c:363-442 | Param grammar `(vNN pNN sNN eNN tNN)`; unrecognized content silently truncates parsing | `for( ; *psz && *psz != 'v' && ...; psz++ ) { if( *psz == ')' ) break; }` | high |
| s_vox.c:416-430 | Bounds: e/s clamp [0,100]; p/v clamp [0,UINT16_MAX]; t clamps [0,100] | `case 'e': pvoxword->end = bound( 0, i, 100 ); break;` | high |
| s_vox.c:434-439 | Defaults-only block (empty word text) doesn't emit a word — becomes the new running default, returns false | `if( Q_strlen( pszsave ) == 0 ) { *default_voxword = *pvoxword; return false; }` | high |
| s_vox.c:354-361 | Sentence-wide starting defaults: volume=100,pitch=100,end=100 | `*voxword = (voxword_t) { .volume = 100, .pitch = 100, .end = 100, };` | high |
| s_vox.c:141-168 | `VOX_LoadWord`: SENTENCE_FINISHED set FIRST unconditionally, cleared only on full success | `SetBits( ch->flags, FL_CHAN_SENTENCE_FINISHED ); if( ch->word_index < 0 \|\| ... ) return; ... ClearBits(...);` | high |
| s_vox.c:161-167 | Word start/end are percentages of total samples; `end<=start` forces end=0 | `if( end <= start ) end = 0; S_TrimStartEndTimes(...);` | high |
| s_vox.c:170-188 | `VOX_FreeWord`: unconditionally zeroes sample/forced_end/clears FINISHED/nulls data EVERY call (author-flagged suspect but preserved) | `// TODO: don't set random fields to zero lol, was memset before` | high |
| s_vox.c:182-187 | Cache-ownership gate: only frees `word->sfx->cache` if NOT `FL_VOXWORD_IN_CACHE` | `if( !word->sfx \|\| FBitSet( word->flags, FL_VOXWORD_IN_CACHE )) return;` | high |
| sound_api.h:39 | `FL_VOXWORD_IN_CACHE`=BIT(0): "if set, it was loaded prior and shouldn't be freed" | `#define FL_VOXWORD_IN_CACHE BIT( 0 )` | high |
| s_vox.c:444-521 | `VOX_LoadSound` reload sequence: FreeWord+Mem_Free2 old words, parse into stack scratch, heap-copy used prefix | `if( ch->words ) { VOX_FreeWord( ch ); Mem_Free2( &ch->words ); } ...` | high |
| s_vox.c:493-512 | Defaults-only blocks are `continue`d — `j` (real word count) can be < num_words | `if( !VOX_ParseWordParams( rgpparseword[i], &words_buf[j], &default_voxword )) continue;` | high |
| s_vox.c:190-204 | `VOX_SetChanVol`: multiplicative scale on top of existing leftvol/rightvol, no-op if volume==100 | `if( word->volume == 100 ) return; ch->leftvol = ch->leftvol * word->volume * 0.01f;` | high |
| s_vox.c:206-221 | `VOX_ModifyPitch`: additive offset `(pitch-PITCH_NORM)*0.01`, no-op if pitch==PITCH_NORM | `pitch += ( word->pitch - PITCH_NORM ) * 0.01f;` | high |
| s_vox.c:53-125 | Amplitude trim: PCM only, scans up to 255 frames, threshold 2 (8-bit) / 512 (16-bit) | `#define TRIM_SAMPLES_BELOW_8 2` `#define TRIM_SAMPLES_BELOW_16 512 // 65k * 2 / 256` | high |
| s_vox.c:31-40 | Trim eligibility = symmetric AND across channels | `if( abs( buf[0] ) > TRIM_SAMPLES_BELOW_8 ) return false;` | high |
| s_vox.c:127-139 | `end==0` sentinel derives to `samples-channels`; end clamped up to start | `if( end == 0 ) end = wav->samples - wav->channels;` | high |
| s_mix.c:279-306 | Word-advance loop: FreeWord→word_index++→LoadWord on FINISHED | `if( FBitSet( chan->flags, FL_CHAN_FINISHED )) { VOX_FreeWord( chan ); chan->word_index++; VOX_LoadWord( chan ); ... }` | high |
| s_mix.c:283-284 | Early-out on entry if SENTENCE_FINISHED already set | `if( FBitSet( chan->flags, FL_CHAN_SENTENCE_FINISHED )) return 0;` | high |
| s_mix.c:395-401 | VOX channels routed purely by `ch->words != NULL`; freed on SENTENCE_FINISHED vs. FINISHED for plain channels | `if( ch->words ) { ...if( FBitSet( ch->flags, FL_CHAN_SENTENCE_FINISHED )) S_FreeChannel( ch ); } else { ... }` | high |
| s_load.c:33,312-321,338-345 | `s_sentenceImmediateName` = single file-static slot; second `!`-registration before resolve overwrites the first | `static string   s_sentenceImmediateName;\t// keep dummy sentence name` | high |
| sound_api.h:76-105 | `channel_t::words` presence (!=NULL) is the sole VOX-vs-plain discriminator | `voxword_t *words; // dynamically allocated, (num_words + 1) entries, null sfx terminates` | high |
| s_mouth.c:23-34 | Running average over CAVGSAMPLES=10; partial progress persists across calls | `mouth->sndavg += savg; mouth->sndcount = (byte)scount; if( mouth->sndcount >= CAVGSAMPLES ) { ... }` | high |
| s_mouth.c:50-57,76-84,123-131 | Data-dependent stride `i += 80 + (data & 0x1F)` in all 3 scanners | `i += 80 + ((byte)data & 0x1F); scount++;` | high |
| s_mouth.c:79 | 16-bit downconvert: clamp `[-32767,0x7ffe]` then `>>8` before stride mask | `data = (bound( -32767, data, 0x7ffe ) >> 8);` | high |
| s_mouth.c:36-44,62-70 | Mouth source uses `S_RetrieveAudioSamples` — same routine mixing uses; bails on NULL | `count = S_RetrieveAudioSamples( sc, (const void **)&pdata, pos, count, use_loop ); if( pdata == NULL ) return;` | high |
| s_mix.c:376-388 | Mouth write site 1 (normal channels): gated on VOICE/STREAM, rate-converts sample count | `int mouth_count = (int)( num_samples * (double)sc->rate / out_rate );` | high |
| s_mix.c:467 | Mouth write site 2 (raw channels): reads directly from raw ring | `SND_MoveMouthRaw( &ent->mouth, &ch->rawsamples[pos], count );` | med (surrounding gate not fully re-derived) |
| s_mouth.c:89-101,103-111 | Init zeroes all 3 mouth fields; Close zeroes only mouthopen (accumulators untouched) | `clientEntity->mouth.mouthopen = 0; clientEntity->mouth.sndavg = 0; clientEntity->mouth.sndcount = 0;` (Init) vs mouthopen-only (Close) | high |

### 3.3 Test port list

Every `XASH_ENGINE_TESTS` case in `s_vox.c:600-729` (`Test_RunVOX`,
s_vox.c:721-727):

| file:line | claim | confidence |
|---|---|---|
| s_vox.c:603-622 | `Test_VOX_GetDirectory`: 4 table-driven cases pin leading-slash-skip, default-dir behavior | high |
| s_vox.c:624-656 | `Test_VOX_LookupString`: 9 cases against 5 manually-seeded entries — numeric index, negative/OOB→NULL, name search case-insensitivity | high |
| s_vox.c:658-690 | `Test_VOX_ParseString`: 2 sentences pin punctuation-splitting (`_comma`/`_period` synthesis, paren-block non-splitting) | high |
| s_vox.c:692-719 | `Test_VOX_ParseWordParams`: 3 cases pin cross-call default-carryover as intentional | high |
| s_vox.c:721-727 | `Test_RunVOX` registers all 4; no coverage exists for `VOX_LoadWord`/`FreeWord`/`LoadSound` end-to-end/`SetChanVol`/`ModifyPitch`/trim/`VOX_MixChannelToBuffer`; `s_mouth.c` has zero embedded tests | high |

### 3.4 Extension axes (Q-21)

- **P-3**: every VOX/mouth fn already takes explicit `channel_t*`/`mouth_t*`
  except `rgpszrawsentence`/`cszrawsentences` (global sentence table) and
  `s_sentenceImmediateName` (single-slot handoff quirk). Rewrite should own
  the sentence table on a Sound-subsystem object and replace the immediate-name
  hack with an explicit return/parameter — a live door concern, not style.
- **P-4**: none of this material exposes debug surfaces — reasoned "none".
- Other axes (G-1/2/3/4/5, P-1/2/5/6/7/8): reasoned "none" — not touched.

### 3.5 Uncertainties

- Raw-channel mouth write site (s_mix.c:467) gate/loop not fully re-derived
  (belongs to streaming/voice fragment scope).
- `PITCH_NORM` exact definition site not tracked down (assumed 100).
- `Mem_Free2` semantics vs. `Mem_Free` not investigated.

______________________________________________________________________

## 4. DSP (`s_dsp.c`)

Source: `engine/client/sound/s_dsp.c` (917 lines) — preset tables, table
selection, room selection, AMod/Reverb/Delay/StereoDelay, lifecycle, cvars,
profiling, `idsp_room` global.

### 4.1 Interface

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_dsp.c:211 | `SX_Init` zeroes 3 dly_t instances, zeroes rgsxlp, seeds amod, registers 11 cvars+1 cmd, calls `SX_ReloadRoomFX` | `void SX_Init( void ) { dly_t nulldly = { 0 }; monodly = nulldly;` + `Cmd_AddRestrictedCommand( "dsp_profile", SX_Profiling_f, ...)` | high |
| s_dsp.c:278 | `SX_Free` frees 4 delay lines, removes `dsp_profile` | `void SX_Free( void ) { DLY_Free( &monodly ); ... Cmd_RemoveCommand( "dsp_profile" ); }` | high |
| s_dsp.c:853 | `SX_RoomFX` sole per-block entry: early-out on room_off/0 samples, else CheckPresets→AMod→Reverb→Delay→StereoDelay | `void SX_RoomFX( portable_samplepair_t *paint, int num_samples ) { if( room_off.value \|\| !num_samples ) return; SX_CheckPresets(); RVB_DoAMod(...); ...}` | high |
| s_dsp.c:873 | `SX_ClearState` resets room_type cvar to "0", forces reload — does NOT free delay lines itself | `void SX_ClearState( void ) { Cvar_DirectSet( &room_type, "0" ); SX_ReloadRoomFX(); }` | high |
| s_dsp.c:196 | `SX_ReloadRoomFX` sets FCVAR_CHANGED on 4 cvars — the mechanism forcing preset re-derivation | `SetBits( sxste_delay.flags, FCVAR_CHANGED ); ...` | high |
| sound.h:98 | `SX_RoomFX(portable_samplepair_t*,int)` is the only cross-file contract besides Init/Free/ClearState/idsp_room | `void SX_RoomFX( portable_samplepair_t *paint, int num_samples );` | high |
| s_main.c:1668 | `idsp_room` (extern int) is read only by s_main.c's `s_show` debug overlay | `Con_NXPrintf( &info, "room_type: %i (%s) ----(%i)---- painted: %i\n", idsp_room, ...` | high |

### 4.2 Quirks and invariants

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_dsp.c:72-105 vs 109-142 | Two 29-entry preset tables (release vs. HL alpha 0.52); same count, differing values (e.g. water room_mod: 0.0 release vs 1.0 alpha) | `static const sx_preset_t rgsxpre_hlalpha052[] =` (SHA256-cited disassembly comment) | high |
| s_dsp.c:148,787-798,803 | `dsp_coeff_table` cvar selects table pointer; anything but 0/1 silently falls to `rgsxpre` default | `case 0: ptable = rgsxpre; break; case 1: ptable = rgsxpre_hlalpha052; break; default: ptable = rgsxpre; break;` | high |
| s_dsp.c:806 | Room selection: submerged (`waterlevel>2`) uses `roomwater_type`; else `room_type`; `waterlevel` set from network snapshot on T_Main | `idsp_room = cl.local.waterlevel > 2 ? roomwater_type.value : room_type.value;` | high |
| s_dsp.c:21,809 | **Off-by-one**: `MAX_ROOM_TYPES=29` (=ARRAYSIZE), but clamp is inclusive (`bound(0,idsp_room,29)`), permitting index 29 OOB for a 29-elem array (valid 0-28) | `#define MAX_ROOM_TYPES ARRAYSIZE( rgsxpre )` ... `idsp_room = bound( 0, idsp_room, MAX_ROOM_TYPES );` | high |
| s_dsp.c:817-843 | Edge-triggered preset re-apply cached via `room_typeprev`; 9 cvar writes on change, 3 re-check fns run every call once room≠0 | `if( idsp_room == room_typeprev && idsp_room == 0 ) return;` | high |
| s_dsp.c:824-832 | The 9 writes each set FCVAR_CHANGED, piggybacking on the same edge-detect machinery a manual console set would use | `Cvar_DirectSetValue( &sxrvb_size, cur->room_size ); ...` | med |
| s_dsp.c:300,355,464,551 | Delay sizing: `samples=(delay*idsp_dma_speed)<<sxhires`; `idsp_dma_speed` fixed to SOUND_11k at init, never re-queried — dead scaling generality | `idsp_dma_speed = SOUND_11k;` (set once, never reassigned) | high |
| s_dsp.c:300,312 | `DLY_Init` sizing precondition: `delaysamples` must be set before calling (else size_t underflow) | `cur->idelayoutput = cur->cdelaysamplesmax - cur->delaysamples; // NOTE: delaysamples must be set!!!` | high |
| s_dsp.c:346-376 | Stereo delay: only kind with lazy alloc-then-maybe-free dance on zero-delay | `if( delay == 0 ) { DLY_Free( dly ); } else { ... if( !dly->lpdelayline ) { ... DLY_Init( dly, MAX_STEREO_DELAY ); }` | high |
| s_dsp.c:386-441 | Stereo delay only writes `paint->left`; crossfades via 8-bit fixed-point blend | `samplexf = dly->lpdelayline[dly->idelayoutputxf] * (128 - dly->xfade) >> 7;` | high |
| s_dsp.c:496-537 | Mono delay averages L+R, feedback `(feedback*delay)>>8`, optional 3-tap lowpass, `>>2` attenuation added equally to both channels | `int val = (( paint->left + paint->right ) >> 1 ) + (( dly->delayfeedback * delay ) >> 8);` | high |
| s_dsp.c:582-604,597-598 | Reverb = 2 independent delay lines from one `room_size`: dly2 gets `delay*0.71`, different mod rates (500 vs 700) | `RVB_SetUpDly( dly1, delay, 500 ); RVB_SetUpDly( dly2, delay * 0.71f, 700 );` | high |
| s_dsp.c:613-678 | Reverb tap modulation: RNG re-randomizes readback offset when `mod==0` (a rate divisor, counter to what the name suggests) | `if( !dly->mod ) { dly->idelayoutputxf = dly->idelayoutput + ((COM_RandomLong( 0, 255 ) * delay) >> 9 ); ... }` | med |
| s_dsp.c:687-710 | Reverb mix gain depends on active coeff table: alpha `÷6` vs release `(11*voutm)>>6` — a second independent effect of `dsp_coeff_table` beyond table-pointer swap | `if( dsp_coeff_table.value == 1.0f ) voutm /= 6; else voutm = (11 * voutm) >> 6;` | high |
| s_dsp.c:719-781 | AMod (underwater lowpass+warble): early-out if both room_lp/room_mod are 0; 5-tap FIFO lowpass; LFO targets chase ±1/sample (sample-count-dependent, not time-dependent convergence) | `if( !sxmod_lowpass.value && !sxmod_mod.value ) return;` | med |
| s_dsp.c:228-229 | `sxmod1`/`sxmod2` scaled by dma-speed ratio at init, but always ×1 in practice since dma_speed is hardcoded — dead generality | `sxmod1cur = sxmod1 = 350 * ( idsp_dma_speed / SOUND_11k );` | high |
| s_dsp.c:40 | All int16 clamps route through shared `CLIP16` = `[SHRT_MIN+8,SHRT_MAX-8]` — 8-unit guard band, not full range | `#define CLIP16( x ) bound( SHRT_MIN + 8, x, SHRT_MAX - 8 )` | high |
| s_dsp.c:879-916 | `dsp_profile` cmd: synchronous 10000-call stress test with optional room_type override, times via `Platform_DoubleTime`; second `SX_CheckPresets` call path beyond `SX_RoomFX` | `start = Platform_DoubleTime(); for( calls = 10000; calls; calls-- ) { SX_RoomFX( testbuffer, 512 ); }` | high |
| s_dsp.c:172 vs sound.h:50 | `idsp_room` is the ONLY non-static global in the file — pure read cache for the `s_show` overlay | `int\t\t\tidsp_room;` (non-static; all else `static`) | high |

### 4.3 Cvars (registered `SX_Init`)

`room_off` (0, FCVAR_ARCHIVE, goldsrc-compat disable), `dsp_coeff_table` (0,
FCVAR_ARCHIVE, table select), `room_type` (0, no flags, live server/map
value), `waterroom_type` (14, no flags), `room_hires` (`hisound`, 2,
FCVAR_ARCHIVE, 1=22k/2=44k/3=96k), `room_mod` (`sxmod_mod`, 0), `room_lp`
(`sxmod_lowpass`, 0), `room_left` (`sxste_delay`, 0), `room_rvblp`
(`sxrvb_lp`, 1), `room_refl` (`sxrvb_feedback`, 0), `room_size`
(`sxrvb_size`, 0), `room_dlylp` (`sxdly_lp`, 1), `room_feedback`
(`sxdly_feedback`, 0.2), `room_delay` (`sxdly_delay`, 0.8). Only
`room_off`/`dsp_coeff_table`/`room_hires` carry `FCVAR_ARCHIVE` — preset-derived
cvars are expected to be overwritten every level/preset change (s_dsp.c:147-169).

### 4.4 Threading

| file:line | claim | class | confidence |
|---|---|---|---|
| s_dsp.c:171-186 | All file-scope statics touched only within the SX_*/RVB_*/DLY_* call chain — single-threaded-in-practice, unsynchronized | Race-static-buf | high |
| s_dsp.c:806 | Room selection reads `cl.local.waterlevel` (T_Main sim state). Under ratified SND-OQ-1, must be captured into `ListenerSnapshot`, NOT read live by the audio thread | Race-shared if naively ported | high |
| s_dsp.c:198-201,785,811,588,455,343 | The FCVAR_CHANGED edge-detect dance is Safe-TLS-equivalent iff preset selection stays co-located with cvar-owning thread; splitting sample-processing onto T_AudioCallback while leaving selection on T_Main requires a snapshot for the derived values | Safe-TLS (co-located) / Race-shared (split without snapshot) | med |

### 4.5 Uncertainties

- `Cvar_DirectSetValue`'s exact semantics (always sets FCVAR_CHANGED? fires
  `ICvarObserver`?) not read this pass — assumed from call-site usage.
- Exact SX_RoomFX call-site thread/callback context in the current build not
  traced (platform owns SNDDMA; threading-model doc not read this pass).
- Whether `dsp_coeff_table` should be runtime-switchable or build-time/compat-only
  in xash3dpp is a design question, not answered here.
- No G-*/P-* axis genuinely touched beyond the general P-3 door — flagged as
  door-debt if the file-scope-state/zero-arg shape is kept verbatim.

______________________________________________________________________

## 5. Sound-tree client-state coupling inventory

Source: `engine/client/sound/{s_main,s_mix,s_load,s_stream,s_dsp,s_mouth}.c`

- the two engine-side spatialization providers in `cl_frame.c`. `voice.c`/

`s_vox.c` EXCLUDED except where a raw/voice channel is touched.

### 5.1 Groups (a)-(g)

**Group (a) — listener pose:**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:1590-1597 | `S_UpdateFrame` sole listener-pose writer, from renderer `ref_viewpass_s`, NOT from `cl.*` directly | `VectorCopy( rvp->vieworigin, snd.origin );` |
| s_main.c:1592 | Gated on world-draw view pass only | `if( !FBitSet( rvp->flags, RF_DRAW_WORLD ) \|\| FBitSet( rvp->flags, RF_ONLY_CLIENTDRAW )) return;` |
| s_main.c:591,595 | Consumed by `SND_Spatialize`: `source=ch->origin-snd.origin`, `dot=snd.right·src` | `VectorSubtract( ch->origin, snd.origin, source_vec );` |
| s_main.c:1391,1395 | Same pose consumed by `S_SpatializeRawChannels` | `VectorSubtract( ch->origin, snd.origin, source_vec );` |
| s_main.c:157-160,568 | `S_IsClient(entnum)=entnum==snd.entnum`; view entity forced full volume | `return entnum == snd.entnum;` |
| s_main.c:651 | NULL pos → `refState.vieworg` | `if( !pos ) pos = refState.vieworg;` |

Cadence: per-frame (S_UpdateFrame once per view pass; respatialize loop
re-reads every frame for all channels).

**Group (b) — ambient (worldmodel/leaf levels):**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:1091-1094 | Reads `cl.worldmodel`, calls `Mod_PointInLeaf(snd.origin,...)`; bails if none | `if( !cl.worldmodel ) return;` |
| s_main.c:1117 | Volume = `s_ambient_level * leaf->ambient_sound_level[i]` (map_loader-owned BSP data) | `float vol = s_ambient_level.value * leaf->ambient_sound_level[i];` |
| s_main.c:1124,1130 | Fade rate scaled by `cl_clientframetime()=cl.time-cl.oldtime` | `chan->master_vol += round( cl_clientframetime() * s_ambient_fade.value );` |
| client.h:116 | `cl_clientframetime()` macro | `#define cl_clientframetime() (cl.time - cl.oldtime)` |

Cadence: per-frame, unconditional from `SND_UpdateSound`.

**Group (c) — DSP water:**

| file:line | claim | evidence |
|---|---|---|
| s_dsp.c:806 | Only client-state read in the DSP path: `cl.local.waterlevel>2` gates room preset | `idsp_room = cl.local.waterlevel > 2 ? roomwater_type.value : room_type.value;` |
| s_dsp.c:806,857 | Read lives inside `SX_CheckPresets()`, called from `SX_RoomFX()` — inside the mix path | `SX_CheckPresets();` |
| client.h:137 | `waterlevel` int field of `cl.local` | `int waterlevel;` |

Cadence: per-mix, gated `cls.key_dest != key_menu`.

**Group (d) — per-entity spatialization providers (`cl_frame.c`):**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:580,900 | Dynamic/static channels resolve via `CL_GetEntitySpatialization(ch)`; failure zeroes volume | `if( !CL_GetEntitySpatialization( ch )) { ch->leftvol = ch->rightvol = 0; return; }` |
| s_main.c:1380-1382 | Raw/movie via `CL_GetMovieSpatialization(ch)`, bounded by `GI->max_edicts` | `if( !S_IsClient( ch->entnum ) && ch->dist_mult && ch->entnum >= 0 && ch->entnum < GI->max_edicts )` |
| cl_frame.c:1375-1378 | `entnum==0` ⇒ static world sound, no client read | `if( ch->entnum == 0 ) { SetBits( ch->flags, FL_CHAN_STATIC_SOUND ); return true; }` |
| cl_frame.c:1381-1385 | View-player channel copies `refState.vieworg` | `if(( ch->entnum - 1 ) == cl.playernum ) { VectorCopy( refState.vieworg, ch->origin ); return true; }` |
| cl_frame.c:1387-1392 | Else `CL_GetEntityByIndex`; presence test = `messagenum != cl.parsecount`; stale/missing keeps prior origin iff non-null | `if( !ent \|\| !ent->model \|\| ent->curstate.messagenum != cl.parsecount ) return valid_origin;` |
| cl_frame.c:1395-1403 | Brush ents use model-bbox center+origin; others use origin | `if( ent->model->type == mod_brush ) { VectorAverage(...); VectorAdd(...); }` |
| cl_frame.c:1413-1418 | Movie provider: presence test differs (`index && messagenum!=0`, no player special-case) | `if( !ent \|\| !ent->index \|\| ent->curstate.messagenum == 0 ) return valid_origin;` |
| client.h:184,199,195 | Provider fields: `cl.playernum`, `cl.parsecount`, `cl.servercount` | `int playernum; int parsecount; int servercount;` |

Cadence: per-sound-start AND per-frame respatialize; origins re-pulled from
live entity state every frame, not cached at start.

**Group (e) — mouth write-back (atomic write into client entity):**

| file:line | claim | evidence |
|---|---|---|
| s_mix.c:376-388 | VOICE/STREAM channels drive `SND_MoveMouth8/16(&ent->mouth,...)` | `SND_MoveMouth16( &ent->mouth, ch->sample, sc, mouth_count, ... );` |
| s_mix.c:462-467 | Raw channels drive `SND_MoveMouthRaw(&ent->mouth,...)` | `if( ent ) SND_MoveMouthRaw( &ent->mouth, &ch->rawsamples[pos], count );` |
| s_mouth.c:23-33 | Mutates `mouth->{sndavg,sndcount,mouthopen}` on the CLIENT entity | `mouth->sndavg += savg; mouth->mouthopen = mouth->sndavg / CAVGSAMPLES;` |
| s_mouth.c:89-111 | Force Init/Close also write `clientEntity->mouth` | `if( clientEntity ) clientEntity->mouth.mouthopen = 0;` |
| s_main.c:1326 | Idle raw-channel reaper calls `SND_ForceCloseMouth` | `SND_ForceCloseMouth( ch->entnum );` |

Cadence: per-mix; this is the **only WRITE** into client state from the
sound tree.

**Group (f) — gating (key_dest/paused/background/focus/timescale):**

| file:line | claim | evidence |
|---|---|---|
| s_main.c:119 | Master volume 0 when unfocused + snd_mute_losefocus | `if( host.status == HOST_NOFOCUS && snd_mute_losefocus.value != 0.0f ) return 0.0f;` |
| s_main.c:127-132 | Scaled by soundfade unless in menu | `if( cls.key_dest != key_menu && soundfade.percent != 0 ) { ... }` |
| s_main.c:149,222-224 | Soundfade timeline driven by `host.realtime` | `soundfade.start_time = host.realtime;` |
| s_mix.c:316,391 | Pitch mult `(sys_timescale+1)/2` (chipmunk, FWGS) | `const float pitch_mult = ( sys_timescale.value + 1 ) / 2;` |
| s_main.c:929 | Same scaling at `S_StartStaticSound` | `pitch *= (sys_timescale.value + 1) / 2;` |
| s_mix.c:323-324 | Mix skipped when `cl.background && key_dest==key_console` | `if( cl.background && cls.key_dest == key_console ) return num_mixed_channels;` |
| s_mix.c:333-348 | Per-channel gate branches on background/key_dest/paused/singleplayer/ingame/local/static flags | `else if(( cls.key_dest == key_menu \|\| cl.paused ) && !FBitSet( ch->flags, FL_CHAN_LOCAL_SOUND ) && sp ) continue;` |
| s_mix.c:310-311 | `sp`/`ingame` sampled once per mix block | `const qboolean sp = Host_IsSinglePlayerGame();` |
| s_mix.c:366-367 | Inaudible-channel free timer keyed on `host.realtime` | `ch->inauduble_free_time = host.realtime + MAX_CHANNEL_INAUDIBLE_TIME;` |
| s_mix.c:418 | Raw mix skipped when `cl.paused` | `if( cl.paused ) return 0;` |
| s_mix.c:434 | Voice test = `CL_IsPlayerIndex` + loopback sentinels | `qboolean is_voice = CL_IsPlayerIndex( ch->entnum ) \|\| ch->entnum == VOICE_LOOPBACK_INDEX ...` |
| s_mix.c:562,565 | DSP + gain-mix gated on `key_dest != key_menu` | `if( cls.key_dest != key_menu ) SX_RoomFX( roombuffer, num_samples );` |
| s_main.c:597 | Panning suppression reads `host.bugcomp` | `if( !FBitSet( host.bugcomp, BUGCOMP_SPATIALIZE_SOUND_WITH_ATTN_NONE ))` |
| s_main.c:1036 | Looped static handling reads `Host_IsQuakeCompatible()` | `if( ch->entchannel == CHAN_STATIC && looped && !Host_IsQuakeCompatible())` |
| s_stream.c:71,77 | Music volume mirrors master gates | `if( host.status == HOST_NOFOCUS && snd_mute_losefocus.value != 0.0f ) return 0.0f;` |
| s_stream.c:204,207-214 | Background-track pump gated on paused/background/source-vs-key_dest | `if( !s_musicvolume.value \|\| cl.paused \|\| snd.stream_paused ) return;` |
| s_stream.c:119 | Track start captures `cls.key_dest` into `s_bgTrack.source` | `s_bgTrack.source = cls.key_dest;` |
| cl_main.c:118-127 | `CL_IsInGame()` reads host.type/cl.background/maxclients/key_dest | `if( cl.background \|\| cl.maxclients > 1 ) return true; return ( cls.key_dest == key_game );` |
| common.h:861-866 | `Host_IsSinglePlayerGame()` reaches into server state (`SV_Active`/`SV_GetMaxClients`) | `if( SV_Active( )) return SV_GetMaxClients() == 1 ...;` |
| common.h:569-572 | `Host_IsQuakeCompatible()` reads `host.features` | `return FBitSet( host.features, ENGINE_QUAKE_COMPATIBLE ) ? true : false;` |
| client.h:1040-1043 | `CL_IsPlayerIndex` reads `cl.maxclients` | `return idx >= 1 && idx <= cl.maxclients ? true : false;` |

Cadence: mixed (per-paint, per-mix, per-frame, per-start — see per-row notes).

**Group (g) — registration-time:**

| file:line | claim | evidence |
|---|---|---|
| s_load.c:261-266 | `S_BeginRegistration` registers auto-ambient set from `GI->ambientsound` (gameinfo-owned) | `snd.ambient_sfx[i] = S_RegisterSound( GI->ambientsound[i] );` |
| s_load.c:176,196,332 | sfx registration stamps `sfx->servercount = cl.servercount` | `sfx->servercount = cl.servercount;` |
| s_load.c:292-293 | `S_EndRegistration` frees sfx with stale `servercount` (per-connection GC) | `if( sfx->servercount != cl.servercount ) S_FreeSound( sfx );` |
| s_load.c:120 | Late-precache warning fires when `cls.state==ca_active` | `if( s_warn_late_precache.value > 0 && cls.state == ca_active )` |
| s_main.c:1380 | Raw-channel bound = `GI->max_edicts` | `... && ch->entnum < GI->max_edicts )` |

Cadence: per-registration (connect/level-load) + re-stamped per `S_RegisterSound`.

### 5.2 Provider interface shapes (feeds the Interface section of the boundary spec)

- **`ListenerSnapshot`** POD (groups a+c+f frame-invariants): origin,
  forward/right/up, view_entnum, view_origin, waterlevel, master_scale_gates
  {unfocused,in_menu}, realtime, frametime, timescale, bugcomp_attn_none,
  quake_compat. SND-OQ-1: ships-in-command-stream.
- **`MixGateSnapshot`** POD (group f mix-time): key_dest, paused, background,
  singleplayer, in_game, maxclients. SND-OQ-1: ships-in-command-stream.
- **`IEntitySpatialProvider`** (group d): resolves channel origin from entity
  index using client entity state, T_Main-only. Two variants (entity/movie)
  differ only in presence test + view-player special-case — one provider
  with a mode flag covers both. SND-OQ-1: main-computed-channel-param.
- **`IMouthSink`** (group e): {entnum, mouthopen (WRITE)}. SND-OQ-1:
  atomic-write-back; sndavg/sndcount can stay mix-local.
- **`RegistrationSnapshot`** POD (group g): ambientsound[4], servercount,
  cl_active, max_edicts. SND-OQ-1: ships-in-command-stream.

### 5.3 Threading

Legacy: everything on T_Main (no threads in the sound tree; `SNDDMA_Submit`
hands the filled buffer to the platform callback). xash3dpp target
(threading-model §3.4): T_Main → MPSC → T_AudioDecoder → SPSC ring →
T_AudioCallback.

| shared state | legacy thread | xash3dpp thread | class | note |
|---|---|---|---|---|
| `snd.origin/forward/right/up/entnum` | T_Main (write@UpdateFrame, read@Spatialize) | T_Main writes snapshot; mix reads copy | Race-shared → resolved by snapshot | canonical ships-in-command-stream payload |
| `cl.worldmodel`+BSP leaf | T_Main | T_Main only | Race-shared if read off-Main | worldmodel pointer swaps at level load |
| `cl.local.waterlevel` (DSP) | T_Main (read in mix) | must be snapshot field | Race-shared | clearest OQ-1 mix-thread read to hoist |
| `CL_GetEntityByIndex` entity array | T_Main | T_Main provider only | Race-shared | mutated by netchan parse on T_Main |
| `cl_entity_t.mouth` (write-back) | T_Main (written during mix) | mix computes, marshals to T_Main | Race-shared (write) | the only WRITE into client state |
| `cls.key_dest`/`cl.paused`/`cl.background` | T_Main | T_Main snapshot | Race-shared if read live | fold into MixGateSnapshot |
| `host.realtime`/`host.status`/`sys_timescale`/`host.bugcomp`/`host.features` | T_Main | T_Main snapshot | Race-shared if read live | snapshot at frame boundary |
| `soundfade`/`musicfade`/`s_bgTrack` | T_Main | T_Main (owned) | Safe-TLS→Race-static-buf | shared if stream pump moves off-Main |
| `cl.servercount`/`GI->ambientsound`/`GI->max_edicts` | T_Main | T_Main | Safe-RO during mix | control-plane, stable during connection |
| `snd.channels[]`/`snd.raw_channels[]` | T_Main | producer T_Main, consumer T_AudioDecoder | Race-shared | the command-stream boundary itself |

### 5.4 Extension axes (Q-21)

| axis | relevance |
|---|---|
| P-1 | Direct — Chunk 9 is the named "first production MPSC" validator; group (e) mouth write-back needs a reverse-direction inbox slot too |
| P-2 | Direct — the three snapshot PODs ARE the P-2 shape; every live group a/c/f global read is a §9 Rule-1 violation if left un-snapshotted |
| P-3 | Direct — `IEntitySpatialProvider`/`IMouthSink` replace capture-less `CL_GetEntitySpatialization`/`CL_GetEntityByIndex` globals |
| P-5 | Direct — provider interfaces take the sub-aggregate touched, not `client_t&` |
| G-2 | Indirect — `pfnS_Spatialize`/`pfnGetSoundInterface` are the GoldSrc client-DLL seam a v2 interface replaces |

G-1/G-3/G-4/P-4/P-6/P-7/P-8: reasoned "none" — this is the read-inventory,
not a service/lifecycle surface.

### 5.5 Uncertainties

- Voice path (loopback sentinels, `Voice_IsRecording()`) noted only where
  gating a raw channel — full coupling out of scope.
- Whether mouth accumulators must cross the decode-block boundary (vs.
  staying mix-private) is a design call under blocking cadence, not
  derivable from single-threaded legacy.
- `refState` update ordering relative to `SND_UpdateSound` assumed from call
  site, not traced end-to-end.
- `Host_IsSinglePlayerGame()`'s client→server read crosses subsystem
  boundary safely today (same thread); whether the snapshot should carry a
  precomputed bool vs. the audio side calling server APIs is a
  boundary-owner decision.

______________________________________________________________________

## 6. SNDDMA + SoundAPI ABI

Source: the 5 `SNDDMA_*` platform functions + `VoiceCapture_*` functions
(`platform.h:389-402`), SDL2 + stub implementations, full
`common/sound_api.h` client-extension ABI.

### 6.1 SNDDMA_* platform seam

| file:line | claim | evidence | confidence |
|---|---|---|---|
| platform.h:390 | `SNDDMA_Init` returns qboolean | `qboolean SNDDMA_Init( void );` | high |
| platform.h:391-394 | Full 5-fn set: Init, Shutdown, BeginPainting, Submit, Activate(qboolean) | `void SNDDMA_Shutdown( void ); void SNDDMA_BeginPainting( void ); void SNDDMA_Submit( void ); void SNDDMA_Activate( qboolean active );` | high |
| platform.h:395-397 | 3 more SNDDMA_* fns declared-but-commented-out (dead surface) | `// void SNDDMA_PrintDeviceName( void ); // unused` | high |
| platform.h:399-402 | **VoiceCapture_* is 4 fns, not 3**: Init, Shutdown, Activate, Lock | `qboolean VoiceCapture_Init( void ); void VoiceCapture_Shutdown( void ); qboolean VoiceCapture_Activate( qboolean activate ); qboolean VoiceCapture_Lock( qboolean lock );` | high |
| s_main.c:2013-2020 | Init-failure path: pool allocated + backend_name="None" BEFORE `SNDDMA_Init`; on failure, pool freed, no other cleanup | `sndpool = Mem_AllocPool( "Sound Zone" ); ... if( !SNDDMA_Init( )) { ... Mem_FreePool( &sndpool ); return false; }` | high |
| s_main.c:1992-1996 | `-nosound` check happens even before that, never calls SNDDMA_Init | `if( Sys_CheckParm( "-nosound" )) { ... return false; }` | high |

### 6.2 SDL2 backend

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_sdl2.c:184-187 | `SNDDMA_BeginPainting` = `SDL_LockAudioDevice` — passthrough, no return/failure path | `void SNDDMA_BeginPainting( void ) { SDL_LockAudioDevice( sdl_dev ); }` | high |
| s_sdl2.c:197-200 | `SNDDMA_Submit` = `SDL_UnlockAudioDevice` — the paired unlock | `void SNDDMA_Submit( void ) { SDL_UnlockAudioDevice( sdl_dev ); }` | high |
| s_sdl2.c:238-244 | `SNDDMA_Activate` no-op guarded by `snd.initialized` | `if( !snd.initialized ) return; SDL_PauseAudioDevice( sdl_dev, !active );` | high |
| s_sdl2.c:40-68 | Callback ring-advance: `pos=samplepos<<1`, wrap detection via `wrapped=pos+len-size`, split memcpy on wrap | `pos = snd.samplepos << 1; if( pos >= size ) pos = snd.samplepos = 0; wrapped = pos + len - size;` | high |
| s_sdl2.c:42 | `size = snd.samples<<1` (bytes, 16-bit width) | `const int size = snd.samples << 1;` | high |
| s_sdl2.c:123-128 | Requests `AUDIO_S16SYS` at `SOUND_DMA_SPEED`, stereo, 1024-sample internal buffer, push-model callback | `desired.format = AUDIO_S16SYS; desired.samples = 1024; desired.callback = SDL_SoundCallback;` | high |
| s_sdl2.c:138-148 | Format negotiation NOT best-effort: any format other than S16SYS or channel count outside {1,2} fails init | `if( obtained.format != AUDIO_S16SYS ) { ... goto fail; }` | high |
| s_sdl2.c:150-158 | `s_samplecount` sizing: default `0x8000` if unset; `samples=samplecount*channels`; buffer=`samples*2` bytes (width hardcoded 2) | `snd.samples = samplecount * obtained.channels; snd.buffer = Mem_Calloc( sndpool, snd.samples * 2 );` | high |
| s_sdl2.c:99-121 | Driver hints: Win32 forces directsound (FMOD/WASAPI hang workaround) unless user sets env; PulseAudio env vars set unconditionally; audio subsystem force-reinit'd every call | `#if XASH_WIN32 ... SDL_SetHint( SDL_HINT_AUDIODRIVER, driver ); #endif` | high |
| s_sdl2.c:167 | `SNDDMA_Activate(true)` called unconditionally at end of successful Init | `SNDDMA_Activate( true ); return true;` | high |
| s_sdl2.c:209-228 | Shutdown: `initialized=false` first, then conditional pause+close, unconditional SDL_INIT_AUDIO quit, then buffer free | `snd.initialized = false; if( sdl_dev ) { SNDDMA_Activate( false ); SDL_CloseAudioDevice( sdl_dev ); } SDL_QuitSubSystem( SDL_INIT_AUDIO );` | high |

### 6.3 VoiceCapture_* (SDL2)

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_sdl2.c:268-294 | `VoiceCapture_Init` self-heals stale device before opening new one sized to `voice.samplerate`/`frame_size` | `if( !SDLash_IsAudioError( in_dev )) { VoiceCapture_Shutdown(); } wanted.freq = voice.samplerate;` | high |
| s_sdl2.c:301-324 | `Activate`/`Lock` both early-return false on sentinel "no device" | `if( SDLash_IsAudioError( in_dev )) return false;` | high |
| s_sdl2.c:251-261 | Input callback clamps to remaining buffer space, silently drops audio if engine hasn't drained (no error) | `int size = Q_min( len, sizeof( voice.input_buffer ) - voice.input_buffer_pos ); if( !size ) return;` | high |

### 6.4 Stub backend — null-device precedent

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_stub.c:19,46-50 | Compiles only under `XASH_SOUND==SOUND_NULL`; `Init` always returns false | `qboolean SNDDMA_Init( void ) { Msg( "Audio is not enabled\n" ); return false; }` | high |
| s_stub.c:59-75 | `BeginPainting`/`Submit` both empty bodies — no lock semantics under null backend | `void SNDDMA_BeginPainting( void ) { }` | high |
| s_stub.c (whole file) | **`SNDDMA_Activate` NOT defined at all** — asymmetric vs. every other backend (sdl1/2/3, alsa all define it) | (absence confirmed by full-file read + repo-wide grep) | high |
| s_stub.c:84-94 | `Shutdown` still sets initialized=false, frees buffer if set — symmetric teardown despite never succeeding at Init | `snd.initialized = false; if( snd.buffer ) { Mem_Free( snd.buffer ); snd.buffer = NULL; }` | high |
| s_stub.c:96-114 | All 3 VoiceCapture_* stub fns are trivial false/no-op | `qboolean VoiceCapture_Init( void ) { return false; }` | high |
| s_stub.c:33-35 | Stub also defines `S_Activate(qboolean)` — outside the SNDDMA block's scope but co-located | `void S_Activate( qboolean active ) { }` | med |

### 6.5 Interface — `common/sound_api.h` full layout inventory

| file:line | claim | evidence | confidence |
|---|---|---|---|
| sound_api.h:37 | `CL_SOUND_INTERFACE_VERSION=1`, header marked experimental/no-ABI-guarantee | `#define CL_SOUND_INTERFACE_VERSION\t1` | high |
| sound_api.h:76-105 | `channel_t` full layout (name[16], sfx*, origin, dist_mult, entchannel, flags, entnum/master_vol/leftvol/rightvol/basePitch shorts, word_index byte, inauduble_free_time float, sample/forced_end double, data*, words*) THEN `engine_reserved[8]`/`game_reserved[8]` | `uintptr_t engine_reserved[8]; uintptr_t game_reserved[8]; } channel_t;` | high |
| sound_api.h:107-123 | `rawchan_t` full layout: entnum/vols/dist_mult/origin, `volatile uint s_rawend`, oldtime, THEN same reserved-padding pair, then max_samples + flexible rawsamples[] | `volatile uint s_rawend; float oldtime; uintptr_t engine_reserved[8]; uintptr_t game_reserved[8]; size_t max_samples; portable_samplepair_t rawsamples[];` | high |
| sound_api.h:115 | `s_rawend` is the ONLY `volatile` field across all 3 structs | `volatile uint s_rawend;` | high |
| sound_api.h:132-160 | `snd_globals_t` full layout: dma group, timing group, listener group, SoundAPI shared-pointer group (`channel_t *const channels`, `max_channels`/`total_channels`, `rawchan_t **const raw_channels`, `max_raw_channels`, `ambient_sfx[NUM_AMBIENTS]`, `have_ambient_sfx`) | `channel_t *const channels; int max_channels; int total_channels; rawchan_t **const raw_channels;` | high |
| sound_api.h:125-130 | `snd_format_t` 3-field POD: speed, width, channels | `typedef struct snd_format_s { uint speed; byte width; byte channels; } snd_format_t;` | high |
| sound_api.h:170-178 | `sound_api_t` (engine→client, 5 slots): `CL_GetEntitySpatialization`, `S_GetSfxByHandle`, `pfnS_RawEntSamples`, `pfnSND_ForceInitMouth`, `pfnVoice_GetAudioInfo` | `qboolean (*CL_GetEntitySpatialization)( channel_t *ch ); sfx_t* (*S_GetSfxByHandle)( sound_t handle );` | high |
| sound_api.h:181-194 | `sound_interface_t` (client→engine, versioned, 8 slots): pfnS_Init/Shutdown/UpdateSound/PaintChannels/UpdateChannel/UpdateRawChannel/Spatialize/FreeSound; ch=NULL sentinel documented for both Update* | `void (*pfnS_UpdateChannel)( int ch_idx, const channel_t *ch, sound_t handle ); // ch=NULL -> channel freed` | high |
| sound_api.h:188 | `pfnS_PaintChannels` doc: engine passes endtime+implicit dma buffer+in/out paintedtime; client owns mix+transfer | `/* Full paint: endtime (sample pairs), dma buffer, paintedtime in/out. Client does mix + transfer to dma.buffer. */` | high |
| sound_api.h:65-74 | `voxword_t` (reachable from `channel_t.words`): sfx*, volume/pitch uint16, timecompress/start/end/flags uint8 | `typedef struct voxword_s { sfx_t *sfx; uint16_t volume; uint16_t pitch; ... } voxword_t;` | high |

### 6.6 SoundAPI dispatch sites

| file:line | claim | evidence | confidence |
|---|---|---|---|
| s_main.c:1935-1941 | `gSoundAPI` wires exactly the 5 declared slots | `static const sound_api_t gSoundAPI = { CL_GetEntitySpatialization, S_GetSfxByHandle, S_RawEntSamples, SND_ForceInitMouth, Voice_GetAudioInfo, };` | high |
| s_main.c:1948-1971 | `S_InitSoundAPI`: clears soundFuncs first, negotiates, re-clears to zero on failure | `memset( &clgame.soundFuncs, 0, sizeof( clgame.soundFuncs )); if( clgame.dllFuncs.pfnGetSoundInterface ) { ... }` | high |
| s_main.c:1967-1968 | `pfnS_Init` dispatched unconditionally-checked at tail, passed `&snd` — even the zeroed failure path probes it | `if( clgame.soundFuncs.pfnS_Init ) clgame.soundFuncs.pfnS_Init( &snd );` | high |
| s_main.c:2060-2061 | `pfnS_Shutdown` dispatch before engine's own StopAllSounds/FreeRawChannels/FreeSounds | `if( clgame.soundFuncs.pfnS_Shutdown ) clgame.soundFuncs.pfnS_Shutdown();` | high |
| s_main.c:1611-1612 | `pfnS_UpdateSound` dispatch, no engine fallback | `if( clgame.soundFuncs.pfnS_UpdateSound ) clgame.soundFuncs.pfnS_UpdateSound();` | high |
| s_main.c:1562-1565 | `pfnS_PaintChannels` — the ONLY override with an engine-side fallback (`S_PaintChannels`) | `if( clgame.soundFuncs.pfnS_PaintChannels ) clgame.soundFuncs.pfnS_PaintChannels( endtime ); else S_PaintChannels( endtime );` | high |
| s_main.c:167-173 | `pfnS_UpdateChannel` dispatch wrapped in private helper `S_NotifyChannelUpdate` | `static void S_NotifyChannelUpdate(...) { if( !clgame.soundFuncs.pfnS_UpdateChannel ) return; ... }` | high |
| s_main.c:207-212 | `pfnS_UpdateRawChannel` same private-helper pattern | `static void S_NotifyRawChannelUpdate(...) { ... }` | high |
| s_main.c:561-563 | `pfnS_Spatialize` dispatch, guarded inline, no fallback | `if( clgame.soundFuncs.pfnS_Spatialize ) { clgame.soundFuncs.pfnS_Spatialize( ch );` | high |
| s_load.c:235-237 | `pfnS_FreeSound` — the only dispatch site living in s_load.c, passes pointer-arithmetic index | `if( clgame.soundFuncs.pfnS_FreeSound ) { clgame.soundFuncs.pfnS_FreeSound( sfx, sfx - s_knownSfx );` | high |
| cdll_exp.h:84 | `pfnGetSoundInterface` client DLL export slot signature | `int (*pfnGetSoundInterface)( int version, const sound_api_t *api, sound_interface_t *callback );` | high |
| cl_game.c:110 | Export name `"HUD_GetSoundInterface"`, marked Xash3D FWGS extension (not stock GoldSrc) | `{ "HUD_GetSoundInterface", (void **)&clgame.dllFuncs.pfnGetSoundInterface }, // Xash3D FWGS ext` | high |
| cl_game.c:4135 | `S_InitSoundAPI()` call site — invoked once from client DLL init, not from s_main.c's own S_Init | `S_InitSoundAPI();` | high |

### 6.7 Threading

| file:line | claim | class | legacy thread | xash3dpp thread | confidence |
|---|---|---|---|---|---|
| s_sdl2.c:40-68 | `SDL_SoundCallback` reads/mutates `snd.samples`/`samplepos`/`buffer` — same globals `S_UpdateChannels` writes under lock | Race-shared (mitigated) | SDL's own audio callback thread vs. T_Main | maps to T_AudioCallback vs. T_AudioDecoder/mix stage | med |
| s_sdl2.c:184-200 | `BeginPainting`/`Submit` = `SDL_Lock/UnlockAudioDevice` — the actual sync primitive in legacy | Safe-TLS-equivalent | T_Main holds lock during mix; SDL thread implicitly during callback | SPSC ring is designed to replace this lock-around-buffer pattern | med |
| sound_api.h:115 | `s_rawend` volatile — cross-context marker but legacy is single-writer/single-reader same-thread (compiler-reordering guard only) | Race-lazy-init-adjacent / Safe-RO-by-convention | T_Main only (legacy single-threaded sound) | needs to move onto the ratified SPSC ring, not stay a bare volatile | low |
| s_stub.c:96-114 | VoiceCapture stub fns touch no shared state (constant returns) | Safe-RO | any (never reached) | any | high |
| s_main.c:1953-1968 | `clgame.soundFuncs` written once at init, read-only thereafter | Safe-RO (post-init) / Race-lazy-init (init window) | T_Main (single client init) | T_Main only — cold-path bootstrap, audio pipeline threads never touch it | high |

### 6.8 Uncertainties

- Assignment brief said "3 VoiceCapture fns" — R9.6 verified 4 at
  `platform.h:399-402`; documented all 4, flagged the discrepancy for
  adjudication (resolved in the boundary spec as a correction).
- `s_stub`'s missing `SNDDMA_Activate`: confirmed absent, confirmed present
  elsewhere; whether this is dead code (Init always fails ⇒ never reached)
  or a genuine link-time gap was not exhaustively verified across every
  guard condition.
- `SDL_SoundCallback`'s final defensive bounds check possibly-dead-code
  status not resolved.
- SDL3/ALSA backends not verified in detail (out of assignment scope;
  confirmed via grep to also define the full SNDDMA_Activate set).
- `S_RawEntSamples`/`SND_ForceInitMouth`/`Voice_GetAudioInfo`/
  `CL_GetEntitySpatialization`/`S_GetSfxByHandle` implementations (behind
  `gSoundAPI`'s 5 slots) not traced — out of this fragment's SNDDMA/dispatch
  scope.
- `snd_format_t` vs. SDL's `SDL_AudioSpec` cross-assignment only verified at
  one citation (s_sdl2.c:150-152), not cross-checked against every backend.
