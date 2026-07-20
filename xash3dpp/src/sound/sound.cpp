// xash3dpp — Sound: audio engine core implementation.
// Legacy reference: engine/client/sound/s_main.c (S_Init/S_Shutdown lifecycle,
// S_StartSound/S_StartLocalSound/S_StopSound/S_StopAllSounds/S_UpdateFrame),
// s_load.c (S_RegisterSound/S_SoundList_f), s_dsp.c (cvar census). Slice
// S9.1 wired the lifecycle + device open/close. Slice S9.6 ("entry surface")
// layers channel allocation/spatialize/stop/register + the cvar/command
// census on top, driving the S9.2-S9.5 mixer/vox/dsp/codec components.
//
// Existing subsystems used:
//   xash3dpp_core     — thread-role assertions + structured logging
//   xash3dpp_memory   — pool-backed allocations (create_pool / destroy_pool)
//   xash3dpp_cmd_cvar — cvar/command registration (S9.6)

#include <xash3dpp/sound/sound.hpp>

#include <xash3dpp/private/sound/channel_alloc.hpp>
#include <xash3dpp/private/sound/dsp.hpp>
#include <xash3dpp/private/sound/mixer.hpp>
#include <xash3dpp/private/sound/owned_state.hpp>
#include <xash3dpp/private/sound/registry.hpp>
#include <xash3dpp/private/sound/vox.hpp>
#include <xash3dpp/sound/constants.hpp>

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/platform/platform.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace xash::sound {

namespace {

constexpr std::string_view k_log_tag = "sound";

// bound(min, num, max) verbatim (public/xash3d_mathlib.h:141) — duplicated
// locally per the mixer.cpp/vox.cpp/dsp.cpp/channel_alloc.cpp precedent.
[[nodiscard]] constexpr float bound_float( float lo, float v, float hi ) noexcept
{
    return v >= lo ? ( v < hi ? v : hi ) : lo;
}

// SilenceFillSource — the placeholder pull target Sound registers on its device.
// Nothing drives Mixer::paint_channels() as the device's fill source yet
// (deferred past S9.6 — see the uncertainty list). Zero-fills the request.
//
// @thread-safety: fill() runs on T_AudioCallback (real-time): no alloc, no
// lock, no block — a plain memset-shaped zero-fill (threading-model §Forbidden).
class SilenceFillSource final : public IAudioFillSource
{
public:
    void fill( std::span<std::int16_t> out ) noexcept override
    {
        for( std::int16_t &s : out )
            s = 0;
    }
};

// register_cvar — the CVAR_DEFINE-equivalent field-by-field setup +
// cvar_register_engine call (context_init.cpp precedent; Cvar has a deleted
// copy-assignment via its atomic member, so fields are set individually).
void register_cvar( ::xash::cmd_cvar::CmdCvarContext &ctx, ::xash::cmd_cvar::Cvar &cv, const char *name,
                    const char *def, std::uint32_t flags, const char *desc ) noexcept
{
    cv.abi.name   = const_cast<char *>( name );  // SAFETY: static string literal; never written or freed
    cv.abi.string = const_cast<char *>( def );    // SAFETY: static string literal; never written or freed
    cv.abi.flags  = flags;
    cv.abi.value  = ::xash::utilities::atof( def );
    cv.abi.next   = nullptr;
    cv.desc       = desc;
    cv.def_string = def;
    cv.generation.store( 0, std::memory_order_relaxed );
    ctx.cvar_register_engine( cv );
}

} // namespace

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct Sound::Impl
{
    xash::memory::PoolHandle pool_ {};

    // @lifetime: non-owning injected device; owned by the caller/EngineContext
    //   and outlives this Sound.  Null until init() when no fallback is needed.
    IAudioDevice *device_ = nullptr;
    // Owned fallback used only when SoundInitParams::device is null.
    std::unique_ptr<NullDevice> null_device_;

    // @lifetime: non-owning SND-OQ-1 seams (may be null on a headless load).
    IEntitySpatialProvider *spatial_ = nullptr;
    IMouthSink             *mouth_   = nullptr;

    // The placeholder pull source (silence) registered on the device this slice.
    SilenceFillSource silence_ {};

    // Owned-state stubs (boundary §Owned state disposition) — typed, inert.
    DmaState        dma_ {};
    MixClock        mix_clock_ {};
    ChannelState    channels_ {};
    RawChannelState raw_channels_ {};
    AmbientState    ambient_state_ {};
    FadeState       fade_state_ {};

    DeviceCaps caps_ {};
    bool       initialized_ = false;
    SoundStats stats_ {};

    // ------------------------------------------------------------------
    // S9.6 additions — the entry surface's owned components.
    // ------------------------------------------------------------------
    Mixer     mixer_;    // paint pipeline core (S9.3); channels() resized to
                          // sound_max_channels at init() time (S9.6).
    VoxSystem vox_;       // sentence word-sequencer (S9.4) + IVoxTimeLeftQuery (S9.6).

    // Constructed at init() time (RoomDsp needs the pool; the audio loader
    // needs SoundInitParams::filesystem; the registry needs the loader).
    std::unique_ptr<IAudioLoader> audio_loader_;
    std::unique_ptr<SfxRegistry>  registry_;
    std::unique_ptr<RoomDsp>      room_dsp_;

    ListenerSnapshot listener_ {};       // update_frame()'s published snapshot (SND-OQ-1)
    int              total_channels_ = 0; // snd.total_channels — the static-range high-water mark

    // cmd_cvar wiring (nullable — a headless/test load skips registration).
    ::xash::cmd_cvar::CmdCvarContext *cmd_cvar_ = nullptr;

    // Cvar storage (registered via cvar_register_engine; stable addresses for
    // the CmdCvarContext's lifetime — see context_init.cpp precedent).
    struct Cvars
    {
        ::xash::cmd_cvar::Cvar volume, musicvolume, mixahead, show, lerping, ambient_level, ambient_fade,
            mute_losefocus, test, samplecount, warn_late_precache;
        ::xash::cmd_cvar::Cvar room_off, dsp_coeff_table, room_type, waterroom_type, room_hires, room_mod, room_lp,
            room_left, room_rvblp, room_refl, room_size, room_dlylp, room_feedback, room_delay;
    } cv_;

    // Reset every channel (S_FreeChannel per occupied slot) and the
    // total_channels_ high-water mark back to MAX_DYNAMIC_CHANNELS — the
    // shared core of S_StopAllSounds AND the teardown half of S_Shutdown.
    void free_all_channels() noexcept
    {
        for( MixChannel &ch : mixer_.channels() )
        {
            if( ch.source == nullptr )
                continue;
            free_channel( ch, &vox_ );
        }
        for( MixChannel &ch : mixer_.channels() )
            ch = MixChannel{}; // full-array wipe (s_main.c:1483's memset)

        total_channels_ = static_cast<int>( ::xash::limits::sound_num_ambient_channels +
                                            ::xash::limits::sound_num_dynamic_channels );
    }
};

Sound::Sound() : impl_{ std::make_unique<Impl>() } {}
Sound::~Sound()
{
    // Deterministic teardown: release the device + pool if init() ran and the
    // caller did not call shutdown() explicitly.
    if( impl_ && impl_->initialized_ )
        shutdown();
}
Sound::Sound( Sound && ) noexcept            = default;
Sound &Sound::operator=( Sound && ) noexcept = default;

// ===========================================================================
// Lifecycle
// ===========================================================================

Result<void> Sound::init( const SoundInitParams &params )
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    if( impl_->initialized_ )
    {
        xash::core::log( xash::core::LogLevel::Error, k_log_tag,
                         "Sound::init called twice without shutdown" );
        return std::unexpected( SoundError::AlreadyInitialized );
    }

    // Select the device: injected, or the owned NullDevice fallback.
    if( params.device != nullptr )
    {
        impl_->device_ = params.device;
    }
    else
    {
        impl_->null_device_ = std::make_unique<NullDevice>();
        impl_->device_      = impl_->null_device_.get();
    }

    // SNDDMA_Init: open the device against the requested format.
    Result<DeviceCaps> opened = impl_->device_->open( params.spec );
    if( !opened.has_value() )
    {
        xash::core::log( xash::core::LogLevel::Error, k_log_tag,
                         "audio device open failed" );
        impl_->device_ = nullptr;
        impl_->null_device_.reset();
        return std::unexpected( opened.error() );
    }
    impl_->caps_ = *opened;

    // Record the device-facing DMA state (boundary Owned-state dma_ mapping).
    impl_->dma_.format.speed    = impl_->caps_.speed;
    impl_->dma_.format.width    = impl_->caps_.width;
    impl_->dma_.format.channels = impl_->caps_.channels;
    impl_->dma_.initialized     = true;
    impl_->dma_.samples         = static_cast<int>( impl_->caps_.buffer_frames * 2 );

    // Wire the pull source (silence — see the file-header note) and gate
    // output on (SNDDMA_Activate).
    impl_->device_->set_fill_source( &impl_->silence_ );
    impl_->device_->set_active( true );

    // Retain the SND-OQ-1 seams for the mix side.
    impl_->spatial_ = params.spatial;
    impl_->mouth_   = params.mouth;

    impl_->pool_ = xash::memory::create_pool( "sound" );
    if( !impl_->pool_ )
    {
        xash::core::log( xash::core::LogLevel::Error, k_log_tag,
                         "sound pool allocation failed" );
        impl_->device_->close();
        impl_->device_ = nullptr;
        impl_->null_device_.reset();
        impl_->dma_ = {};
        return std::unexpected( SoundError::DeviceUnavailable );
    }

    // ------------------------------------------------------------------
    // S9.6 — entry-surface components.
    // ------------------------------------------------------------------

    // Mixer's channel array: reserved to sound_max_channels in the Mixer
    // constructor already; resize ONCE here so allocation indexes directly
    // into it (mirrors snd.channels[MAX_CHANNELS]'s fixed size). Never
    // resized again for the life of this Sound — addresses (VoxSystem's
    // bound_ map keys on MixChannel*) stay stable.
    impl_->mixer_.channels().resize( ::xash::limits::sound_max_channels );
    impl_->mixer_.set_vox_advance( &impl_->vox_ );

    impl_->room_dsp_ = std::make_unique<RoomDsp>( impl_->pool_ );
    impl_->mixer_.set_room_dsp( impl_->room_dsp_.get() );

    if( params.filesystem != nullptr )
        impl_->audio_loader_ = std::make_unique<FilesystemAudioLoader>( *params.filesystem );
    impl_->registry_ = std::make_unique<SfxRegistry>( impl_->audio_loader_.get() );

    impl_->cmd_cvar_ = params.cmd_cvar;

    if( impl_->cmd_cvar_ != nullptr )
    {
        using ::xash::cmd_cvar::FCVAR_ARCHIVE;
        using ::xash::cmd_cvar::FCVAR_FILTERABLE;
        auto &ctx = *impl_->cmd_cvar_;
        auto &cv  = impl_->cv_;

        // -- General cvars (11, s_main.c:47-57) ---------------------------
        register_cvar( ctx, cv.volume, "volume", "0.7", FCVAR_ARCHIVE | FCVAR_FILTERABLE, "sound volume" );
        register_cvar( ctx, cv.musicvolume, "MP3Volume", "1.0", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "background music volume" );
        register_cvar( ctx, cv.mixahead, "_snd_mixahead", "0.12", FCVAR_FILTERABLE,
                      "how much sound to mix ahead of time" );
        register_cvar( ctx, cv.show, "s_show", "0", FCVAR_ARCHIVE | FCVAR_FILTERABLE, "show playing sounds" );
        register_cvar( ctx, cv.lerping, "s_lerping", "0", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "apply interpolation to sound output" );
        register_cvar( ctx, cv.ambient_level, "ambient_level", "0.3", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "volume of environment noises (water and wind)" );
        register_cvar( ctx, cv.ambient_fade, "ambient_fade", "1000", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "rate of volume fading when client is moving" );
        register_cvar( ctx, cv.mute_losefocus, "snd_mute_losefocus", "1", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "silence the audio when game window loses focus" );
        register_cvar( ctx, cv.test, "s_test", "0", 0, "engine developer cvar for quick testing new features" );
        register_cvar( ctx, cv.samplecount, "s_samplecount", "0", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "sample count (0 for default value)" );
        register_cvar( ctx, cv.warn_late_precache, "s_warn_late_precache", "0", FCVAR_ARCHIVE | FCVAR_FILTERABLE,
                      "warn about late precached sounds on client-side" );

        // -- DSP cvars (14, boundary §4.3) --------------------------------
        register_cvar( ctx, cv.room_off, "room_off", "0", FCVAR_ARCHIVE, "disable the room-effects DSP chain" );
        register_cvar( ctx, cv.dsp_coeff_table, "dsp_coeff_table", "0", FCVAR_ARCHIVE,
                      "select the DSP preset table (0=release, 1=HL alpha 0.52)" );
        register_cvar( ctx, cv.room_type, "room_type", "0", 0, "current room DSP preset (live server/map value)" );
        register_cvar( ctx, cv.waterroom_type, "waterroom_type", "14", 0, "room DSP preset while submerged" );
        register_cvar( ctx, cv.room_hires, "room_hires", "2", FCVAR_ARCHIVE,
                      "DSP delay-line resolution (1=22k/2=44k/3=96k)" );
        register_cvar( ctx, cv.room_mod, "room_mod", "0", 0, "underwater lowpass/warble modulation rate" );
        register_cvar( ctx, cv.room_lp, "room_lp", "0", 0, "underwater lowpass amount" );
        register_cvar( ctx, cv.room_left, "room_left", "0", 0, "stereo delay: left channel delay time" );
        register_cvar( ctx, cv.room_rvblp, "room_rvblp", "1", 0, "reverb low-pass filtering level" );
        register_cvar( ctx, cv.room_refl, "room_refl", "0", 0, "reverb decay time (feedback)" );
        register_cvar( ctx, cv.room_size, "room_size", "0", 0, "reverb initial reflection size" );
        register_cvar( ctx, cv.room_dlylp, "room_dlylp", "1", 0, "mono delay low-pass filtering level" );
        register_cvar( ctx, cv.room_feedback, "room_feedback", "0.2", 0, "mono delay decay time" );
        register_cvar( ctx, cv.room_delay, "room_delay", "0.8", 0, "mono delay time" );

        // -- Commands (13, s_main.c:1998-2011) -----------------------------
        void *user = this;
        ctx.cmd_add( "play", &Sound::cmd_play_f, user, 0, "playing a specified sound file" );
        ctx.cmd_add( "play2", &Sound::cmd_play2_f, user, 0, "playing a group of specified sound files" );
        ctx.cmd_add( "playvol", &Sound::cmd_playvol_f, user, 0,
                    "playing a specified sound file with specified volume" );
        ctx.cmd_add( "stopsound", &Sound::cmd_stopsound_f, user, 0, "stop all sounds" );
        ctx.cmd_add( "music", &Sound::cmd_music_f, user, ::xash::cmd_cvar::FCMD_OVERRIDABLE,
                    "starting a background track" );
        ctx.cmd_add( "soundlist", &Sound::cmd_soundlist_f, user, 0, "display loaded sounds" );
        ctx.cmd_add( "s_info", &Sound::cmd_s_info_f, user, 0, "print sound system information" );
        ctx.cmd_add( "s_fade", &Sound::cmd_s_fade_f, user, 0, "fade all sounds then stop all" );
        ctx.cmd_add( "soundfade", &Sound::cmd_soundfade_f, user, 0,
                    "fade all sounds then stop all (goldsrc compatible)" );
        ctx.cmd_add( "+voicerecord", &Sound::cmd_voicerecord_start_f, user, 0, "start voice recording" );
        ctx.cmd_add( "-voicerecord", &Sound::cmd_voicerecord_stop_f, user, 0, "stop voice recording" );
        ctx.cmd_add( "spk", &Sound::cmd_spk_f, user, 0, "reliable play a specified sententce" );
        ctx.cmd_add( "speak", &Sound::cmd_speak_f, user, 0, "playing a specified sententce" );
        // Cmd_AddRestrictedCommand (s_dsp.c:250) — the ONE restricted
        // command in the subsystem; FCMD_PRIVILEGED is the restricted-flag
        // parity mapping (same as the input bind commands).
        ctx.cmd_add( "dsp_profile", &Sound::cmd_dsp_profile_f, user, ::xash::cmd_cvar::FCMD_PRIVILEGED,
                     "dsp stress-test, first argument is room_type" );
    }

    // S_Init calls S_StopAllSounds(true) before returning (s_main.c:2029) —
    // establishes total_channels_'s starting value.
    impl_->free_all_channels();

    impl_->initialized_ = true;
    return {};
}

