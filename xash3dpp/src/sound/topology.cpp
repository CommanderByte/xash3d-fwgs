// xash3dpp — sound thread topology implementation (Chunk 9, slice S9.7b).
// Legacy reference: none (legacy is single-threaded — see topology.hpp).
// Boundary: sound-boundary.md §Threading; SND-OQ-2/3/5.
//
// Existing subsystems used:
//   xash3dpp_core     — thread-role assertions, SpscRing/MpscQueue, logging
//   xash3dpp_platform — spawn_thread (the Q-24 OS-boilerplate half)

#include <xash3dpp/private/sound/topology.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/platform/platform.hpp> // sleep (Platform_Sleep)

#include <algorithm>
#include <thread> // std::this_thread::yield

namespace xash::sound {

namespace {

constexpr std::string_view k_log_tag = "sound";

} // namespace

// ===========================================================================
// RingFillSource — T_AudioCallback
// ===========================================================================

void RingFillSource::fill( std::span<std::int16_t> out ) noexcept
{
    // The one assert on the real-time path: a TLS load + compare.  Everything
    // else here is wait-free (threading-model §5.2 / §Forbidden audio-callback
    // patterns): no lock, no allocation, no logging, no syscall.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::AudioCallback );

    // CONC-5: mark this thread as INSIDE fill() for the whole body, so
    // AudioTopology::stop() can wait out an OS callback it cannot join before
    // destroying the ring.  Still wait-free — one fetch_add, one fetch_sub.
    // The single-exit shape below is deliberate: an early return would have to
    // duplicate the decrement.
    in_callback_.fetch_add( 1, std::memory_order_acquire );

    PcmRing          *ring = ring_.load( std::memory_order_acquire );
    const std::size_t got  = ring != nullptr ? ring->read( out ) : 0;
    if( got != out.size() )
    {
        // Underrun: the decoder fell behind.  Silence the tail and bump the
        // ALWAYS-ON counter (threading-model §5.2: "This counter is always-on
        // (no XASH_STATS guard) because audio underruns are always a bug").
        for( std::size_t i = got; i < out.size(); ++i )
            out[i] = 0;
        stats_->underruns.fetch_add( 1, std::memory_order_relaxed );
    }

    // Release: every read of *ring above happens-before stop() observes zero.
    in_callback_.fetch_sub( 1, std::memory_order_release );
}

// ===========================================================================
// MouthSlots — SND-OQ-1 reverse channel
// ===========================================================================

void MouthSlots::set_mouth_open( int entnum, int mouthopen ) noexcept
{
    // Producer half of the SND-OQ-1 reverse channel — mix-side only.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::AudioDecoder );

    const std::size_t idx =
        static_cast<std::size_t>( static_cast<std::uint32_t>( entnum ) ) % slots_.size();
    // ONE store of the whole pair (CONC-4): the drain can never observe this
    // entnum next to a different publish's amplitude.
    slots_[idx].store( pack( entnum, mouthopen ), std::memory_order_relaxed );
}

void MouthSlots::drain( IMouthSink *out ) noexcept
{
    // Consumer half — T_Main once per frame (Sound::update_frame).  `seen_` is
    // T_Main-private, so this assert is what makes that ownership claim real.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for( std::size_t i = 0; i < slots_.size(); ++i )
    {
        const std::uint64_t packed = slots_[i].load( std::memory_order_relaxed );
        if( packed == seen_[i] )
            continue; // unchanged since the last drain (a re-publish of the SAME
                      // pair is an idempotent no-op for the sink)
        seen_[i] = packed;
        if( out != nullptr )
            out->set_mouth_open( packed_entnum( packed ), packed_mouthopen( packed ) );
    }
}

// ===========================================================================
// AudioTopology
// ===========================================================================

AudioTopology::AudioTopology( const TopologyParams &params ) noexcept
    : ctx_( params.ctx ), stats_( params.stats ), device_( params.device ),
      internal_pump_( params.internal_pump ), internal_decoder_( params.internal_decoder ),
      fill_( *params.stats )
{
}

AudioTopology::~AudioTopology() { stop(); }

// ---------------------------------------------------------------------------
// Thread bodies
// ---------------------------------------------------------------------------

void AudioTopology::decoder_thread_main( void *user ) noexcept
{
    auto *self = static_cast<AudioTopology *>( user );
    // spawn_thread already registered ThreadRole::AudioDecoder as this
    // thread's FIRST action (platform/thread.hpp); decoder_step() asserts it.
    std::size_t idle_spins = 0;
    while( self->decoder_running_.load( std::memory_order_acquire ) )
    {
        if( self->decoder_step() )
        {
            idle_spins = 0;
            continue;
        }
        // Spin-then-park: yield a bounded number of times so a freshly-submitted
        // command is applied in microseconds, then give the core back.
        if( idle_spins < ::xash::limits::sound_decoder_idle_spin_max )
        {
            ++idle_spins;
            std::this_thread::yield();
            continue;
        }
        ::xash::platform::sleep( ::xash::limits::sound_decoder_idle_sleep_ms );
    }

    // Final drain so nothing submitted before set_accepting(false) is lost —
    // in particular the FlushEpoch a concurrent flush() may still be waiting
    // on (shutdown step 4).
    while( self->decoder_step() )
    {
    }
}

void AudioTopology::pump_thread_main( void *user ) noexcept
{
    auto *self = static_cast<AudioTopology *>( user );
    // Role AudioCallback registered by spawn_thread; fill() asserts it.
    // Pacing: one buffer's worth of wall clock per pull, so the pump consumes
    // the ring at roughly the device rate (>=1 ms so a stopped topology joins
    // promptly).
    const std::uint32_t interval_ms = static_cast<std::uint32_t>(
        std::max<std::uint64_t>( 1, static_cast<std::uint64_t>( ::xash::limits::sound_device_pump_frames ) * 1000ULL /
                                        static_cast<std::uint64_t>( ::xash::limits::sound_dma_speed ) ) );

    while( self->pump_running_.load( std::memory_order_acquire ) )
    {
        self->fill_.fill( std::span<std::int16_t>( self->pump_scratch_ ) );
        ::xash::platform::sleep( interval_ms );
    }
}

// ---------------------------------------------------------------------------
// Lifecycle (T_Main)
// ---------------------------------------------------------------------------

bool AudioTopology::start() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if( running_.load( std::memory_order_acquire ) )
        return true;
    if( ctx_ == nullptr || ctx_->mixer == nullptr || stats_ == nullptr )
        return false;

