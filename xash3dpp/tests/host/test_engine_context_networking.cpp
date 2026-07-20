// xash3dpp — EngineContext ↔ networking wiring tests
// Covers: NetworkContext member init/shutdown sequencing inside
//         EngineContext::init()/shutdown(), IPlatformSockets injection via
//         EngineContextInitParams::sockets, default-singleton fallback, and
//         re-init after shutdown (socket_init/socket_shutdown ref-counting).
//
// FakePlatformSockets follows tests/networking/test_network_context_sockets.cpp:
// open_udp hands out a synthetic non-zero handle so NetworkContext treats the
// slot as open; ~OsSocket's closesocket(0x42) fails silently on every platform.

#include <xash3dpp/host/engine_context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

#include "../test_helpers.hpp"

#include <bit>
#include <cstdint>

using xash::EngineContext;
using xash::EngineContextInitParams;
using xash::networking::NetAddress;
using xash::networking::NetError;
using xash::networking::Result;
using xash::platform::IPlatformSockets;
using xash::platform::IpFamily;
using xash::platform::OsSocket;

static int g_pass = 0, g_fail = 0;

namespace
{

struct FakePlatformSockets final : IPlatformSockets
{
    int open_udp_calls = 0;

    Result<OsSocket> open_udp( IpFamily /*family*/,
                               std::uint16_t /*port*/,
                               std::string_view /*iface*/ ) noexcept override
    {
        ++open_udp_calls;
        return OsSocket{ static_cast<xash::platform::SocketHandle>( 0x42 ) };
    }
    Result<OsSocket> open_tcp( IpFamily ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
    Result<std::size_t> sendto( const OsSocket &,
                                std::span<const std::byte>,
                                const NetAddress & ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
    Result<std::size_t> recvfrom( const OsSocket &,
                                  std::span<std::byte>,
                                  NetAddress & ) noexcept override
    {
        return std::unexpected( NetError::WouldBlock );
    }
    Result<std::size_t> send_stream( const OsSocket &,
                                     std::span<const std::byte> ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
    Result<std::size_t> recv_stream( const OsSocket &,
                                     std::span<std::byte> ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
    Result<void> connect_stream( const OsSocket &,
                                 const NetAddress & ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
};

EngineContextInitParams make_params( IPlatformSockets *sockets )
{
    EngineContextInitParams p;
    p.rootdir   = ".";
    p.basedir   = "valve";
    p.gamedir   = "valve";
    p.dedicated = true;
    p.sockets   = sockets;
    // trust_oracle / compat_policy left null — accepted by CmdCvarContext.
    return p;
}

} // namespace

// -- init() brings networking up through the injected fake -----------------

static void test_init_activates_networking()
{
    FakePlatformSockets fake;
    EngineContext ctx;

    REQUIRE( ctx.init( make_params( &fake ) ) );
    CHECK( ctx.networking.is_active() );

    // No sockets are opened by init() itself — only config() opens them.
    CHECK_EQ( fake.open_udp_calls, 0 );

    // Dedicated multiplayer opens exactly one (server) socket.
    REQUIRE( ctx.networking.config( true, false ).has_value() );
    CHECK_EQ( fake.open_udp_calls, 1 );

    ctx.shutdown();
    CHECK( !ctx.networking.is_active() );
}

// -- sockets == nullptr falls back to the production singleton -------------

static void test_default_sockets_fallback()
{
    EngineContext ctx;

    // init() must succeed without an injected IPlatformSockets: the wiring
    // substitutes platform::default_platform_sockets().  config() is NOT
    // called here so no real OS socket is ever opened.
    REQUIRE( ctx.init( make_params( nullptr ) ) );
    CHECK( ctx.networking.is_active() );

    ctx.shutdown();
    CHECK( !ctx.networking.is_active() );
}

// -- shutdown() then init() again — socket_init refcount survives ----------

static void test_reinit_cycle()
{
    FakePlatformSockets fake;
    EngineContext ctx;

    REQUIRE( ctx.init( make_params( &fake ) ) );
    ctx.shutdown();

    REQUIRE( ctx.init( make_params( &fake ) ) );
    CHECK( ctx.networking.is_active() );
    REQUIRE( ctx.networking.config( true, false ).has_value() );
    CHECK_EQ( fake.open_udp_calls, 1 );

    ctx.shutdown();
    CHECK( !ctx.networking.is_active() );
}

// -- canonical random callbacks reach the EngineContext-owned stream -------

static void test_legacy_random_callbacks()
{
    CHECK_EQ( xash::legacy_random_long_callback( 17, 99 ), 17 );
    CHECK( xash::legacy_random_float_callback( 2.5f, 9.0f ) == 2.5f );

    FakePlatformSockets fake;
    EngineContext ctx;
    REQUIRE( ctx.init( make_params( &fake ) ) );

    xash::core::LegacyRandom expected;
    expected.set_seed( 1 );
    ctx.legacy_random.set_seed( 1 );

    CHECK_EQ( xash::legacy_random_long_callback( 0, 1000000 ),
              expected.random_long( 0, 1000000 ) );
    CHECK( std::bit_cast<std::uint32_t>(
               xash::legacy_random_float_callback( -4.0f, 8.0f )) ==
           std::bit_cast<std::uint32_t>(
               expected.random_float( -4.0f, 8.0f )) );

    ctx.shutdown();
    CHECK_EQ( xash::legacy_random_long_callback( -7, 40 ), -7 );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // EngineContext now brings up the server (main-thread only, OQ-9), so the
    // test presents as the engine's main thread — as the launcher does.
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_activates_networking );
    RUN_TEST( test_default_sockets_fallback );
    RUN_TEST( test_reinit_cycle );
    RUN_TEST( test_legacy_random_callbacks );

    std::printf( "engine_context_networking: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
