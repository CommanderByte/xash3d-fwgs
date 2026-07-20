// xash3dpp — room-effects DSP parity tests (Chunk 9, slice S9.5). PARITY-
// CRITICAL. Every expected value below is HAND-DERIVED from the legacy
// integer math (engine/client/sound/s_dsp.c) — never captured from this
// implementation. See dsp.hpp/dsp.cpp for the per-function line map.
//
// Coverage: preset-table content pins (6 rows/table vs source values), the
// SND-OQ-6 idsp_room==29 sentinel (content pin + behavioural equivalence to
// "room off"), the room-selection matrix (select_room, waterlevel/room_type/
// clamp cases), hand-derived pass-math vectors for all 4 passes (AMod,
// Reverb x2 coeff tables, mono Delay x2 coeff tables/lowpass states, Stereo
// Delay), a fresh-instance passthrough pin, ClearState's lazy-teardown
// semantics, and a full RoomFX block wired through the S9.3 Mixer paint
// pipeline with a byte-pinned output.

#include <xash3dpp/private/sound/dsp.hpp>
#include <xash3dpp/private/sound/mixer.hpp>

#include <xash3dpp/sound/audio_data.hpp>
#include <xash3dpp/sound/providers.hpp>

#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstdint>

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;
using ::xash::abi::portable_samplepair_t;

// Named-field equality with a helpful failure message.
static void expect_pair( const char *label, const portable_samplepair_t &got, int L, int R )
{
    if( got.left == L && got.right == R )
    {
        ++g_pass;
    }
    else
    {
        ++g_fail;
        std::printf( "FAIL [%s]: got {%d,%d} want {%d,%d}\n", label, got.left, got.right, L, R );
    }
}

static void expect_preset( const char *label, const RoomPreset &got, float lp, float mod, float size, float refl,
                           float rvblp, float delay, float feedback, float dlylp, float left )
{
    const bool ok = got.room_lp == lp && got.room_mod == mod && got.room_size == size && got.room_refl == refl
        && got.room_rvblp == rvblp && got.room_delay == delay && got.room_feedback == feedback
        && got.room_dlylp == dlylp && got.room_left == left;
    if( ok )
    {
        ++g_pass;
    }
    else
    {
        ++g_fail;
        std::printf( "FAIL [%s]: got {%g,%g,%g,%g,%g,%g,%g,%g,%g}\n", label,
                    static_cast<double>( got.room_lp ), static_cast<double>( got.room_mod ),
                    static_cast<double>( got.room_size ), static_cast<double>( got.room_refl ),
                    static_cast<double>( got.room_rvblp ), static_cast<double>( got.room_delay ),
                    static_cast<double>( got.room_feedback ), static_cast<double>( got.room_dlylp ),
                    static_cast<double>( got.room_left ) );
    }
}

// ===========================================================================
// 1. Preset-table content pins (s_dsp.c:72-105 rgsxpre, :109-142 rgsxpre_hlalpha052)
// ===========================================================================

static void test_preset_table_release_spot_rows()
{
    // 6 spot rows transcribed verbatim from source, re-checked independently
    // of dsp.hpp's own transcription (row comments cite the // N marker).
    expect_preset( "release[0] off", k_room_presets_release[0],
                   0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f );
    expect_preset( "release[1] generic", k_room_presets_release[1],
                   0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.065f, 0.1f, 0.0f, 0.01f );
    expect_preset( "release[5] tunnel", k_room_presets_release[5],
                   0.0f, 0.0f, 0.05f, 0.85f, 1.0f, 0.008f, 0.96f, 2.0f, 0.01f );
    expect_preset( "release[14] water", k_room_presets_release[14],
                   1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.01f );
    expect_preset( "release[26] weirdo", k_room_presets_release[26],
                   0.0f, 1.0f, 0.01f, 0.9f, 0.0f, 0.0f, 0.0f, 2.0f, 0.05f );
    expect_preset( "release[28]", k_room_presets_release[28],
                   0.0f, 0.0f, 0.001f, 0.999f, 0.0f, 0.2f, 0.8f, 2.0f, 0.05f );
}