void Sound::shutdown()
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    // S_Shutdown's command teardown (s_main.c:2046-2058) — 12 of the 13
    // registered commands are removed; "play2" is a PRESERVED leaked
    // registration (boundary §Quirks: "13 added, 12 removed"), not a bug fix.
    if( impl_->cmd_cvar_ != nullptr )
    {
        auto &ctx = *impl_->cmd_cvar_;
        ctx.cmd_remove( "play" );
        ctx.cmd_remove( "playvol" );
        ctx.cmd_remove( "stopsound" );
        ctx.cmd_remove( "music" );
        ctx.cmd_remove( "soundlist" );
        ctx.cmd_remove( "s_info" );
        ctx.cmd_remove( "s_fade" );
        ctx.cmd_remove( "soundfade" );
        ctx.cmd_remove( "+voicerecord" );
        ctx.cmd_remove( "-voicerecord" );
        ctx.cmd_remove( "speak" );
        ctx.cmd_remove( "spk" );
        ctx.cmd_remove( "dsp_profile" ); // legacy DOES remove this one (s_dsp.c:285)
    }

    // S_StopAllSounds(false) + S_FreeSounds' internals (s_main.c:2063-2065) —
    // tear down every channel/VOX binding before the pool (backing RoomDsp's
    // delay lines) is destroyed below.
    impl_->free_all_channels();

    impl_->room_dsp_.reset();   // frees pool-backed PoolIntBuffer members — MUST run before destroy_pool
    impl_->registry_.reset();   // heap-owned AudioData caches; no pool dependency
    impl_->audio_loader_.reset();

    if( impl_->device_ != nullptr )
    {
        impl_->device_->set_active( false );
        impl_->device_->set_fill_source( nullptr );
        impl_->device_->close();
    }
    impl_->device_ = nullptr;
    impl_->null_device_.reset();

    if( impl_->pool_ )
    {
        xash::memory::destroy_pool( impl_->pool_ );
        impl_->pool_ = {};
    }

    impl_->spatial_ = nullptr;
    impl_->mouth_   = nullptr;
    impl_->dma_     = {};
    impl_->caps_    = {};
    impl_->cmd_cvar_ = nullptr;
    impl_->initialized_ = false;
}

