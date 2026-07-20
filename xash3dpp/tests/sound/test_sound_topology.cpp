// xash3dpp — unit tests for the sound thread topology (Chunk 9, slice S9.7b):
//   the POD AudioCommand stream + the SND-OQ-3 admission policy
//   (private/sound/audio_command.hpp) and the
//   T_Main -> MPSC -> T_AudioDecoder -> SPSC ring -> T_AudioCallback chain
//   (private/sound/topology.hpp).
//
// Coverage (mapped to the S9.7b gates):
//   1. AudioCommand POD shape + the SND-OQ-3 class map (a START that CARRIES
//      stop/change bits is reserved-class, not normal).
//   2. SND-OQ-3, DIRECTLY: a STOP-class command survives a START-saturated
//      queue; the overflowing START is the only thing reported dropped.
//   3. Command round-trip: StartSound -> an audible, correctly-spatialized
//      channel; AlterChannel(SND_STOP) frees it again.
//   4. Ring underrun on T_AudioCallback: silence out AND the always-on
//      `underruns` counter bumped (threading-model §5.2).
//   5. Deterministic decoder drive (no internal threads): commands cross the
//      real MPSC and painted PCM lands in the real SPSC ring.
//   6. SND-OQ-2 stop-and-free race, driven hard in a loop: audio freed on
//      T_Main immediately after a flush() ack is never touched by a
//      mid-paint decoder.
//   7. Clean start/stop of the topology many times over — no leak, no join
//      hang, primitives re-created per run.
//   8. Shutdown ordering: queue closed first, epoch settled, submits refused.
//   9. Threaded soak through the real Sound entry surface with debug asserts
//      live on both arches.
//  10. S_GetMasterVolume's derivation (parity F-5 / conformance F4).
//  11. FrameUpdate is CONTROL, not a droppable sound (parity F-4).
//  12. flush() reports failure instead of failing open (concurrency CONC-3).
//  13. MouthSlots publishes an untearable (entnum, mouthopen) pair (CONC-4).
//  14. The external-callback quiesce counter stop() waits on (CONC-5).

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/platform/platform.hpp>
#include <xash3dpp/private/sound/audio_command.hpp>
#include <xash3dpp/private/sound/mixer.hpp>
#include <xash3dpp/private/sound/topology.hpp>
#include <xash3dpp/private/sound/vox.hpp>
#include <xash3dpp/sound/constants.hpp>
#include <xash3dpp/sound/sound.hpp>

#include "../test_helpers.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <vector>

static int g_pass = 0, g_fail = 0;

using xash::core::ThreadRole;
using namespace xash::sound;

namespace {

constexpr int k_dynamic_hi = static_cast<int>( ::xash::limits::sound_num_ambient_channels +
                                               ::xash::limits::sound_num_dynamic_channels );

// A short, non-silent mono PCM buffer — enough for the paint loop to consume
// real samples rather than immediately hitting FL_CHAN_FINISHED.
[[nodiscard]] AudioData make_audio( std::uint32_t frames = 4410 )
{
    AudioData d;
    d.rate     = ::xash::limits::sound_dma_speed;
    d.width    = 2;
    d.channels = 1;
    d.samples  = frames;
    d.type     = AudioFormatType::Pcm;
    d.buffer.resize( static_cast<std::size_t>( frames ) * 2 );
    for( std::uint32_t i = 0; i < frames; ++i )
    {
        const std::int16_t v = static_cast<std::int16_t>( ( i % 64 ) * 256 - 8192 );
        std::memcpy( d.buffer.data() + static_cast<std::size_t>( i ) * 2, &v, sizeof( v ) );
    }
    return d;
}

// The decoder-owned channel state a topology drives, standing in for
// Sound::Impl's own components.
struct MixFixture
{
    Mixer               mixer;
    VoxSystem           vox;
    SoundStats          stats;
    ChannelApplyContext ctx {};

