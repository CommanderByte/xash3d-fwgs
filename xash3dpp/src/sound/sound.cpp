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

#include <xash3dpp/private/sound/audio_command.hpp>
#include <xash3dpp/private/sound/channel_alloc.hpp>
#include <xash3dpp/private/sound/dsp.hpp>
#include <xash3dpp/private/sound/mixer.hpp>
#include <xash3dpp/private/sound/owned_state.hpp>
#include <xash3dpp/private/sound/registry.hpp>
#include <xash3dpp/private/sound/topology.hpp>
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
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xash::sound {

namespace {

constexpr std::string_view k_log_tag = "sound";

// bound(min, num, max) verbatim (public/xash3d_mathlib.h:141) — duplicated
// locally per the mixer.cpp/vox.cpp/dsp.cpp/channel_alloc.cpp precedent.
[[nodiscard]] constexpr float bound_float( float lo, float v, float hi ) noexcept
{
    return v >= lo ? ( v < hi ? v : hi ) : lo;
}

// SilenceFillSource — the pull target Sound registers on its device while the
// S9.7b thread topology is NOT running. Nothing paints in that mode (exactly
// the S9.6 behaviour), so the device gets silence. Once start_topology() runs,
// the topology swaps in its RingFillSource (topology.hpp).
//
// @thread-safety: fill() runs on T_AudioCallback (real-time): no alloc, no
// lock, no block — a plain memset-shaped zero-fill (threading-model §Forbidden).
// The role is NOT asserted here (unlike RingFillSource): with the topology off
// this object is also what a synchronous SinkDevice::pump() call on T_Main
// reaches, and "the pipeline is not running, so the device gets silence" is a
// statement about the pipeline, not about which thread asked.  (Once the
// topology IS running the device reaches RingFillSource instead, which DOES
// assert AudioCallback — so the S9.8 witness pumps from a thread registered
// with that role.)
class SilenceFillSource final : public IAudioFillSource
{
public:
    void fill( std::span<std::int16_t> out ) noexcept override
    {
        for( std::int16_t &s : out )
            s = 0;
    }
};

// LockedSfxResolver — the ONE seam through which T_AudioDecoder reaches the
// (T_Main-owned) SfxRegistry: VOX word resolution happens at word-ADVANCE time
// inside the paint (VoxSystem::next_word -> load_word -> resolve), so it cannot
// be hoisted to T_Main the way S_LoadSound's plain-channel decode was.
//
// A plain std::mutex is the right tool here and NOT a threading-model
// violation: T_AudioDecoder is a background worker, not the real-time thread —
// the §Forbidden-audio-callback-patterns rule (no lock / no alloc / no I/O)
// binds T_AudioCallback only, and T_AudioCallback never touches this. A decoder
// stall shows up exactly where the design says it should: as a ring underrun on
// the always-on counter.
//
// @thread-safety: any thread. Every SfxRegistry access in this file goes
// through the same mutex, so registry mutation on T_Main and word resolution on
// T_AudioDecoder are serialized.
class LockedSfxResolver final : public IVoxAudioResolver
{
public:
    LockedSfxResolver( SfxRegistry *&registry, std::mutex &mutex ) noexcept
        : registry_( &registry ), mutex_( &mutex )
    {
    }

    // CONC-9: the mutex is held for the two SHORT table phases only, never
    // across the loader's blocking file I/O + decode. Holding it across the
    // decode inverted priority against the High-priority decoder that needs the
    // same mutex inside its paint; the audible symptom was ring underruns.
    [[nodiscard]] const AudioData *resolve( std::string_view path, bool &in_cache ) noexcept override
    {
        SfxRegistry::ResolveLookup lu;
        {
            const std::lock_guard<std::mutex> lock( *mutex_ );
            if( *registry_ == nullptr )
            {
                in_cache = false;
                return nullptr;
            }
            lu = ( *registry_ )->lookup( path );
        }

        in_cache = lu.in_cache;
        if( lu.handle == k_invalid_sound_handle )
            return nullptr; // !word->sfx (s_vox.c:150-151)
        if( lu.cached != nullptr )
            return lu.cached; // "see if still in memory" (s_load.c:110-111)

        // UNLOCKED: S_LoadSound's decode (s_vox.c:153).
        std::optional<AudioData> decoded =
            lu.loader != nullptr ? lu.loader->load( lu.name ) : std::nullopt;

        const std::lock_guard<std::mutex> lock( *mutex_ );
        if( *registry_ == nullptr )
            return nullptr; // registry torn down while we decoded (shutdown joins first, so unreachable in practice)
        return ( *registry_ )->install( lu.handle, std::move( decoded ) ); // double-checked
    }

    void release( const AudioData *data ) noexcept override
    {
        // Retained by design — SfxRegistry::release() is a documented no-op
        // (F-1/F-2/CONC-1). The seam is kept so the VOX call site's ownership
        // statement stays honest; see registry.cpp.
        const std::lock_guard<std::mutex> lock( *mutex_ );
        if( *registry_ != nullptr )
            ( *registry_ )->release( data );
    }

private:
    SfxRegistry **registry_; // @lifetime: points at Sound::Impl's own member
    std::mutex   *mutex_;    // @lifetime: Sound::Impl's own member
};

// register_cvar — the CVAR_DEFINE-equivalent field-by-field setup +
// cvar_register_engine call (context_init.cpp precedent; Cvar has a deleted
// copy-assignment via its atomic member, so fields are set individually).
// compliance-allow(thread-assert): init-time cvar registration over
// caller-owned Cvar storage; main-thread-only lifecycle by contract (sole
// caller Sound::init() asserts Main)
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
    // owned_loader_ holds the FilesystemAudioLoader we build ourselves; it is
    // EMPTY when SoundInitParams::audio_loader injected one instead. loader_ is
    // the borrowed pointer the registry actually uses, so exactly one code path
    // reads it either way.
    std::unique_ptr<IAudioLoader> owned_loader_;
    IAudioLoader                 *loader_ = nullptr; // @lifetime: owned_loader_ or the caller's injected loader
    std::unique_ptr<SfxRegistry>  registry_;
    std::unique_ptr<RoomDsp>      room_dsp_;

    ListenerSnapshot listener_ {};       // update_frame()'s published snapshot (SND-OQ-1)

    // ------------------------------------------------------------------
    // S9.7b additions — the command stream + thread topology.
    // ------------------------------------------------------------------

    // The channel-mutation context. Owned by whichever thread owns the channel
    // array: T_AudioDecoder while the topology runs, T_Main otherwise — NEVER
    // both. `total_channels` inside it is snd.total_channels (the static-range
    // high-water mark; was Impl::total_channels_ in S9.6).
    ChannelApplyContext apply_ctx_ {};

    // Serializes SfxRegistry access between T_Main and T_AudioDecoder's VOX
    // word resolution — see LockedSfxResolver above.
    std::mutex        registry_mutex_;
    SfxRegistry      *registry_raw_ = nullptr; // what the resolver adapter reads
    LockedSfxResolver vox_resolver_ { registry_raw_, registry_mutex_ };

    std::unique_ptr<AudioTopology> topology_;

    [[nodiscard]] bool threaded() const noexcept { return topology_ && topology_->running(); }

    // Route one command to whichever side owns the channel array. The SAME
    // apply_command() body runs in both modes (audio_command.cpp), which is
    // what makes the threaded and single-threaded paths identical.
    void dispatch( const AudioCommand &cmd ) noexcept
    {
        if( threaded() )
        {
            bool dropped = false;
            if( !topology_->submit( cmd, &dropped ) && dropped &&
                cmd.type == AudioCommandType::StartSound )
            {
                // SND-OQ-3 genuine drop of an actual SOUND.  Only StartSound
                // counts (parity F-4): AlterChannel/StopAllSounds/FrameUpdate/
                // FlushEpoch are CONTROL messages — a refused control message is
                // a pipeline fault, not a sound the player failed to hear, and
                // conflating the two would make the Tier-1 counter unreadable.
                stats_.dropped_sounds.fetch_add( 1, std::memory_order_relaxed );
            }
            return;
        }
        apply_command( apply_ctx_, cmd );
    }

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

    // Poll the §4.3 per-frame cvar set into the POD the decoder applies. Reads
    // cvars on T_Main only (SND-OQ-1); the values cross as a snapshot.
    [[nodiscard]] MixConfigSnapshot poll_mix_config() const noexcept
    {
        MixConfigSnapshot cfg {};
        if( cmd_cvar_ == nullptr )
            return cfg; // cvars_polled stays false — decoder leaves config alone

        auto &ctx           = *cmd_cvar_;
        cfg.cvars_polled    = true;
        cfg.lerping         = ctx.cvar_variable_value( "s_lerping" ) != 0.0f;
        cfg.room_off        = ctx.cvar_variable_value( "room_off" ) != 0.0f;
        // dsp_coeff_table: RAW cvar float, unmodified (dsp.hpp dual-coercion note).
        cfg.dsp_coeff_table = ctx.cvar_variable_value( "dsp_coeff_table" );
        cfg.room_type       = ctx.cvar_variable_value( "room_type" );
        cfg.waterroom_type  = ctx.cvar_variable_value( "waterroom_type" );
        cfg.hisound         = static_cast<int>( ctx.cvar_variable_value( "room_hires" ) );
        cfg.room_mod        = ctx.cvar_variable_value( "room_mod" );
        cfg.room_lp         = ctx.cvar_variable_value( "room_lp" );
        cfg.room_rvblp      = ctx.cvar_variable_value( "room_rvblp" );
        cfg.room_refl       = ctx.cvar_variable_value( "room_refl" );
        cfg.room_dlylp      = ctx.cvar_variable_value( "room_dlylp" );
        cfg.room_feedback   = ctx.cvar_variable_value( "room_feedback" );
        cfg.room_size       = ctx.cvar_variable_value( "room_size" );
        cfg.room_delay      = ctx.cvar_variable_value( "room_delay" );
        cfg.room_left       = ctx.cvar_variable_value( "room_left" );

        // ------------------------------------------------------------------
        // S_GetMasterVolume (s_main.c:115-135) — the paint gain.  APPLIED
        // HERE: the `volume` cvar and the snd_mute_losefocus focus gate.  The
        // derivation itself lives in master_volume_from() (audio_command.hpp),
        // term by term in legacy's float order, so it is unit-testable.
        //
        // XASH3DPP-STUB(chunk12): the soundfade term is NOT applied — its input
        // (soundfade.percent, produced by S_UpdateSoundFade, s_main.c:220-256)
        // has no implementation in this tree.  See master_volume_from()'s
        // `soundfade_scale` parameter, which is therefore left at its 1.0f
        // default rather than invented here.
        //
        // XASH3DPP-STUB(chunk12): MixGateSnapshot has NO producer either —
        // nothing in the tree supplies host.status/cls.key_dest/cl.paused/
        // cl.background/CL_IsInGame()/Host_IsSinglePlayerGame().  cfg.gate stays
        // default-constructed (every gate false), so the focus-mute branch below
        // is live but dormant: it starts working the moment a client-side
        // producer fills the gate POD, with no further change here.
        // ------------------------------------------------------------------
        cfg.master_volume = master_volume_from( ctx.cvar_variable_value( "volume" ), cfg.gate.lost_focus,
                                                ctx.cvar_variable_value( "snd_mute_losefocus" ) );

        return cfg;
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

    // An injected loader (Q-4) wins outright: it is a complete replacement for
    // the decode seam, not an addition to it, so no FilesystemAudioLoader is
    // built and `filesystem` is left unread.
    if( params.audio_loader != nullptr )
    {
        impl_->loader_ = params.audio_loader;
    }
    else if( params.filesystem != nullptr )
    {
        impl_->owned_loader_ = std::make_unique<FilesystemAudioLoader>( *params.filesystem );
        impl_->loader_       = impl_->owned_loader_.get();
    }
    impl_->registry_     = std::make_unique<SfxRegistry>( impl_->loader_ );
    impl_->registry_raw_ = impl_->registry_.get(); // what LockedSfxResolver reads

    // S9.7b: the single channel-mutation context both modes drive.
    impl_->apply_ctx_.mixer        = &impl_->mixer_;
    impl_->apply_ctx_.vox          = &impl_->vox_;
    impl_->apply_ctx_.room_dsp     = impl_->room_dsp_.get();
    impl_->apply_ctx_.stats        = &impl_->stats_;
    impl_->apply_ctx_.vox_resolver = &impl_->vox_resolver_;

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
    // establishes total_channels's starting value.
    free_all_channels( impl_->apply_ctx_ );

    // S9.7b: the topology object always exists (so start/stop are cheap), but
    // stays STOPPED unless asked for — a stopped topology is byte-for-byte the
    // S9.6 single-threaded pipeline.
    TopologyParams tp {};
    tp.ctx           = &impl_->apply_ctx_;
    tp.stats         = &impl_->stats_;
    tp.device        = impl_->device_;
    // CONC-6: ASK THE DEVICE instead of hardcoding true.  A device that drives
    // its own callback (SinkDevice::pump(), a real SDL backend) must not also
    // get an internal pump thread — that would be a second reader on the
    // single-consumer PCM ring, and it would put a wall clock back into the
    // path of the ratified determinism witness.
    tp.internal_pump    = !impl_->device_->drives_own_callback();
    tp.internal_decoder = !params.external_decoder;
    // compliance-allow(make-unique-outside-pimpl, unique-ptr-nonpimpl):
    // AudioTopology is built with
    // std::make_unique outside a pimpl Impl, against Q-22 rule 4 (conformance
    // F3).  Deliberate: the object is a thin owner of two fixed-storage
    // lock-free primitives (AudioCommandQueue + PcmRing, see topology.hpp) plus
    // two JoinHandles, and neither it nor they touch an allocator after
    // construction — so pool allocation buys nothing.  The pool's rules also do
    // not fit: no over-aligned-storage guarantee for the cache-line-padded
    // atomics it transitively contains, and the sound pool is destroyed at
    // shutdown AFTER this object must already be gone (SND-OQ-2 orders the
    // topology teardown first).  It is not a pimpl and is never handed out.
    impl_->topology_ = std::make_unique<AudioTopology>( tp );

    impl_->initialized_ = true;

    if( params.threaded && !start_topology().has_value() )
    {
        xash::core::log( xash::core::LogLevel::Error, k_log_tag, "audio topology start failed" );
        shutdown();
        return std::unexpected( SoundError::DeviceUnavailable );
    }

    return {};
}

// ---------------------------------------------------------------------------
// Thread topology (S9.7b)
// ---------------------------------------------------------------------------

Result<void> Sound::start_topology() noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ || !impl_->topology_ )
        return std::unexpected( SoundError::NotInitialized );
    if( impl_->topology_->running() )
        return {};

    if( !impl_->topology_->start() )
        return std::unexpected( SoundError::DeviceUnavailable );

    // Re-publish the current T_Main-side configuration so the freshly-started
    // decoder is not running on default-constructed gates until the next frame.
    AudioCommand cmd {};
    cmd.type       = AudioCommandType::FrameUpdate;
    cmd.listener   = impl_->listener_;
    cmd.mix_config = impl_->poll_mix_config();
    impl_->dispatch( cmd );
    return {};
}

