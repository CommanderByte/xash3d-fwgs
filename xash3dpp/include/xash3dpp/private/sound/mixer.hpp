#pragma once
// xash3dpp — the paint pipeline core (Chunk 9, slice S9.3). PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_mix.c — S_PaintChannels (:542),
// S_MixNormalChannelsToRoombuffer (:308), S_MixRawChannels (:414),
// S_MixChannelToBuffer (:197), S_AdjustNumSamples (:175), S_TransferPaintBuffer
// (:503), S_ClearBuffers (:534), S_MixBufferWithGain (:474) + s_main.c
// S_RetrieveAudioSamples / S_AdjustLoopedSamplePosition (:66-108) and
// S_GetSoundtime (:1503, the 0x40000000 overflow reset).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Quirks (paint order, kernel
// law, 0x40000000 reset, menu-gate asymmetry), §Owned state (paintbuffer/
// roombuffer = worker-local; snd.channels/raw_channels via limits; S_GetSoundtime
// statics = mix-clock reconstruction), §Threading (SND-OQ-1: gates come ONLY from
// the MixGateSnapshot POD — no live cl.* reads).
//
// @thread-safety: the Mixer is confined to the T_AudioDecoder mix worker
// (amended §3.4 — mixer = T_AudioDecoder-owned). It owns worker-local scratch
// (paintbuffer_/roombuffer_) + the mix-clock reconstruction state + the mix-side
// channel arrays. As of S9.7b the role is ENFORCED: paint_channels() asserts
// ThreadRole::AudioDecoder (the S9.3 compliance-allow(thread-assert) exemption is
// retired now that a real decoder thread exists). Any driver of the paint —
// including a synchronous/main-pumped one — must therefore run on a thread that
// registered that role. The free-function primitives and mix kernels below stay
// assert-free (hot path; the role is established once at the paint entry).

