// xash3dpp — channel allocation + alter/spatialize implementation
// (Chunk 9, slice S9.6). PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_main.c (see channel_alloc.hpp for
// the per-function line map). Legacy C engine is REFERENCE-ONLY.

#include <xash3dpp/private/sound/channel_alloc.hpp>

#include <xash3dpp/private/sound/registry.hpp> // k_invalid_sound_handle, test_sound_char
#include <xash3dpp/sound/constants.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/core/assert.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace xash::sound {

namespace {

// bound(min, num, max) verbatim (public/xash3d_mathlib.h:141) — duplicated
// locally per the mixer.cpp/vox.cpp/dsp.cpp precedent.
[[nodiscard]] constexpr int bound_int( int lo, int v, int hi ) noexcept
{
    return v >= lo ? ( v < hi ? v : hi ) : lo;
}

[[nodiscard]] constexpr float bound_float( float lo, float v, float hi ) noexcept
{
    return v >= lo ? ( v < hi ? v : hi ) : lo;
}

// VectorCompare (public/xash3d_mathlib.h) — exact float equality.
[[nodiscard]] constexpr bool vec3_equal( const Vec3 &a, const Vec3 &b ) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

} // namespace

void free_channel( MixChannel &ch, VoxSystem *vox ) noexcept
{
    if( ch.is_sentence && vox != nullptr )
        vox->unbind_channel( ch ); // VOX_FreeWord + Mem_Free2(&ch->words) (s_main.c:189-190)

    ch.sfx_handle = k_invalid_sound_handle;
    ch.name.clear();
    ch.flags      = 0;
    ch.forced_end = 0.0;
    ch.sample     = 0.0;
    ch.source     = nullptr;
}

int channel_time_left( const MixChannel &ch, IVoxTimeLeftQuery *vox ) noexcept
{
    if( ( ch.flags & ::xash::abi::k_fl_chan_finished ) != 0 || ch.source == nullptr )
        return 0; // FBitSet(ch->flags,FL_CHAN_FINISHED) || !ch->sfx || !ch->sfx->cache (s_main.c:288)

    if( ch.is_sentence ) // ch->words (s_main.c:291)
    {
        if( ( ch.flags & ::xash::abi::k_fl_chan_sentence_finished ) != 0 )
            return 0; // s_main.c:295-296
        return vox != nullptr ? vox->time_left( ch ) : 0;
    }

    const int samples = static_cast<int>( ch.source->samples );
    const int curpos  = adjust_looped_sample_position( *ch.source, static_cast<int>( ch.sample ),
                                                        ( ch.flags & ::xash::abi::k_fl_chan_use_loop ) != 0 );
    return bound_int( 0, samples - curpos, samples ); // s_main.c:324-328
}

bool snd_stream_is_playing( std::span<const MixChannel> channels, SfxHandle sfx ) noexcept
{
    const std::size_t lo = ::xash::limits::sound_num_ambient_channels;
    const std::size_t hi = std::min<std::size_t>(
        channels.size(), lo + ::xash::limits::sound_num_dynamic_channels );

    for( std::size_t i = lo; i < hi; ++i )
        if( channels[i].sfx_handle == sfx )
            return true;
    return false;
}

