// xash3dpp — Sound lifecycle tests (Chunk 9, slice S9.1).
// create / init / shutdown, double-init rejection, the NullDevice fallback vs.
// an injected SinkDevice, the create_sound factory, and the SoundStats shape.

#include <xash3dpp/sound/sound.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;

static void test_init_null_fallback()
{
    Sound snd;
    CHECK( !snd.initialized() );

    SoundInitParams params;               // device == nullptr -> NullDevice fallback
    auto r = snd.init( params );
    CHECK( r.has_value() );
    CHECK( snd.initialized() );

    // The fallback device is live and reports the requested (default) format.
    CHECK( snd.device() != nullptr );
    CHECK_EQ( snd.caps().speed,
              static_cast<std::uint32_t>( xash::limits::sound_dma_speed ) );
    CHECK_EQ( static_cast<int>( snd.caps().channels ), 2 );
    CHECK_EQ( static_cast<int>( snd.caps().width ), 2 );

    snd.shutdown();
    CHECK( !snd.initialized() );
    CHECK( snd.device() == nullptr );
}

static void test_double_init_rejected()
{
    Sound snd;
    SoundInitParams params;
    CHECK( snd.init( params ).has_value() );

    // Second init without shutdown -> AlreadyInitialized (house double-init reject).
    auto again = snd.init( params );
    CHECK( !again.has_value() );
    CHECK( again.error() == SoundError::AlreadyInitialized );

    // Still usable / still the first session.
    CHECK( snd.initialized() );
    snd.shutdown();

    // Re-init after shutdown is allowed.
    CHECK( snd.init( params ).has_value() );
    CHECK( snd.initialized() );
    snd.shutdown();
}

static void test_injected_sink_device()
{
    SinkDevice sink;
    SoundInitParams params;
    params.device = &sink;

    Sound snd;
    CHECK( snd.init( params ).has_value() );
    CHECK_EQ( snd.device(), static_cast<IAudioDevice *>( &sink ) );
    CHECK( sink.is_open() );
    CHECK( sink.is_active() );   // init wires set_active(true)

    snd.shutdown();
    CHECK( !sink.is_open() );    // shutdown closes the injected device
}

static void test_create_sound_factory()
{
    SoundInitParams params;
    auto snd = create_sound( params );
    REQUIRE( snd != nullptr );
    CHECK( snd->initialized() );
    snd->shutdown();
}

static void test_stats_shape()
{
    Sound snd;
    SoundInitParams params;
    CHECK( snd.init( params ).has_value() );

    // Whole-struct relaxed atomics — all loadable from here (any thread).
    const SoundStats &st = snd.stats();
    CHECK_EQ( st.mix_blocks.load(), 0u );
    CHECK_EQ( st.underruns.load(), 0u );          // Tier-1 always-on (§5.2)
    CHECK_EQ( st.active_channels.load(), 0u );
    CHECK_EQ( st.dropped_sounds.load(), 0u );
    CHECK_EQ( st.dsp_room.load(), 0 );

    snd.shutdown();
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_null_fallback );
    RUN_TEST( test_double_init_rejected );
    RUN_TEST( test_injected_sink_device );
    RUN_TEST( test_create_sound_factory );
    RUN_TEST( test_stats_shape );

    std::printf( "sound_lifecycle: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
