#pragma once
// xash3dpp — the 12 mix kernels as one template family (Chunk 9, slice S9.3).
// PARITY-CRITICAL. Legacy reference: engine/client/sound/s_mix.c:22-173 — the
// S_MakeMix{Mono,Stereo}[Pitch|Lerp]{8,16} macro family (12 variants) + the
// S_MixAudio nested-switch dispatch. This header replaces the 6 code-gen macros
// (each expanded ×2 for 8/16-bit) with ONE `if constexpr` template selected by a
// constexpr function-pointer table indexed (width, channels, interp) — the plan's
// affirmed shape (NO per-kernel virtual interface).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Quirks "Kernel arithmetic
// law" + "Compat scope (Q-12) — Frozen wire-exact: the 12 mix-kernel variants'
// integer arithmetic". Every intermediate width/shift/truncation matches the
// macro expansion by hand (see per-variant derivation comments below).
//
// THE VOLUME LAW (all 12 variants share it, s_mix.c:22-31,107-118):
//     pbuf[i].{left,right} += ( sample * volume[{0,1}] ) >> ( x - 8 )
//   where x ∈ {8,16} is the SOURCE bit width. The shift is (x-8): 0 for 8-bit,
//   8 for 16-bit. There is NO scale table — the volume is a direct integer
//   multiply by the [0,255]-clamped per-side volume (see mixer.cpp
//   mix_channel_to_buffer, s_mix.c:200-205). This is a straight port of GoldSrc's
//   scale-table-free FWGS mixer, confirmed by reading s_mix.c (no snd_scaletable
//   anywhere in the tree).
//
// 8-BIT IS SIGNED: soundlib (S9.2, codec_wav.cpp:371-386, "now convert 8-bit
// sounds to signed") stores WAV 8-bit PCM as SIGNED bytes (raw - 128). So an
// 8-bit kernel reads `const std::int8_t*`, matching legacy's `const int8_t*data`
// cast for x=8 (s_mix.c:25). 16-bit reads `const std::int16_t*`.
//
// @thread-safety: pure functions over caller-owned buffers — no shared state, no
// affinity. The kernels are the innermost leaf of the T_AudioDecoder mix worker
// (mixer.hpp) but carry no role assertion themselves (leaf arithmetic, asserted
// once at the paint-loop entry — see mixer.cpp).

#include <xash3dpp/abi/sound_api.hpp> // portable_samplepair_t

#include <cstdint>
#include <type_traits>

