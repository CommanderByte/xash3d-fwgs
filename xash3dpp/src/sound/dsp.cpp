// xash3dpp — room-effects DSP implementation (Chunk 9, slice S9.5).
// PARITY-CRITICAL. Legacy reference: engine/client/sound/s_dsp.c (see dsp.hpp
// for the full per-function line map). Every pass reproduces the legacy
// integer math verbatim — the >>8/>>7/>>6 shift constants, the delay-line
// indexing (including the size_t/int mixed-arithmetic quirks in the
// crossfade pointer math), the MONODLY averaging, and the two coefficient
// tables' selection + the reverb gain divergence between them.

#include <xash3dpp/private/sound/dsp.hpp>

#include <cstdint>
#include <cstring>

namespace xash::sound {

namespace {

// DLY_MovePointer (s_dsp.c:322-329) — shared by DLY_DoDelay, DLY_DoStereoDelay
// and RVB_DoReverbForOneDly; stateless w.r.t. RoomDsp, so a free function.
void dly_move_pointer( DelayLine &dly ) noexcept
{
    if( ++dly.idelayinput >= dly.cdelaysamplesmax )
        dly.idelayinput = 0;
    if( ++dly.idelayoutput >= dly.cdelaysamplesmax )
        dly.idelayoutput = 0;
}

// COM_RandomLong stand-in (common/common.c:130). The three legacy call sites
// that use it (RVB_DoAMod's `!sxmod1`/`!sxmod2` branches, RVB_DoReverbForOneDly's
// `!dly->mod` branch, DLY_DoStereoDelay's `dly->mod` branch) are ALL
// unreachable given RoomDsp's fixed sxmod1_/sxmod2_ (350/450 — always
// nonzero, since idsp_dma_speed is hardcoded and never re-queried, s_dsp.c:
// 503 recon note) and the fact that reverb's `mod` field is always set
// nonzero by rvb_set_up_dly while stereodly's `mod` is always forced to 0 by
// dly_check_new_stereo_delay_val (see the member comments in dsp.hpp). A
// bit-exact COM_RandomLong port is therefore NOT load-bearing for parity;
// this stand-in exists only so the (dead) branch structure compiles and
// matches source shape 1:1 for structural fidelity / future-proofing.
[[nodiscard]] int random_long( int lo, int hi ) noexcept
{
    static std::uint32_t state = 0x9E3779B9u;
    state                      = state * 1664525u + 1013904223u;
    if( hi <= lo )
        return lo;
    const std::uint32_t range = static_cast<std::uint32_t>( hi - lo ) + 1u;
    return lo + static_cast<int>( state % range );
}

} // namespace

void PoolIntBuffer::zero() noexcept
{
    if( data_ != nullptr )
        std::memset( data_, 0, size_ * sizeof( int ) );
}

// ===========================================================================
// Lifecycle
// ===========================================================================

RoomDsp::RoomDsp( ::xash::memory::PoolHandle pool ) noexcept : pool_( pool )
{
    // monodly_/reverbdly_[0,1]/stereodly_ default-construct inactive
    // (dly_t's `nulldly` zero-struct assignment, s_dsp.c:213-218). rgsxlp_
    // zero-inits via {} (s_dsp.c:220). sxamod*_/sxmod*_ members already
    // carry SX_Init's literal values (255/350/450) as in-class defaults.
    reload_room_fx(); // SX_ReloadRoomFX() tail call (s_dsp.c:252)
}

// ===========================================================================
// Cvar-equivalent setters
// ===========================================================================

// compliance-allow(thread-assert): the §4.3 cvar-equivalent setters below run
// on whichever thread owns the channel array — T_AudioDecoder while the
// S9.7b topology runs (apply_command() applies a main-polled
// MixConfigSnapshot decoder-side, audio_command.cpp:263-275), T_Main when it
// does not — a CONDITIONAL role per dsp.hpp's file-header rationale, not a
// fixed one. No single per-call assert can express that; it is enforced at
// the entry points that DO know the mode (Sound::* asserts Main, sound.cpp;
// AudioTopology::decoder_step() asserts AudioDecoder, topology.cpp).
void RoomDsp::set_dsp_coeff_table( float table ) noexcept
{
    if( table != dsp_coeff_table_ )
        dsp_coeff_table_dirty_ = true;
    dsp_coeff_table_ = table;
}

// compliance-allow(thread-assert): same conditional-role rationale as
// set_dsp_coeff_table() above (dsp.hpp file header).
void RoomDsp::set_hisound( int quality ) noexcept
{
    if( quality != hisound_ )
        hisound_dirty_ = true;
    hisound_ = quality;
}

// compliance-allow(thread-assert): same conditional-role rationale as
// set_dsp_coeff_table() above (dsp.hpp file header).
void RoomDsp::set_room_size( float v ) noexcept
{
    if( v != room_size_ )
        room_size_dirty_ = true;
    room_size_ = v;
}

// compliance-allow(thread-assert): same conditional-role rationale as
// set_dsp_coeff_table() above (dsp.hpp file header).
void RoomDsp::set_room_delay( float v ) noexcept
{
    if( v != room_delay_ )
        room_delay_dirty_ = true;
    room_delay_ = v;
}

// compliance-allow(thread-assert): same conditional-role rationale as
// set_dsp_coeff_table() above (dsp.hpp file header).
void RoomDsp::set_room_left( float v ) noexcept
{
    if( v != room_left_ )
        room_left_dirty_ = true;
    room_left_ = v;
}

// ===========================================================================
// SX_ReloadRoomFX (s_dsp.c:196-202)
// ===========================================================================

void RoomDsp::reload_room_fx() noexcept
{
    // SetBits(sxste_delay.flags, CHANGED); SetBits(sxdly_delay.flags, CHANGED);
    // — the only 2 of the legacy function's 4 SetBits calls whose target flag
    // is ever READ anywhere in s_dsp.c (see dsp.hpp's dirty-bit comment for
    // why sxrvb_feedback/room_type are intentionally not modelled).
    room_left_dirty_  = true;
    room_delay_dirty_ = true;
}

// ===========================================================================
// apply_preset — the 9 Cvar_DirectSetValue calls (s_dsp.c:824-832)
// ===========================================================================

void RoomDsp::apply_preset( const RoomPreset &preset ) noexcept
{
    // Cvar_DirectSetValue only marks CHANGED if the value actually differs
    // (cvar.c:607-621, Cvar_SanitizeAndSet's string-compare early return) —
    // applied per-field, same order as source.
    room_lp_  = preset.room_lp;
    room_mod_ = preset.room_mod;

    if( preset.room_size != room_size_ )
        room_size_dirty_ = true;
    room_size_ = preset.room_size;

    room_refl_  = preset.room_refl;
    room_rvblp_ = preset.room_rvblp;

    if( preset.room_delay != room_delay_ )
        room_delay_dirty_ = true;
    room_delay_ = preset.room_delay;

    room_feedback_ = preset.room_feedback;
    room_dlylp_    = preset.room_dlylp;

    if( preset.room_left != room_left_ )
        room_left_dirty_ = true;
    room_left_ = preset.room_left;
}

// ===========================================================================
// SX_CheckPresets (s_dsp.c:783-844)
// ===========================================================================

void RoomDsp::check_presets() noexcept
{
    if( dsp_coeff_table_dirty_ )
    {
        switch( static_cast<int>( dsp_coeff_table_ ) ) // legacy (int)value coercion (s_dsp.c:787)
        {
        case 0: // release
            ptable_ = &k_room_presets_release;
            break;
        case 1: // alpha
            ptable_ = &k_room_presets_hlalpha052;
            break;
        default:
            ptable_ = &k_room_presets_release;
            break;
        }

        reload_room_fx();
        room_typeprev_ = -1;

        dsp_coeff_table_dirty_ = false;
    }

    // select_room (T_Main-computable half) folded in here via the stored
    // waterlevel_/room_type_/waterroom_type_ members (S9.6 wiring point).
    idsp_room_ = select_room( waterlevel_, room_type_, waterroom_type_ );

    if( hisound_dirty_ )
    {
        sxhires_       = hisound_;
        hisound_dirty_ = false;
    }

    if( idsp_room_ == room_typeprev_ && idsp_room_ == 0 )
        return;

    if( idsp_room_ != room_typeprev_ )
    {
        apply_preset( ( *ptable_ )[ static_cast<std::size_t>( idsp_room_ ) ] );
    }

    room_typeprev_ = idsp_room_;

    rvb_check_new_reverb_val();
    dly_check_new_delay_val();
    dly_check_new_stereo_delay_val();

    room_size_dirty_  = false;
    room_delay_dirty_ = false;
    room_left_dirty_  = false;
}

// ===========================================================================
// DLY_Init (s_dsp.c:296-313)
// ===========================================================================

void RoomDsp::dly_init( DelayLine &dly, float delay_seconds ) noexcept
{
    dly.buffer.reset(); // DLY_Free(cur) first (s_dsp.c:298)

    const int computed = static_cast<int>( delay_seconds * static_cast<float>( k_idsp_dma_speed ) ) << sxhires_;
    dly.cdelaysamplesmax = static_cast<std::size_t>( computed ) + 1;

    if( !dly.buffer.allocate( pool_, dly.cdelaysamplesmax ) )
        return; // OOM — dly stays inactive

    dly.xfade      = 0;
    dly.mod        = 0;
    dly.modcur     = 0;
    dly.lp         = 1;
    dly.lp0 = dly.lp1 = dly.lp2 = 0;

    dly.idelayinput = 0;
    // cdelaysamplesmax - delaysamples: size_t subtraction, matching legacy's
    // exact arithmetic (delaysamples must already be set by the caller —
    // DLY_Init's own precondition note, s_dsp.c:312).
    dly.idelayoutput = dly.cdelaysamplesmax - static_cast<std::size_t>( dly.delaysamples );
}

// ===========================================================================
// RVB_SetUpDly (s_dsp.c:546-573)
// ===========================================================================

void RoomDsp::rvb_set_up_dly( DelayLine &dly, float delay, int kmod ) noexcept
{
    delay              = delay < k_max_reverb_delay ? delay : k_max_reverb_delay; // Q_min
    const int samples  = static_cast<int>( delay * static_cast<float>( k_idsp_dma_speed ) ) << sxhires_;

    if( !dly.buffer.active() )
    {
        dly.delaysamples = samples;
        dly_init( dly, k_max_reverb_delay );
    }

    // (kmod * idsp_dma_speed / SOUND_11k) << sxhires — idsp_dma_speed ==
    // SOUND_11k always (k_idsp_dma_speed == k_sound_11k), so this ratio is
    // exactly 1; kept exactly as written in source for structural fidelity
    // (the deep-dive flags this as "dead scaling generality").
    dly.modcur = dly.mod = ( kmod * k_idsp_dma_speed / k_sound_11k ) << sxhires_;

    // set up crossfade, if delay has changed. NOTE (preserved quirk, not a
    // bug fix): dly.delaysamples is only ever updated to `samples` in the
    // `!active()` branch above — on an ALREADY-active line this branch keeps
    // re-triggering every call where samples != the STALE delaysamples,
    // exactly matching s_dsp.c:562-568's own omission.
    if( dly.delaysamples != samples )
    {
        dly.idelayoutputxf = static_cast<int>( dly.idelayinput - static_cast<std::size_t>( samples ) );
        if( dly.idelayoutputxf < 0 )
            dly.idelayoutputxf += static_cast<int>( dly.cdelaysamplesmax );
        dly.xfade = k_reverb_xfade;
    }

    if( dly.delaysamples == 0 )
        dly.buffer.reset();
}

// ===========================================================================
// RVB_CheckNewReverbVal (s_dsp.c:582-604)
// ===========================================================================

void RoomDsp::rvb_check_new_reverb_val() noexcept
{
    DelayLine &dly1        = reverbdly_[0];
    DelayLine &dly2        = reverbdly_[1];
    const float delay      = room_size_; // sxrvb_size.value

    if( room_size_dirty_ )
    {
        if( delay == 0.0f )
        {
            dly1.buffer.reset();
            dly2.buffer.reset();
        }
        else
        {
            rvb_set_up_dly( dly1, delay, 500 );
            rvb_set_up_dly( dly2, delay * 0.71f, 700 );
        }
    }

    dly1.lp  = dly2.lp  = static_cast<int>( room_rvblp_ );
    dly1.delayfeedback = dly2.delayfeedback = static_cast<int>( 255.0f * room_refl_ );
}

// ===========================================================================
// DLY_CheckNewDelayVal (mono, s_dsp.c:450-487)
// ===========================================================================

void RoomDsp::dly_check_new_delay_val() noexcept
{
    DelayLine &dly     = monodly_;
    float      delay   = room_delay_; // sxdly_delay.value

    if( room_delay_dirty_ )
    {
        if( delay == 0.0f )
        {
            dly.buffer.reset();
        }
        else
        {
            delay             = delay < k_max_mono_delay ? delay : k_max_mono_delay; // Q_min
            dly.delaysamples  = static_cast<int>( delay * static_cast<float>( k_idsp_dma_speed ) ) << sxhires_;

            if( !dly.buffer.active() )
                dly_init( dly, k_max_mono_delay );

            if( dly.buffer.active() )
            {
                dly.buffer.zero();
                dly.lp0 = dly.lp1 = dly.lp2 = 0;
            }

            dly.idelayinput  = 0;
            dly.idelayoutput = dly.cdelaysamplesmax - static_cast<std::size_t>( dly.delaysamples );

            if( dly.delaysamples == 0 )
                dly.buffer.reset();
        }
    }

    dly.lp            = static_cast<int>( room_dlylp_ );          // sxdly_lp.value -> int
    dly.delayfeedback = static_cast<int>( 255.0f * room_feedback_ ); // 255 * sxdly_feedback.value
}

// ===========================================================================
// DLY_CheckNewStereoDelayVal (s_dsp.c:338-377)
// ===========================================================================

void RoomDsp::dly_check_new_stereo_delay_val() noexcept
{
    DelayLine &dly   = stereodly_;
    float      delay = room_left_; // sxste_delay.value

    if( !room_left_dirty_ )
        return;

    if( delay == 0.0f )
    {
        dly.buffer.reset();
    }
    else
    {
        delay               = delay < k_max_stereo_delay ? delay : k_max_stereo_delay; // Q_min
        const int samples   = static_cast<int>( delay * static_cast<float>( k_idsp_dma_speed ) ) << sxhires_;

        if( !dly.buffer.active() )
        {
            dly.delaysamples = samples;
            dly_init( dly, k_max_stereo_delay );
        }

        // NOTE (preserved quirk, matches rvb_set_up_dly's comment above):
        // dly.delaysamples is not updated to `samples` here either.
        if( dly.delaysamples != samples )
        {
            dly.xfade           = 128;
            dly.idelayoutputxf  = static_cast<int>( dly.idelayinput - static_cast<std::size_t>( samples ) );
            if( dly.idelayoutputxf < 0 )
                dly.idelayoutputxf += static_cast<int>( dly.cdelaysamplesmax );
        }

        dly.modcur = dly.mod = 0;

        if( dly.delaysamples == 0 )
            dly.buffer.reset();
    }
}

// ===========================================================================
// RVB_DoAMod (s_dsp.c:719-781)
// ===========================================================================

void RoomDsp::rvb_do_amod( portable_samplepair_t *paint, int count ) noexcept
{
    if( room_lp_ == 0.0f && room_mod_ == 0.0f )
        return;

    for( ; count; count--, paint++ )
    {
        portable_samplepair_t res = *paint;

        if( room_lp_ != 0.0f )
        {
            res.left  = rgsxlp_[0] + rgsxlp_[1] + rgsxlp_[2] + rgsxlp_[3] + rgsxlp_[4] + res.left;
            res.right = rgsxlp_[5] + rgsxlp_[6] + rgsxlp_[7] + rgsxlp_[8] + rgsxlp_[9] + res.right;

            res.left >>= 2;
            res.right >>= 2;

            // Transcribed VERBATIM including the apparent self-overwrite of
            // rgsxlp_[4]/[8] (s_dsp.c:736-747) — this is NOT a clean 5-tap
            // FIFO per channel; index 4's original value is discarded and
            // index 3 ends up holding the NEW left sample (see dsp.hpp's
            // file header for why this must not be "cleaned up").
            rgsxlp_[4] = paint->left;
            rgsxlp_[9] = paint->right;

            rgsxlp_[0] = rgsxlp_[1];
            rgsxlp_[1] = rgsxlp_[2];
            rgsxlp_[2] = rgsxlp_[3];
            rgsxlp_[3] = rgsxlp_[4];
            rgsxlp_[4] = rgsxlp_[5];
            rgsxlp_[5] = rgsxlp_[6];
            rgsxlp_[6] = rgsxlp_[7];
            rgsxlp_[7] = rgsxlp_[8];
            rgsxlp_[8] = rgsxlp_[9];
        }

        if( room_mod_ != 0.0f )
        {
            if( --sxmod1cur_ < 0 )
                sxmod1cur_ = sxmod1_;

            if( !sxmod1_ )
                sxamodlt_ = random_long( 32, 255 );

            if( --sxmod2cur_ < 0 )
                sxmod2cur_ = sxmod2_;

            if( !sxmod2_ )
                sxamodrt_ = random_long( 32, 255 );

            res.left  = ( sxamodl_ * res.left ) >> 8;
            res.right = ( sxamodr_ * res.right ) >> 8;

            if( sxamodl_ < sxamodlt_ )
                sxamodl_++;
            else if( sxamodl_ > sxamodlt_ )
                sxamodl_--;

            if( sxamodr_ < sxamodrt_ )
                sxamodr_++;
            else if( sxamodr_ > sxamodrt_ )
                sxamodr_--;
        }

        paint->left  = static_cast<int>( clip16( res.left ) );
        paint->right = static_cast<int>( clip16( res.right ) );
    }
}

// ===========================================================================
// RVB_DoReverbForOneDly (s_dsp.c:613-678)
// ===========================================================================

int RoomDsp::rvb_do_reverb_for_one_dly( DelayLine &dly, int vlr, const portable_samplepair_t &samplepair ) noexcept
{
    int voutm = 0;

    if( --dly.modcur < 0 )
        dly.modcur = dly.mod;

    int delay = dly.buffer[ dly.idelayoutput ];

    if( dly.xfade || delay || samplepair.left || samplepair.right )
    {
        // modulate delay rate (DEAD in practice — dly.mod is always nonzero
        // for reverb; see the random_long comment above).
        if( !dly.mod )
        {
            dly.idelayoutputxf = static_cast<int>( dly.idelayoutput ) + ( ( random_long( 0, 255 ) * delay ) >> 9 );
            dly.idelayoutputxf %= static_cast<int>( dly.cdelaysamplesmax );
            dly.xfade = k_reverb_xfade;
        }

        if( dly.xfade )
        {
            const int samplexf =
                ( dly.buffer[ static_cast<std::size_t>( dly.idelayoutputxf ) ] * ( k_reverb_xfade - dly.xfade ) ) / k_reverb_xfade;
            delay = ( ( delay * dly.xfade ) / k_reverb_xfade ) + samplexf;

            if( ++dly.idelayoutputxf >= static_cast<int>( dly.cdelaysamplesmax ) )
                dly.idelayoutputxf = 0;

            if( --dly.xfade == 0 )
                dly.idelayoutput = static_cast<std::size_t>( dly.idelayoutputxf );
        }

        int val;
        if( delay )
        {
            val = vlr + ( ( dly.delayfeedback * delay ) >> 8 );
            val = static_cast<int>( clip16( val ) );
        }
        else
        {
            val = vlr;
        }

        int valt;
        if( dly.lp )
        {
            valt    = ( dly.lp0 + val ) >> 1;
            dly.lp0 = val;
        }
        else
        {
            valt = val;
        }

        voutm                          = valt;
        dly.buffer[ dly.idelayinput ]  = valt;
    }
    else
    {
        voutm                         = 0;
        dly.buffer[ dly.idelayinput ] = 0;
        dly.lp0                       = 0;
    }

    dly_move_pointer( dly );

    return voutm;
}

// ===========================================================================
// RVB_DoReverb (s_dsp.c:687-710)
// ===========================================================================

void RoomDsp::rvb_do_reverb( portable_samplepair_t *paint, int count ) noexcept
{
    DelayLine &dly1 = reverbdly_[0];
    DelayLine &dly2 = reverbdly_[1];

    if( !dly1.buffer.active() )
        return;

    for( ; count; count--, paint++ )
    {
        const int vlr = ( paint->left + paint->right ) >> 1;

        int voutm  = rvb_do_reverb_for_one_dly( dly1, vlr, *paint );
        voutm     += rvb_do_reverb_for_one_dly( dly2, vlr, *paint );

        if( dsp_coeff_table_ == 1.0f ) // legacy EXACT-float compare (s_dsp.c:703) — deliberately
                                       // NOT the (int) table coercion; see the field comment
            voutm /= 6; // alpha
        else
            voutm = ( 11 * voutm ) >> 6;

        paint->left  = static_cast<int>( clip16( paint->left + voutm ) );
        paint->right = static_cast<int>( clip16( paint->right + voutm ) );
    }
}

// ===========================================================================
// DLY_DoDelay (mono, s_dsp.c:496-537)
// ===========================================================================

void RoomDsp::dly_do_delay( portable_samplepair_t *paint, int count ) noexcept
{
    DelayLine &dly = monodly_;

    if( !dly.buffer.active() || count == 0 )
        return;

    for( ; count; count--, paint++ )
    {
        const int delay = dly.buffer[ dly.idelayoutput ];

        if( delay || paint->left || paint->right )
        {
            int val = ( ( paint->left + paint->right ) >> 1 ) + ( ( dly.delayfeedback * delay ) >> 8 );
            val     = static_cast<int>( clip16( val ) );

            if( dly.lp ) // lowpass
            {
                val     = ( dly.lp0 + dly.lp1 + val ) / 3;
                dly.lp0 = dly.lp1;
                dly.lp1 = val;
            }

            dly.buffer[ dly.idelayinput ] = val;

            val >>= 2;

            paint->left  = static_cast<int>( clip16( paint->left + val ) );
            paint->right = static_cast<int>( clip16( paint->right + val ) );
        }
        else
        {
            dly.buffer[ dly.idelayinput ] = 0;
            dly.lp0 = dly.lp1 = dly.lp2 = 0;
        }

        dly_move_pointer( dly );
    }
}

// ===========================================================================
// DLY_DoStereoDelay (s_dsp.c:386-441)
// ===========================================================================

void RoomDsp::dly_do_stereo_delay( portable_samplepair_t *paint, int count ) noexcept
{
    DelayLine &dly = stereodly_;

    if( !dly.buffer.active() )
        return;

    for( ; count; count--, paint++ )
    {
        if( dly.mod && --dly.modcur < 0 )
            dly.modcur = dly.mod;

        int delay = dly.buffer[ dly.idelayoutput ];

        // process only if crossfading, active left value or delayline
        if( delay || paint->left || dly.xfade )
        {
            // set up new crossfade (DEAD in practice — dly.mod is always 0
            // for stereodly; see the random_long comment above).
            if( !dly.xfade && !dly.modcur && dly.mod )
            {
                dly.idelayoutputxf =
                    static_cast<int>( dly.idelayoutput ) + ( ( random_long( 0, 255 ) * dly.delaysamples ) >> 9 );
                dly.xfade = 128;
            }

            dly.idelayoutputxf %= static_cast<int>( dly.cdelaysamplesmax );

            if( dly.xfade )
            {
                const int samplexf = ( dly.buffer[ static_cast<std::size_t>( dly.idelayoutputxf ) ] * ( 128 - dly.xfade ) ) >> 7;
                delay               = samplexf + ( ( delay * dly.xfade ) >> 7 );

                if( ++dly.idelayoutputxf >= static_cast<int>( dly.cdelaysamplesmax ) )
                    dly.idelayoutputxf = 0;

                if( --dly.xfade == 0 )
                    dly.idelayoutput = static_cast<std::size_t>( dly.idelayoutputxf );
            }

            // save left value to delay line
            dly.buffer[ dly.idelayinput ] = static_cast<int>( clip16( paint->left ) );

            // paint new delay value (NOT re-clamped — matches source exactly)
            paint->left = delay;
        }
        else
        {
            dly.buffer[ dly.idelayinput ] = 0;
        }

        dly_move_pointer( dly );
    }
}

// ===========================================================================
// SX_RoomFX (s_dsp.c:846-864)
// ===========================================================================

// compliance-allow(thread-assert): called from Mixer::paint_channels(), which
// itself asserts ThreadRole::AudioDecoder while the S9.7b topology runs — but
// paint_channels() also runs (unasserted at this depth) when the topology is
// not running, so process() is confined to whichever thread owns the channel
// array at the time (T_AudioDecoder or T_Main), same conditional role as the
// setters above (dsp.hpp file header). No per-call assert here can encode
// that; the mode is only known at Mixer::paint_channels()/Sound::* entry.
void RoomDsp::process( portable_samplepair_t *roombuffer, int num_samples ) noexcept
{
    if( room_off_ || num_samples == 0 )
        return;

    check_presets();

    rvb_do_amod( roombuffer, num_samples );
    rvb_do_reverb( roombuffer, num_samples );
    dly_do_delay( roombuffer, num_samples );
    dly_do_stereo_delay( roombuffer, num_samples );
}

// ===========================================================================
// SX_ClearState (s_dsp.c:868-877)
// ===========================================================================

void RoomDsp::clear_state() noexcept
{
    // Cvar_DirectSet(&room_type, "0") — room_type's own CHANGED flag has no
    // reader anywhere in s_dsp.c (dsp.hpp's dirty-bit comment), so no
    // edge-tracking is needed for this write; the actual teardown happens
    // lazily on the NEXT process() call once it notices idsp_room changed
    // (matches the boundary doc: "does NOT free delay lines itself").
    room_type_ = 0.0f;
    reload_room_fx();
}

// ===========================================================================
// SX_Profiling_f (s_dsp.c:879-916), as a callable
// ===========================================================================

RoomDsp::ProfilingResult RoomDsp::profile( int calls, std::optional<float> room_type_override,
                                           double ( *now_seconds )() noexcept ) noexcept
{
    std::array<portable_samplepair_t, 512> testbuffer {};

    for( auto &s : testbuffer )
    {
        s.left  = random_long( 0, 3000 );
        s.right = random_long( 0, 3000 );
    }

    const float old_room_type = room_type_;

    if( room_type_override.has_value() )
    {
        set_room_type( *room_type_override );
        reload_room_fx();
        check_presets(); // "we just need idsp_room immediately, for message below"
    }

    ProfilingResult result;
    result.room_type_for_message = idsp_room_;

    const double start = now_seconds();
    for( int i = 0; i < calls; i++ )
        process( testbuffer.data(), static_cast<int>( testbuffer.size() ) );
    const double end = now_seconds();

    result.seconds = end - start;

    if( room_type_override.has_value() )
    {
        set_room_type( old_room_type );
        reload_room_fx();
        check_presets();
    }

    return result;
}

} // namespace xash::sound
