// xash3dpp — mixer kernels + paint pipeline parity tests (Chunk 9, slice S9.3).
// PARITY-CRITICAL. Every expected value below is HAND-DERIVED from the legacy
// macro expansion (engine/client/sound/s_mix.c) — never captured from this
// implementation. The volume law shared by all 12 kernels (s_mix.c:22-31):
//
//     pbuf[i].{left,right} += ( sample * volume[{0,1}] ) >> ( x - 8 )
//
//   x = source bit width: shift 0 for 8-bit, shift 8 for 16-bit. 8-bit source
//   data is SIGNED (int8_t). The right shift is arithmetic (C++23 guarantees it
//   for signed) — negative products floor toward -inf.
//
// Coverage: 12 per-kernel bit-exact vectors (one per variant), the dispatch-table
// selection matrix + q_equal guard band, CLIP16 boundary cases (the +8/-8 guard
// band pinned), loop-wrap out_count edge math, a mini paint pipeline (8-bit mono
// flat + 16-bit stereo lerp) + the gate matrix, the raw-channel mix + paused
// skip, the DSP menu-gate, and the 0x40000000 mix-clock reset.

#include <xash3dpp/private/sound/mix_kernels.hpp>
#include <xash3dpp/private/sound/mixer.hpp>

#include <xash3dpp/sound/audio_data.hpp>
#include <xash3dpp/sound/providers.hpp>

#include <xash3dpp/limits.hpp>

#include "../test_helpers.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;
using ::xash::abi::portable_samplepair_t;

// Named accumulator comparison that reports which kernel/index mismatched.
static void expect_pair( const char *label, int idx, const portable_samplepair_t &got, int L, int R )
{
    if( got.left == L && got.right == R )
    {
        ++g_pass;
    }
    else
    {
        ++g_fail;
        std::printf( "FAIL [%s idx %d]: got {%d,%d} want {%d,%d}\n", label, idx, got.left, got.right, L, R );
    }
}

// ===========================================================================
// 1. The 12 kernels — one hand-derived bit-exact vector each.
// ===========================================================================

// Kernel 1/12 — mix_kernel<8,1,Flat> (S_MixMono8), shift 0.
//   data={10,-20,100}, vol={2,3}: (data[i]*vol[side])>>0.
//   i0: 10*2=20,10*3=30  i1: -20*2=-40,-20*3=-60  i2: 100*2=200,100*3=300
static void test_kernel_mono8_flat()
{
    const std::int8_t data[] = { 10, -20, 100 };
    const int         vol[2] = { 2, 3 };
    portable_samplepair_t pbuf[3] = {};
    mix_kernel<8, 1, Interp::Flat>( pbuf, vol, data, 0.0, 0.0, 3 );
    expect_pair( "Mono8Flat", 0, pbuf[0], 20, 30 );
    expect_pair( "Mono8Flat", 1, pbuf[1], -40, -60 );
    expect_pair( "Mono8Flat", 2, pbuf[2], 200, 300 );
}

// Kernel 2/12 — mix_kernel<16,1,Flat> (S_MixMono16), shift 8.
//   data={256,-512,1000}, vol={2,3}: (data*vol)>>8 (arithmetic).
//   i0: 512>>8=2,768>>8=3  i1:-1024>>8=-4,-1536>>8=-6  i2:2000>>8=7,3000>>8=11
static void test_kernel_mono16_flat()
{
    const std::int16_t data[] = { 256, -512, 1000 };
    const int          vol[2] = { 2, 3 };
    portable_samplepair_t pbuf[3] = {};
    mix_kernel<16, 1, Interp::Flat>( pbuf, vol, data, 0.0, 0.0, 3 );
    expect_pair( "Mono16Flat", 0, pbuf[0], 2, 3 );
    expect_pair( "Mono16Flat", 1, pbuf[1], -4, -6 );
    expect_pair( "Mono16Flat", 2, pbuf[2], 7, 11 );
}

// Kernel 3/12 — mix_kernel<8,2,Flat> (S_MixStereo8), shift 0, interleaved.
//   data={10,20,-30,40}, vol={2,3}: L=data[2i]*vol0, R=data[2i+1]*vol1.
//   i0: 10*2=20,20*3=60  i1: -30*2=-60,40*3=120
static void test_kernel_stereo8_flat()
{
    const std::int8_t data[] = { 10, 20, -30, 40 };
    const int         vol[2] = { 2, 3 };
    portable_samplepair_t pbuf[2] = {};
    mix_kernel<8, 2, Interp::Flat>( pbuf, vol, data, 0.0, 0.0, 2 );
    expect_pair( "Stereo8Flat", 0, pbuf[0], 20, 60 );
    expect_pair( "Stereo8Flat", 1, pbuf[1], -60, 120 );
}

// Kernel 4/12 — mix_kernel<16,2,Flat> (S_MixStereo16), shift 8, interleaved.
//   data={256,512,-1024,768}, vol={2,3}.
//   i0: 512>>8=2,1536>>8=6  i1:-2048>>8=-8,2304>>8=9
static void test_kernel_stereo16_flat()
{
    const std::int16_t data[] = { 256, 512, -1024, 768 };
    const int          vol[2] = { 2, 3 };
    portable_samplepair_t pbuf[2] = {};
    mix_kernel<16, 2, Interp::Flat>( pbuf, vol, data, 0.0, 0.0, 2 );
    expect_pair( "Stereo16Flat", 0, pbuf[0], 2, 6 );
    expect_pair( "Stereo16Flat", 1, pbuf[1], -8, 9 );
}