DynamicPickResult pick_dynamic_channel( std::span<MixChannel> channels, int listener_entnum, int entnum,
                                        int channel, SfxHandle sfx, IVoxTimeLeftQuery *time_left_source,
                                        VoxSystem *vox ) noexcept
{
    DynamicPickResult result;

    if( channel == k_chan_stream && snd_stream_is_playing( channels, sfx ) )
    {
        result.ignore = true; // s_main.c:354-359
        return result;
    }

    const std::size_t lo = ::xash::limits::sound_num_ambient_channels;
    const std::size_t hi = std::min<std::size_t>(
        channels.size(), lo + ::xash::limits::sound_num_dynamic_channels );

    int first_to_die = -1;
    int life_left    = 0x7fffffff;

    for( std::size_t i = lo; i < hi; ++i )
    {
        MixChannel &ch = channels[i];

        // "Never override a streaming sound ... or any sound on CHAN_VOICE" —
        // the comment names CHAN_VOICE/voice-over-IP too but the CODE only
        // guards CHAN_STREAM (s_main.c:367-368, verified against source).
        if( ch.source != nullptr && ch.entchannel == k_chan_stream )
            continue;

        if( channel != k_chan_auto && ch.entnum == entnum && ( ch.entchannel == channel || channel == -1 ) )
        {
            // always override sound from same entity (s_main.c:370-375)
            first_to_die = static_cast<int>( i );
            break;
        }

        // don't let monster sounds override player sounds (s_main.c:378-379)
        if( ch.source != nullptr && ch.entnum == listener_entnum && entnum != listener_entnum )
            continue;

        const int timeleft = channel_time_left( ch, time_left_source );
        if( timeleft < life_left )
        {
            life_left     = timeleft;
            first_to_die  = static_cast<int>( i );
        }
    }

    if( first_to_die == -1 )
        return result;

    MixChannel &victim = channels[static_cast<std::size_t>( first_to_die )];
    if( victim.source != nullptr )
    {
        // don't restart looping sounds for the same entity (s_main.c:396-409):
        // tests the SOURCE AUDIO's own loop flag (SOUND_LOOPED on sfx->cache),
        // NOT the channel's FL_CHAN_USE_LOOP bit.
        if( has_flag( victim.source->flags, AudioFlags::Looped ) && victim.entnum == entnum &&
            victim.entchannel == channel && victim.sfx_handle == sfx )
        {
            result.ignore = true;
            return result;
        }

        free_channel( victim, vox ); // "be sure and release previous channel if sentence" (s_main.c:411-412)
    }

    result.index = first_to_die;
    return result;
}

int pick_static_channel( std::span<MixChannel> channels, int &total_channels, const Vec3 &pos,
                         SfxHandle sfx ) noexcept
{
    XASH_ASSERT( total_channels <= static_cast<int>( channels.size() ) );

    const int lo = static_cast<int>( ::xash::limits::sound_num_ambient_channels +
                                     ::xash::limits::sound_num_dynamic_channels );

    int i = lo;
    for( ; i < total_channels; ++i )
    {
        const MixChannel &ch = channels[static_cast<std::size_t>( i )];
        if( ch.source == nullptr )
            break; // reuse an empty static sound channel (s_main.c:436-437)
        if( vec3_equal( pos, ch.origin ) && ch.sfx_handle == sfx )
            break; // exact-match reuse (s_main.c:439-440)
    }

    if( i < total_channels )
        return i;

    if( total_channels >= static_cast<int>( channels.size() ) )
        return -1; // "no free channels" (s_main.c:451-455) — caller logs

    const int idx = total_channels;
    ++total_channels;
    return idx;
}

namespace {

[[nodiscard]] bool maybe_alter_channel( MixChannel &ch, int entnum, int entchannel, bool sfx_is_sentence,
                                        SfxHandle sfx, int pitch, int vol, std::uint32_t flags,
                                        VoxSystem *vox ) noexcept
{
    if( ch.source == nullptr )
        return false; // !ch->sfx (s_main.c:466-467)
    if( ch.entnum != entnum )
        return false; // s_main.c:469-470

    if( sfx_is_sentence )
    {
        if( !ch.is_sentence )
            return false; // !ch->words (s_main.c:475-476)
    }
    else
    {
        if( ch.sfx_handle != sfx )
            return false; // ch->sfx != sfx (s_main.c:480-481)
    }

    if( ch.entchannel != entchannel )
        return false; // s_main.c:484-485

    if( ( flags & k_snd_stop ) != 0 )
    {
        free_channel( ch, vox ); // s_main.c:487-491
        return true;
    }

    if( ( flags & k_snd_change_pitch ) != 0 )
        ch.base_pitch = static_cast<double>( pitch ); // s_main.c:493-494
    if( ( flags & k_snd_change_vol ) != 0 )
        ch.master_vol = vol; // s_main.c:496-497

    return true;
}

} // namespace