#include <xash3dpp/abi/sound_api.hpp>              // sound_t (S9.6 MixChannel::sfx_handle)
#include <xash3dpp/private/sound/mix_kernels.hpp> // Interp, mix_audio, portable_samplepair_t
#include <xash3dpp/sound/audio_data.hpp>          // AudioData
#include <xash3dpp/sound/providers.hpp>           // MixGateSnapshot

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace xash::sound {

// ---------------------------------------------------------------------------
// Named arithmetic-law constants (not tunable budgets — clamp bounds / thresholds
// that MUST stay verbatim for bit-exact parity; kept here, not in limits.hpp).
// ---------------------------------------------------------------------------

// CLIP16 guard band (sound.h:40): bound(SHRT_MIN + 8, x, SHRT_MAX - 8). NOT the
// full int16 range — the ±8 guard band is a load-bearing quirk.
inline constexpr int k_clip16_min = -32768 + 8; // SHRT_MIN + 8 = -32760
inline constexpr int k_clip16_max = 32767 - 8;  // SHRT_MAX - 8 =  32759

// S_MixBufferWithGain unity fast-path (s_mix.c:476): gain == 256 == 1.0 * 256.
inline constexpr int k_gain_unity = 256;

// S_GetSoundtime 32-bit overflow-reset threshold (s_main.c:1519).
inline constexpr int k_soundtime_wrap_reset = 0x40000000;

// Channel audibility floor (s_mix.c:361): leftvol<8 && rightvol<8 ⇒ inaudible.
inline constexpr int k_channel_inaudible_vol = 8;

// PITCH_NORM (common/const.h:627) — the "no pitch shift" percent value, both
// for MixChannel::base_pitch/vox_pitch and VOX_ModifyPitch's no-op gate.
inline constexpr std::uint16_t k_pitch_norm = 100;

// ---------------------------------------------------------------------------
// MixChannel — the mix-side per-channel state the kernels need (boundary
// deliverable 3: "pos/frac/end/volumes/sfx data pointer/looping fields per
// channel_t semantics"). This is the REAL mix-side channel; the ABI channel_t
// array (owned_state.hpp ChannelState) stays layout-pinned/plumbing-deferred and
// is bridged to these by channel allocation (SND_PickDynamicChannel etc.) in
// S9.6. `source` is the per-channel sample-source abstraction — the VOX seam
// (S9.4) re-points it at each sentence word's AudioData between mix calls.
// ---------------------------------------------------------------------------
struct MixChannel
{
    const AudioData *source = nullptr; // per-channel sample source (chan->sfx->cache); VOX rebinds per word
    double sample       = 0.0;         // playback position in source frames (chan->sample)
    double forced_end   = 0.0;         // save/restore truncation point; 0 = none (chan->forced_end)
    int    leftvol      = 0;           // 0-255 (chan->leftvol) — re-clamped at mix
    int    rightvol     = 0;           // 0-255 (chan->rightvol)
    double base_pitch   = 100.0;       // percent, 100 = normal (chan->basePitch)
    std::uint32_t flags = 0;           // FL_CHAN_* (use_loop / finished / sentence_finished / static / local)
    int    entchannel   = 0;           // CHAN_* (voice/stream/static) — gating + mouth
    int    entnum       = 0;           // entity soundsource — mouth sink (S9.4)
    int    timecompress = 0;           // per-word timecompress percent (VOX); 0 for plain channels
    bool   is_sentence  = false;       // has a VOX word list (routes through vox_mix_channel_to_buffer)

    // S9.4: the CURRENT sentence word's pitch percent (voxword_t::pitch),
    // cached here by the IVoxWordAdvance implementation exactly the way
    // `timecompress` above already is — set on bind + on every next_word().
    // k_pitch_norm (100, the default) is VOX_ModifyPitch's no-op value, so a
    // plain (non-sentence) channel leaving this at its default reproduces
    // legacy's `!ch->words` early return with no special-casing at the mix
    // site (see compute_channel_pitch's word_pitch parameter below).
    std::uint16_t vox_pitch = k_pitch_norm;

    // ------------------------------------------------------------------
    // S9.6 additions — channel_t identity + allocation/spatialize fields
    // this slice's channel_alloc.{hpp,cpp} needs (SND_PickDynamicChannel/
    // SND_PickStaticChannel/S_AlterChannel/SND_Spatialize). Appended at the
    // END of the struct (not interleaved with the S9.3/S9.4 fields above) so
    // every existing `MixChannel ch{}; ch.field = ...;` non-designated call
    // site in the S9.3/S9.4 tests keeps compiling unchanged.
    // ------------------------------------------------------------------

    // chan->sfx identity (registry.hpp SfxHandle == abi::sound_t). Comparable
    // for the SND_PickDynamicChannel/PickStaticChannel/S_AlterChannel
    // identity tests (`ch->sfx == sfx`). k_invalid_sound_handle (registry.hpp)
    // when unset — `source == nullptr` remains the authoritative "channel is
    // free" test (matches `!ch->sfx`; see channel_alloc.cpp).
    ::xash::abi::sound_t sfx_handle = -1; // registry.hpp's k_invalid_sound_handle, duplicated here to avoid a mixer.hpp -> registry.hpp include

    std::string name;        // chan->name — sentence name only (empty otherwise; s_main.c:700,808,911)
    Vec3        origin {};   // chan->origin (world position; ignored for entity-relative channels)
    float       dist_mult  = 0.0f; // chan->dist_mult (attn / SND_CLIP_DISTANCE)
    int         master_vol = 0;    // chan->master_vol (0-255, pre-spatialize)
};

// ---------------------------------------------------------------------------
// RawMixChannel — the mix side of a raw/voice streaming channel (S_MixRawChannels,
// s_mix.c:414). Registration/allocation (S_FindRawChannel) is S9.6-adjacent;
// here only the MIX side. `is_voice` and the paintbuffer-vs-roombuffer routing
// are pre-classified on T_Main (CL_IsPlayerIndex etc.) and shipped in — the mix
// never re-reads client state (SND-OQ-1).
// ---------------------------------------------------------------------------
struct RawMixChannel
{
    std::span<const portable_samplepair_t> rawsamples {}; // ring storage (length == max_samples, power of two)
    std::uint32_t s_rawend     = 0;   // producer write cursor (rawchan_t::s_rawend)
    std::size_t   max_samples  = 0;   // ring length (rawchan_t::max_samples) — mask = max_samples-1
    int           leftvol      = 0;   // 0-255
    int           rightvol     = 0;   // 0-255
    int           entnum       = 0;   // entity soundsource
    bool          direct_to_paint = false; // voice / background-track ⇒ paint directly (bypass DSP), s_mix.c:439
};

// ---------------------------------------------------------------------------
// IRoomDsp — the DSP-step injection seam (S9.5 = SX_RoomFX, s_mix.c:563). The
// paint loop calls process() on the roombuffer between the raw-mix step and the
// gain-mix step, and ONLY when not in menu (the menu-gate asymmetry). Null hook
// ⇒ no DSP (as if SX_RoomFX were a no-op) — S9.5 supplies the real reverb/delay.
//
// @thread-safety: invoked on the mix worker (T_AudioDecoder); implementations own
// their own delay-line state confined to that role.
// ---------------------------------------------------------------------------
class IRoomDsp
{
public:
    IRoomDsp() noexcept                       = default;
    virtual ~IRoomDsp()                       = default;
    IRoomDsp( const IRoomDsp & )              = delete;
    IRoomDsp &operator=( const IRoomDsp & )   = delete;

    // Process num_samples pairs of the roombuffer in place (SX_RoomFX shape).
    virtual void process( portable_samplepair_t *roombuffer, int num_samples ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// IVoxWordAdvance — the VOX word-advance seam (S9.4 = VOX_FreeWord/word_index++/
// VOX_LoadWord, s_mix.c:294-302). vox_mix_channel_to_buffer calls next_word()
// whenever the current word finishes: the implementation frees the finished
// word, loads the next, re-points MixChannel::source at it, resets sample/
// forced_end, clears FL_CHAN_FINISHED, and updates timecompress — OR sets
// FL_CHAN_SENTENCE_FINISHED. Returns true while the sentence continues.
//
// @thread-safety: mix worker (T_AudioDecoder) — same confinement as the mixer.
// ---------------------------------------------------------------------------
class IVoxWordAdvance
{
public:
    IVoxWordAdvance() noexcept                            = default;
    virtual ~IVoxWordAdvance()                            = default;
    IVoxWordAdvance( const IVoxWordAdvance & )            = delete;
    IVoxWordAdvance &operator=( const IVoxWordAdvance & ) = delete;

    // Advance to the next sentence word. Returns false once the sentence is done
    // (FL_CHAN_SENTENCE_FINISHED set); true while another word was loaded.
    [[nodiscard]] virtual bool next_word( MixChannel &chan ) noexcept = 0;
};

// ===========================================================================
// Free-function primitives (exposed for direct bit-exact unit pinning).
// ===========================================================================

// VOX_ModifyPitch (s_vox.c:206-221; S9.4) — FLOAT in/out, matching legacy's
// `float VOX_ModifyPitch( channel_t *ch, float pitch )` exactly (see
// compute_channel_pitch below for why the float-ness matters). `word_pitch`
// == k_pitch_norm (100, MixChannel::vox_pitch's default) is the no-op case —
// legacy's own `!ch->words || FL_CHAN_SENTENCE_FINISHED` early return
// collapses to this same identity once a non-sentence channel simply never
// sets vox_pitch away from its default.
[[nodiscard]] constexpr float modify_pitch( float pitch, std::uint16_t word_pitch ) noexcept
{
    if( word_pitch == k_pitch_norm )
        return pitch;
    return pitch + ( static_cast<float>( word_pitch ) - static_cast<float>( k_pitch_norm ) ) * 0.01f;
}

// Per-channel pitch — reproduces the legacy FLOAT rounding chain exactly
// (S9.3 parity-audit fix 2026-07-20): `VOX_ModifyPitch(ch, basePitch*0.01)`
// takes and returns FLOAT (s_vox.c:206), and `pitch_mult` is float
// (s_mix.c:316,391) — so legacy rounds basePitch*0.01 to float, multiplies
// float*float, and only then widens to double for the rate math. Evaluating
// in double throughout diverges by ~1e-8 and de-syncs the resample
// accumulator for every pitched sound (basePitch != 100 or timescale != 1).
//
// `word_pitch` (S9.4, default k_pitch_norm) folds in the VOX sentence pitch
// bend: `pitch = VOX_ModifyPitch(ch, basePitch*0.01) * pitch_mult` (s_mix.c:
// 391) — the bend happens BETWEEN the float-rounded basePitch*0.01 and the
// final pitch_mult multiply, all still in float, per modify_pitch above.
// Every existing call site that omits this argument (non-VOX channels) gets
// the identity and is bit-for-bit unchanged from before this parameter
// existed.
[[nodiscard]] constexpr double compute_channel_pitch( double base_pitch, double pitch_mult,
                                                      std::uint16_t word_pitch = k_pitch_norm ) noexcept
{
    // basePitch (short) promotes to DOUBLE against the 0.01 double literal;
    // the float rounding happens at VOX_ModifyPitch's float PARAMETER, not
    // inside the multiply — so: double multiply, round to float, then
    // VOX_ModifyPitch's bend (float), then the float*float product against
    // the float pitch_mult, widened to double.
    const float base_f = static_cast<float>( base_pitch * 0.01 );
    const float bent_f = modify_pitch( base_f, word_pitch );
    return static_cast<double>( bent_f * static_cast<float>( pitch_mult ) );
}

// CLIP16 (sound.h:40) — bound(SHRT_MIN+8, x, SHRT_MAX-8). The bound() macro uses
// `x < max` (not <=), so the reachable range is exactly [k_clip16_min, k_clip16_max].
[[nodiscard]] constexpr std::int16_t clip16( int x ) noexcept
{
    const int clamped = x >= k_clip16_min ? ( x < k_clip16_max ? x : k_clip16_max ) : k_clip16_min;
    return static_cast<std::int16_t>( clamped );
}

// S_AdjustLoopedSamplePosition (s_main.c:66) — wrap the read cursor back into the
// [loop_start, samples) loop region. Reproduces the exact int/uint mixed
// arithmetic (AudioData.samples/loop_start are uint32, current_sample is int).
[[nodiscard]] int adjust_looped_sample_position( const AudioData &src, int current_sample,
                                                 bool enable_looping ) noexcept;

// S_RetrieveAudioSamples (s_main.c:83) — returns the count of source samples
// available from `start_position` (after loop adjust) and sets `*audio` to the
// byte pointer into src.buffer. `start_position` is the truncated (int) channel
// sample position (legacy passes the double chan->sample into an int parameter).
[[nodiscard]] int retrieve_audio_samples( const AudioData &src, const void **audio,
                                          int start_position, int num_samples,
                                          bool enable_looping ) noexcept;

// S_MixChannelToBuffer (s_mix.c:197) — resample-and-mix one channel's audio into
// `pbuf` starting at `offset`. Ports the request/available/out_count loop-wrap
// edge math verbatim, advancing chan.sample and setting FL_CHAN_FINISHED on
// exhaustion. `lerping` == the s_lerping cvar (lookahead 1 vs 0). Returns the
// number of samples written.
[[nodiscard]] int mix_channel_to_buffer( portable_samplepair_t *pbuf, MixChannel &chan,
                                         int num_samples, int out_rate, double pitch,
                                         int offset, int timecompress, bool lerping ) noexcept;

// VOX_MixChannelToBuffer (s_mix.c:279) — the sentence word-advance loop over the
// per-word mix_channel_to_buffer primitive, using the IVoxWordAdvance seam for
// the actual word swap (S9.4). Returns the number of samples written.
[[nodiscard]] int vox_mix_channel_to_buffer( portable_samplepair_t *pbuf, MixChannel &chan,
                                             int num_samples, int out_rate, double pitch,
                                             bool lerping, IVoxWordAdvance &advance ) noexcept;

// ===========================================================================
// Mixer — owns the worker-local paint scratch, the mix-side channel arrays, the
// mix-clock reconstruction state, and drives S_PaintChannels.
// ===========================================================================
class Mixer
{
public:
    Mixer();

    // --- configuration (set on T_Main before the worker starts) --------------
    void set_out_rate( int out_rate ) noexcept { out_rate_ = out_rate; }
    void set_lerping( bool on ) noexcept { lerping_ = on; }         // s_lerping cvar
    void set_room_dsp( IRoomDsp *dsp ) noexcept { room_dsp_ = dsp; }// S9.5 hook (nullable)
    void set_vox_advance( IVoxWordAdvance *v ) noexcept { vox_ = v; }// S9.4 hook (nullable)

    [[nodiscard]] int  out_rate() const noexcept { return out_rate_; }
    [[nodiscard]] int  painted_time() const noexcept { return painted_time_; }
    void set_painted_time( int t ) noexcept { painted_time_ = t; }

    // Mix-side channel arrays (boundary: channels_ 320 + raw_channels_ 48 "made
    // real"). Reserved to the ABI-frozen limits at construction. Populated by
    // channel allocation (S9.6); iterated here on the MIX side.
    [[nodiscard]] std::vector<MixChannel>    &channels() noexcept { return channels_; }
    [[nodiscard]] std::vector<RawMixChannel> &raw_channels() noexcept { return raw_channels_; }

    // --- S_GetSoundtime (s_main.c:1503) mix-clock reconstruction -------------
    struct SoundtimeResult
    {
        int  soundtime      = 0;     // reconstructed monotone clock
        bool overflow_reset = false; // the 0x40000000 wrap fired ⇒ caller must S_StopAllSounds(true) (S9.6)
    };
    // Reconstruct soundtime from the device write cursor. Maintains the wrap
    // counter + the 0x40000000 overflow reset (which also resets painted_time_).
    [[nodiscard]] SoundtimeResult reconstruct_soundtime( int samplepos, int dma_samples ) noexcept;

    // --- S_PaintChannels (s_mix.c:542) ---------------------------------------
    // Paint [painted_time(), endtime) in <=PAINTBUFFER_SIZE blocks and return the
    // interleaved-stereo int16 output for the whole range (CLIP16-narrowed).
    // `master_volume` is S_GetMasterVolume()'s result, computed on T_Main by
    // master_volume_from() (audio_command.hpp) — the `volume` cvar and the
    // focus mute; the SOUNDFADE term of that function is still a documented
    // XASH3DPP-STUB(chunk12). gain = master_volume * 256. `pitch_mult` is the
    // FWGS chipmunk multiplier (sys_timescale), not wired yet. Gates come ONLY
    // from `gate`, which likewise has no producer yet.
    [[nodiscard]] std::span<const std::int16_t> paint_channels( int endtime, const MixGateSnapshot &gate,
                                                                float master_volume, double pitch_mult );

private:
    // S_MixNormalChannelsToRoombuffer (s_mix.c:308) — returns # channels mixed.
    [[nodiscard]] int mix_normal_channels_to_roombuffer( int end, const MixGateSnapshot &gate,
                                                         double pitch_mult ) noexcept;
    // S_MixRawChannels (s_mix.c:414) — returns # non-voice (room) raw channels.
    [[nodiscard]] int mix_raw_channels( int end, const MixGateSnapshot &gate ) noexcept;
    // S_ClearBuffers (s_mix.c:534) — zero (num_samples+1) pairs of both scratch buffers.
    void clear_buffers( int num_samples ) noexcept;
    // S_MixBufferWithGain (s_mix.c:474) — roombuffer -> paintbuffer with gain.
    void mix_buffer_with_gain( int num_samples, int gain ) noexcept;
    // S_TransferPaintBuffer (s_mix.c:503) — CLIP16-narrow num_samples pairs into
    // `out` (interleaved int16) starting at pair index `out_pair_offset`.
    void transfer_paint_buffer( int num_samples, std::size_t out_pair_offset ) noexcept;

    int  out_rate_     = 44100; // snd.format.speed (sound_dma_speed default)
    bool lerping_      = false; // s_lerping cvar (default "0")
    int  painted_time_ = 0;     // snd.paintedtime (mix-thread-private)

    // S_GetSoundtime function-local statics (s_main.c:1505) -> mixer members.
    int  wrap_buffers_    = 0;  // `buffers`
    int  old_samplepos_   = 0;  // `oldsamplepos`

    IRoomDsp        *room_dsp_ = nullptr; // S9.5 DSP hook (nullable)
    IVoxWordAdvance *vox_      = nullptr; // S9.4 VOX hook (nullable)

    // Worker-local scratch (boundary: never file-scope shared). Sized
    // PAINTBUFFER_SIZE+1 (the +1 lerp-lookahead slot cleared by clear_buffers).
    std::vector<portable_samplepair_t> paintbuffer_;   // @pre-reserved: sound_paintbuffer_size+1 (resize in ctor; fixed for life)
    std::vector<portable_samplepair_t> roombuffer_;    // @pre-reserved: sound_paintbuffer_size+1 (resize in ctor; fixed for life)

    std::vector<MixChannel>    channels_;     // @pre-reserved: sound_max_channels (reserve in ctor; ABI-frozen 320, populated by S9.6 allocation)
    std::vector<RawMixChannel> raw_channels_; // @pre-reserved: sound_max_raw_channels (reserve in ctor; ABI-frozen 48, populated by S9.6 allocation)

    std::vector<std::int16_t> transfer_out_;  // @pre-reserved: per-paint result set — assign(2*range) each paint_channels call (hot but bounded by endtime-paintedtime; grows to the largest range seen, no pre-sizing)
};

} // namespace xash::sound