// Kernel 5/12 — mix_kernel<8,1,Pitch> (S_MixMonoPitch8), nearest, shift 0.
//   rate=0.5, frac0=0.0, vol={1,1}, data={10,20,30,40,50}. Accumulator
//   (frac += rate; idx += (uint)frac; frac -= (uint)frac):
//   i0 idx0=10; i1 idx0=10(frac 0.5->1.0->idx1); i2 idx1=20; i3 idx1=20.
static void test_kernel_mono8_pitch()
{
    const std::int8_t data[] = { 10, 20, 30, 40, 50 };
    const int         vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[4] = {};
    mix_kernel<8, 1, Interp::Pitch>( pbuf, vol, data, 0.0, 0.5, 4 );
    expect_pair( "Mono8Pitch", 0, pbuf[0], 10, 10 );
    expect_pair( "Mono8Pitch", 1, pbuf[1], 10, 10 );
    expect_pair( "Mono8Pitch", 2, pbuf[2], 20, 20 );
    expect_pair( "Mono8Pitch", 3, pbuf[3], 20, 20 );
}

// Kernel 6/12 — mix_kernel<16,1,Pitch> (S_MixMonoPitch16), shift 8, rate=2.0.
//   data step 256; idx 0,2,4: 256>>8=1, 768>>8=3, 1280>>8=5.
static void test_kernel_mono16_pitch()
{
    const std::int16_t data[] = { 256, 512, 768, 1024, 1280, 1536, 1792 };
    const int          vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[3] = {};
    mix_kernel<16, 1, Interp::Pitch>( pbuf, vol, data, 0.0, 2.0, 3 );
    expect_pair( "Mono16Pitch", 0, pbuf[0], 1, 1 );
    expect_pair( "Mono16Pitch", 1, pbuf[1], 3, 3 );
    expect_pair( "Mono16Pitch", 2, pbuf[2], 5, 5 );
}

// Kernel 7/12 — mix_kernel<8,2,Pitch> (S_MixStereoPitch8), stereo stride <<1.
//   rate=0.5, data L,R={10,11,20,21,30,31,40,41}. idx 0,0,2 (frame stride):
//   i0 (10,11) i1 (10,11) i2 (20,21).
static void test_kernel_stereo8_pitch()
{
    const std::int8_t data[] = { 10, 11, 20, 21, 30, 31, 40, 41 };
    const int         vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[3] = {};
    mix_kernel<8, 2, Interp::Pitch>( pbuf, vol, data, 0.0, 0.5, 3 );
    expect_pair( "Stereo8Pitch", 0, pbuf[0], 10, 11 );
    expect_pair( "Stereo8Pitch", 1, pbuf[1], 10, 11 );
    expect_pair( "Stereo8Pitch", 2, pbuf[2], 20, 21 );
}

// Kernel 8/12 — mix_kernel<16,2,Pitch> (S_MixStereoPitch16), shift 8, rate=2.0.
//   idx 0 then 4: (256>>8,512>>8)=(1,2), (1280>>8,1536>>8)=(5,6).
static void test_kernel_stereo16_pitch()
{
    const std::int16_t data[] = { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 };
    const int          vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[2] = {};
    mix_kernel<16, 2, Interp::Pitch>( pbuf, vol, data, 0.0, 2.0, 2 );
    expect_pair( "Stereo16Pitch", 0, pbuf[0], 1, 2 );
    expect_pair( "Stereo16Pitch", 1, pbuf[1], 5, 6 );
}

// Kernel 9/12 — mix_kernel<8,1,Lerp> (S_MixMonoLerp8), shift 0, 1-sample lookahead.
//   s = (int)( data[idx]*(1-frac) + data[idx+1]*frac ). rate=0.5, data={10,20,30,40}.
//   i0 frac0.0: s=10;  i1 frac0.5: s=(int)(10*0.5+20*0.5)=15;  i2 frac0.0 idx1: s=20.
static void test_kernel_mono8_lerp()
{
    const std::int8_t data[] = { 10, 20, 30, 40 };
    const int         vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[3] = {};
    mix_kernel<8, 1, Interp::Lerp>( pbuf, vol, data, 0.0, 0.5, 3 );
    expect_pair( "Mono8Lerp", 0, pbuf[0], 10, 10 );
    expect_pair( "Mono8Lerp", 1, pbuf[1], 15, 15 );
    expect_pair( "Mono8Lerp", 2, pbuf[2], 20, 20 );
}

