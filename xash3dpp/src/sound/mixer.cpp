// xash3dpp — the paint pipeline core (Chunk 9, slice S9.3). PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_mix.c + s_main.c (see mixer.hpp for the
// per-function line map). Every arithmetic step is a verbatim port of the legacy
// integer math — the 12 mix kernels (mix_kernels.hpp), the resample loop-wrap
// edge math, CLIP16, the 7-step paint order + menu-gate asymmetry, and the
// 0x40000000 mix-clock reset. Legacy C engine is REFERENCE-ONLY.
//
// @thread-safety: the Mixer is confined to the T_AudioDecoder mix worker (amended
// threading-model §3.4 — mixer = T_AudioDecoder-owned). As of S9.7b that role is
// ENFORCED, not merely annotated: paint_channels() asserts ThreadRole::AudioDecoder
// (the S9.3 compliance-allow(thread-assert) exemption is retired — a real decoder
// thread now exists, so the "asserting would fire on T_Main" rationale is gone).
//
// Consequence for callers: ANY thread that drives the paint must have registered
// ThreadRole::AudioDecoder. That includes a synchronous/main-pumped paint (the
// boundary's pfnS_PaintChannels fallback, §External ABI) — such a driver must run
// on a thread registered as AudioDecoder rather than on T_Main proper. The mix
// kernels and the free-function primitives below stay assert-free: they are the
// hot path, and the role is established once at the paint entry.

#include <xash3dpp/private/sound/mixer.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace xash::sound {

namespace {

// bound(min, num, max) verbatim (public/xash3d_mathlib.h:141):
//   num >= min ? (num < max ? num : max) : min   — note `<` (not `<=`).
[[nodiscard]] constexpr int bound_int( int lo, int v, int hi ) noexcept
{
    return v >= lo ? ( v < hi ? v : hi ) : lo;
}

// S_AdjustNumSamples (s_mix.c:175) — forced_end save/restore truncation. Marks
// FL_CHAN_FINISHED and returns the truncated count when the channel is scheduled
// to end at a specific sample this pass.
[[nodiscard]] int adjust_num_samples( MixChannel &chan, int num_samples, double rate,
                                      double timecompress_rate ) noexcept
{
    if( chan.flags & abi::k_fl_chan_finished )
        return 0;

    if( chan.forced_end != 0.0 ) // legacy: if( chan->forced_end )
    {
        const double end_sample = chan.sample + rate * num_samples * timecompress_rate;
        if( end_sample >= chan.forced_end )
        {
            chan.flags |= abi::k_fl_chan_finished;
            return static_cast<int>( std::floor( ( chan.forced_end - chan.sample ) / ( rate * timecompress_rate ) ) );
        }
    }

    return num_samples;
}

} // namespace

// ---------------------------------------------------------------------------
// S_AdjustLoopedSamplePosition (s_main.c:66) — reproduces the exact mixed int/
// uint arithmetic (AudioData.samples/loop_start are uint32; current_sample int).
// ---------------------------------------------------------------------------
int adjust_looped_sample_position( const AudioData &src, int current_sample, bool enable_looping ) noexcept
{
    if( enable_looping && has_flag( src.flags, AudioFlags::Looped )
        && static_cast<std::uint32_t>( current_sample ) >= src.samples )
    {
        // current_sample -= source->loop_start  (int - uint -> uint -> int)
        current_sample = static_cast<int>( static_cast<std::uint32_t>( current_sample ) - src.loop_start );

        // int loop_range = source->samples - source->loop_start
        const int loop_range = static_cast<int>( src.samples - src.loop_start );
        if( loop_range > 0 )
        {
            // current_sample = source->loop_start + (current_sample % loop_range)
            current_sample = static_cast<int>( src.loop_start
                                               + static_cast<std::uint32_t>( current_sample % loop_range ) );
        }
    }

    return current_sample;
}