static void test_preset_table_hlalpha052_spot_rows()
{
    // Rows chosen to also demonstrate the two tables genuinely differ
    // (row 1's delay/feedback, row 14's room_mod, row 17's room_delay).
    expect_preset( "alpha[0] off", k_room_presets_hlalpha052[0],
                   0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f );
    expect_preset( "alpha[1] generic", k_room_presets_hlalpha052[1],
                   0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.08f, 0.8f, 2.0f, 0.0f );
    expect_preset( "alpha[14] water", k_room_presets_hlalpha052[14],
                   1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.01f );
    expect_preset( "alpha[17] concrete", k_room_presets_hlalpha052[17],
                   0.0f, 0.0f, 0.05f, 0.8f, 1.0f, 0.15f, 0.48f, 2.0f, 0.008f );
    expect_preset( "alpha[26] weirdo", k_room_presets_hlalpha052[26],
                   0.0f, 1.0f, 0.01f, 0.9f, 0.0f, 0.0f, 0.0f, 2.0f, 0.05f );
    expect_preset( "alpha[28]", k_room_presets_hlalpha052[28],
                   0.0f, 0.0f, 0.001f, 0.999f, 0.0f, 0.2f, 0.8f, 2.0f, 0.05f );
}

// ===========================================================================
// 2. SND-OQ-6 — idsp_room == 29 (s_dsp.c:21,809 off-by-one). Decision:
//    REPRODUCE-WITH-DEFINED-BEHAVIOUR — pad both tables to a 30th row, all
//    fields zeroed (dsp.hpp file header documents why zeroed rather than any
//    "recovered" content). Pinned here: (a) the padded row's raw content is
//    the defined superset (all-zero), (b) applying it is BEHAVIOURALLY
//    indistinguishable from preset 0 ("off") for real DSP output.
// ===========================================================================