    MixFixture()
    {
        mixer.channels().resize( ::xash::limits::sound_max_channels );
        mixer.set_vox_advance( &vox );
        ctx.mixer          = &mixer;
        ctx.vox            = &vox;
        ctx.stats          = &stats;
        ctx.total_channels = k_dynamic_hi;
        // A default-constructed MixGateSnapshot leaves every gate false, which
        // is the "in game, not paused, not in menu" shape the paint expects.
        ctx.mix_config.master_volume = 1.0f;
        ctx.mix_config.pitch_mult    = 1.0;
    }

    [[nodiscard]] std::size_t occupied_channels() const
    {
        std::size_t n = 0;
        for( const MixChannel &ch : const_cast<Mixer &>( mixer ).channels() )
            if( ch.source != nullptr )
                ++n;
        return n;
    }
};

[[nodiscard]] AudioCommand make_start( const AudioData *src, int entnum, int entchannel, int vol = 255 )
{
    AudioCommand cmd {};
    cmd.type       = AudioCommandType::StartSound;
    cmd.sfx_handle = 7;
    cmd.entnum     = entnum;
    cmd.entchannel = entchannel;
    cmd.vol        = vol;
    cmd.pitch      = k_pitch_norm_flag;
    cmd.dist_mult  = 0.0f;
    cmd.source     = src;
    set_command_name( cmd, "test/beep.wav" );
    // entnum == listener entnum (0) -> "always full volume" branch, no provider
    // consulted (s_main.c:568-576) — keeps the fixture free of a provider mock.
    cmd.listener.entnum = entnum;
    return cmd;
}

} // namespace

// ===========================================================================
// 1. POD shape + SND-OQ-3 class map
// ===========================================================================
void test_command_pod_and_class_map()
{
    static_assert( std::is_trivially_copyable_v<AudioCommand> );

    AudioCommand cmd {};
    set_command_name( cmd, "abc" );
    CHECK_STREQ( cmd.sfx_name.data(), "abc" );

    // Q_strncpy-shaped truncation: always NUL-terminated, never overflowing.
    const std::string overlong( ::xash::limits::sound_command_name_max + 40, 'x' );
    set_command_name( cmd, overlong );
    CHECK_EQ( std::strlen( cmd.sfx_name.data() ), ::xash::limits::sound_command_name_max - 1 );

    cmd.type  = AudioCommandType::StartSound;
    cmd.flags = 0;
    CHECK( command_class( cmd ) == AudioCommandClass::Normal );

    // A START carrying stop/change bits runs the alter prologue first — it is a
    // stop request in START's clothing and must ride the reserved lane.
    for( const std::uint32_t bit : { k_snd_stop, k_snd_change_vol, k_snd_change_pitch } )
    {
        cmd.flags = bit;
        CHECK( command_class( cmd ) == AudioCommandClass::Reserved );
    }

    cmd.flags = 0;
    cmd.type  = AudioCommandType::AlterChannel;
    CHECK( command_class( cmd ) == AudioCommandClass::Reserved );
    cmd.type = AudioCommandType::StopAllSounds;
    CHECK( command_class( cmd ) == AudioCommandClass::Reserved );
    cmd.type = AudioCommandType::FlushEpoch;
    CHECK( command_class( cmd ) == AudioCommandClass::Reserved );
    // FrameUpdate is a CONTROL message (parity F-4): legacy's S_UpdateFrame is a
    // direct call and can never be lost, so it must not be droppable here.
    cmd.type = AudioCommandType::FrameUpdate;
    CHECK( command_class( cmd ) == AudioCommandClass::Reserved );
}

// ===========================================================================
// 1b. S_GetMasterVolume derivation (parity F-5 / conformance F4)
// ===========================================================================
void test_master_volume_derivation()
{
    // No fade, focus held: the `volume` cvar passes straight through
    // (s_main.c:134 `return s_volume.value * scale` with scale == 1).
    CHECK_EQ( master_volume_from( 0.7f, /*lost_focus*/ false, /*mute_losefocus*/ 1.0f ), 0.7f );

    // Focus lost AND snd_mute_losefocus set -> hard zero, SHORT-CIRCUITING
    // before any other term (s_main.c:119-123).
    CHECK_EQ( master_volume_from( 0.7f, true, 1.0f ), 0.0f );

    // Focus lost but the cvar is off -> the mute does not apply.
    CHECK_EQ( master_volume_from( 0.7f, true, 0.0f ), 0.7f );

    // The soundfade term (XASH3DPP-STUB(chunk12): no producer yet) still
    // multiplies in legacy's order when someone eventually supplies it, and the
    // focus mute still wins over it.
    CHECK_EQ( master_volume_from( 0.8f, false, 1.0f, 0.5f ), 0.4f );
    CHECK_EQ( master_volume_from( 0.8f, true, 1.0f, 0.5f ), 0.0f );
}