// Kernel 10/12 — mix_kernel<16,1,Lerp> (S_MixMonoLerp16), shift 8. data={256,768,1280,1792}.
//   i0: s=256>>8=1;  i1 frac0.5: s=(int)(256*.5+768*.5)=512>>8=2;  i2 idx1: s=768>>8=3.
static void test_kernel_mono16_lerp()
{
    const std::int16_t data[] = { 256, 768, 1280, 1792 };
    const int          vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[3] = {};
    mix_kernel<16, 1, Interp::Lerp>( pbuf, vol, data, 0.0, 0.5, 3 );
    expect_pair( "Mono16Lerp", 0, pbuf[0], 1, 1 );
    expect_pair( "Mono16Lerp", 1, pbuf[1], 2, 2 );
    expect_pair( "Mono16Lerp", 2, pbuf[2], 3, 3 );
}

// Kernel 11/12 — mix_kernel<8,2,Lerp> (S_MixStereoLerp8), lookahead one frame (+2/+3).
//   rate=0.5, data L,R={10,20,30,40,50,60}.
//   i0 frac0: sl=10,sr=20;  i1 frac0.5: sl=(int)(10*.5+30*.5)=20, sr=(int)(20*.5+40*.5)=30.
static void test_kernel_stereo8_lerp()
{
    const std::int8_t data[] = { 10, 20, 30, 40, 50, 60 };
    const int         vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[2] = {};
    mix_kernel<8, 2, Interp::Lerp>( pbuf, vol, data, 0.0, 0.5, 2 );
    expect_pair( "Stereo8Lerp", 0, pbuf[0], 10, 20 );
    expect_pair( "Stereo8Lerp", 1, pbuf[1], 20, 30 );
}

// Kernel 12/12 — mix_kernel<16,2,Lerp> (S_MixStereoLerp16), shift 8.
//   data L,R={256,512,768,1024,1280,1536}. i0 frac0: sl=256>>8=1,sr=512>>8=2.
//   i1 frac0.5: sl=(int)(256*.5+768*.5)=512>>8=2, sr=(int)(512*.5+1024*.5)=768>>8=3.
static void test_kernel_stereo16_lerp()
{
    const std::int16_t data[] = { 256, 512, 768, 1024, 1280, 1536 };
    const int          vol[2] = { 1, 1 };
    portable_samplepair_t pbuf[2] = {};
    mix_kernel<16, 2, Interp::Lerp>( pbuf, vol, data, 0.0, 0.5, 2 );
    expect_pair( "Stereo16Lerp", 0, pbuf[0], 1, 2 );
    expect_pair( "Stereo16Lerp", 1, pbuf[1], 2, 3 );
}

// ===========================================================================
// 2. Dispatch-table selection matrix + q_equal guard band (s_mix.c:120-173).
// ===========================================================================
static void test_dispatch_selection_matrix()
{
    // width_bytes 1 -> 8-bit, 2 -> 16-bit; channels 1 -> mono, 2 -> stereo.
    CHECK( select_kernel( 1, 1, Interp::Flat )  == ( &mix_kernel<8, 1, Interp::Flat> ) );
    CHECK( select_kernel( 1, 1, Interp::Pitch ) == ( &mix_kernel<8, 1, Interp::Pitch> ) );
    CHECK( select_kernel( 1, 1, Interp::Lerp )  == ( &mix_kernel<8, 1, Interp::Lerp> ) );
    CHECK( select_kernel( 1, 2, Interp::Flat )  == ( &mix_kernel<8, 2, Interp::Flat> ) );
    CHECK( select_kernel( 1, 2, Interp::Pitch ) == ( &mix_kernel<8, 2, Interp::Pitch> ) );
    CHECK( select_kernel( 1, 2, Interp::Lerp )  == ( &mix_kernel<8, 2, Interp::Lerp> ) );
    CHECK( select_kernel( 2, 1, Interp::Flat )  == ( &mix_kernel<16, 1, Interp::Flat> ) );
    CHECK( select_kernel( 2, 1, Interp::Pitch ) == ( &mix_kernel<16, 1, Interp::Pitch> ) );
    CHECK( select_kernel( 2, 1, Interp::Lerp )  == ( &mix_kernel<16, 1, Interp::Lerp> ) );
    CHECK( select_kernel( 2, 2, Interp::Flat )  == ( &mix_kernel<16, 2, Interp::Flat> ) );
    CHECK( select_kernel( 2, 2, Interp::Pitch ) == ( &mix_kernel<16, 2, Interp::Pitch> ) );
    CHECK( select_kernel( 2, 2, Interp::Lerp )  == ( &mix_kernel<16, 2, Interp::Lerp> ) );

    // legacy else-branch semantics: any width != 1 -> 16-bit; any chan != 1 -> stereo.
    CHECK( select_kernel( 4, 3, Interp::Flat ) == ( &mix_kernel<16, 2, Interp::Flat> ) );
}

static void test_q_equal_guard_band()
{
    // EQUAL_EPSILON = 0.001f (public/xash3d_mathlib.h:70).
    CHECK( q_equal( 1.0, 1.0 ) );
    CHECK( q_equal( 1.0009, 1.0 ) );   // within +epsilon
    CHECK( q_equal( 0.9991, 1.0 ) );   // within -epsilon
    CHECK( !q_equal( 1.0011, 1.0 ) );  // outside +epsilon
    CHECK( !q_equal( 0.9989, 1.0 ) );  // outside -epsilon
    CHECK( q_equal( 0.0005, 0.0 ) );
    CHECK( !q_equal( 0.0011, 0.0 ) );
}