void Sound::stop_topology() noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->topology_ )
        return;
    impl_->topology_->stop();

    // Back to the S9.6 pull target: nothing paints while the topology is
    // stopped, so the device gets silence.
    if( impl_->device_ != nullptr && impl_->initialized_ )
        impl_->device_->set_fill_source( &impl_->silence_ );
}

bool Sound::topology_running() const noexcept
{
    return impl_ && impl_->topology_ && impl_->topology_->running();
}

bool Sound::flush() noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    // No topology object at all == no decoder == no borrower: quiesced by
    // construction, so the licence is granted (matching AudioTopology::flush()'s
    // own not-running case).
    return impl_->topology_ == nullptr || impl_->topology_->flush();
}

bool Sound::decoder_step() noexcept
{
    // NO assert_thread_role(Main) here, deliberately: this is the decoder side.
    // AudioTopology::decoder_step() asserts ThreadRole::AudioDecoder and
    // Mixer::paint_channels() asserts it again underneath, so an owner that
    // drives this from the wrong thread aborts at the same site the spawned
    // decoder thread would have.
    if( impl_->topology_ == nullptr || !impl_->topology_->running() )
        return false;
    return impl_->topology_->decoder_step();
}

void Sound::shutdown()
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    // SND-OQ-2 step 1: the topology goes down FIRST. stop() stops accepting
    // commands, detaches the device, joins T_AudioCallback then T_AudioDecoder
    // — so by the time the registry (and every AudioData it owns) is destroyed
    // below, no other thread exists that could be holding a reference. This is
    // ordering by JOIN, the strongest form of the epoch guarantee.
    stop_topology();
    impl_->topology_.reset();

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
    free_all_channels( impl_->apply_ctx_ );

    impl_->apply_ctx_ = ChannelApplyContext{};
    impl_->room_dsp_.reset();   // frees pool-backed PoolIntBuffer members — MUST run before destroy_pool
    impl_->registry_raw_ = nullptr;
    impl_->registry_.reset();   // heap-owned AudioData caches; no pool dependency
    impl_->loader_ = nullptr;   // borrowed (injected or owned_loader_'s) — drop before the owner
    impl_->owned_loader_.reset();

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
    const std::lock_guard<std::mutex> lock( impl_->registry_mutex_ );
    return impl_->registry_->register_sound( name ); // S_RegisterSound (s_load.c:312-336)
}