// ===========================================================================
// 1c. A FrameUpdate survives a START-saturated queue (parity F-4)
// ===========================================================================
void test_frame_update_survives_start_saturation()
{
    auto queue = std::make_unique<AudioCommandQueue>();

    AudioCommand start {};
    start.type = AudioCommandType::StartSound;
    for( std::size_t i = 0; i < AudioCommandQueue::capacity(); ++i )
    {
        start.entnum = static_cast<int>( i );
        CHECK( queue->submit( start ) );
    }

    // The per-frame listener pose / waterlevel / cvar poll must still land: if
    // this were droppable the decoder would keep spatializing against a frozen
    // world for as long as the START flood lasted.
    AudioCommand frame {};
    frame.type            = AudioCommandType::FrameUpdate;
    frame.listener.entnum = 42;
    bool dropped          = true;
    CHECK( queue->submit( frame, &dropped ) );
    CHECK( !dropped );

    // It rode the RESERVE region, so it is behind every START in FIFO order
    // (the reserve is an admission relaxation, not a priority lane).
    AudioCommand out;
    std::size_t  starts = 0;
    bool         frame_seen_last = false;
    while( queue->try_pop( out ) )
    {
        if( out.type == AudioCommandType::StartSound )
        {
            CHECK( !frame_seen_last );
            ++starts;
        }
        else
        {
            CHECK( out.type == AudioCommandType::FrameUpdate );
            CHECK_EQ( out.listener.entnum, 42 );
            frame_seen_last = true;
        }
    }
    CHECK_EQ( starts, AudioCommandQueue::capacity() );
    CHECK( frame_seen_last );
}

// ===========================================================================
// 2. SND-OQ-3: a STOP survives a START-saturated queue (the headline guarantee)
// ===========================================================================
void test_stop_survives_start_saturation()
{
    // Heap-allocated: the queue carries capacity+reserve commands inline.
    auto queue = std::make_unique<AudioCommandQueue>();

    // Fill the ENTIRE normal region with plain STARTs.  No consumer exists, so
    // nothing drains.
    AudioCommand start {};
    start.type = AudioCommandType::StartSound;
    for( std::size_t i = 0; i < AudioCommandQueue::capacity(); ++i )
    {
        start.entnum = static_cast<int>( i );
        CHECK( queue->submit( start ) );
    }

    // The next START must NOT be admitted (the reserve is unreachable to it) —
    // it blocks for the bounded spin and is then genuinely dropped.
    bool dropped = false;
    start.entnum = 9999;
    CHECK( !queue->submit( start, &dropped ) );
    CHECK( dropped );

    // ...and with the normal region saturated, EVERY stop-class command still
    // lands.  This is SND-OQ-3's guarantee, asserted directly.
    for( std::size_t i = 0; i < AudioCommandQueue::reserve_capacity(); ++i )
    {
        AudioCommand stop {};
        stop.type   = AudioCommandType::AlterChannel;
        stop.flags  = k_snd_stop;
        stop.entnum = static_cast<int>( 1000 + i );
        dropped     = true;
        CHECK( queue->submit( stop, &dropped ) );
        CHECK( !dropped );
    }

    // Drain: the STOPs are all present, in FIFO order behind the STARTs (the
    // reserve is an ADMISSION relaxation, not a priority lane).
    std::size_t  starts = 0, stops = 0;
    AudioCommand out;
    bool         order_ok = true;
    while( queue->try_pop( out ) )
    {
        if( out.type == AudioCommandType::StartSound )
        {
            if( stops != 0 )
                order_ok = false; // a STOP appeared before a later START
            ++starts;
        }
        else
        {
            CHECK_EQ( out.entnum, static_cast<int>( 1000 + stops ) );
            ++stops;
        }
    }
    CHECK_EQ( starts, AudioCommandQueue::capacity() );
    CHECK_EQ( stops, AudioCommandQueue::reserve_capacity() );
    CHECK( order_ok );

    // Closing the queue refuses everything, and a refusal-while-closed is NOT
    // reported as a drop (shutdown step (a), not a lost sound).
    queue->set_accepting( false );
    dropped = true;
    CHECK( !queue->submit( start, &dropped ) );
    CHECK( !dropped );
}