// ---------------------------------------------------------------------------
// S_RetrieveAudioSamples (s_main.c:83) — available-count + byte pointer into the
// source buffer. `start_position` is the truncated int channel sample position.
// ---------------------------------------------------------------------------
int retrieve_audio_samples( const AudioData &src, const void **audio, int start_position,
                            int num_samples, bool enable_looping ) noexcept
{
    start_position = adjust_looped_sample_position( src, start_position, enable_looping );

    // Q_max( 0, source->samples - start_position ): the subtract is unsigned
    // (samples is uint32), so Q_max(0,·) always yields the unsigned value —
    // stored to int, an underflow past the end lands negative and trips the
    // num_samples<=0 guard below, exactly like legacy.
    const std::uint32_t avail_u    = src.samples - static_cast<std::uint32_t>( start_position );
    int available_samples          = static_cast<int>( avail_u );

    if( num_samples > available_samples )
        num_samples = available_samples;

    if( num_samples <= 0 )
        return 0;

    const int frame_size = std::max( 1, static_cast<int>( src.width ) * static_cast<int>( src.channels ) );
    start_position *= frame_size; // sample position -> byte offset

    *audio = src.buffer.data() + start_position;
    return num_samples;
}

// ---------------------------------------------------------------------------
// S_MixChannelToBuffer (s_mix.c:197) — the per-channel resample driver.
// ---------------------------------------------------------------------------
int mix_channel_to_buffer( portable_samplepair_t *pbuf, MixChannel &chan, int num_samples,
                           int out_rate, double pitch, int offset, int timecompress, bool lerping ) noexcept
{
    const int initial_offset = offset;
    const int pvol[2]        = {
        bound_int( 0, chan.leftvol, 255 ),
        bound_int( 0, chan.rightvol, 255 ),
    };
    const AudioData &src = *chan.source;
    const double rate    = pitch * src.rate / static_cast<double>( out_rate );

    // timecompress at 100% is skipping the entire sfx, so mark finished and exit.
    if( timecompress >= 100 )
    {
        chan.flags |= abi::k_fl_chan_finished;
        return 0;
    }

    const double timecompress_rate = 1 / ( 1 - timecompress / 100.0 );

    num_samples = adjust_num_samples( chan, num_samples, rate, timecompress_rate );
    if( num_samples == 0 )
        return 0;

    // linear interpolation needs one sample of lookahead beyond the last read.
    const int  lookahead = lerping ? 1 : 0;
    const bool use_loop  = ( chan.flags & abi::k_fl_chan_use_loop ) != 0;

    while( num_samples > 0 )
    {
        const double end_sample = chan.sample + rate * num_samples * timecompress_rate;

        // total samples wanted, including lookahead for interpolation.
        const int request_num_samples =
            static_cast<int>( std::ceil( end_sample ) - std::floor( chan.sample ) ) + lookahead;

        const void *audio = nullptr;
        const int   available =
            retrieve_audio_samples( src, &audio, static_cast<int>( chan.sample ), request_num_samples, use_loop );

        if( !available )
            break;

        const double sample_frac = chan.sample - std::floor( chan.sample );

        // can interpolate only when at least two source samples are available.
        bool lerp = lookahead && available >= 2;

        int out_count = num_samples;
        if( request_num_samples > available )
        {
            if( lerp )
                out_count = static_cast<int>( std::floor( ( available - 1 - sample_frac ) / rate ) );
            else
                out_count = static_cast<int>( std::ceil( ( available - sample_frac ) / rate ) );
        }

        // near a buffer boundary (e.g. just before a loop wrap) lerp may yield
        // zero; fall back to nearest for one sample to keep chan.sample advancing.
        if( out_count <= 0 )
        {
            lerp      = false;
            out_count = 1;
        }

        mix_audio( pbuf + offset, pvol, audio, src.channels, src.width, sample_frac, rate, out_count, lerp );

        chan.sample += out_count * rate * timecompress_rate;
        offset += out_count;
        num_samples -= out_count;
    }

    // samples couldn't be retrieved, mark finished.
    if( num_samples > 0 )
        chan.flags |= abi::k_fl_chan_finished;

    return offset - initial_offset;
}