void Sound::start_sound( std::optional<Vec3> pos, int ent, int chan, ::xash::abi::sound_t handle, float fvol,
                         float attn, int pitch, std::uint32_t flags ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ || !impl_->registry_ )
        return;

    Impl &impl = *impl_;

    // ------------------------------------------------------------------
    // T_Main half (S9.7b): everything that needs the registry, the providers
    // or the listener pose. The result is a POD AudioCommand; the channel-side
    // half (alter prologue -> pick -> init -> bind -> spatialize -> audibility
    // drop) is apply_start() in audio_command.cpp, which runs verbatim on
    // whichever thread owns the channel array.
    // ------------------------------------------------------------------
    AudioCommand cmd {};
    cmd.type = AudioCommandType::StartSound;

    // Set when the sfx still needs decoding — the decode itself runs BELOW,
    // with registry_mutex_ RELEASED (CONC-9).
    SfxHandle   decode_handle = k_invalid_sound_handle;
    std::string decode_name;
    IAudioLoader *decode_loader = nullptr;

    {
        const std::lock_guard<std::mutex> lock( impl.registry_mutex_ );
        const SfxSlot                    *sfx = impl.registry_->get( handle );
        if( sfx == nullptr )
            return; // !sfx (s_main.c:635-636)
        set_command_name( cmd, sfx->name );
        cmd.sfx_is_sentence = test_sound_char( sfx->name, '!' ); // s_main.c:692

        if( !cmd.sfx_is_sentence )
        {
            // S_LoadSound(sfx) (s_main.c:707) — hoisted to T_Main because the
            // registry is T_Main-owned. SfxRegistry reserves slots_ to
            // sound_max_sfx at construction and never destroys a cache entry
            // (SfxSlot::cache's address-stability invariant), so the returned
            // AudioData address is valid for the registry's whole lifetime —
            // which is what makes it safe to put on the command queue.
            cmd.source = impl.registry_->cached( handle );
            if( cmd.source == nullptr )
            {
                decode_handle = handle;
                decode_name   = sfx->name;
                decode_loader = impl.registry_->loader();
            }
        }
    }

    if( decode_handle != k_invalid_sound_handle )
    {
        // UNLOCKED decode (CONC-9): blocking file I/O + codec work must never
        // run under registry_mutex_ — the High-priority decoder needs that same
        // mutex inside its paint, and the inversion surfaces as ring underruns.
        std::optional<AudioData> decoded =
            decode_loader != nullptr ? decode_loader->load( decode_name ) : std::nullopt;

        const std::lock_guard<std::mutex> lock( impl.registry_mutex_ );
        // Double-checked install: if the decoder resolved the same sfx while we
        // were decoding, ours is discarded and we use the winner's (stable)
        // pointer. Either way the address never changes afterwards.
        cmd.source = impl.registry_->install( decode_handle, std::move( decoded ) );
    }

    const int vol = static_cast<int>( bound_float( 0.0f, fvol * 255.0f, 255.0f ) ); // s_main.c:638
    if( pitch <= 1 )
        pitch = k_pitch_norm_flag; // "Invasion issues" (s_main.c:639)

    cmd.sfx_handle = handle;
    cmd.entnum     = ent;
    cmd.entchannel = chan;
    cmd.vol        = vol;
    cmd.pitch      = pitch;
    cmd.flags      = flags;
    cmd.dist_mult  = attn / k_sound_clip_distance; // s_main.c:683

    // NULL pos -> refState.vieworg (s_main.c:651). This port has no separate
    // refState surface; ListenerSnapshot::origin (the SAME S_UpdateFrame
    // publish point legacy's refState.vieworg update shares a source with) is
    // the closest available approximation — see the S9.6 uncertainty list.
    cmd.origin   = pos.value_or( impl.listener_.origin );
    cmd.listener = impl.listener_;

    // SND-OQ-1: the ONLY provider read. spatialize_needs_provider() reproduces
    // SND_Spatialize's own early-outs (s_main.c:568-586) so we make exactly the
    // calls legacy would, and only the RESULT crosses to the mix side.
    const std::uint32_t chan_flags_for_spatialize =
        ( ent == 0 ) ? ::xash::abi::k_fl_chan_static_sound : 0u; // s_main.c:674-675
    if( spatialize_needs_provider( ent, impl.listener_.entnum, chan_flags_for_spatialize ) )
    {
        // IN/OUT (F-8, cl_frame.c:1387-1392): pre-fill with the origin the
        // channel is about to be given (`pos`, or the listener view origin),
        // because the provider may legally return true WITHOUT writing it — and
        // legacy then plays the sound at exactly that pre-existing origin
        // instead of silencing it. See providers.hpp for the full contract.
        cmd.entity_origin = cmd.origin;
        cmd.entity_origin_valid =
            impl.spatial_ != nullptr && impl.spatial_->resolve_origin( ent, cmd.entity_origin );
    }

    impl.dispatch( cmd );

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

    AudioCommand cmd {};
    cmd.type = AudioCommandType::AlterChannel;

    {
        const std::lock_guard<std::mutex> lock( impl.registry_mutex_ );

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

        cmd.sfx_handle = sfx;
        set_command_name( cmd, slot->name );
    }

    cmd.entnum     = entnum;
    cmd.entchannel = channel;
    cmd.vol        = 0;
    cmd.pitch      = 0;
    cmd.flags      = k_snd_stop; // s_main.c:1457

    impl.dispatch( cmd ); // SND-OQ-3 reserved lane — a stop is never dropped
}