// ===========================================================================
// 3. Command round-trip: start -> audible channel state; stop -> freed
// ===========================================================================
void test_command_roundtrip_start_and_stop()
{
    MixFixture       fx;
    const AudioData  audio = make_audio();

    AudioCommand start = make_start( &audio, /*entnum*/ 0, k_chan_auto, /*vol*/ 200 );
    apply_command( fx.ctx, start );

    CHECK_EQ( fx.occupied_channels(), std::size_t{ 1 } );

    const MixChannel *found = nullptr;
    for( const MixChannel &ch : fx.mixer.channels() )
        if( ch.source != nullptr )
            found = &ch;
    REQUIRE( found != nullptr );
    CHECK_EQ( found->source, &audio );
    CHECK_EQ( found->entnum, 0 );
    CHECK_EQ( found->master_vol, 200 );
    CHECK_EQ( found->sfx_handle, 7 );
    // entnum == listener entnum -> full volume both sides (s_main.c:568-576).
    CHECK_EQ( found->leftvol, 200 );
    CHECK_EQ( found->rightvol, 200 );
    // ent == 0 -> FL_CHAN_STATIC_SOUND (s_main.c:674-675).
    CHECK( ( found->flags & ::xash::abi::k_fl_chan_static_sound ) != 0 );

    // S_StopSound's shape: S_AlterChannel(..., SND_STOP).
    AudioCommand stop {};
    stop.type       = AudioCommandType::AlterChannel;
    stop.sfx_handle = 7;
    stop.entnum     = 0;
    stop.entchannel = k_chan_auto;
    stop.flags      = k_snd_stop;
    set_command_name( stop, "test/beep.wav" );
    apply_command( fx.ctx, stop );
    CHECK_EQ( fx.occupied_channels(), std::size_t{ 0 } );

    // StopAllSounds resets the static high-water mark (s_main.c:1470-1483).
    apply_command( fx.ctx, make_start( &audio, 0, k_chan_static ) );
    CHECK_EQ( fx.occupied_channels(), std::size_t{ 1 } );
    AudioCommand stop_all {};
    stop_all.type = AudioCommandType::StopAllSounds;
    apply_command( fx.ctx, stop_all );
    CHECK_EQ( fx.occupied_channels(), std::size_t{ 0 } );
    CHECK_EQ( fx.ctx.total_channels, k_dynamic_hi );
}

// ===========================================================================
// 4. Ring underrun on T_AudioCallback: silence + always-on counter
// ===========================================================================
void test_ring_underrun_silence_and_counter()
{
    SoundStats stats;
    auto       ring = std::make_unique<PcmRing>();
    RingFillSource fill( stats );
    fill.set_ring( ring.get() );

    // Producer side is THIS thread (SpscRing latches one producer identity).
    constexpr std::size_t k_have = 8;
    const std::int16_t    src[k_have] = { 11, 12, 13, 14, 15, 16, 17, 18 };
    CHECK_EQ( ring->try_write( std::span<const std::int16_t>( src, k_have ) ), k_have );

    // Consumer side is ONE dedicated T_AudioCallback thread for the whole test
    // (a second consumer thread would trip SpscRing's own single-consumer
    // assert — which is exactly the contract we want enforced).
    std::int16_t out_partial[16];
    std::int16_t out_empty[8];
    std::thread  cb( [&] {
        xash::core::register_thread_role( ThreadRole::AudioCallback );
        for( std::int16_t &v : out_partial )
            v = -1;
        fill.fill( std::span<std::int16_t>( out_partial, 16 ) );
        for( std::int16_t &v : out_empty )
            v = -1;
        fill.fill( std::span<std::int16_t>( out_empty, 8 ) );
    } );
    cb.join();

    // Short read: the available frames, then SILENCE for the shortfall.
    for( std::size_t i = 0; i < k_have; ++i )
        CHECK_EQ( out_partial[i], src[i] );
    for( std::size_t i = k_have; i < 16; ++i )
        CHECK_EQ( out_partial[i], std::int16_t{ 0 } );
    // Fully empty ring: all silence.
    for( std::int16_t v : out_empty )
        CHECK_EQ( v, std::int16_t{ 0 } );

    // Two underruns, on the ALWAYS-ON counter (no XASH_STATS guard).
    CHECK_EQ( stats.underruns.load( std::memory_order_relaxed ), std::uint64_t{ 2 } );
}