    // Pre-allocate the callback scratch: the T_AudioCallback path must never
    // allocate (2 int16 per stereo frame).
    pump_scratch_.assign( ::xash::limits::sound_device_pump_frames * 2, std::int16_t{ 0 } );

    // Fresh primitives per run — see the queue_/ring_ declaration note in
    // topology.hpp for the full Q-22 rule 4 / conformance F3 rationale.
    // compliance-allow(make-unique-outside-pimpl, unique-ptr-nonpimpl):
    // fixed-storage lock-free primitives that never touch an allocator after
    // construction, so pool allocation buys nothing, and the pool's
    // alignment/lifetime rules do not fit them (over-aligned padded atomics;
    // per-run lifetime vs. the pool's session lifetime).
    queue_ = std::make_unique<AudioCommandQueue>();
    // compliance-allow(make-unique-outside-pimpl, unique-ptr-nonpimpl): as above.
    ring_ = std::make_unique<PcmRing>();
    fill_.set_ring( ring_.get() );

    // Register the ring as the device's pull target (Q-23).  A real backend
    // drives fill() from its own callback thread; the devices in the tree today
    // do not, hence the internal pump below.
    if( device_ != nullptr )
    {
        device_->set_fill_source( &fill_ );
        device_->set_active( true );
    }

    decoder_running_.store( true, std::memory_order_release );
    if( internal_decoder_ )
        decoder_thread_ = ::xash::platform::spawn_thread( ::xash::core::ThreadRole::AudioDecoder,
                                                          "xash-audio-decoder",
                                                          ::xash::platform::ThreadPriority::High,
                                                          &AudioTopology::decoder_thread_main, this );

    if( internal_pump_ )
    {
        pump_running_.store( true, std::memory_order_release );
        pump_thread_ = ::xash::platform::spawn_thread( ::xash::core::ThreadRole::AudioCallback,
                                                       "xash-audio-callback",
                                                       ::xash::platform::ThreadPriority::Realtime,
                                                       &AudioTopology::pump_thread_main, this );
    }

    running_.store( true, std::memory_order_release );
    ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "audio topology started" );
    return true;
}

