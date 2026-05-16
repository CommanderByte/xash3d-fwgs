// xash3dpp — networking subsystem lifecycle smoke test
// Verifies that NetworkContext can be constructed, refuses init() without an
// IPlatformSockets, and shuts down cleanly.  Real I/O coverage lands when
// the platform sockets interface ships (Chunk 4).

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/networking.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static void test_construct_and_destruct()
{
    xash::networking::NetworkContext ctx;
    CHECK( !ctx.is_active() );
}

static void test_init_requires_platform_sockets()
{
    xash::networking::NetworkContext  ctx;
    xash::networking::NetworkInitParams params{}; // sockets == nullptr
    CHECK( !ctx.init( params ) );
    CHECK( !ctx.is_active() );
}

static void test_shutdown_without_init_is_safe()
{
    xash::networking::NetworkContext ctx;
    ctx.shutdown();
    CHECK( !ctx.is_active() );
}

static void test_address_factories()
{
    using xash::networking::IpFamily;
    using xash::networking::NetAddress;

    constexpr NetAddress lo = NetAddress::loopback_v4( 27015 );
    CHECK_EQ( static_cast<int>( lo.family ), static_cast<int>( IpFamily::V4 ) );
    CHECK_EQ( static_cast<int>( lo.port ),   27015 );

    constexpr NetAddress any = NetAddress::any_v4( 0 );
    CHECK_EQ( static_cast<int>( any.family ), static_cast<int>( IpFamily::V4 ) );
}

static void test_calls_before_init_return_error()
{
    xash::networking::NetworkContext ctx;

    auto cfg = ctx.config( /*multiplayer=*/true, /*change_port=*/false );
    CHECK( !cfg.has_value() );
    CHECK( cfg.error() == xash::networking::NetError::NotInitialised );

    xash::networking::NetAddress from{};
    std::byte buf[16] {};
    auto rx = ctx.get_packet( xash::networking::SocketKind::Client, from, buf );
    CHECK( !rx.has_value() );
    CHECK( rx.error() == xash::networking::NetError::NotInitialised );
}

int main()
{
    std::printf( "test_networking_lifecycle\n" );
    RUN_TEST( test_construct_and_destruct );
    RUN_TEST( test_init_requires_platform_sockets );
    RUN_TEST( test_shutdown_without_init_is_safe );
    RUN_TEST( test_address_factories );
    RUN_TEST( test_calls_before_init_return_error );
    std::printf( "test_networking_lifecycle: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