// ===========================================================================
// 5. Deterministic decoder drive: real MPSC in, real SPSC ring out
// ===========================================================================
void test_deterministic_decoder_step()
{
    MixFixture      fx;
    const AudioData audio = make_audio();

    TopologyParams tp {};
    tp.ctx              = &fx.ctx;
    tp.stats            = &fx.stats;
    tp.device           = nullptr;
    tp.internal_pump    = false; // this test owns the consumer
    tp.internal_decoder = false; // ...and the producer
    AudioTopology topo( tp );
    REQUIRE( topo.start() );

    CHECK( topo.submit( make_start( &audio, 0, k_chan_auto ) ) );

    // Drive the decoder from a thread that registered the role — the same body
    // the real decoder thread runs.
    std::thread dec( [&] {
        xash::core::register_thread_role( ThreadRole::AudioDecoder );
        for( int i = 0; i < 4; ++i )
            (void)topo.decoder_step();
    } );
    dec.join();

    // The command crossed the MPSC and allocated a channel...
    CHECK_EQ( fx.occupied_channels(), std::size_t{ 1 } );
    // ...and the paint wrote int16 frames into the SPSC ring (SND-OQ-5).
    CHECK( fx.stats.mix_blocks.load( std::memory_order_relaxed ) > 0 );
    REQUIRE( topo.ring() != nullptr );
    CHECK( topo.ring()->occupancy() > 0 );

    // Non-silent output proves the channel actually mixed (not just allocated).
    std::vector<std::int16_t> pcm( ::xash::limits::sound_decoder_block_frames * 2, 0 );
    std::size_t               got     = 0;
    bool                      nonzero = false;
    std::thread               cb( [&] {
        xash::core::register_thread_role( ThreadRole::AudioCallback );
        got = topo.ring()->read( std::span<std::int16_t>( pcm ) );
    } );
    cb.join();
    for( std::size_t i = 0; i < got; ++i )
        if( pcm[i] != 0 )
            nonzero = true;
    CHECK( got > 0 );
    CHECK( nonzero );

    topo.stop();
}