void Sound::stop_all_sounds( bool ambient ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    // s_main.c:1470-1480 (channel teardown + SX_ClearState) both live on the
    // channel-owning side. `ambient` (the S_InitAmbientChannels restart,
    // s_main.c:1486) is carried for signature parity and remains a no-op, as
    // in S9.6.
    AudioCommand cmd {};
    cmd.type    = AudioCommandType::StopAllSounds;
    cmd.ambient = ambient;
    impl_->dispatch( cmd ); // SND-OQ-3 reserved lane

    impl_->fade_state_ = FadeState{}; // memset(&soundfade,0,...) (s_main.c:1491) — T_Main state
}

void Sound::update_frame( const ListenerSnapshot &snapshot ) noexcept
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

    impl_->listener_ = snapshot; // S_UpdateFrame's publish point (s_main.c:1590-1598)

    // The waterlevel -> RoomDsp handoff and the §4.3 per-frame cvar poll both
    // target decoder-owned state, so they ship as one FrameUpdate command. The
    // cvars themselves are read HERE, on T_Main (SND-OQ-1).
    AudioCommand cmd {};
    cmd.type       = AudioCommandType::FrameUpdate;
    cmd.listener   = snapshot;
    cmd.mix_config = impl_->poll_mix_config();
    impl_->dispatch( cmd );

    // SND-OQ-1 reverse channel: drain the relaxed-atomic mouth slots the
    // decoder publishes into, on T_Main, once per frame. (No producer exists
    // yet — the mixer's mouth write-back is still deferred — so this is live
    // plumbing with a dormant source, not dead code by intent.)
    if( impl_->topology_ )
        impl_->topology_->mouth_slots().drain( impl_->mouth_ );
}