// mix_audio dispatch must match a direct kernel call for each of the 3 cases.
static void test_mix_audio_dispatch()
{
    const std::int16_t data[] = { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 };
    const int          vol[2] = { 1, 1 };

    // Flat: rate==1 && frac==0.
    {
        portable_samplepair_t via_dispatch[3] = {};
        portable_samplepair_t via_kernel[3]   = {};
        mix_audio( via_dispatch, vol, data, 1, 2, 0.0, 1.0, 3, false );
        mix_kernel<16, 1, Interp::Flat>( via_kernel, vol, data, 0.0, 1.0, 3 );
        for( int i = 0; i < 3; ++i )
            expect_pair( "dispatchFlat", i, via_dispatch[i], via_kernel[i].left, via_kernel[i].right );
    }
    // Pitch: not-flat, lerp==false.
    {
        portable_samplepair_t via_dispatch[3] = {};
        portable_samplepair_t via_kernel[3]   = {};
        mix_audio( via_dispatch, vol, data, 1, 2, 0.0, 2.0, 3, false );
        mix_kernel<16, 1, Interp::Pitch>( via_kernel, vol, data, 0.0, 2.0, 3 );
        for( int i = 0; i < 3; ++i )
            expect_pair( "dispatchPitch", i, via_dispatch[i], via_kernel[i].left, via_kernel[i].right );
    }
    // Lerp: not-flat (frac!=0), lerp==true.
    {
        portable_samplepair_t via_dispatch[2] = {};
        portable_samplepair_t via_kernel[2]   = {};
        mix_audio( via_dispatch, vol, data, 1, 2, 0.25, 1.0, 2, true );
        mix_kernel<16, 1, Interp::Lerp>( via_kernel, vol, data, 0.25, 1.0, 2 );
        for( int i = 0; i < 2; ++i )
            expect_pair( "dispatchLerp", i, via_dispatch[i], via_kernel[i].left, via_kernel[i].right );
    }
}

// ===========================================================================
// 3. CLIP16 boundary cases — the +8/-8 guard band pinned (sound.h:40).
// ===========================================================================
static void test_compute_channel_pitch_float_chain()
{
    // S9.3 parity-audit pin (2026-07-20): legacy rounds basePitch*0.01 to
    // FLOAT at the VOX_ModifyPitch parameter and multiplies float*float
    // (s_vox.c:206, s_mix.c:316,391).  basePitch=95: double 0.95 rounds to
    // the float 0x3F733333 = 0.949999988079071044921875 — NOT the double
    // 0.95 a double-only evaluation would produce.
    const double p95 = compute_channel_pitch( 95.0, 1.0 );
    CHECK_EQ( p95, static_cast<double>( 0.95f ) );
    CHECK( p95 != 0.95 ); // the double-math value the fixed divergence produced

    // basePitch=100 with pitch_mult=1 is exact in both representations —
    // the case that masked the divergence in every earlier test.
    CHECK_EQ( compute_channel_pitch( 100.0, 1.0 ), 1.0 );

    // Non-unity pitch_mult (timescale chipmunk): the mult is rounded to
    // float BEFORE the product; 1.15 as float is 0x3F933333.
    const double p_ts = compute_channel_pitch( 100.0, 1.15 );
    CHECK_EQ( p_ts, static_cast<double>( 1.0f * static_cast<float>( 1.15 ) ) );
}

static void test_clip16_guard_band()
{
    CHECK_EQ( clip16( 0 ), static_cast<std::int16_t>( 0 ) );
    CHECK_EQ( clip16( 32758 ), static_cast<std::int16_t>( 32758 ) );   // passthrough below max
    CHECK_EQ( clip16( 32759 ), static_cast<std::int16_t>( 32759 ) );   // reachable max (bound uses `<`)
    CHECK_EQ( clip16( 32760 ), static_cast<std::int16_t>( 32759 ) );   // clamped to guard band
    CHECK_EQ( clip16( 40000 ), static_cast<std::int16_t>( 32759 ) );
    CHECK_EQ( clip16( 32767 ), static_cast<std::int16_t>( 32759 ) );   // NOT full int16 range
    CHECK_EQ( clip16( -32759 ), static_cast<std::int16_t>( -32759 ) ); // passthrough above min
    CHECK_EQ( clip16( -32760 ), static_cast<std::int16_t>( -32760 ) ); // reachable min
    CHECK_EQ( clip16( -32761 ), static_cast<std::int16_t>( -32760 ) ); // clamped
    CHECK_EQ( clip16( -40000 ), static_cast<std::int16_t>( -32760 ) );
    CHECK_EQ( clip16( -32768 ), static_cast<std::int16_t>( -32760 ) ); // NOT full int16 range
}

