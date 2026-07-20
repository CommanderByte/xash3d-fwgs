// xash3dpp — Sound: audio engine core implementation (Chunk 9, slice S9.1).
// Legacy reference: engine/client/sound/s_main.c (S_Init/S_Shutdown lifecycle).
// This slice wires ONLY the lifecycle + device open/close (boundary S9.1);
// the mixer, codec, and channel plumbing land in S9.2/S9.3.
//
// Existing subsystems used:
//   xash3dpp_core   — thread-role assertions + structured logging
//   xash3dpp_memory — pool-backed allocations (create_pool / destroy_pool)

#include <xash3dpp/sound/sound.hpp>

#include <xash3dpp/private/sound/owned_state.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace xash::sound {

namespace {

constexpr std::string_view k_log_tag = "sound";

// SilenceFillSource — the placeholder pull target Sound registers on its device
// until the mixer lands (S9.2).  Zero-fills the request: the pipeline is wired
// end-to-end (a SinkDevice can pump through it) but produces silence.
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

    // Wire the pull source (silence until the S9.2 mixer replaces it) and gate
    // output on (SNDDMA_Activate).
    impl_->device_->set_fill_source( &impl_->silence_ );
    impl_->device_->set_active( true );

    // Retain the SND-OQ-1 seams for the mix side (S9.2+).
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

    impl_->initialized_ = true;
    return {};
}

void Sound::shutdown()
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    if( !impl_->initialized_ )
        return;

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
    impl_->initialized_ = false;
}

bool Sound::initialized() const noexcept { return impl_->initialized_; }

const SoundStats &Sound::stats() const noexcept { return impl_->stats_; }

IAudioDevice *Sound::device() const noexcept { return impl_->device_; }

const DeviceCaps &Sound::caps() const noexcept { return impl_->caps_; }

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