static void test_snd_oq6_sentinel_row_content()
{
    CHECK_EQ( k_room_preset_count, static_cast<std::size_t>( k_max_room_types ) + 1 ); // 30
    expect_preset( "release[29] SND-OQ-6 sentinel", k_room_presets_release[ static_cast<std::size_t>( k_max_room_types ) ],
                   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
    expect_preset( "alpha[29] SND-OQ-6 sentinel", k_room_presets_hlalpha052[ static_cast<std::size_t>( k_max_room_types ) ],
                   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
}

static void test_snd_oq6_index29_behaves_like_off()
{
    ::xash::memory::ScopedPool poolA( "test_dsp_oq6_a" );
    ::xash::memory::ScopedPool poolB( "test_dsp_oq6_b" );
    RoomDsp dspA( poolA.handle() ); // will select idsp_room == 29 (the padded sentinel)
    RoomDsp dspB( poolB.handle() ); // will select idsp_room == 0 ("off")

    dspA.set_room_type( 29.0f ); // bound(0,29,29) == 29 reachable (the off-by-one, preserved)
    dspB.set_room_type( 0.0f );

    const std::array<portable_samplepair_t, 4> original { { { 1000, 2000 }, { -500, 700 }, { 12345, -6789 }, { 0, 0 } } };
    std::array<portable_samplepair_t, 4>       bufA = original;
    std::array<portable_samplepair_t, 4>       bufB = original;

    dspA.process( bufA.data(), static_cast<int>( bufA.size() ) );
    dspB.process( bufB.data(), static_cast<int>( bufB.size() ) );

    CHECK_EQ( dspA.room_index(), 29 );
    CHECK_EQ( dspB.room_index(), 0 );

    // Both are no-op passthroughs (room 0 / the zeroed sentinel both leave
    // every pass inactive: room_size==0 disables reverb, room_delay==0
    // disables the mono delay, room_left==0 disables the stereo delay, and
    // room_lp==room_mod==0 disables AMod, identically for both rows), so
    // each buffer must come out unchanged from its original input AND the
    // two independently-processed buffers must be byte-identical to each
    // other (the actual SND-OQ-6 behavioural-equivalence pin).
    for( std::size_t i = 0; i < bufA.size(); i++ )
    {
        expect_pair( "SND-OQ-6 idsp_room=29 unchanged", bufA[i], original[i].left, original[i].right );
        expect_pair( "SND-OQ-6 idsp_room=29 vs 0 identical", bufB[i], bufA[i].left, bufA[i].right );
    }
}

// ===========================================================================
// 3. select_room — the T_Main-computable selection half (s_dsp.c:806,809).
// ===========================================================================

static void test_select_room_matrix()
{
    CHECK_EQ( select_room( /*waterlevel*/ 0, /*room_type*/ 5.0f, /*waterroom_type*/ 14.0f ), 5 );
    CHECK_EQ( select_room( 2, 5.0f, 14.0f ), 5 );  // waterlevel==2 still uses room_type (">2" not ">=2")
    CHECK_EQ( select_room( 3, 5.0f, 14.0f ), 14 ); // waterlevel>2 switches to waterroom_type
    CHECK_EQ( select_room( 3, 5.0f, 29.0f ), 29 ); // inclusive clamp max reachable (SND-OQ-6)
    CHECK_EQ( select_room( 3, 5.0f, 30.0f ), 29 ); // clamped down from 30
    CHECK_EQ( select_room( 3, 5.0f, -5.0f ), 0 );  // negative clamps to 0
    CHECK_EQ( select_room( 0, -1.0f, 14.0f ), 0 ); // negative room_type clamps to 0
    CHECK_EQ( select_room( 0, 0.0f, 14.0f ), 0 );
}

// ===========================================================================
// 4. Fresh-instance passthrough (check_presets' early-return, s_dsp.c:817-818,
//    reached on every totally-untouched RoomDsp: idsp_room==room_typeprev==0).
// ===========================================================================

static void test_fresh_instance_is_passthrough()
{
    ::xash::memory::ScopedPool pool( "test_dsp_fresh" );
    RoomDsp dsp( pool.handle() );

    std::array<portable_samplepair_t, 3> buf { { { 111, 222 }, { -333, 444 }, { 5, -5 } } };
    const std::array<portable_samplepair_t, 3> original = buf;

    dsp.process( buf.data(), static_cast<int>( buf.size() ) );

    for( std::size_t i = 0; i < buf.size(); i++ )
        expect_pair( "fresh instance passthrough", buf[i], original[i].left, original[i].right );

    CHECK_EQ( dsp.room_index(), 0 );
}

static void test_room_off_and_zero_samples_are_noops()
{
    ::xash::memory::ScopedPool pool( "test_dsp_off" );
    RoomDsp dsp( pool.handle() );
    dsp.set_room_off( true );
    dsp.set_room_type( 5.0f ); // would otherwise activate a preset

    std::array<portable_samplepair_t, 2> buf { { { 900, -900 }, { 1, 2 } } };
    const std::array<portable_samplepair_t, 2> original = buf;
    dsp.process( buf.data(), static_cast<int>( buf.size() ) );
    for( std::size_t i = 0; i < buf.size(); i++ )
        expect_pair( "room_off gate", buf[i], original[i].left, original[i].right );

    dsp.set_room_off( false );
    std::array<portable_samplepair_t, 1> buf0 { { { 42, 42 } } };
    dsp.process( buf0.data(), 0 ); // num_samples==0
    expect_pair( "zero num_samples", buf0[0], 42, 42 );
}

// ===========================================================================
// 5. Pass-math hand-derived vectors.
// ===========================================================================

// RVB_DoAMod, lowpass-only path (room_mod==0 avoids the RNG-adjacent, in-
// practice-dead sxamod state entirely — see dsp.cpp's random_long comment).
// Two samples, by hand, tracing rgsxlp_'s exact (non-obvious) update order
// verbatim from s_dsp.c:730-747.
static void test_pass_math_amod_lowpass()
{
    ::xash::memory::ScopedPool pool( "test_dsp_amod" );
    RoomDsp dsp( pool.handle() );
    dsp.set_room_lp( 1.0f );
    dsp.set_room_mod( 0.0f );

    // Sample 1: input {100,200}, rgsxlp_ starts all-zero.
    //   res.left  = (0+0+0+0+0)+100 = 100; >>2 = 25
    //   res.right = (0+0+0+0+0)+200 = 200; >>2 = 50
    std::array<portable_samplepair_t, 1> s1 { { { 100, 200 } } };
    dsp.process( s1.data(), 1 );
    expect_pair( "amod lowpass sample1", s1[0], 25, 50 );

    // Sample 2: input {40,60}; rgsxlp_ after sample1 = [0,0,0,100,0,0,0,0,200,200]
    //   res.left  = rgsxlp[0..4]+40 = (0+0+0+100+0)+40 = 140; >>2 = 35
    //   res.right = rgsxlp[5..9]+60 = (0+0+0+200+200)+60 = 460; >>2 = 115
    std::array<portable_samplepair_t, 1> s2 { { { 40, 60 } } };
    dsp.process( s2.data(), 1 );
    expect_pair( "amod lowpass sample2", s2[0], 35, 115 );
}

// Helper: drive a RoomDsp to a chosen nonzero idsp_room via room_type (so
// check_presets' early-return no longer gates the RVB_*/DLY_* re-check
// functions), applying `preset_index`'s table row, then let the caller
// override individual fields via setters — a second "settle" process() call
// (1 zero sample; harmless, since a uniformly-zero buffer touches nothing
// observable) commits those overrides' dirty-flag-gated re-init before the
// real hand-derived test sample runs.
static void settle( RoomDsp &dsp, float room_type )
{
    dsp.set_room_type( room_type );
    std::array<portable_samplepair_t, 1> zero { { { 0, 0 } } };
    dsp.process( zero.data(), 1 );
}

// RVB_DoReverb + RVB_DoReverbForOneDly, both dsp_coeff_table branches
// (s_dsp.c:687-710's /6 vs (11*x)>>6 gain divergence). room_size=0.05,
// rvblp=0 (lowpass off, simpler math), refl=0 (feedback irrelevant — the
// delay line is freshly zeroed so `delay` reads 0 on the first live sample
// regardless of feedback). Other passes forced off.
static void test_pass_math_reverb_both_coeff_tables()
{
    auto run = [&]( float coeff_table ) -> portable_samplepair_t {
        ::xash::memory::ScopedPool pool( "test_dsp_reverb" );
        RoomDsp dsp( pool.handle() );
        dsp.set_dsp_coeff_table( coeff_table );
        settle( dsp, 5.0f ); // any nonzero room to escape the idsp_room==0 early-return
        dsp.set_room_size( 0.05f );
        dsp.set_room_rvblp( 0.0f );
        dsp.set_room_refl( 0.0f );
        dsp.set_room_delay( 0.0f );
        dsp.set_room_left( 0.0f );
        dsp.set_room_mod( 0.0f );
        dsp.set_room_lp( 0.0f );
        std::array<portable_samplepair_t, 1> zero { { { 0, 0 } } };
        dsp.process( zero.data(), 1 ); // commit the overrides (settle2)

        std::array<portable_samplepair_t, 1> real { { { 800, 400 } } };
        dsp.process( real.data(), 1 );
        return real[0];
    };

    // Hand derivation (both dly1/dly2 read delay==0 on this fresh line):
    //   vlr = (800+400)>>1 = 600
    //   dly1: lp off -> valt=vlr=600; voutm(dly1)=600. dly2: identical -> 600.
    //   voutm_total = 1200.
    //   release: (11*1200)>>6 = 13200>>6 = 206  -> {800+206,400+206} = {1006,606}
    //   alpha:   1200/6 = 200                   -> {800+200,400+200} = {1000,600}
    expect_pair( "reverb release coeff", run( 0.0f ), 1006, 606 );
    expect_pair( "reverb alpha coeff", run( 1.0f ), 1000, 600 );

    // S9.5 parity-audit pin: legacy coerces the cvar with TWO different
    // rules — `switch((int)value)` picks the TABLE (s_dsp.c:787) but the
    // reverb GAIN uses an exact-float `== 1.0f` compare (s_dsp.c:703).  A
    // fractional 1.5 therefore selects the ALPHA table with the RELEASE
    // gain: same voutm=1200 -> (11*1200)>>6 = 206, NOT 1200/6 = 200.
    expect_pair( "reverb fractional 1.5 coercion asymmetry", run( 1.5f ), 1006, 606 );
}

// DLY_DoDelay (mono), via preset[1] applied under BOTH coeff tables — release
// row1 has room_dlylp==0.0 (lowpass OFF), alpha row1 has room_dlylp==2.0
// (lowpass ON): this single test therefore also pins the dsp_coeff_table
// TABLE-POINTER switch (distinct from the reverb test's GAIN-FORMULA switch
// above) and both branches of DLY_DoDelay's `if(dly->lp)`.
static void test_pass_math_mono_delay_both_coeff_tables()
{
    auto run = [&]( float coeff_table ) -> portable_samplepair_t {
        ::xash::memory::ScopedPool pool( "test_dsp_monodelay" );
        RoomDsp dsp( pool.handle() );
        dsp.set_dsp_coeff_table( coeff_table );
        settle( dsp, 1.0f ); // preset[1] "generic" — differs materially between tables
        dsp.set_room_left( 0.0f ); // preset[1] release/alpha both set a stereo-delay left
                                    // value (0.01 / 0.0) — force off either way to isolate
                                    // the mono delay pass cleanly.
        std::array<portable_samplepair_t, 1> zero { { { 0, 0 } } };
        dsp.process( zero.data(), 1 ); // commit the override

        std::array<portable_samplepair_t, 1> real { { { 1000, 2000 } } };
        dsp.process( real.data(), 1 );
        return real[0];
    };

    // release preset[1]: delay=0.065, feedback=0.1, dlylp=0.0 (lowpass OFF).
    //   delaysamples = (int)(0.065*11025)<<2 = 716<<2 = 2864 (unused for sample1 math)
    //   delay read = 0 (fresh line)
    //   val = ((1000+2000)>>1) + (delayfeedback*0>>8) = 1500; clip16 -> 1500 (no clamp)
    //   lp off -> val unchanged = 1500
    //   val >>= 2 -> 375
    //   out = {clip16(1000+375), clip16(2000+375)} = {1375, 2375}
    expect_pair( "mono delay release (lp off)", run( 0 ), 1375, 2375 );

    // alpha preset[1]: delay=0.08, feedback=0.8, dlylp=2.0 (lowpass ON).
    //   delay read = 0 (fresh line)
    //   val = 1500 + (204*0>>8) = 1500; clip16 -> 1500
    //   lp on -> val = (lp0(0)+lp1(0)+1500)/3 = 500
    //   val >>= 2 -> 125
    //   out = {clip16(1000+125), clip16(2000+125)} = {1125, 2125}
    expect_pair( "mono delay alpha (lp on)", run( 1 ), 1125, 2125 );

    // Table side of the S9.5 fractional-coercion pin (gain side lives in the
    // reverb test above): `switch((int)value)` truncates, so 1.5 selects the
    // ALPHA preset table — identical output to the run(1) case.
    expect_pair( "mono delay fractional 1.5 -> alpha table", run( 1.5f ), 1125, 2125 );
}

// DLY_DoStereoDelay — first activation reads a freshly-zeroed line (delay==0,
// xfade==0 so no crossfade blend), so the output is SILENCE on the left
// channel while the current input is written into the line for future taps;
// the right channel is never touched by this pass at all.
static void test_pass_math_stereo_delay()
{
    ::xash::memory::ScopedPool pool( "test_dsp_stereodelay" );
    RoomDsp dsp( pool.handle() );
    // Settle at preset[20] ("outside"), NOT preset[5]: room 20's room_left is
    // already 0.0, so the stereo delay stays INACTIVE after settling — the
    // override below then activates it FRESH (the `!buffer.active()` branch
    // of dly_check_new_stereo_delay_val, which sets delaysamples==samples
    // BEFORE the xfade-trigger check, so xfade is guaranteed 0). Settling at
    // a room whose OWN preset already has a nonzero room_left (e.g. room 5's
    // 0.01) would instead hit the "already active, length changed" branch,
    // which sets xfade=128 and invalidates the "fresh line" derivation below.
    settle( dsp, 20.0f );
    dsp.set_room_size( 0.0f );
    dsp.set_room_delay( 0.0f );
    dsp.set_room_mod( 0.0f );
    dsp.set_room_lp( 0.0f );
    dsp.set_room_left( 0.02f );
    std::array<portable_samplepair_t, 1> zero { { { 0, 0 } } };
    dsp.process( zero.data(), 1 ); // commit

    std::array<portable_samplepair_t, 1> real { { { 500, 999 } } };
    dsp.process( real.data(), 1 );
    // delay = lpdelayline[idelayoutput] = 0 (fresh); xfade==0 (mod is always 0
    // for stereodly, so the crossfade-trigger branch never fires) -> the
    // crossfade blend is skipped entirely -> paint->left = delay = 0.
    // paint->right is untouched by this pass (stereo delay only writes left).
    expect_pair( "stereo delay first activation", real[0], 0, 999 );
}

// ===========================================================================
// 6. SX_ClearState (s_dsp.c:868-877) — resets room_type to 0 and forces a
//    reload; does NOT free the delay lines itself (torn down lazily by the
//    NEXT process() call noticing idsp_room changed back to 0/"off").
// ===========================================================================

static void test_clear_state_lazy_teardown()
{
    ::xash::memory::ScopedPool pool( "test_dsp_clear" );
    RoomDsp dsp( pool.handle() );
    settle( dsp, 1.0f ); // activates preset[1]'s mono delay (release table)
    dsp.set_room_left( 0.0f );
    std::array<portable_samplepair_t, 1> zero { { { 0, 0 } } };
    dsp.process( zero.data(), 1 );

    // Confirm the delay is genuinely active before clearing (same vector as
    // test_pass_math_mono_delay_both_coeff_tables' release case).
    {
        std::array<portable_samplepair_t, 1> probe { { { 1000, 2000 } } };
        dsp.process( probe.data(), 1 );
        expect_pair( "pre-clear delay active", probe[0], 1375, 2375 );
    }

    dsp.clear_state();
    CHECK_EQ( dsp.room_index(), 1 ); // NOT yet re-evaluated — clear_state() only sets room_type_=0;
                                       // idsp_room_ stays stale (1) until the next process() call

    // clear_state() does not itself run check_presets(); the reset is
    // observed on the NEXT process() call. One throwaway call lets
    // idsp_room settle back to 0 and the mono delay get freed.
    dsp.process( zero.data(), 1 );
    CHECK_EQ( dsp.room_index(), 0 );

    std::array<portable_samplepair_t, 1> after { { { 1000, 2000 } } };
    const auto before = after;
    dsp.process( after.data(), 1 );
    expect_pair( "post-clear passthrough (delay freed)", after[0], before[0].left, before[0].right );
}

// ===========================================================================
// 7. Full RoomFX block through the S9.3 paint pipeline — a byte-pinned
//    roombuffer transform through Mixer::paint_channels with a known preset.
// ===========================================================================

static AudioData make_mono8_one( int sample )
{
    AudioData a;
    a.rate     = 44100;
    a.width    = 1;
    a.channels = 1;
    a.samples  = 1;
    a.type     = AudioFormatType::Pcm;
    a.flags    = AudioFlags::None;
    a.buffer.resize( 1 );
    a.buffer[0] = std::byte{ static_cast<std::uint8_t>( static_cast<std::int8_t>( sample ) ) };
    return a;
}

static void test_full_paint_pipeline_with_known_preset()
{
    ::xash::memory::ScopedPool pool( "test_dsp_pipeline" );
    RoomDsp dsp( pool.handle() );
    settle( dsp, 1.0f ); // preset[1] release: delay=0.065, feedback=0.1, dlylp=0 (off), left=0.01
    dsp.set_room_left( 0.0f ); // isolate mono delay (kill the stereo-delay tap preset[1] also sets)
    std::array<portable_samplepair_t, 1> zero { { { 0, 0 } } };
    dsp.process( zero.data(), 1 ); // commit the override

    AudioData src = make_mono8_one( 100 );

    Mixer m;
    m.set_out_rate( 44100 );
    m.set_room_dsp( &dsp );
    m.set_painted_time( 0 );

    MixChannel ch {};
    ch.source     = &src;
    ch.leftvol    = 200;
    ch.rightvol   = 200;
    ch.base_pitch = 100.0;
    m.channels().push_back( ch );

    MixGateSnapshot gate {};
    gate.in_game       = true;
    gate.single_player = false; // channel always mixes (test_sound_mixer.cpp's own isolation pattern)
    gate.in_menu       = false; // DSP step runs (not skipped)

    auto out = m.paint_channels( 1, gate, /*master_volume*/ 1.0f, /*pitch_mult*/ 1.0 );

    // Hand derivation:
    //   channel mix (mix_kernel<8,1,Flat>, shift 0): 100*200 = 20000 both channels
    //     -> roombuffer[0] = {20000,20000}
    //   RoomDsp::process (mono delay, fresh line, delay==0 read):
    //     val = ((20000+20000)>>1) + (delayfeedback*0>>8) = 20000; clip16 -> 20000
    //     lp off -> unchanged; val>>=2 -> 5000
    //     roombuffer[0] = {clip16(20000+5000), clip16(20000+5000)} = {25000,25000}
    //   gain-mix (unity, master_volume==1.0 -> gain==256): paintbuffer += roombuffer
    //     -> paintbuffer[0] = {25000,25000}
    //   transfer (CLIP16, in range) -> int16 {25000,25000}
    REQUIRE( out.size() == 2 );
    CHECK_EQ( out[0], static_cast<std::int16_t>( 25000 ) );
    CHECK_EQ( out[1], static_cast<std::int16_t>( 25000 ) );
}

// ===========================================================================
int main()
{
    RUN_TEST( test_preset_table_release_spot_rows );
    RUN_TEST( test_preset_table_hlalpha052_spot_rows );

    RUN_TEST( test_snd_oq6_sentinel_row_content );
    RUN_TEST( test_snd_oq6_index29_behaves_like_off );

    RUN_TEST( test_select_room_matrix );

    RUN_TEST( test_fresh_instance_is_passthrough );
    RUN_TEST( test_room_off_and_zero_samples_are_noops );

    RUN_TEST( test_pass_math_amod_lowpass );
    RUN_TEST( test_pass_math_reverb_both_coeff_tables );
    RUN_TEST( test_pass_math_mono_delay_both_coeff_tables );
    RUN_TEST( test_pass_math_stereo_delay );

    RUN_TEST( test_clear_state_lazy_teardown );

    RUN_TEST( test_full_paint_pipeline_with_known_preset );

    std::printf( "sound_dsp: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