// ===========================================================================
// AudioData builders (native PCM; 8-bit is stored SIGNED per codec_wav.cpp).
// ===========================================================================
static AudioData make_mono8( std::initializer_list<int> samples, std::uint32_t loop_start = 0, bool looped = false )
{
    AudioData a;
    a.rate       = 44100;
    a.width      = 1;
    a.channels   = 1;
    a.samples    = static_cast<std::uint32_t>( samples.size() );
    a.loop_start = loop_start;
    a.type       = AudioFormatType::Pcm;
    a.flags      = looped ? AudioFlags::Looped : AudioFlags::None;
    a.buffer.resize( samples.size() );
    std::size_t i = 0;
    for( int s : samples )
        a.buffer[i++] = std::byte{ static_cast<std::uint8_t>( static_cast<std::int8_t>( s ) ) };
    return a;
}

static AudioData make_stereo16( std::initializer_list<std::int16_t> interleaved, std::uint32_t frames )
{
    AudioData a;
    a.rate     = 44100;
    a.width    = 2;
    a.channels = 2;
    a.samples  = frames; // per-channel frame count
    a.type     = AudioFormatType::Pcm;
    a.flags    = AudioFlags::None;
    a.buffer.resize( interleaved.size() * sizeof( std::int16_t ) );
    std::vector<std::int16_t> tmp( interleaved );
    std::memcpy( a.buffer.data(), tmp.data(), a.buffer.size() ); // native int16 (little-endian test hosts)
    return a;
}

// ===========================================================================
// 4. Loop-wrap edge math (S_AdjustLoopedSamplePosition / S_RetrieveAudioSamples /
//    S_MixChannelToBuffer out_count, s_main.c:66-108 + s_mix.c:226-273).
// ===========================================================================
static void test_adjust_looped_sample_position()
{
    AudioData looped     = make_mono8( { 1, 2, 3, 4 }, /*loop_start*/ 0, /*looped*/ true );
    looped.samples       = 100;
    looped.loop_start    = 20;
    AudioData not_looped = make_mono8( { 1, 2, 3, 4 } ); // no Looped flag
    not_looped.samples   = 100;

    CHECK_EQ( adjust_looped_sample_position( looped, 50, true ), 50 );   // below samples, no wrap
    CHECK_EQ( adjust_looped_sample_position( looped, 100, true ), 20 );  // exactly samples -> loop_start
    CHECK_EQ( adjust_looped_sample_position( looped, 130, true ), 50 );  // 20 + (110 % 80) = 50
    CHECK_EQ( adjust_looped_sample_position( looped, 100, false ), 100 );// looping disabled
    CHECK_EQ( adjust_looped_sample_position( not_looped, 100, true ), 100 ); // no Looped flag
}

static void test_retrieve_audio_samples()
{
    AudioData looped = make_mono8( { 1, 2, 3, 4 }, /*loop_start*/ 0, /*looped*/ true );
    const void *audio = nullptr;

    // start within bounds: available = samples - start.
    CHECK_EQ( retrieve_audio_samples( looped, &audio, 2, 10, true ), 2 );
    CHECK( audio == looped.buffer.data() + 2 );

    // start at end + looping wraps to 0: available = 4.
    CHECK_EQ( retrieve_audio_samples( looped, &audio, 4, 10, true ), 4 );
    CHECK( audio == looped.buffer.data() + 0 );

    // start at end, NO looping: available underflows -> num<=0 guard returns 0.
    CHECK_EQ( retrieve_audio_samples( looped, &audio, 4, 10, false ), 0 );
    // start past end, no looping: unsigned underflow still guarded.
    CHECK_EQ( retrieve_audio_samples( looped, &audio, 6, 10, false ), 0 );
}

static void test_mix_channel_loop_wrap()
{
    // Looped mono8 {1,2,3,4}, loop_start 0, rate 1.0 (flat). Start mid-buffer at
    // sample 2.0 and mix 4 samples: reads {3,4} then WRAPS to {1,2}. Volume 1
    // (shift 0) so the output equals the source samples.
    AudioData src = make_mono8( { 1, 2, 3, 4 }, /*loop_start*/ 0, /*looped*/ true );

    MixChannel ch{};
    ch.source   = &src;
    ch.sample   = 2.0;
    ch.leftvol  = 1;
    ch.rightvol = 1;
    ch.flags    = ::xash::abi::k_fl_chan_use_loop;

    portable_samplepair_t pbuf[4] = {};
    const int written = mix_channel_to_buffer( pbuf, ch, /*num_samples*/ 4, /*out_rate*/ 44100,
                                               /*pitch*/ 1.0, /*offset*/ 0, /*timecompress*/ 0,
                                               /*lerping*/ false );
    CHECK_EQ( written, 4 );
    expect_pair( "loopwrap", 0, pbuf[0], 3, 3 ); // reads index 2..3
    expect_pair( "loopwrap", 1, pbuf[1], 4, 4 );
    expect_pair( "loopwrap", 2, pbuf[2], 1, 1 ); // wrapped back to loop_start
    expect_pair( "loopwrap", 3, pbuf[3], 2, 2 );
    CHECK( ch.sample == 6.0 ); // advanced by 4 at rate 1.0
}