bool Sound::initialized() const noexcept { return impl_->initialized_; }

const SoundStats &Sound::stats() const noexcept { return impl_->stats_; }

IAudioDevice *Sound::device() const noexcept { return impl_->device_; }

const DeviceCaps &Sound::caps() const noexcept { return impl_->caps_; }

// ===========================================================================
// Entry surface (S9.6)
// ===========================================================================

::xash::abi::sound_t Sound::register_sound( std::string_view name ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ || !impl_->registry_ )
        return k_invalid_sound_handle;
    return impl_->registry_->register_sound( name ); // S_RegisterSound (s_load.c:312-336)
}

void Sound::start_sound( std::optional<Vec3> pos, int ent, int chan, ::xash::abi::sound_t handle, float fvol,
                         float attn, int pitch, std::uint32_t flags ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ || !impl_->registry_ )
        return;

    Impl &impl = *impl_;

    const SfxSlot *sfx = impl.registry_->get( handle );
    if( sfx == nullptr )
        return; // !sfx (s_main.c:635-636)

    const int vol = static_cast<int>( bound_float( 0.0f, fvol * 255.0f, 255.0f ) ); // s_main.c:638
    if( pitch <= 1 )
        pitch = k_pitch_norm_flag; // "Invasion issues" (s_main.c:639)

    if( ( flags & ( k_snd_stop | k_snd_change_vol | k_snd_change_pitch ) ) != 0 )
    {
        if( alter_channel( impl.mixer_.channels(), impl.total_channels_, ent, chan, handle, sfx->name, vol, pitch,
                           flags, &impl.vox_ ) )
            return; // s_main.c:643-644
        if( ( flags & k_snd_stop ) != 0 )
            return; // s_main.c:646
        // fall through — start the sound (s_main.c:647-648)
    }

    // NULL pos -> refState.vieworg (s_main.c:651). This port has no separate
    // refState surface; ListenerSnapshot::origin (the SAME S_UpdateFrame
    // publish point legacy's refState.vieworg update shares a source with) is
    // the closest available approximation — see the S9.6 uncertainty list.
    const Vec3 use_pos = pos.value_or( impl.listener_.origin );

    if( chan == k_chan_stream )
        flags |= k_snd_stop_looping; // s_main.c:653-654

    int  target_index = -1;
    bool ignore        = false;

    if( chan == k_chan_static )
    {
        target_index = pick_static_channel( impl.mixer_.channels(), impl.total_channels_, use_pos, handle );
    }
    else
    {
        const DynamicPickResult r = pick_dynamic_channel( impl.mixer_.channels(), impl.listener_.entnum, ent, chan,
                                                          handle, &impl.vox_, &impl.vox_ );
        target_index               = r.index;
        ignore                      = r.ignore;
    }

    if( target_index < 0 )
    {
        if( !ignore )
            ::xash::core::logf( ::xash::core::LogLevel::Error, k_log_tag, "dropped sound \"%s\"",
                                sfx->name.c_str() ); // s_main.c:662-666
        return;
    }

    MixChannel &target = impl.mixer_.channels()[static_cast<std::size_t>( target_index )];

    target = MixChannel{}; // memset(target_chan,0,sizeof(*target_chan)) (s_main.c:670)

    target.origin = use_pos;
    if( ent == 0 )
        target.flags |= ::xash::abi::k_fl_chan_static_sound; // s_main.c:674-675
    if( ( flags & k_snd_stop_looping ) == 0 )
        target.flags |= ::xash::abi::k_fl_chan_use_loop; // s_main.c:677-678
    if( ( flags & k_snd_localsound ) != 0 )
        target.flags |= ::xash::abi::k_fl_chan_local_sound; // s_main.c:680-681

    target.dist_mult  = attn / k_sound_clip_distance; // s_main.c:683
    target.master_vol = vol;
    target.entnum     = ent;
    target.entchannel = chan;
    target.base_pitch = static_cast<double>( pitch );
    target.sfx_handle = handle;

    bool has_source = false;

    if( test_sound_char( sfx->name, '!' ) ) // s_main.c:692
    {
        target.name = sfx->name; // Q_strncpy(target_chan->name, sfx->name, ...) (s_main.c:700)
        std::optional<VoxSentence> sentence = impl.vox_.build_sentence( skip_sound_char( sfx->name ) );
        if( sentence.has_value() )
        {
            impl.vox_.bind_channel( target, std::move( *sentence ), impl.registry_.get() ); // VOX_LoadSound (s_main.c:699)
            has_source = target.source != nullptr;
        }
        // else: unknown sentence -> ch->words stays unbound (legacy leaves
        // target_chan untouched too — s_vox.c:466-471's warning path).
    }
    else
    {
        target.source = impl.registry_->load_sfx( handle ); // S_LoadSound(sfx) (s_main.c:707)
        target.name.clear();
        has_source = target.source != nullptr;
    }

    if( !has_source )
    {
        free_channel( target, &impl.vox_ ); // s_main.c:713
        return;
    }

    spatialize( target, impl.listener_.entnum, impl.listener_.origin, impl.listener_.right,
               impl.listener_.bugcomp_attn_none, impl.spatial_ ); // SND_Spatialize (s_main.c:717)
    if( target.is_sentence )
        impl.vox_.apply_word_volume( target ); // VOX_SetChanVol (s_main.c:574,607 equivalent)

    // First-audibility drop (s_main.c:719-734).
    if( target.leftvol == 0 && target.rightvol == 0 )
    {
        const bool looped = has_flag( target.source->flags, AudioFlags::Looped );
        if( !looped && chan != k_chan_stream )
        {
            free_channel( target, &impl.vox_ );
            return;
        }
    }

    // S_NotifyChannelUpdate (SoundAPI client-override notify) + SND_InitMouth
    // — NOT wired this slice (ABI plumbing deferred per the boundary's
    // "layout-pinned, plumbing deferred" note; mouth init/close hooks are a
    // separate feature from IMouthSink's mix-time write-back, see the
    // uncertainty list).
}