std::vector<ChannelInfo> Sound::channels_snapshot() const
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    std::vector<ChannelInfo> result;
    if( !impl_->initialized_ )
        return result;

    const int dynamic_hi = static_cast<int>( ::xash::limits::sound_num_ambient_channels +
                                             ::xash::limits::sound_num_dynamic_channels );

    // Resolve a registry name on T_Main (the registry never leaves this
    // thread), matching S_GetCurrentStaticSounds/S_GetCurrentDynamicSounds'
    // rule (s_main.c:989-992,1039-1042): a bound sentence uses ch->name; else
    // the registered sfx's own name.
    const auto resolve_name = [this]( bool is_sentence, const std::string &sentence_name,
                                      ::xash::abi::sound_t handle ) -> std::string {
        if( is_sentence && !sentence_name.empty() )
            return sentence_name;
        if( !impl_->registry_ || handle == k_invalid_sound_handle )
            return {};
        const std::lock_guard<std::mutex> lock( impl_->registry_mutex_ );
        const SfxSlot                    *slot = impl_->registry_->get( handle );
        return slot != nullptr ? slot->name : std::string{};
    };

    // Threaded mode: T_Main must NOT read Mixer::channels() — the decoder owns
    // it. Ask the decoder for a snapshot instead (P-4 surface, cold path).
    if( impl_->topology_ && impl_->topology_->running() )
    {
        std::vector<PublishedChannel> published;
        if( impl_->topology_->channel_snapshot( published ) )
        {
            for( const PublishedChannel &p : published )
            {
                ChannelInfo info;
                info.entnum           = p.entnum;
                info.origin           = p.origin;
                info.left_vol         = p.leftvol;
                info.right_vol        = p.rightvol;
                info.position_samples = p.sample;
                info.channel_class =
                    ( static_cast<int>( p.index ) < dynamic_hi ) ? ChannelClass::Dynamic : ChannelClass::Static;
                info.sfx_name = resolve_name( p.is_sentence, p.sentence_name, p.sfx_handle );
                result.push_back( std::move( info ) );
            }
            return result;
        }
    }

    auto &channels = impl_->mixer_.channels();

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

        info.sfx_name = resolve_name( ch.is_sentence, ch.name, ch.sfx_handle );

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
    // Every SfxRegistry read takes registry_mutex_ — including this cosmetic
    // one. T_AudioDecoder can be inside SfxRegistry::resolve -> find_name,
    // which inserts into slots_/by_name_; an unlocked count() races that
    // insertion (an unordered_map rehash is not a benign read).
    std::size_t count = 0;
    {
        const std::lock_guard<std::mutex> lock( sound->impl_->registry_mutex_ );
        count = sound->impl_->registry_->count();
    }
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_log_tag, "%zu registered sound(s)",
                        count ); // S_SoundList_f (s_load.c:40-73), summary only
}