static void test_mix_channel_out_count_lerp_fallback()
{
    // Non-looped mono8 {1..10}, lerp on, start sample 8.5 (frac 0.5), rate 1.0.
    // Near the end available drops to 2 then 1: exercises the lerp out_count<=0
    // fallback (out_count = floor((available-1-frac)/rate) = floor(0.5) = 0 ->
    // nearest, 1 sample) then the ceil-nearest out_count, then FINISHED on
    // exhaustion (s_mix.c:239-273).
    AudioData src = make_mono8( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } );

    MixChannel ch{};
    ch.source   = &src;
    ch.sample   = 8.5;
    ch.leftvol  = 1;
    ch.rightvol = 1;
    ch.flags    = 0; // no loop

    portable_samplepair_t pbuf[5] = {};
    const int written = mix_channel_to_buffer( pbuf, ch, /*num_samples*/ 5, /*out_rate*/ 44100,
                                               /*pitch*/ 1.0, /*offset*/ 0, /*timecompress*/ 0,
                                               /*lerping*/ true );
    // First block: fallback nearest reads index 8 (value 9). Second: index 9 (10).
    // Third block retrieves nothing -> break -> FINISHED, so only 2 written.
    CHECK_EQ( written, 2 );
    expect_pair( "outcount", 0, pbuf[0], 9, 9 );
    expect_pair( "outcount", 1, pbuf[1], 10, 10 );
    CHECK( ( ch.flags & ::xash::abi::k_fl_chan_finished ) != 0 );
}

// ===========================================================================
// 5. Mini paint pipeline + gate matrix.
// ===========================================================================

// Counting DSP hook (S9.5 seam) — records invocations for the menu-gate test.
class CountingDsp final : public IRoomDsp
{
public:
    int calls = 0;
    void process( portable_samplepair_t *, int ) noexcept override { ++calls; }
};

// Two-word VOX advance fake (S9.4 seam) — proves vox_mix_channel_to_buffer wires.
class TwoWordVox final : public IVoxWordAdvance
{
public:
    const AudioData *word2 = nullptr;
    int              advances = 0;
    bool next_word( MixChannel &chan ) noexcept override
    {
        ++advances;
        if( advances == 1 )
        {
            // Load the second word: rebind source, reset, clear FINISHED.
            chan.source     = word2;
            chan.sample     = 0.0;
            chan.forced_end = 0.0;
            chan.flags &= ~::xash::abi::k_fl_chan_finished;
            return true;
        }
        // No more words -> sentence finished.
        chan.flags |= ::xash::abi::k_fl_chan_sentence_finished;
        return false;
    }
};

// Build the standard 2-channel scene (8-bit mono flat + 16-bit stereo lerp) with
// vol 100 each and paint one 2-sample block. Returns the mixer + sources by ref.
static void paint_two_channel_scene( Mixer &m, AudioData &srcA, AudioData &srcB, const MixGateSnapshot &gate,
                                     std::vector<std::int16_t> &out )
{
    m.set_out_rate( 44100 );
    m.set_lerping( true );
    m.set_painted_time( 0 );

    MixChannel a{};
    a.source     = &srcA;
    a.sample     = 0.0;   // frac 0, rate 1 -> flat
    a.leftvol    = 100;
    a.rightvol   = 100;
    a.base_pitch = 100.0;

    MixChannel b{};
    b.source     = &srcB;
    b.sample     = 0.5;   // frac 0.5 -> lerp (lerping on)
    b.leftvol    = 100;
    b.rightvol   = 100;
    b.base_pitch = 100.0;

    m.channels().clear();
    m.channels().push_back( a );
    m.channels().push_back( b );

    auto span = m.paint_channels( /*endtime*/ 2, gate, /*master_volume*/ 1.0f, /*pitch_mult*/ 1.0 );
    out.assign( span.begin(), span.end() );
}

static void test_paint_two_channels_normal()
{
    AudioData srcA = make_mono8( { 10, 20, 30, 40 } );
    AudioData srcB = make_stereo16( { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 }, /*frames*/ 4 );

    Mixer m;
    MixGateSnapshot gate{};
    gate.in_game = true; // normal play

    std::vector<std::int16_t> out;
    paint_two_channel_scene( m, srcA, srcB, gate, out );

    // Hand derived: A(flat mono8, vol100): rb += {1000,1000},{2000,2000}.
    //   B(stereo16 lerp @ frac0.5 rate1, vol100): rb += {200,300},{400,500}.
    //   sum rb = {1200,1300},{2400,2500}; gain 256 (unity) -> paint; CLIP16 pass.
    REQUIRE( out.size() == 4 );
    CHECK_EQ( out[0], static_cast<std::int16_t>( 1200 ) );
    CHECK_EQ( out[1], static_cast<std::int16_t>( 1300 ) );
    CHECK_EQ( out[2], static_cast<std::int16_t>( 2400 ) );
    CHECK_EQ( out[3], static_cast<std::int16_t>( 2500 ) );
    CHECK_EQ( m.painted_time(), 2 );
}