// ===========================================================================
// 6. SND-OQ-2: stop-and-free race, driven hard
// ===========================================================================
void test_snd_oq2_stop_and_free_race()
{
    MixFixture fx;

    TopologyParams tp {};
    tp.ctx              = &fx.ctx;
    tp.stats            = &fx.stats;
    tp.device           = nullptr;
    tp.internal_pump    = true;  // a live consumer keeps the ring draining
    tp.internal_decoder = true;  // a live decoder is mid-paint against `audio`
    AudioTopology topo( tp );
    REQUIRE( topo.start() );

    constexpr int k_iterations = 200;
    for( int i = 0; i < k_iterations; ++i )
    {
        // T_Main owns the decoded audio (as SfxRegistry does in production).
        auto audio = std::make_unique<AudioData>( make_audio( 2048 ) );

        for( int c = 0; c < 8; ++c )
            (void)topo.submit( make_start( audio.get(), 0, k_chan_auto ) );

        // Wait until the decoder has actually DRAINED those starts, i.e. its
        // channels now hold borrowed pointers into `audio` and its paint may
        // dereference them at any instant.  Yield-spinning (rather than
        // sleeping) keeps the race window tight AND the test fast.
        for( int spin = 0; spin < 100000; ++spin )
        {
            if( topo.queue() == nullptr || topo.queue()->occupancy() == 0 )
                break;
            std::this_thread::yield();
        }

        // SND-OQ-2 pairing: STOP first (drops the borrowed pointers), then the
        // FENCE.  flush() returns only once the decoder has popped the fence,
        // which — the queue being FIFO — proves the stop was already applied,
        // and which cannot happen while a paint is dereferencing the source
        // (same thread, drain sequenced before paint).
        AudioCommand stop_all {};
        stop_all.type = AudioCommandType::StopAllSounds;
        (void)topo.submit( stop_all );
        // CONC-3: the RETURN VALUE is the licence, and it must be checked — a
        // flush that failed open would hand out a licence it never earned.
        REQUIRE( topo.flush() );

        // The ack is the license to free.  If the protocol were unsound this
        // destructor would race a mid-paint decoder read — 200 iterations with
        // a live pump is a hard drive of exactly that window.
        audio.reset();
    }

    // Post-fence the decoder holds nothing.
    CHECK_EQ( fx.occupied_channels(), std::size_t{ 0 } );
    CHECK( topo.acked_epoch() >= static_cast<std::uint64_t>( k_iterations ) );

    topo.stop();
}

// ===========================================================================
// 7. Clean start/stop cycles — no leak, no join hang
// ===========================================================================
void test_topology_start_stop_cycles()
{
    MixFixture fx;

    TopologyParams tp {};
    tp.ctx   = &fx.ctx;
    tp.stats = &fx.stats;
    AudioTopology topo( tp );

    constexpr int k_cycles = 32;
    for( int i = 0; i < k_cycles; ++i )
    {
        REQUIRE( topo.start() );
        CHECK( topo.running() );
        CHECK( topo.start() ); // idempotent while running
        (void)topo.submit( make_start( nullptr, 0, k_chan_auto ) ); // null source -> channel freed again
        topo.stop();
        CHECK( !topo.running() );
        topo.stop(); // idempotent
        // Both primitives are re-created per run, so the next cycle's fresh
        // decoder/callback threads do not trip their single-consumer asserts.
        CHECK( topo.ring() == nullptr );
        CHECK( topo.queue() == nullptr );
    }
    CHECK_EQ( fx.occupied_channels(), std::size_t{ 0 } );
}

// ===========================================================================
// 8. Shutdown ordering
// ===========================================================================
void test_shutdown_ordering()
{
    MixFixture fx;

    TopologyParams tp {};
    tp.ctx   = &fx.ctx;
    tp.stats = &fx.stats;
    AudioTopology topo( tp );
    REQUIRE( topo.start() );

    for( int i = 0; i < 16; ++i )
        (void)topo.submit( make_start( nullptr, i, k_chan_auto ) );
    CHECK( topo.flush() );
    const std::uint64_t acked = topo.acked_epoch();
    CHECK( acked >= 1 );

    topo.stop();

    // (a) the queue is gone -> nothing can be admitted after shutdown began.
    CHECK( !topo.submit( make_start( nullptr, 0, k_chan_auto ) ) );
    // The epoch is settled, so a flush racing shutdown can never spin.
    CHECK( topo.acked_epoch() >= acked );
    // flush()/channel_snapshot() are no-ops once stopped.  A flush against a
    // stopped topology returns TRUE: there is no decoder, hence no borrower, so
    // prior audio really is quiesced (CONC-3's "licence granted" case).
    CHECK( topo.flush() );
    std::vector<PublishedChannel> pub;
    CHECK( !topo.channel_snapshot( pub ) );
}