void Sound::start_local_sound( std::string_view name, float volume, bool reliable ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    const ::xash::abi::sound_t handle  = register_sound( name ); // S_RegisterSound (s_main.c:964)
    const int                  channel = reliable ? k_chan_static : k_chan_auto;

    start_sound( std::nullopt, impl_->listener_.entnum, channel, handle, volume, k_attn_none, k_pitch_norm_flag,
                k_snd_localsound | k_snd_stop_looping ); // s_main.c:965
}

void Sound::stop_sound( int entnum, int channel, std::string_view soundname ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ || !impl_->registry_ )
        return;

    Impl &impl = *impl_;

    // S_FindName (s_main.c:1456) — NOT register_sound (no '!' routing, no
    // lazy-decode obligation: S_StopSound never plays anything).
    const SfxHandle sfx = impl.registry_->find_name( soundname );
    if( sfx == k_invalid_sound_handle )
        return; // defensive: legacy dereferences sfx->name unconditionally here
                // (UB for an unresolvable name) — not reachable by any real
                // GAME_EXPORT caller; see the S9.6 uncertainty list.

    const SfxSlot *slot = impl.registry_->get( sfx );
    if( slot == nullptr )
        return;

    (void)alter_channel( impl.mixer_.channels(), impl.total_channels_, entnum, channel, sfx, slot->name, 0, 0,
                         k_snd_stop, &impl.vox_ ); // s_main.c:1457
}