static void test_paint_gate_menu_and_pause_skip()
{
    // Menu + single-player + non-local channels -> per-channel gate skips both
    // (s_mix.c:339); room_channels 0 -> gain-mix skipped in menu -> silence.
    {
        AudioData srcA = make_mono8( { 10, 20, 30, 40 } );
        AudioData srcB = make_stereo16( { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 }, 4 );
        Mixer m;
        MixGateSnapshot gate{};
        gate.in_menu       = true;
        gate.single_player = true;
        gate.in_game       = true;
        std::vector<std::int16_t> out;
        paint_two_channel_scene( m, srcA, srcB, gate, out );
        REQUIRE( out.size() == 4 );
        for( std::int16_t s : out )
            CHECK_EQ( s, static_cast<std::int16_t>( 0 ) );
    }
    // Paused + single-player -> same (in_menu||paused) gate -> silence.
    {
        AudioData srcA = make_mono8( { 10, 20, 30, 40 } );
        AudioData srcB = make_stereo16( { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 }, 4 );
        Mixer m;
        MixGateSnapshot gate{};
        gate.paused        = true;
        gate.single_player = true;
        gate.in_game       = true;
        std::vector<std::int16_t> out;
        paint_two_channel_scene( m, srcA, srcB, gate, out );
        REQUIRE( out.size() == 4 );
        for( std::int16_t s : out )
            CHECK_EQ( s, static_cast<std::int16_t>( 0 ) );
    }
    // Background + console -> mix_normal early-out -> silence (s_mix.c:323).
    {
        AudioData srcA = make_mono8( { 10, 20, 30, 40 } );
        AudioData srcB = make_stereo16( { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 }, 4 );
        Mixer m;
        MixGateSnapshot gate{};
        gate.background = true;
        gate.in_console = true;
        std::vector<std::int16_t> out;
        paint_two_channel_scene( m, srcA, srcB, gate, out );
        REQUIRE( out.size() == 4 );
        for( std::int16_t s : out )
            CHECK_EQ( s, static_cast<std::int16_t>( 0 ) );
    }
}

static void test_paint_dsp_menu_gate()
{
    // Isolate the DSP-step menu gate from the channel gate by using multiplayer
    // (single_player=false) so the channel always mixes; only the DSP hook's
    // presence differs with in_menu (s_mix.c:562-563).
    AudioData srcA = make_mono8( { 10, 20, 30, 40 } );

    auto run = [&]( bool in_menu, CountingDsp &dsp ) {
        Mixer m;
        m.set_out_rate( 44100 );
        m.set_room_dsp( &dsp );
        m.set_painted_time( 0 );
        MixChannel a{};
        a.source     = &srcA;
        a.leftvol    = 100;
        a.rightvol   = 100;
        a.base_pitch = 100.0;
        m.channels().push_back( a );
        MixGateSnapshot gate{};
        gate.in_menu       = in_menu;
        gate.single_player = false; // channel not gated out
        gate.in_game       = true;
        (void)m.paint_channels( 2, gate, 1.0f, 1.0 );
    };

    CountingDsp out_of_menu;
    run( false, out_of_menu );
    CHECK_EQ( out_of_menu.calls, 1 ); // DSP applied outside menu

    CountingDsp in_menu;
    run( true, in_menu );
    CHECK_EQ( in_menu.calls, 0 );     // DSP skipped in menu
}

static void test_paint_raw_channels()
{
    // One non-voice raw channel: paints into roombuffer, counts as a room channel,
    // then gain-mixes to paint. vol 128 (>>8) halves the ring samples.
    const portable_samplepair_t ring[4] = { { 100, 200 }, { 300, 400 }, { 500, 600 }, { 700, 800 } };

    auto run = [&]( bool paused, std::vector<std::int16_t> &out ) {
        Mixer m;
        m.set_out_rate( 44100 );
        m.set_painted_time( 0 );
        RawMixChannel rc{};
        rc.rawsamples      = std::span<const portable_samplepair_t>{ ring, 4 };
        rc.s_rawend        = 2;
        rc.max_samples     = 4;
        rc.leftvol         = 128;
        rc.rightvol        = 128;
        rc.direct_to_paint = false; // room channel (DSP-eligible)
        m.raw_channels().push_back( rc );
        MixGateSnapshot gate{};
        gate.in_game = true;
        gate.paused  = paused;
        auto span = m.paint_channels( 2, gate, 1.0f, 1.0 );
        out.assign( span.begin(), span.end() );
    };

    std::vector<std::int16_t> out;
    run( false, out );
    // (100*128)>>8=50, (200*128)>>8=100 ; (300*128)>>8=150, (400*128)>>8=200.
    REQUIRE( out.size() == 4 );
    CHECK_EQ( out[0], static_cast<std::int16_t>( 50 ) );
    CHECK_EQ( out[1], static_cast<std::int16_t>( 100 ) );
    CHECK_EQ( out[2], static_cast<std::int16_t>( 150 ) );
    CHECK_EQ( out[3], static_cast<std::int16_t>( 200 ) );

    // Paused -> raw channels skipped entirely (s_mix.c:418) -> silence.
    std::vector<std::int16_t> quiet;
    run( true, quiet );
    for( std::int16_t s : quiet )
        CHECK_EQ( s, static_cast<std::int16_t>( 0 ) );
}