void Sound::cmd_s_info_f( void *user ) noexcept
{
    auto     *sound = static_cast<Sound *>( user );
    // total_channels lives on whichever side owns the channel array; while the
    // topology runs the decoder publishes it to a relaxed atomic (cosmetic
    // read, never a correctness input).
    const int total = sound->impl_->threaded() ? sound->impl_->topology_->total_channels()
                                               : sound->impl_->apply_ctx_.total_channels;
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_log_tag, "%d samples | %d total_channels",
                        sound->impl_->dma_.samples, total ); // S_SoundInfo_f (s_main.c:1896-1906), summary only
}

void Sound::cmd_dsp_profile_f( void *user ) noexcept
{
    auto *sound = static_cast<Sound *>( user );
    if( sound->impl_->cmd_cvar_ == nullptr || sound->impl_->room_dsp_ == nullptr )
        return;

    // DELIBERATE RESTRICTION (parity F-3 / concurrency CONC-2, confirmed):
    // RoomDsp is DECODER-OWNED once the topology runs, and profile() mutates
    // every delay line in place — running it from this T_Main console handler
    // would race the decoder painting through the same object.
    //
    // Refusing is the right answer rather than routing it through the command
    // stream: dsp_profile is a debug STRESS tool (10,000 DSP iterations over a
    // 512-sample buffer), and making the decoder run it would stall the paint —
    // and therefore the ring — for the whole duration. The measurement is also
    // only meaningful on an otherwise-idle DSP, which a live topology is not.
    if( sound->topology_running() )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag,
                           "dsp_profile is unavailable while the audio topology is running" );
        return;
    }

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