// ---------------------------------------------------------------------------
// VOX_MixChannelToBuffer (s_mix.c:279) — sentence word-advance loop. The VOX
// mixing itself is S9.4; this is the ENTRY shape over the per-word primitive,
// with the actual word swap injected through IVoxWordAdvance.
// ---------------------------------------------------------------------------
int vox_mix_channel_to_buffer( portable_samplepair_t *pbuf, MixChannel &chan, int num_samples,
                               int out_rate, double pitch, bool lerping, IVoxWordAdvance &advance ) noexcept
{
    int offset = 0;

    if( chan.flags & abi::k_fl_chan_sentence_finished )
        return 0;

    while( num_samples > 0 && !( chan.flags & abi::k_fl_chan_sentence_finished ) )
    {
        const int output_count =
            mix_channel_to_buffer( pbuf, chan, num_samples, out_rate, pitch, offset, chan.timecompress, lerping );

        offset += output_count;
        num_samples -= output_count;

        // if we finished the current word, load the next (VOX_FreeWord +
        // word_index++ + VOX_LoadWord, s_mix.c:294-302) via the S9.4 seam. The
        // hook rebinds chan.source, resets sample/forced_end/timecompress and
        // clears FL_CHAN_FINISHED, or sets FL_CHAN_SENTENCE_FINISHED.
        if( chan.flags & abi::k_fl_chan_finished )
            (void)advance.next_word( chan );
    }

    return offset;
}

// ===========================================================================
// Mixer
// ===========================================================================

Mixer::Mixer()
{
    // Worker-local scratch: PAINTBUFFER_SIZE + 1 pairs (the +1 lerp-lookahead slot).
    paintbuffer_.resize( ::xash::limits::sound_paintbuffer_size + 1 );
    roombuffer_.resize( ::xash::limits::sound_paintbuffer_size + 1 );

    // channels_ (320) + raw_channels_ (48) "made real" per the owned-state
    // disposition — reserved to the ABI-frozen limits; populated by channel
    // allocation (S9.6).
    channels_.reserve( ::xash::limits::sound_max_channels );
    raw_channels_.reserve( ::xash::limits::sound_max_raw_channels );
}

void Mixer::clear_buffers( int num_samples ) noexcept
{
    // Zero (num_samples+1) pairs of both scratch buffers (s_mix.c:534).
    const std::size_t count = static_cast<std::size_t>( num_samples ) + 1;
    std::fill_n( paintbuffer_.begin(), count, portable_samplepair_t{} );
    std::fill_n( roombuffer_.begin(), count, portable_samplepair_t{} );
}

void Mixer::mix_buffer_with_gain( int num_samples, int gain ) noexcept
{
    portable_samplepair_t       *dst = paintbuffer_.data();
    const portable_samplepair_t *src = roombuffer_.data();

    if( gain == k_gain_unity )
    {
        for( int i = 0; i < num_samples; i++ )
        {
            dst[i].left += src[i].left;
            dst[i].right += src[i].right;
        }
    }
    else
    {
        for( int i = 0; i < num_samples; i++ )
        {
            dst[i].left += ( src[i].left * gain ) >> 8;
            dst[i].right += ( src[i].right * gain ) >> 8;
        }
    }
}