void Sound::stop_all_sounds( bool ambient ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    impl_->free_all_channels(); // s_main.c:1470-1477

    if( impl_->room_dsp_ )
        impl_->room_dsp_->clear_state(); // SX_ClearState (s_main.c:1480)

    // S_InitAmbientChannels restart (s_main.c:1486): NOT wired this slice —
    // ambient-channel allocation/S_UpdateAmbientSounds is out of the S9.6
    // task list (see the uncertainty list). `ambient` is accepted (matching
    // the legacy signature exactly) but currently a no-op.
    (void)ambient;

    impl_->fade_state_ = FadeState{}; // memset(&soundfade,0,...) (s_main.c:1491)
}

void Sound::update_frame( const ListenerSnapshot &snapshot ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    impl_->listener_ = snapshot; // S_UpdateFrame's publish point (s_main.c:1590-1598)

    if( impl_->room_dsp_ )
        impl_->room_dsp_->set_waterlevel( snapshot.waterlevel );

    // Per-frame cvar polling into the existing DSP setters + the mixer's
    // lerping flag (boundary §4.3).
    if( impl_->cmd_cvar_ != nullptr )
    {
        auto &ctx = *impl_->cmd_cvar_;
        impl_->mixer_.set_lerping( ctx.cvar_variable_value( "s_lerping" ) != 0.0f );

        if( impl_->room_dsp_ )
        {
            RoomDsp &dsp = *impl_->room_dsp_;
            dsp.set_room_off( ctx.cvar_variable_value( "room_off" ) != 0.0f );
            // dsp_coeff_table: pass the RAW cvar float through unmodified —
            // RoomDsp::set_dsp_coeff_table's own dual-coercion note (dsp.hpp).
            dsp.set_dsp_coeff_table( ctx.cvar_variable_value( "dsp_coeff_table" ) );
            dsp.set_room_type( ctx.cvar_variable_value( "room_type" ) );
            dsp.set_waterroom_type( ctx.cvar_variable_value( "waterroom_type" ) );
            dsp.set_hisound( static_cast<int>( ctx.cvar_variable_value( "room_hires" ) ) );
            dsp.set_room_mod( ctx.cvar_variable_value( "room_mod" ) );
            dsp.set_room_lp( ctx.cvar_variable_value( "room_lp" ) );
            dsp.set_room_rvblp( ctx.cvar_variable_value( "room_rvblp" ) );
            dsp.set_room_refl( ctx.cvar_variable_value( "room_refl" ) );
            dsp.set_room_dlylp( ctx.cvar_variable_value( "room_dlylp" ) );
            dsp.set_room_feedback( ctx.cvar_variable_value( "room_feedback" ) );
            dsp.set_room_size( ctx.cvar_variable_value( "room_size" ) );
            dsp.set_room_delay( ctx.cvar_variable_value( "room_delay" ) );
            dsp.set_room_left( ctx.cvar_variable_value( "room_left" ) );
        }
    }
}

