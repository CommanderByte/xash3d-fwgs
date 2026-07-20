// xash3dpp — audio device + provider-seam tests (Chunk 9, slice S9.1).
// NullDevice (opens, no output) + SinkDevice (virtually-clocked pull consumer:
// pump(n) pulls exactly n frames through a registered fill source, no sleeps) +
// the SND-OQ-1 provider/sink POD + interface shapes.

#include <xash3dpp/sound/device.hpp>
#include <xash3dpp/sound/providers.hpp>

#include <xash3dpp/core/thread_role.hpp>

#include "../test_helpers.hpp"

#include <cstdint>
#include <span>

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;

// A deterministic fill source: writes 1,2,3,... into `out` and counts calls.
class RampFill final : public IAudioFillSource
{
public:
    int calls = 0;
    void fill( std::span<std::int16_t> out ) noexcept override
    {
        ++calls;
        for( std::size_t i = 0; i < out.size(); ++i )
            out[i] = static_cast<std::int16_t>( i + 1 );
    }
};

static void test_null_device_opens_no_output()
{
    NullDevice dev;
    CHECK( !dev.is_open() );

    DeviceSpec spec;
    auto caps = dev.open( spec );
    CHECK( caps.has_value() );          // UNLIKE legacy s_stub, open() succeeds
    CHECK( dev.is_open() );
    CHECK_EQ( caps->speed, spec.speed );
    CHECK_EQ( static_cast<int>( caps->channels ), 2 );

    // Activate is a tracked no-op (topology symmetry, SND-OQ-2).
    dev.set_active( true );
    CHECK( dev.is_active() );

    // A registered source is accepted but never invoked (no output).
    RampFill src;
    dev.set_fill_source( &src );
    dev.set_active( false );
    CHECK( !dev.is_active() );
    CHECK_EQ( src.calls, 0 );

    dev.close();
    CHECK( !dev.is_open() );
}

static void test_sink_pump_pulls_exact_frames()
{
    SinkDevice dev;
    REQUIRE( dev.open( DeviceSpec{} ).has_value() );
    dev.set_active( true );

    RampFill src;
    dev.set_fill_source( &src );

    // pump(4) -> exactly 4 stereo frames == 8 interleaved int16 samples.
    auto out = dev.pump( 4 );
    CHECK_EQ( out.size(), static_cast<std::size_t>( 8 ) );
    CHECK_EQ( src.calls, 1 );
    CHECK_EQ( dev.frames_pumped(), 4u );
    for( std::size_t i = 0; i < out.size(); ++i )
        CHECK_EQ( out[i], static_cast<std::int16_t>( i + 1 ) );

    // A second pump is cumulative in the frame counter, exact in size.
    auto out2 = dev.pump( 2 );
    CHECK_EQ( out2.size(), static_cast<std::size_t>( 4 ) );
    CHECK_EQ( dev.frames_pumped(), 6u );

    dev.close();
}

static void test_sink_silence_without_source_or_inactive()
{
    SinkDevice dev;
    REQUIRE( dev.open( DeviceSpec{} ).has_value() );

    // Inactive (no set_active) -> silence even with a source registered.
    RampFill src;
    dev.set_fill_source( &src );
    auto quiet = dev.pump( 3 );
    CHECK_EQ( quiet.size(), static_cast<std::size_t>( 6 ) );
    CHECK_EQ( src.calls, 0 );
    for( std::int16_t s : quiet )
        CHECK_EQ( s, static_cast<std::int16_t>( 0 ) );

    // Active but source detached -> silence.
    dev.set_active( true );
    dev.set_fill_source( nullptr );
    auto quiet2 = dev.pump( 3 );
    for( std::int16_t s : quiet2 )
        CHECK_EQ( s, static_cast<std::int16_t>( 0 ) );

    dev.close();
}

// Minimal fakes to exercise the SND-OQ-1 provider/sink seams compile + dispatch.
class FakeSpatial final : public IEntitySpatialProvider
{
public:
    bool resolve_origin( int entnum, Vec3 &origin ) noexcept override
    {
        origin = Vec3{ static_cast<float>( entnum ), 0.0f, 0.0f };
        return entnum > 0; // entity 0 == world/not spatializable
    }
};

class FakeMouth final : public IMouthSink
{
public:
    int last_ent = -1, last_open = -1;
    void set_mouth_open( int entnum, int mouthopen ) noexcept override
    {
        last_ent  = entnum;
        last_open = mouthopen;
    }
};

static void test_provider_seams_and_pods()
{
    // POD defaults (P-2 snapshots).
    ListenerSnapshot ls;
    CHECK_EQ( ls.entnum, 0 );
    CHECK_EQ( ls.waterlevel, 0 );

    MixGateSnapshot mg;
    CHECK( !mg.paused );
    CHECK_EQ( mg.soundfade_gain, 1.0f ); // 1.0 == no fade

    RegistrationSnapshot rs;
    CHECK( !rs.have_ambient_sfx );

    // Seams dispatch.
    FakeSpatial sp;
    Vec3 o{};
    CHECK( sp.resolve_origin( 5, o ) );
    CHECK_EQ( o.x, 5.0f );
    CHECK( !sp.resolve_origin( 0, o ) );

    FakeMouth mo;
    mo.set_mouth_open( 7, 42 );
    CHECK_EQ( mo.last_ent, 7 );
    CHECK_EQ( mo.last_open, 42 );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_null_device_opens_no_output );
    RUN_TEST( test_sink_pump_pulls_exact_frames );
    RUN_TEST( test_sink_silence_without_source_or_inactive );
    RUN_TEST( test_provider_seams_and_pods );

    std::printf( "sound_device: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