void AudioTopology::stop() noexcept
{
    // T_Main: every path here is main-thread — Sound::stop_topology()/shutdown()
    // and ~AudioTopology (which runs from ~Sound::Impl on the owning thread).
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if( !running_.load( std::memory_order_acquire ) )
    {
        // Nothing running, but a half-started topology may still own threads
        // (spawn failure path); joining a default JoinHandle is a no-op.
        decoder_thread_.join();
        pump_thread_.join();
        return;
    }

    // (1) Stop accepting commands.  Also releases any producer parked in the
    //     SND-OQ-3 bounded block (submit() re-checks accepting() each spin).
    if( queue_ )
        queue_->set_accepting( false );

    // (2) Detach from the device so an EXTERNAL (OS-driven) callback can no
    //     longer enter fill().
    if( device_ != nullptr )
    {
        device_->set_active( false );
        device_->set_fill_source( nullptr );
    }

    // (3) Join the device pump — after this there is no ring READER.
    pump_running_.store( false, std::memory_order_release );
    pump_thread_.join();

    // (4) Join the decoder — after this there is no ring WRITER, no channel
    //     mutation, and (via the loop's final drain) every queued command has
    //     been applied.
    decoder_running_.store( false, std::memory_order_release );
    decoder_thread_.join();

    // (4b) CONC-5: an EXTERNAL (OS-driven) audio callback is not joinable.
    //      Step (2) detached the fill source so no NEW callback can enter
    //      fill(), but one may already be inside it, reading *ring_.  Wait it
    //      out before the ring is destroyed.
    //      POST-CONDITION on exit: no thread is executing RingFillSource::fill()
    //      and none can enter, so ring_.reset() below cannot free memory a
    //      callback is reading.
    //      With internal_pump == true this is already guaranteed by the join at
    //      step (3) and the loop below spins zero times.
    {
        std::size_t spin = 0;
        while( fill_.in_callback() != 0 )
        {
            if( ++spin >= ::xash::limits::sound_callback_quiesce_spin_max )
            {
                ::xash::core::log( ::xash::core::LogLevel::Error, k_log_tag,
                                   "audio callback did not return; tearing down the ring anyway" );
                break;
            }
            std::this_thread::yield();
        }
    }

    // (5) Both users are quiesced: the ring and the callback scratch may be
    //     torn down / reused safely.  The decoder's last act was the loop's
    //     final drain, so any pending FlushEpoch has been acked; publish the
    //     final epoch unconditionally so a flush() racing shutdown cannot spin.
    acked_epoch_.store( submitted_epoch_, std::memory_order_release );
    fill_.set_ring( nullptr );
    ring_.reset();
    queue_.reset();
    pump_scratch_.clear();
    pump_scratch_.shrink_to_fit();

    running_.store( false, std::memory_order_release );
    ::xash::core::log( ::xash::core::LogLevel::Info, k_log_tag, "audio topology stopped" );
}

// ---------------------------------------------------------------------------
// Command stream (T_Main)
// ---------------------------------------------------------------------------

bool AudioTopology::submit( const AudioCommand &cmd, bool *out_dropped ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return queue_ && queue_->submit( cmd, out_dropped );
}

bool AudioTopology::flush() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if( !running_.load( std::memory_order_acquire ) )
        return true; // no decoder -> no borrower -> nothing to fence, licence granted

    AudioCommand cmd {};
    cmd.type  = AudioCommandType::FlushEpoch;
    cmd.epoch = ++submitted_epoch_;

    // Reserved class (command_class): a flush cannot be refused by a
    // START-saturated NORMAL region.  It can still be refused two ways, and
    // both are REAL failures the caller has to see (CONC-3) — the previous
    // "only reachable if the queue was closed" comment was wrong:
    //   • the RESERVE region itself is saturated (a decoder that is not
    //     draining at all), or
    //   • set_accepting(false) ran concurrently (shutdown).
    if( !queue_ || !queue_->submit( cmd ) )
        return false;

    for( std::size_t spin = 0; spin < ::xash::limits::sound_flush_spin_max; ++spin )
    {
        // Acquire: pairs with the decoder's release store of the ack —
        // observing it means every decoder-side dereference of borrowed audio
        // that could still concern this epoch is sequenced before it.
        if( acked_epoch_.load( std::memory_order_acquire ) >= cmd.epoch )
            return true;
        if( !running_.load( std::memory_order_acquire ) )
        {
            // A concurrent stop() joined the decoder and published the final
            // epoch; re-read to see whether ours made it in under the wire.
            return acked_epoch_.load( std::memory_order_acquire ) >= cmd.epoch;
        }
        std::this_thread::yield();
    }

    ::xash::core::log( ::xash::core::LogLevel::Error, k_log_tag,
                       "SND-OQ-2 flush timed out waiting for the decoder epoch ack" );
    return false;
}