std::vector<ChannelInfo> Sound::channels_snapshot() const
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    std::vector<ChannelInfo> result;
    if( !impl_->initialized_ )
        return result;

    auto     &channels    = impl_->mixer_.channels();
    const int dynamic_hi = static_cast<int>( ::xash::limits::sound_num_ambient_channels +
                                             ::xash::limits::sound_num_dynamic_channels );

    for( std::size_t i = 0; i < channels.size(); ++i )
    {
        const MixChannel &ch = channels[i];
        if( ch.source == nullptr )
            continue;

        ChannelInfo info;
        info.entnum            = ch.entnum;
        info.origin             = ch.origin;
        info.left_vol           = ch.leftvol;
        info.right_vol          = ch.rightvol;
        info.position_samples   = ch.sample;
        info.channel_class = ( static_cast<int>( i ) < dynamic_hi ) ? ChannelClass::Dynamic : ChannelClass::Static;

        // Name resolution matches S_GetCurrentStaticSounds/S_GetCurrentDynamicSounds
        // (s_main.c:989-992,1039-1042): a bound sentence uses ch->name; else the
        // registered sfx's own name.
        if( ch.is_sentence && !ch.name.empty() )
            info.sfx_name = ch.name;
        else if( impl_->registry_ && ch.sfx_handle != k_invalid_sound_handle )
        {
            const SfxSlot *slot = impl_->registry_->get( ch.sfx_handle );
            if( slot != nullptr )
                info.sfx_name = slot->name;
        }

        result.push_back( std::move( info ) );
    }

    // Raw channels: allocation not wired this slice — always empty (see
    // ChannelClass::Raw's doc comment).

    return result;
}

