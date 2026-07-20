// xash3dpp — audio command stream: admission policy + the channel-mutation
// core (Chunk 9, slice S9.7b).  PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_main.c — S_StartSound (:626-740),
// S_AlterChannel (:514-532), S_StopAllSounds (:1465-1492), S_UpdateFrame
// (:1590-1598).  The bodies below are the S9.6 `Sound::start_sound` /
// `stop_sound` / `stop_all_sounds` / `update_frame` tails MOVED here verbatim,
// not rewritten: this file is the single implementation both the
// single-threaded and the threaded paths execute, which is what makes them
// identical by construction (see audio_command.hpp's header comment).
//
// @thread-safety: apply_command()/free_all_channels() run on whichever thread
// owns the channel array — T_AudioDecoder while the topology runs, T_Main
// otherwise.  The role assert lives at the two entry points (Sound::* for
// T_Main, AudioTopology::decoder_step() for T_AudioDecoder), not here: this
// layer is deliberately role-agnostic so ONE body can serve both.

#include <xash3dpp/private/sound/audio_command.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/private/sound/channel_alloc.hpp>
#include <xash3dpp/sound/constants.hpp>

#include <algorithm>
#include <cstring>
#include <optional>
#include <thread>
#include <utility>

namespace xash::sound {

namespace {

constexpr std::string_view k_log_tag = "sound";

} // namespace

// ===========================================================================
// Command construction helpers
// ===========================================================================

// compliance-allow(thread-assert): pure POD writer over a caller-owned, not-yet-
// published AudioCommand — it touches no shared state and is DELIBERATELY
// role-agnostic (T_Main builds commands in both modes; the tests build them from
// whichever role they are exercising).  The real role gates sit at the Sound::*
// entry points and at AudioTopology::submit()/decoder_step().
void set_command_name( AudioCommand &cmd, std::string_view name ) noexcept
{
    const std::size_t n = std::min( name.size(), cmd.sfx_name.size() - 1 );
    if( n != 0 )
        std::memcpy( cmd.sfx_name.data(), name.data(), n );
    cmd.sfx_name[n] = '\0';
}

// ===========================================================================
// AudioCommandQueue — the SND-OQ-3 policy
// ===========================================================================

bool AudioCommandQueue::submit( const AudioCommand &cmd, bool *out_dropped ) noexcept
{
    if( out_dropped != nullptr )
        *out_dropped = false;

    // Shutdown step (a): a closed queue refuses everything.  NOT a drop — the
    // pipeline is being torn down, there is nothing left to play.
    if( !accepting() )
        return false;

    if( command_class( cmd ) == AudioCommandClass::Reserved )
    {
        // SND-OQ-3's core guarantee: the reserve is unreachable by the normal
        // class, so this admission cannot be blocked by a START flood.  It can
        // still fail if the reserve ITSELF is saturated (only possible when the
        // consumer is not draining at all) — that is a genuine drop.
        if( queue_.push_reserved_class( cmd ) )
            return true;
        if( out_dropped != nullptr )
            *out_dropped = true;
        return false;
    }

    // Normal class: block (bounded) rather than drop.
    for( std::size_t spin = 0; spin < ::xash::limits::sound_command_push_spin_max; ++spin )
    {
        if( queue_.try_push( cmd ) )
            return true;
        // Re-check the gate every iteration: a concurrent stop() must be able
        // to release a producer parked here (otherwise shutdown could wait out
        // the whole spin bound).
        if( !accepting() )
            return false;
        std::this_thread::yield();
    }

    // Bound exhausted.  Dropping is deliberate: a hard assert here would turn a
    // stalled audio thread into a process abort, and dropping a START is the
    // LEAST audible failure available (SND-OQ-3: "a dropped SND_STOP is
    // audible ... worse than a dropped SND_START").  STOP/CHANGE never reach
    // this path at all — they took the reserved lane above.
    if( out_dropped != nullptr )
        *out_dropped = true;
    return false;
}

// ===========================================================================
// Channel mutation — the S9.6 bodies, moved
// ===========================================================================

void free_all_channels( ChannelApplyContext &ctx ) noexcept
{
    if( ctx.mixer == nullptr )
        return;

    for( MixChannel &ch : ctx.mixer->channels() )
    {
        if( ch.source == nullptr )
            continue;
        free_channel( ch, ctx.vox );
    }
    for( MixChannel &ch : ctx.mixer->channels() )
        ch = MixChannel{}; // full-array wipe (s_main.c:1483's memset)

    ctx.total_channels = static_cast<int>( ::xash::limits::sound_num_ambient_channels +
                                           ::xash::limits::sound_num_dynamic_channels );
}

namespace {

// S_StartSound's body from the alter prologue onward (s_main.c:643-740).
// Everything BEFORE it — registry lookup, the vol/pitch clamps, `use_pos`
// resolution and the IEntitySpatialProvider call — already happened on T_Main
// and arrived in `cmd`.
void apply_start( ChannelApplyContext &ctx, const AudioCommand &cmd ) noexcept
{
    Mixer &mixer = *ctx.mixer;
    const std::string_view sfx_name{ cmd.sfx_name.data() };

    std::uint32_t flags = cmd.flags;

    if( ( flags & ( k_snd_stop | k_snd_change_vol | k_snd_change_pitch ) ) != 0 )
    {
        if( alter_channel( mixer.channels(), ctx.total_channels, cmd.entnum, cmd.entchannel, cmd.sfx_handle,
                           sfx_name, cmd.vol, cmd.pitch, flags, ctx.vox ) )
            return; // s_main.c:643-644
        if( ( flags & k_snd_stop ) != 0 )
            return; // s_main.c:646
        // fall through — start the sound (s_main.c:647-648)
    }

    if( cmd.entchannel == k_chan_stream )
        flags |= k_snd_stop_looping; // s_main.c:653-654

    int  target_index = -1;
    bool ignore       = false;

    if( cmd.entchannel == k_chan_static )
    {
        target_index = pick_static_channel( mixer.channels(), ctx.total_channels, cmd.origin, cmd.sfx_handle );
    }
    else
    {
        const DynamicPickResult r = pick_dynamic_channel( mixer.channels(), cmd.listener.entnum, cmd.entnum,
                                                          cmd.entchannel, cmd.sfx_handle, ctx.vox, ctx.vox );
        target_index              = r.index;
        ignore                    = r.ignore;
    }

    if( target_index < 0 )
    {
        if( !ignore )
        {
            // s_main.c:662-666.  This IS a genuinely-dropped start request —
            // the Tier-1 counter's own definition ("start requests dropped
            // (queue full / no channel)").
            if( ctx.stats != nullptr )
                ctx.stats->dropped_sounds.fetch_add( 1, std::memory_order_relaxed );
            ::xash::core::logf( ::xash::core::LogLevel::Error, k_log_tag, "dropped sound \"%s\"",
                                cmd.sfx_name.data() );
        }
        return;
    }

    MixChannel &target = mixer.channels()[static_cast<std::size_t>( target_index )];

    target = MixChannel{}; // memset(target_chan,0,sizeof(*target_chan)) (s_main.c:670)

    target.origin = cmd.origin;
    if( cmd.entnum == 0 )
        target.flags |= ::xash::abi::k_fl_chan_static_sound; // s_main.c:674-675
    if( ( flags & k_snd_stop_looping ) == 0 )
        target.flags |= ::xash::abi::k_fl_chan_use_loop; // s_main.c:677-678
    if( ( flags & k_snd_localsound ) != 0 )
        target.flags |= ::xash::abi::k_fl_chan_local_sound; // s_main.c:680-681

    target.dist_mult  = cmd.dist_mult; // attn / SND_CLIP_DISTANCE, computed on T_Main (s_main.c:683)
    target.master_vol = cmd.vol;
    target.entnum     = cmd.entnum;
    target.entchannel = cmd.entchannel;
    target.base_pitch = static_cast<double>( cmd.pitch );
    target.sfx_handle = cmd.sfx_handle;

    bool has_source = false;

    if( cmd.sfx_is_sentence ) // S_TestSoundChar(sfx->name,'!') — resolved on T_Main (s_main.c:692)
    {
        target.name = sfx_name; // Q_strncpy(target_chan->name, sfx->name, ...) (s_main.c:700)
        if( ctx.vox != nullptr )
        {
            std::optional<VoxSentence> sentence = ctx.vox->build_sentence( skip_sound_char( sfx_name ) );
            if( sentence.has_value() )
            {
                ctx.vox->bind_channel( target, std::move( *sentence ), ctx.vox_resolver ); // VOX_LoadSound (s_main.c:699)
                has_source = target.source != nullptr;
            }
            // else: unknown sentence -> ch->words stays unbound (legacy leaves
            // target_chan untouched too — s_vox.c:466-471's warning path).
        }
    }
    else
    {
        target.source = cmd.source; // S_LoadSound(sfx)'s result, resolved on T_Main (s_main.c:707)
        target.name.clear();
        has_source = target.source != nullptr;
    }

    if( !has_source )
    {
        free_channel( target, ctx.vox ); // s_main.c:713
        return;
    }

    // SND_Spatialize (s_main.c:717) at its legacy call site.  The provider half
    // already ran on T_Main (SND-OQ-1); this is the pure pan/attenuation math.
    spatialize_with_origin( target, cmd.listener.entnum, cmd.listener.origin, cmd.listener.right,
                            cmd.listener.bugcomp_attn_none, cmd.entity_origin_valid, cmd.entity_origin );
    if( target.is_sentence && ctx.vox != nullptr )
        ctx.vox->apply_word_volume( target ); // VOX_SetChanVol (s_main.c:574,607 equivalent)

    // First-audibility drop (s_main.c:719-734).
    if( target.leftvol == 0 && target.rightvol == 0 )
    {
        const bool looped = has_flag( target.source->flags, AudioFlags::Looped );
        if( !looped && cmd.entchannel != k_chan_stream )
        {
            free_channel( target, ctx.vox );
            return;
        }
    }
}

// The §4.3 per-frame cvar poll, applied to the decoder-owned Mixer/RoomDsp.
void apply_mix_config( ChannelApplyContext &ctx, const MixConfigSnapshot &cfg ) noexcept
{
    if( !cfg.cvars_polled )
        return; // matches S9.6's `if (impl_->cmd_cvar_ != nullptr)` guard

    if( ctx.mixer != nullptr )
        ctx.mixer->set_lerping( cfg.lerping );

    if( ctx.room_dsp == nullptr )
        return;

    RoomDsp &dsp = *ctx.room_dsp;
    dsp.set_room_off( cfg.room_off );
    // dsp_coeff_table: pass the RAW cvar float through unmodified —
    // RoomDsp::set_dsp_coeff_table's own dual-coercion note (dsp.hpp).
    dsp.set_dsp_coeff_table( cfg.dsp_coeff_table );
    dsp.set_room_type( cfg.room_type );
    dsp.set_waterroom_type( cfg.waterroom_type );
    dsp.set_hisound( cfg.hisound );
    dsp.set_room_mod( cfg.room_mod );
    dsp.set_room_lp( cfg.room_lp );
    dsp.set_room_rvblp( cfg.room_rvblp );
    dsp.set_room_refl( cfg.room_refl );
    dsp.set_room_dlylp( cfg.room_dlylp );
    dsp.set_room_feedback( cfg.room_feedback );
    dsp.set_room_size( cfg.room_size );
    dsp.set_room_delay( cfg.room_delay );
    dsp.set_room_left( cfg.room_left );
}

} // namespace

void apply_command( ChannelApplyContext &ctx, const AudioCommand &cmd ) noexcept
{
    if( ctx.mixer == nullptr )
        return;

    switch( cmd.type )
    {
    case AudioCommandType::StartSound:
        apply_start( ctx, cmd );
        break;

    case AudioCommandType::AlterChannel:
        // S_AlterChannel standalone — S_StopSound's `S_AlterChannel(...,
        // SND_STOP)` (s_main.c:1457) and the change legs.
        (void)alter_channel( ctx.mixer->channels(), ctx.total_channels, cmd.entnum, cmd.entchannel,
                             cmd.sfx_handle, std::string_view{ cmd.sfx_name.data() }, cmd.vol, cmd.pitch,
                             cmd.flags, ctx.vox );
        break;

    case AudioCommandType::StopAllSounds:
        free_all_channels( ctx );          // s_main.c:1470-1477
        if( ctx.room_dsp != nullptr )
            ctx.room_dsp->clear_state();   // SX_ClearState (s_main.c:1480)
        // S_InitAmbientChannels restart (s_main.c:1486) is still not wired —
        // `ambient` is carried for signature parity exactly as in S9.6.
        (void)cmd.ambient;
        break;

    case AudioCommandType::FrameUpdate:
        ctx.listener   = cmd.listener;
        ctx.mix_config = cmd.mix_config;
        if( ctx.room_dsp != nullptr )
            ctx.room_dsp->set_waterlevel( cmd.listener.waterlevel );
        apply_mix_config( ctx, cmd.mix_config );
        break;

    case AudioCommandType::FlushEpoch:
        // SND-OQ-2 fence — deliberately a NO-OP on channel state.
        //
        // The guarantee is carried by ordering, not by teardown: the MPSC is
        // FIFO, so reaching this command proves every command submitted before
        // it (in particular the AlterChannel(SND_STOP) / StopAllSounds that
        // dropped the sfx being retired) has ALREADY been applied.  And EVERY
        // decoder-side dereference of borrowed audio is sequenced BEFORE this
        // pop on the same thread — the decoder is single-threaded, so the
        // earlier pops in this same drain loop (which DO dereference borrowed
        // audio: apply_start()'s `target.source->flags` audibility test, VOX
        // word resolution) and the previous step's paint all happen-before it,
        // while every later paint can only see the post-stop channel state.
        // (CONC-10: the earlier wording claimed the only dereference site was
        // paint_channels(), which was false — the conclusion survives, the
        // stated reason did not.)  The ack's RELEASE store (topology.cpp)
        // publishes all of that to T_Main's acquire-load.
        //
        // Freeing every channel here instead would make a flush audibly
        // destructive (it would kill unrelated sounds) without adding any
        // safety the ordering does not already provide.
        break;

    case AudioCommandType::None:
        break;
    }
}

} // namespace xash::sound