// ===========================================================================
// 8b. flush() must NOT fail open (CONC-3)
// ===========================================================================
void test_flush_reports_failure_instead_of_failing_open()
{
    MixFixture fx;

    TopologyParams tp {};
    tp.ctx   = &fx.ctx;
    tp.stats = &fx.stats;
    AudioTopology topo( tp );
    REQUIRE( topo.start() );

    // Healthy path: the ack is observed and the licence is granted.
    CHECK( topo.flush() );

    // Now make the FlushEpoch un-submittable (this is what a concurrent
    // shutdown does at step (a)).  flush() used to return normally here — i.e.
    // hand out the licence to free borrowed audio WITHOUT any ack.  It must
    // report failure instead.
    REQUIRE( topo.queue() != nullptr );
    topo.queue()->set_accepting( false );
    CHECK( !topo.flush() );

    topo.stop();
}

// ===========================================================================
// 10. MouthSlots: the published (entnum, mouthopen) pair cannot tear (CONC-4)
// ===========================================================================
void test_mouth_slots_pair_never_tears()
{
    // A sink that validates the PAIRING, not just the values: the producer only
    // ever publishes mouthopen == entnum * 3, so any (entnum, mouthopen) the
    // drain forwards that breaks that relation is a torn pair.
    class PairCheckingSink final : public IMouthSink
    {
    public:
        int  updates = 0;
        bool torn    = false;
        void set_mouth_open( int entnum, int mouthopen ) noexcept override
        {
            ++updates;
            if( mouthopen != entnum * 3 )
                torn = true;
        }
    };

    MouthSlots      slots;
    PairCheckingSink sink;

    // Producer runs on a real T_AudioDecoder thread (set_mouth_open asserts the
    // role), hammering ONE slot with many different entities that all hash to
    // it — the exact collision case the old two-atomic seqlock could mispair.
    constexpr int k_slot_stride = static_cast<int>( ::xash::limits::sound_mouth_slots );
    std::atomic<bool> stop_producer { false };
    std::thread       producer( [&] {
        xash::core::register_thread_role( ThreadRole::AudioDecoder );
        for( int i = 1; !stop_producer.load( std::memory_order_relaxed ); ++i )
        {
            const int ent = 1 + ( i % 64 ) * k_slot_stride; // all map to the same slot
            slots.set_mouth_open( ent, ent * 3 );
        }
    } );

    for( int i = 0; i < 20000; ++i )
        slots.drain( &sink );
    stop_producer.store( true, std::memory_order_relaxed );
    producer.join();

    slots.drain( &sink );
    CHECK( !sink.torn );
    CHECK( sink.updates > 0 );

    // Negative amplitudes and entnums round-trip through the packing unchanged.
    MouthSlots plain;
    class RecordingSink final : public IMouthSink
    {
    public:
        int ent = 0, open = 0, calls = 0;
        void set_mouth_open( int entnum, int mouthopen ) noexcept override
        {
            ent   = entnum;
            open  = mouthopen;
            ++calls;
        }
    } rec;

    std::thread pub( [&] {
        xash::core::register_thread_role( ThreadRole::AudioDecoder );
        plain.set_mouth_open( -7, -255 );
    } );
    pub.join();
    plain.drain( &rec );
    CHECK_EQ( rec.calls, 1 );
    CHECK_EQ( rec.ent, -7 );
    CHECK_EQ( rec.open, -255 );

    // A second drain with nothing republished forwards nothing.
    plain.drain( &rec );
    CHECK_EQ( rec.calls, 1 );
}

// ===========================================================================
// 11. External-callback quiesce counter (CONC-5)
// ===========================================================================
void test_external_callback_quiesce_counter()
{
    SoundStats     stats;
    auto           ring = std::make_unique<PcmRing>();
    RingFillSource fill( stats );
    fill.set_ring( ring.get() );

    // Nobody is inside fill() before or after a completed call — which is the
    // condition AudioTopology::stop() spins on before destroying the ring.
    CHECK_EQ( fill.in_callback(), 0 );

    std::int16_t out[8];
    std::thread  cb( [&] {
        xash::core::register_thread_role( ThreadRole::AudioCallback );
        fill.fill( std::span<std::int16_t>( out, 8 ) );
    } );
    cb.join();
    CHECK_EQ( fill.in_callback(), 0 );

    // The EXTERNAL-pump shape (internal_pump == false) is the one the counter
    // exists for: stop() cannot join an OS callback thread, so it must observe
    // the counter instead.  Drive a full start/stop cycle in that shape and
    // confirm it tears the ring down without hanging.
    MixFixture     fx;
    TopologyParams tp {};
    tp.ctx              = &fx.ctx;
    tp.stats            = &fx.stats;
    tp.internal_pump    = false; // an external backend would own the callback
    tp.internal_decoder = true;
    AudioTopology topo( tp );
    REQUIRE( topo.start() );
    // RingFillSource is `final`, so the downcast needs no RTTI.
    CHECK_EQ( static_cast<RingFillSource &>( topo.fill_source() ).in_callback(), 0 );
    topo.stop();
    CHECK( topo.ring() == nullptr );
}