// ---------------------------------------------------------------------------
// Decoder (T_AudioDecoder)
// ---------------------------------------------------------------------------

bool AudioTopology::decoder_step() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::AudioDecoder );

    bool did_work = false;

    // --- 1. drain the command stream (FIFO) ---------------------------------
    AudioCommand cmd;
    while( queue_ && queue_->try_pop( cmd ) )
    {
        apply_command( *ctx_, cmd );
        if( cmd.type == AudioCommandType::FlushEpoch )
        {
            // Release: everything apply_command() did for this flush (the
            // free_all_channels() that dropped every borrowed AudioData
            // pointer) happens-before any T_Main acquire-load of this value.
            acked_epoch_.store( cmd.epoch, std::memory_order_release );
        }
        did_work = true;
    }

    // --- 2. paint one block into the ring -----------------------------------
    // space() UNDER-estimates the free room (occupancy() over-estimates by
    // construction), so a write sized against it always fits — the short-write
    // path below is a defensive backstop, not the expected stall shape.  The
    // real graceful stall is `frames == 0`: we simply do not paint this step.
    const std::size_t free_samples = ring_ ? ring_->space() : 0;
    const std::size_t frames =
        std::min<std::size_t>( ::xash::limits::sound_decoder_block_frames, free_samples / 2 );

    if( frames > 0 )
    {
        Mixer    &mixer   = *ctx_->mixer;
        const int endtime = mixer.painted_time() + static_cast<int>( frames );

        // paint_channels advances painted_time_ itself (s_mix.c:542's loop).
        const std::span<const std::int16_t> pcm = mixer.paint_channels(
            endtime, ctx_->mix_config.gate, ctx_->mix_config.master_volume, ctx_->mix_config.pitch_mult );

        const std::size_t written = ring_->try_write( pcm );
        XASH_ASSERT( written == pcm.size() ); // guaranteed by the space() gate above
        (void)written;

        stats_->mix_blocks.fetch_add( 1, std::memory_order_relaxed );

        std::uint32_t active = 0;
        for( const MixChannel &ch : mixer.channels() )
            if( ch.source != nullptr )
                ++active;
        stats_->active_channels.store( active, std::memory_order_relaxed );
#if XASH_STATS
        if( active > stats_->peak_active_channels.load( std::memory_order_relaxed ) )
            stats_->peak_active_channels.store( active, std::memory_order_relaxed );
#endif
        did_work = true;
    }

    // --- 3. publish decoder-owned state main-ward ---------------------------
    total_channels_.store( ctx_->total_channels, std::memory_order_relaxed );
    publish_channels_if_requested();

    return did_work;
}

void AudioTopology::publish_channels_if_requested() noexcept
{
    if( !snapshot_request_.exchange( false, std::memory_order_acq_rel ) )
        return;

    std::vector<PublishedChannel> snap;
    const std::vector<MixChannel> &channels = ctx_->mixer->channels();
    for( std::size_t i = 0; i < channels.size(); ++i )
    {
        const MixChannel &ch = channels[i];
        if( ch.source == nullptr )
            continue;
        PublishedChannel p;
        p.index         = i;
        p.sfx_handle    = ch.sfx_handle;
        p.entnum        = ch.entnum;
        p.origin        = ch.origin;
        p.leftvol       = ch.leftvol;
        p.rightvol      = ch.rightvol;
        p.sample        = ch.sample;
        p.is_sentence   = ch.is_sentence;
        p.sentence_name = ch.name;
        snap.push_back( std::move( p ) );
    }

    {
        const std::lock_guard<std::mutex> lock( publish_mutex_ );
        published_ = std::move( snap );
    }
    publish_seq_.fetch_add( 1, std::memory_order_release );
}

// ---------------------------------------------------------------------------
// Introspection (T_Main)
// ---------------------------------------------------------------------------

bool AudioTopology::channel_snapshot( std::vector<PublishedChannel> &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if( !running_.load( std::memory_order_acquire ) )
        return false;

    const std::uint64_t before = publish_seq_.load( std::memory_order_acquire );
    snapshot_request_.store( true, std::memory_order_release );

    for( std::size_t spin = 0; spin < ::xash::limits::sound_snapshot_spin_max; ++spin )
    {
        if( publish_seq_.load( std::memory_order_acquire ) != before )
            break;
        if( !running_.load( std::memory_order_acquire ) )
            break;
        std::this_thread::yield();
    }

    const std::lock_guard<std::mutex> lock( publish_mutex_ );
    out = published_;
    return true;
}

} // namespace xash::sound