namespace xash::sound {

using ::xash::abi::portable_samplepair_t;

// Resample/interpolation mode — the third dispatch axis (s_mix.c:120-173).
//   Flat  — rate==1 && frac==0: no fractional index (S_MixMono/Stereo{8,16}).
//   Pitch — nearest-neighbour resample via the fraction accumulator
//           (S_MixMono/StereoPitch{8,16}).
//   Lerp  — linear interp with a 1-sample lookahead (S_MixMono/StereoLerp{8,16}).
enum class Interp
{
    Flat  = 0,
    Pitch = 1,
    Lerp  = 2,
};

// ---------------------------------------------------------------------------
// mix_kernel<WidthBits, Channels, I> — the one template that expands to all 12
// legacy variants. Selected by the k_mix_kernels table below. Uniform signature
// (offset_frac/rate_scale are ignored by the Flat instantiations, exactly as the
// legacy Flat kernels take no such args) so a single fn-pointer type fits every
// cell of the dispatch table.
//
// Bit-exact derivation, matched against the macro expansion for each variant:
//
//   WidthBits=8:  sample_t = int8_t,  shift = 0   (>> (8  - 8))
//   WidthBits=16: sample_t = int16_t, shift = 8   (>> (16 - 8))
//   data[k] (int8_t/int16_t) integer-promotes to int before the * volume[side]
//     multiply — identical to the macro's `data[...] * volume[...]`.
//   Pitch accumulator (s_mix.c:53-55): offset_frac += rate_scale (double);
//     sample_idx += (uint)offset_frac; offset_frac -= (uint)offset_frac. The
//     (uint) cast truncates toward zero; sample_idx is a 32-bit unsigned index.
//   Stereo stride (s_mix.c:69): the integer advance is `(uint)offset_frac << 1`
//     and sample_idx indexes interleaved samples (…+0 = left, +1 = right).
//   Lerp (s_mix.c:81): int s = (int)( data[i]*(1.0-frac) + data[i+1]*frac ) —
//     the multiply/add are done in `double` (int8/16 promotes to int then to
//     double via the 1.0 literal), then truncated toward zero by the (int) cast.
//   Stereo lerp lookahead (s_mix.c:97-98): one stereo frame ahead — +2 (left),
//     +3 (right).
// ---------------------------------------------------------------------------
template <int WidthBits, int Channels, Interp I>
void mix_kernel( portable_samplepair_t *pbuf, const int volume[2], const void *buf,
                 [[maybe_unused]] double offset_frac, [[maybe_unused]] double rate_scale,
                 int num_samples ) noexcept
{
    static_assert( WidthBits == 8 || WidthBits == 16, "source width is 8 or 16 bits" );
    static_assert( Channels == 1 || Channels == 2, "mono or stereo" );

    using sample_t       = std::conditional_t<WidthBits == 8, std::int8_t, std::int16_t>;
    constexpr int shift  = WidthBits - 8;
    const sample_t *data = static_cast<const sample_t *>( buf );

    if constexpr ( I == Interp::Flat )
    {
        // S_MixMono{8,16} / S_MixStereo{8,16} (s_mix.c:22-42): no fractional index.
        for( int i = 0; i < num_samples; i++ )
        {
            if constexpr ( Channels == 1 )
            {
                pbuf[i].left  += ( data[i] * volume[0] ) >> shift;
                pbuf[i].right += ( data[i] * volume[1] ) >> shift;
            }
            else
            {
                pbuf[i].left  += ( data[i * 2 + 0] * volume[0] ) >> shift;
                pbuf[i].right += ( data[i * 2 + 1] * volume[1] ) >> shift;
            }
        }
    }
    else if constexpr ( I == Interp::Pitch )
    {
        // S_MixMonoPitch{8,16} / S_MixStereoPitch{8,16} (s_mix.c:44-72):
        // nearest-neighbour resample via the fraction accumulator.
        std::uint32_t sample_idx = 0;
        for( int i = 0; i < num_samples; i++ )
        {
            if constexpr ( Channels == 1 )
            {
                pbuf[i].left  += ( data[sample_idx] * volume[0] ) >> shift;
                pbuf[i].right += ( data[sample_idx] * volume[1] ) >> shift;
                offset_frac += rate_scale;
                sample_idx += static_cast<std::uint32_t>( offset_frac );
                offset_frac -= static_cast<std::uint32_t>( offset_frac );
            }
            else
            {
                pbuf[i].left  += ( data[sample_idx + 0] * volume[0] ) >> shift;
                pbuf[i].right += ( data[sample_idx + 1] * volume[1] ) >> shift;
                offset_frac += rate_scale;
                sample_idx += static_cast<std::uint32_t>( offset_frac ) << 1;
                offset_frac -= static_cast<std::uint32_t>( offset_frac );
            }
        }
    }
    else // Interp::Lerp
    {
        // S_MixMonoLerp{8,16} / S_MixStereoLerp{8,16} (s_mix.c:74-105): linear
        // interpolation with a 1-sample (mono) / 1-frame (stereo) lookahead.
        std::uint32_t sample_idx = 0;
        for( int i = 0; i < num_samples; i++ )
        {
            if constexpr ( Channels == 1 )
            {
                const int s = static_cast<int>( data[sample_idx] * ( 1.0 - offset_frac )
                                                + data[sample_idx + 1] * offset_frac );
                pbuf[i].left  += ( s * volume[0] ) >> shift;
                pbuf[i].right += ( s * volume[1] ) >> shift;
                offset_frac += rate_scale;
                sample_idx += static_cast<std::uint32_t>( offset_frac );
                offset_frac -= static_cast<std::uint32_t>( offset_frac );
            }
            else
            {
                const int sl = static_cast<int>( data[sample_idx + 0] * ( 1.0 - offset_frac )
                                                 + data[sample_idx + 2] * offset_frac );
                const int sr = static_cast<int>( data[sample_idx + 1] * ( 1.0 - offset_frac )
                                                 + data[sample_idx + 3] * offset_frac );
                pbuf[i].left  += ( sl * volume[0] ) >> shift;
                pbuf[i].right += ( sr * volume[1] ) >> shift;
                offset_frac += rate_scale;
                sample_idx += static_cast<std::uint32_t>( offset_frac ) << 1;
                offset_frac -= static_cast<std::uint32_t>( offset_frac );
            }
        }
    }
}

// Uniform fn-pointer type for every dispatch-table cell.
using MixKernelFn = void ( * )( portable_samplepair_t *, const int[2], const void *,
                                double, double, int ) noexcept;

// ---------------------------------------------------------------------------
// k_mix_kernels — the constexpr dispatch table replacing S_MixAudio's nested
// switch (s_mix.c:120-173). Indexed [width_idx][chan_idx][interp_idx]:
//   width_idx : 0 = 8-bit (1 byte/sample), 1 = 16-bit (2 bytes/sample)
//   chan_idx  : 0 = mono, 1 = stereo
//   interp_idx: static_cast<int>(Interp) — 0 Flat, 1 Pitch, 2 Lerp
// All 12 leaves are named here — the whole S_MakeMix* family in one place.
// ---------------------------------------------------------------------------
inline constexpr MixKernelFn k_mix_kernels[2][2][3] = {
    // --- 8-bit source ---
    { { &mix_kernel<8, 1, Interp::Flat>, &mix_kernel<8, 1, Interp::Pitch>, &mix_kernel<8, 1, Interp::Lerp> },
      { &mix_kernel<8, 2, Interp::Flat>, &mix_kernel<8, 2, Interp::Pitch>, &mix_kernel<8, 2, Interp::Lerp> } },
    // --- 16-bit source ---
    { { &mix_kernel<16, 1, Interp::Flat>, &mix_kernel<16, 1, Interp::Pitch>, &mix_kernel<16, 1, Interp::Lerp> },
      { &mix_kernel<16, 2, Interp::Flat>, &mix_kernel<16, 2, Interp::Pitch>, &mix_kernel<16, 2, Interp::Lerp> } },
};

// select_kernel — the (width_bytes, channels, interp) → kernel lookup, matching
// S_MixAudio's `width == 1 ? 8-bit : 16-bit` / `channels == 1 ? mono : stereo`
// branch keys (s_mix.c:126-171). width_bytes is the AudioData width field (1 or
// 2 BYTES); anything != 1 selects 16-bit, anything != mono selects stereo — the
// exact legacy else-branch semantics.
[[nodiscard]] constexpr MixKernelFn select_kernel( int width_bytes, int channels, Interp interp ) noexcept
{
    const int width_idx  = ( width_bytes == 1 ) ? 0 : 1;
    const int chan_idx   = ( channels == 1 ) ? 0 : 1;
    const int interp_idx = static_cast<int>( interp );
    return k_mix_kernels[width_idx][chan_idx][interp_idx];
}

// q_equal — verbatim port of public/xash3d_mathlib.h:87-88
//   Q_equal_e(a,b,e) = ((a) >= ((b)-(e))) && ((a) <= ((b)+(e)))
//   Q_equal(a,b)     = Q_equal_e(a,b,EQUAL_EPSILON), EQUAL_EPSILON = 0.001f
// The epsilon is a FLOAT literal (0.001f) — kept as `float` so `b - epsilon`
// promotes exactly as legacy (double minus float→double). Drives the Flat-vs-
// resample dispatch decision, so its epsilon must match bit-for-bit.
[[nodiscard]] constexpr bool q_equal( double a, double b ) noexcept
{
    constexpr float equal_epsilon = 0.001f;
    return a >= ( b - equal_epsilon ) && a <= ( b + equal_epsilon );
}

// ---------------------------------------------------------------------------
// mix_audio — the S_MixAudio dispatch (s_mix.c:120-173). Chooses Flat when the
// rate is unity AND the start fraction is ~0 (q_equal guard band), else Lerp if
// interpolation is requested, else nearest-neighbour Pitch — the exact legacy
// order — then indexes the table by (width, channels).
// ---------------------------------------------------------------------------
inline void mix_audio( portable_samplepair_t *pbuf, const int pvol[2], const void *buf,
                       int channels, int width_bytes, double offset_frac, double rate_scale,
                       int num_samples, bool lerp ) noexcept
{
    Interp interp;
    if( q_equal( rate_scale, 1.0 ) && q_equal( offset_frac, 0.0 ) )
        interp = Interp::Flat;
    else if( lerp )
        interp = Interp::Lerp;
    else
        interp = Interp::Pitch;

    select_kernel( width_bytes, channels, interp )( pbuf, pvol, buf, offset_frac, rate_scale, num_samples );
}

} // namespace xash::sound