void Mixer::transfer_paint_buffer( int num_samples, std::size_t out_pair_offset ) noexcept
{
    // CLIP16-narrow the int32 accumulator into interleaved-stereo int16
    // (s_mix.c:494-501). Legacy reinterprets the pair array as a flat int stream;
    // since portable_samplepair_t is { int left; int right } contiguous, writing
    // out[2i+0]=clip16(left), out[2i+1]=clip16(right) is bit-identical. The
    // legacy DMA ring-wrap belongs to the SPSC ring (SND-OQ-5, S9.7) — here the
    // output is linear for the newly-painted block.
    const portable_samplepair_t *src = paintbuffer_.data();
    std::int16_t                *out = transfer_out_.data() + out_pair_offset * 2;

    for( int i = 0; i < num_samples; i++ )
    {
        out[i * 2 + 0] = clip16( src[i].left );
        out[i * 2 + 1] = clip16( src[i].right );
    }
}

Mixer::SoundtimeResult Mixer::reconstruct_soundtime( int samplepos, int dma_samples ) noexcept
{
    SoundtimeResult result;
    const int       fullsamples = dma_samples / 2;

    // it is possible to miscount buffers if it has wrapped twice between calls.
    if( samplepos < old_samplepos_ )
    {
        wrap_buffers_++; // buffer wrapped

        if( painted_time_ > k_soundtime_wrap_reset )
        {
            // time to chop things off to avoid 32-bit limits.
            wrap_buffers_        = 0;
            painted_time_        = fullsamples;
            result.overflow_reset = true; // caller performs S_StopAllSounds(true) — S9.6
        }
    }

    old_samplepos_  = samplepos;
    result.soundtime = wrap_buffers_ * fullsamples + samplepos / 2;
    return result;
}

int Mixer::mix_normal_channels_to_roombuffer( int end, const MixGateSnapshot &gate, double pitch_mult ) noexcept
{
    const bool sp        = gate.single_player;
    const bool ingame    = gate.in_game;
    const int  out_rate  = out_rate_;
    const int  num_samples = end - painted_time_;

    int num_mixed_channels = 0;

    if( num_samples <= 0 )
        return num_mixed_channels;

    if( gate.background && gate.in_console )
        return num_mixed_channels; // no sounds in console with background map

    portable_samplepair_t *dst = roombuffer_.data();

    for( MixChannel &ch : channels_ )
    {
        if( ch.source == nullptr )
            continue;

        if( !gate.background )
        {
            if( gate.in_console && ( ch.flags & abi::k_fl_chan_local_sound ) )
            {
                // play, playvol (console + local sound plays even when gated)
            }
            else if( ( gate.in_menu || gate.paused ) && !( ch.flags & abi::k_fl_chan_local_sound ) && sp )
            {
                continue; // play only local sounds, keep pause for other
            }
            else if( !gate.in_menu && !ingame && !( ch.flags & abi::k_fl_chan_static_sound ) )
            {
                continue; // play only ambient sounds, keep pause for other
            }
        }

        // (S9.x) S_LoadSound load-on-mix + S_FreeChannel are channel-lifecycle
        // (S9.6); here ch.source is already bound by allocation.

        // if the sound is inaudible, skip it (the inaudible free-timer +
        // S_FreeChannel is S9.6, s_mix.c:361-374).
        if( ch.leftvol < k_channel_inaudible_vol && ch.rightvol < k_channel_inaudible_vol )
            continue;

        // (S9.4) mouth for CHAN_VOICE/STREAM (SND_MoveMouth -> IMouthSink) omitted.

        // pitch = VOX_ModifyPitch(ch, basePitch*0.01) * pitch_mult (s_mix.c:391).
        // ch.vox_pitch (S9.4) is the current sentence word's pitch percent,
        // cached by IVoxWordAdvance's implementation exactly like timecompress
        // already is; it defaults to k_pitch_norm (100) so non-sentence
        // channels get VOX_ModifyPitch's identity with no special-casing here.
        const double pitch = compute_channel_pitch( ch.base_pitch, pitch_mult, ch.vox_pitch );

        num_mixed_channels++;

        if( ch.is_sentence && vox_ != nullptr )
        {
            (void)vox_mix_channel_to_buffer( dst, ch, num_samples, out_rate, pitch, lerping_, *vox_ );
            // free on FL_CHAN_SENTENCE_FINISHED is S9.6.
        }
        else
        {
            (void)mix_channel_to_buffer( dst, ch, num_samples, out_rate, pitch, 0, 0, lerping_ );
            // free on FL_CHAN_FINISHED is S9.6.
        }
    }

    return num_mixed_channels;
}