// ===========================================================================
// 6. VOX word-advance seam (S9.4 entry shape).
// ===========================================================================
static void test_vox_word_advance_seam()
{
    // Two 1-sample mono8 words. The first finishes immediately (start past its
    // single sample), the advance loads the second, which also finishes; the
    // second advance sets SENTENCE_FINISHED and the loop exits.
    AudioData word1 = make_mono8( { 5 } );
    AudioData word2 = make_mono8( { 9 } );

    MixChannel ch{};
    ch.source      = &word1;
    ch.sample      = 0.0;
    ch.leftvol     = 1;
    ch.rightvol    = 1;
    ch.is_sentence = true;

    TwoWordVox vox;
    vox.word2 = &word2;

    portable_samplepair_t pbuf[4] = {};
    const int written = vox_mix_channel_to_buffer( pbuf, ch, /*num_samples*/ 4, /*out_rate*/ 44100,
                                                   /*pitch*/ 1.0, /*lerping*/ false, vox );
    // word1 mixes its 1 sample (5), word2 mixes its 1 sample (9); then finished.
    CHECK_EQ( written, 2 );
    expect_pair( "vox", 0, pbuf[0], 5, 5 );
    expect_pair( "vox", 1, pbuf[1], 9, 9 );
    CHECK_EQ( vox.advances, 2 );
    CHECK( ( ch.flags & ::xash::abi::k_fl_chan_sentence_finished ) != 0 );
}

// ===========================================================================
// 7. The 0x40000000 mix-clock reset (S_GetSoundtime, s_main.c:1503-1531).
// ===========================================================================
static void test_soundtime_overflow_reset()
{
    // Wrap WITHOUT overflow: buffers increments, painted_time untouched.
    {
        Mixer m;
        m.set_painted_time( 1000 );
        auto r1 = m.reconstruct_soundtime( /*samplepos*/ 100, /*dma_samples*/ 1000 );
        CHECK( !r1.overflow_reset );
        CHECK_EQ( r1.soundtime, 50 ); // 0*500 + 100/2
        auto r2 = m.reconstruct_soundtime( /*samplepos*/ 50, /*dma_samples*/ 1000 ); // 50 < 100 -> wrap
        CHECK( !r2.overflow_reset );
        CHECK_EQ( r2.soundtime, 525 ); // 1*500 + 50/2
        CHECK_EQ( m.painted_time(), 1000 );
    }
    // Wrap WITH overflow: painted_time > 0x40000000 -> hard reset to fullsamples.
    {
        Mixer m;
        m.set_painted_time( 0x40000000 + 1 );
        auto r1 = m.reconstruct_soundtime( 100, 1000 ); // no wrap yet (100 !< 0)
        CHECK( !r1.overflow_reset );
        auto r2 = m.reconstruct_soundtime( 50, 1000 );  // wrap + overflow
        CHECK( r2.overflow_reset );                     // caller must S_StopAllSounds(true) (S9.6)
        CHECK_EQ( m.painted_time(), 500 );              // reset to fullsamples = dma_samples/2
    }
}

// ===========================================================================
// 8. Channel arrays "made real" — reserved to the ABI-frozen limits.
// ===========================================================================
static void test_channel_arrays_reserved()
{
    Mixer m;
    CHECK_LE( ::xash::limits::sound_max_channels, m.channels().capacity() );      // 320
    CHECK_LE( ::xash::limits::sound_max_raw_channels, m.raw_channels().capacity() );// 48
}

int main()
{
    // The 12 kernels.
    RUN_TEST( test_kernel_mono8_flat );
    RUN_TEST( test_kernel_mono16_flat );
    RUN_TEST( test_kernel_stereo8_flat );
    RUN_TEST( test_kernel_stereo16_flat );
    RUN_TEST( test_kernel_mono8_pitch );
    RUN_TEST( test_kernel_mono16_pitch );
    RUN_TEST( test_kernel_stereo8_pitch );
    RUN_TEST( test_kernel_stereo16_pitch );
    RUN_TEST( test_kernel_mono8_lerp );
    RUN_TEST( test_kernel_mono16_lerp );
    RUN_TEST( test_kernel_stereo8_lerp );
    RUN_TEST( test_kernel_stereo16_lerp );

    // Dispatch + CLIP16.
    RUN_TEST( test_dispatch_selection_matrix );
    RUN_TEST( test_q_equal_guard_band );
    RUN_TEST( test_mix_audio_dispatch );
    RUN_TEST( test_compute_channel_pitch_float_chain );
    RUN_TEST( test_clip16_guard_band );

    // Resample driver + loop wrap.
    RUN_TEST( test_adjust_looped_sample_position );
    RUN_TEST( test_retrieve_audio_samples );
    RUN_TEST( test_mix_channel_loop_wrap );
    RUN_TEST( test_mix_channel_out_count_lerp_fallback );

    // Paint pipeline + gate matrix + raw + DSP + VOX seam.
    RUN_TEST( test_paint_two_channels_normal );
    RUN_TEST( test_paint_gate_menu_and_pause_skip );
    RUN_TEST( test_paint_dsp_menu_gate );
    RUN_TEST( test_paint_raw_channels );
    RUN_TEST( test_vox_word_advance_seam );

    // Mix clock + channel arrays.
    RUN_TEST( test_soundtime_overflow_reset );
    RUN_TEST( test_channel_arrays_reserved );

    std::printf( "sound_mixer: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