bool alter_channel( std::span<MixChannel> channels, int total_channels, int entnum, int channel, SfxHandle sfx,
                    std::string_view sfx_name, int vol, int pitch, std::uint32_t flags, VoxSystem *vox ) noexcept
{
    // "assume the entity is only playing one sentence at a time" (s_main.c:
    // 516-520) — a sentence alter/stop matches ANY currently-bound sentence
    // channel on this entnum+entchannel, ignoring the specific sfx identity.
    const bool is_sentence = test_sound_char( sfx_name, '!' );

    const std::size_t lo = ::xash::limits::sound_num_ambient_channels; // ambients excluded (s_main.c:522)
    const std::size_t hi =
        std::min<std::size_t>( channels.size(), static_cast<std::size_t>( std::max( total_channels, 0 ) ) );

    for( std::size_t i = lo; i < hi; ++i )
    {
        if( maybe_alter_channel( channels[i], entnum, channel, is_sentence, sfx, pitch, vol, flags, vox ) )
            return true; // "returns on FIRST match" (s_main.c:526-527)
    }

    return false; // "channel not found" (s_main.c:531)
}

SpatializePan spatialize_pan( int master_vol, float dot, float dist ) noexcept
{
    float scale       = ( 1.0f - dist ) * ( 1.0f + dot );
    const float rvol  = std::round( static_cast<float>( master_vol ) * scale );
    scale             = ( 1.0f - dist ) * ( 1.0f - dot );
    const float lvol  = std::round( static_cast<float>( master_vol ) * scale );

    SpatializePan result;
    result.right_vol = static_cast<int>( bound_float( 0.0f, rvol, 255.0f ) );
    result.left_vol  = static_cast<int>( bound_float( 0.0f, lvol, 255.0f ) );
    return result;
}

void spatialize( MixChannel &ch, int listener_entnum, const Vec3 &listener_origin, const Vec3 &listener_right,
                 bool bugcomp_attn_none, IEntitySpatialProvider *provider ) noexcept
{
    // "anything coming from the view entity will always be full volume" (s_main.c:568-576)
    if( ch.entnum == listener_entnum )
    {
        ch.leftvol  = ch.master_vol;
        ch.rightvol = ch.master_vol;
        return;
    }

    if( ( ch.flags & ::xash::abi::k_fl_chan_static_sound ) == 0 )
    {
        Vec3 origin{};
        if( provider == nullptr || !provider->resolve_origin( ch.entnum, origin ) )
        {
            // "origin is null and entity not exist on client" (s_main.c:582-585)
            ch.leftvol = ch.rightvol = 0;
            return;
        }
        ch.origin = origin;
    }

    // source_vec = vector from listener to sound source (s_main.c:588-591).
    const Vec3  source_vec = ch.origin - listener_origin;
    // VectorLength: DotProduct in float, C sqrt(double), narrowed back to
    // float at the assignment (xash3d_mathlib.h:113) — NOT sqrtf; the
    // double-rounding can differ from a direct sqrtf in the last ULP
    // (S9.6 parity audit F-2; same fidelity class as compute_channel_pitch).
    const float dist = static_cast<float>(
        std::sqrt( static_cast<double>( ::xash::utilities::dot( source_vec, source_vec ) ) ) );
    const Vec3  dir        = dist > 0.0f ? source_vec * ( 1.0f / dist ) : Vec3{};
    float       dot        = ::xash::utilities::dot( listener_right, dir );

    // `bugcomp_attn_none` == FBitSet(host.bugcomp, BUGCOMP_SPATIALIZE_SOUND_WITH_ATTN_NONE)
    // (the OLD buggy pan-with-ATTN_NONE behaviour, enabled == true skips the
    // zeroing below). "don't pan sounds with no attenuation" (s_main.c:597-601).
    if( !bugcomp_attn_none )
    {
        if( ch.dist_mult <= 0.0f )
            dot = 0.0f;
    }

    const SpatializePan pan = spatialize_pan( ch.master_vol, dot, dist * ch.dist_mult ); // s_main.c:604
    ch.leftvol  = pan.left_vol;
    ch.rightvol = pan.right_vol;
}

} // namespace xash::sound