int Mixer::mix_raw_channels( int end, const MixGateSnapshot &gate ) noexcept
{
    int num_room_channels = 0;

    if( gate.paused )
        return 0;

    for( RawMixChannel &ch : raw_channels_ )
    {
        if( ch.rawsamples.empty() ) // unallocated slot
            continue;

        if( !ch.leftvol && !ch.rightvol ) // not audible
            continue;

        // voice + background-track paint DIRECTLY into paintbuffer (bypass DSP);
        // others into roombuffer and count as room channels (s_mix.c:439-449).
        portable_samplepair_t *pbuf;
        if( ch.direct_to_paint )
        {
            pbuf = paintbuffer_.data();
        }
        else
        {
            pbuf = roombuffer_.data();
            num_room_channels++;
        }

        const std::uint32_t stop =
            ( static_cast<std::uint32_t>( end ) < ch.s_rawend ) ? static_cast<std::uint32_t>( end ) : ch.s_rawend;
        const std::uint32_t mask = static_cast<std::uint32_t>( ch.max_samples ) - 1;

        std::size_t i = 0;
        for( std::uint32_t j = static_cast<std::uint32_t>( painted_time_ ); j < stop; i++, j++ )
        {
            pbuf[i].left += ( ch.rawsamples[j & mask].left * ch.leftvol ) >> 8;
            pbuf[i].right += ( ch.rawsamples[j & mask].right * ch.rightvol ) >> 8;
        }

        // (S9.4) raw mouth sync (SND_MoveMouthRaw -> IMouthSink) omitted.
    }

    return num_room_channels;
}

std::span<const std::int16_t> Mixer::paint_channels( int endtime, const MixGateSnapshot &gate,
                                                     float master_volume, double pitch_mult )
{
    // S9.7b: the paint pipeline is T_AudioDecoder-owned (threading-model §3.4).
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::AudioDecoder );

    const int start = painted_time_;
    const int total = endtime - start; // full range painted this call

    transfer_out_.assign( total > 0 ? static_cast<std::size_t>( total ) * 2 : 0, std::int16_t{ 0 } );

    // gain = S_GetMasterVolume() * 256 (float -> int truncation, s_mix.c:544).
    const int gain = static_cast<int>( master_volume * 256 );

    while( painted_time_ < endtime )
    {
        // if paintbuffer is smaller than the remaining range, cap the block.
        int end = endtime;
        if( end - painted_time_ > static_cast<int>( ::xash::limits::sound_paintbuffer_size ) )
            end = painted_time_ + static_cast<int>( ::xash::limits::sound_paintbuffer_size );

        const int num_samples = end - painted_time_;

        clear_buffers( num_samples );

        int room_channels = mix_normal_channels_to_roombuffer( end, gate, pitch_mult );
        room_channels += mix_raw_channels( end, gate );

        // DSP step (SX_RoomFX): skipped in menu; a null hook is a no-op (S9.5).
        if( !gate.in_menu && room_dsp_ != nullptr )
            room_dsp_->process( roombuffer_.data(), num_samples );

        // gain-mix room -> paint: skipped in menu iff no room channels were mixed.
        if( room_channels > 0 || !gate.in_menu )
            mix_buffer_with_gain( num_samples, gain );

        // transfer out (CLIP16 -> int16) at this block's offset in the result.
        const std::size_t out_pair_offset = static_cast<std::size_t>( painted_time_ - start );
        transfer_paint_buffer( num_samples, out_pair_offset );

        painted_time_ = end;
    }

    return std::span<const std::int16_t>{ transfer_out_.data(), transfer_out_.size() };
}

} // namespace xash::sound