// ===========================================================================
// Console command handlers
// ===========================================================================

void Sound::cmd_play_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    if( sound->impl_->cmd_cvar_->cmd_argc() == 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "usage: play <soundfile>" );
        return;
    }
    sound->start_local_sound( sound->impl_->cmd_cvar_->cmd_argv( 1 ), k_vol_norm, false ); // s_main.c:1684-1693
}

void Sound::cmd_play2_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    const int argc = sound->impl_->cmd_cvar_->cmd_argc();
    if( argc == 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "usage: play2 <soundfile>" );
        return;
    }
    for( int i = 1; i < argc; ++i )
        sound->start_local_sound( sound->impl_->cmd_cvar_->cmd_argv( i ), k_vol_norm, true ); // s_main.c:1695-1710
}

void Sound::cmd_playvol_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    if( sound->impl_->cmd_cvar_->cmd_argc() == 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "usage: playvol <soundfile volume>" );
        return;
    }
    sound->start_local_sound( sound->impl_->cmd_cvar_->cmd_argv( 1 ),
                              ::xash::utilities::atof( sound->impl_->cmd_cvar_->cmd_argv( 2 ) ),
                              false ); // s_main.c:1712-1721
}

void Sound::cmd_stopsound_f( void *user ) noexcept
{
    static_cast<Sound *>( user )->stop_all_sounds( true ); // s_main.c:1829-1832
}