// ===========================================================================
// 9. Threaded soak through the real Sound entry surface
// ===========================================================================
void test_sound_threaded_soak()
{
    SoundInitParams params {};
    params.threaded = true;

    Sound sound;
    REQUIRE( sound.init( params ).has_value() );
    CHECK( sound.topology_running() );

    ListenerSnapshot listener {};
    listener.entnum = 0;

    const xash::abi::sound_t handle = sound.register_sound( "test/soak.wav" );
    CHECK( handle != -1 );

    constexpr int k_frames = 300;
    for( int f = 0; f < k_frames; ++f )
    {
        sound.update_frame( listener );
        sound.start_sound( Vec3{}, /*ent*/ 0, k_chan_auto, handle, 1.0f, k_attn_none, k_pitch_norm_flag, 0 );
        if( ( f % 7 ) == 0 )
            sound.stop_sound( 0, k_chan_auto, "test/soak.wav" );
        if( ( f % 53 ) == 0 )
            sound.stop_all_sounds( true );
    }

    // The decoder ran: mix blocks accumulated and the P-4 snapshot handshake
    // answered from the decoder side (T_Main never touched Mixer::channels()).
    // stop-then-flush is the SND-OQ-2 pairing: the fence itself is
    // non-destructive, the stop is what empties the channel array.
    sound.stop_all_sounds( true );
    CHECK( sound.flush() ); // CONC-3: the licence must be earned, not assumed
    const std::vector<ChannelInfo> snap = sound.channels_snapshot();
    CHECK_EQ( snap.size(), std::size_t{ 0 } );
    CHECK( sound.stats().mix_blocks.load( std::memory_order_relaxed ) > 0 );

    // Toggle the topology off and back on through the public surface, then run
    // the SINGLE-THREADED path (identical apply_command body) and confirm it
    // still mutates channel state inline.
    sound.stop_topology();
    CHECK( !sound.topology_running() );
    sound.start_sound( Vec3{}, 0, k_chan_auto, handle, 1.0f, k_attn_none, k_pitch_norm_flag, 0 );
    CHECK_EQ( sound.channels_snapshot().size(), std::size_t{ 1 } );

    REQUIRE( sound.start_topology().has_value() );
    CHECK( sound.topology_running() );
    CHECK_EQ( sound.channels_snapshot().size(), std::size_t{ 1 } ); // survived the restart

    sound.shutdown();
    CHECK( !sound.topology_running() );
}

int main()
{
    xash::core::register_thread_role( ThreadRole::Main );

    RUN_TEST( test_command_pod_and_class_map );
    RUN_TEST( test_master_volume_derivation );
    RUN_TEST( test_frame_update_survives_start_saturation );
    RUN_TEST( test_stop_survives_start_saturation );
    RUN_TEST( test_command_roundtrip_start_and_stop );
    RUN_TEST( test_ring_underrun_silence_and_counter );
    RUN_TEST( test_deterministic_decoder_step );
    RUN_TEST( test_snd_oq2_stop_and_free_race );
    RUN_TEST( test_topology_start_stop_cycles );
    RUN_TEST( test_shutdown_ordering );
    RUN_TEST( test_flush_reports_failure_instead_of_failing_open );
    RUN_TEST( test_mouth_slots_pair_never_tears );
    RUN_TEST( test_external_callback_quiesce_counter );
    RUN_TEST( test_sound_threaded_soak );

    std::printf( "sound_topology: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