void Sound::cmd_music_f( void *user ) noexcept
{
    // S_StreamBackgroundTrack / the s_stream.c satellite (SND-OQ-4) is not
    // built this slice — the command is registered (matching the census +
    // CMD_OVERRIDABLE flag exactly, since a game DLL may claim this name
    // first) but is currently a no-op beyond a usage note.
    (void)user;
    ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag,
                       "music: background-track streaming not implemented in this build" );
}

void Sound::cmd_soundlist_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( !sound->impl_->registry_ )
        return;
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_log_tag, "%zu registered sound(s)",
                        sound->impl_->registry_->count() ); // S_SoundList_f (s_load.c:40-73), summary only
}

void Sound::cmd_s_info_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_log_tag, "%d samples | %d total_channels",
                        sound->impl_->dma_.samples,
                        sound->impl_->total_channels_ ); // S_SoundInfo_f (s_main.c:1896-1906), summary only
}

void Sound::cmd_dsp_profile_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr || sound->impl_->room_dsp_ == nullptr )
        return;

    // SX_Profiling_f (s_dsp.c:879-916): optional first argument overrides
    // room_type for the run (restored inside profile()).
    std::optional<float> room_override;
    if( sound->impl_->cmd_cvar_->cmd_argc() > 1 )
        room_override = ::xash::utilities::atof( sound->impl_->cmd_cvar_->cmd_argv( 1 ) );

    const RoomDsp::ProfilingResult r =
        sound->impl_->room_dsp_->profile( 10000, room_override, &::xash::platform::get_time );
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_log_tag,
                        "Profiling 10000 calls to DSP. Sample count is 512, room_type is %i",
                        r.room_type_for_message ); // s_dsp.c:899
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_log_tag,
                        "----------\nTook %g seconds.", r.seconds ); // s_dsp.c:908
}

void Sound::cmd_s_fade_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    int hold_time = 5;
    if( sound->impl_->cmd_cvar_->cmd_argc() == 2 )
        hold_time = static_cast<int>(
            bound_float( 1.0f, ::xash::utilities::atof( sound->impl_->cmd_cvar_->cmd_argv( 1 ) ), 60.0f ) );

    sound->impl_->fade_state_.start_percent = 100.0f;
    sound->impl_->fade_state_.active = true; // s_main.c:1839-1851 (snd_fade_sequence); the fade CURVE
                                              // (S_UpdateSoundFade) is not wired this slice
    (void)hold_time;
}

void Sound::cmd_soundfade_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    const int c = sound->impl_->cmd_cvar_->cmd_argc();
    if( c != 3 && c != 5 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "usage: soundfade <percent> <hold> [out] [in]" );
        return;
    }
    const float percent = bound_float(
        0.0f, static_cast<float>( ::xash::utilities::atoi( sound->impl_->cmd_cvar_->cmd_argv( 1 ) ) ), 100.0f );
    sound->impl_->fade_state_.start_percent = percent;
    sound->impl_->fade_state_.active = true; // s_main.c:1858-1889 — fade curve not wired (see cmd_s_fade_f note)
}

void Sound::cmd_voicerecord_start_f( void * ) noexcept
{
    // Voice capture is fenced out of the sound recon pack entirely (boundary
    // "voice.c is its own recon scope, not covered by R9.1-R9.6") — no-op.
}
void Sound::cmd_voicerecord_stop_f( void * ) noexcept {}

void Sound::cmd_speak_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    if( sound->impl_->cmd_cvar_->cmd_argc() == 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "usage: speak <vox sentence>" );
        return;
    }
    const std::string_view name = sound->impl_->cmd_cvar_->cmd_argv( 1 );
    // S_Say (s_main.c:1723-1736): '!'-prefixed plays directly; else wraps as
    // an immediate "!#name" sentence.
    if( !name.empty() && name.front() == '!' )
        sound->start_local_sound( name, k_vol_norm, false );
    else
    {
        const std::string sentence = std::string( "!#" ) + std::string( name );
        sound->start_local_sound( sentence, k_vol_norm, false );
    }
}

void Sound::cmd_spk_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr )
        return;
    if( sound->impl_->cmd_cvar_->cmd_argc() == 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "usage: spk <vox sentence>" );
        return;
    }
    const std::string_view name = sound->impl_->cmd_cvar_->cmd_argv( 1 );
    if( !name.empty() && name.front() == '!' )
        sound->start_local_sound( name, k_vol_norm, true );
    else
    {
        const std::string sentence = std::string( "!#" ) + std::string( name );
        sound->start_local_sound( sentence, k_vol_norm, true );
    }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<Sound> create_sound( const SoundInitParams &params )
{
    auto sound = std::make_unique<Sound>();
    if( !sound->init( params ).has_value() )
        return nullptr;
    return sound;
}

} // namespace xash::sound
