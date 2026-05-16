// xash3dpp — NetworkContext::config socket open/close wiring
// Verifies that config(multiplayer, change_port) drives the injected
// IPlatformSockets::open_udp() in the patterns required by Q-12 + the
// platform sockets boundary (docs/architecture/platform/sockets.md).
//
// Notes:
//   • FakePlatformSockets returns OsSocket{ k_invalid_socket } from open_udp.
//     That means NetworkContext sees `os_sockets[idx].valid() == false`, so
//     it will re-open on every config() call.  The test exercises that
//     observable behaviour rather than a "real" cached-socket optimisation
//     — the production code only ever sees real handles from the OS.
//   • All other IPlatformSockets methods are unreachable in this fixture and
//     return NetError::NotInitialised.

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

#include "../test_helpers.hpp"

#include <cstdint>

using xash::networking::NetAddress;
using xash::networking::NetError;
using xash::networking::NetworkContext;
using xash::networking::NetworkInitParams;
using xash::networking::Result;
using xash::networking::SocketKind;
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
        // Returning an invalid-handle OsSocket keeps the test free of any
        // real OS interaction; the destructor's close() is a no-op on an
        // invalid handle (see os_socket.hpp).
        return OsSocket{};
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
        return std::unexpected( NetError::NotInitialised );
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

NetworkInitParams make_params( FakePlatformSockets &fake, bool dedicated )
{
    NetworkInitParams p{};
    p.sockets   = &fake;
    p.dedicated = dedicated;
    return p;
}

} // namespace

// -- config(false, _) on a fresh context opens nothing --------------------

static void test_config_singleplayer_opens_nothing()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );

    auto r = ctx.config( /*multiplayer=*/ false, /*change_port=*/ false );
    CHECK( r.has_value() );
    CHECK( fake.open_udp_calls == 0 );

    ctx.shutdown();
}

// -- config(true, _) on a client/server build opens BOTH sockets ----------

static void test_config_multiplayer_opens_both_sockets()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );

    auto r = ctx.config( /*multiplayer=*/ true, /*change_port=*/ false );
    CHECK( r.has_value() );
    // server socket + client socket
    CHECK( fake.open_udp_calls == 2 );

    ctx.shutdown();
}

// -- config(true, _) on a dedicated build opens ONLY the server socket ----

static void test_config_dedicated_opens_only_server_socket()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );

    auto r = ctx.config( /*multiplayer=*/ true, /*change_port=*/ false );
    CHECK( r.has_value() );
    CHECK( fake.open_udp_calls == 1 );

    ctx.shutdown();
}

// -- config(false, _) after a multiplayer config closes both --------------
//
// Because our fake returns invalid-handle sockets, the open_udp call count
// is effectively the "number of opens attempted".  A second config(true)
// should therefore re-open both because both slots are still invalid.

static void test_config_off_then_on_reopens()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );

    REQUIRE( ctx.config( true, false ).has_value() );
    CHECK( fake.open_udp_calls == 2 );

    REQUIRE( ctx.config( false, false ).has_value() );
    CHECK( fake.open_udp_calls == 2 ); // no extra opens on shutdown path

    REQUIRE( ctx.config( true, false ).has_value() );
    CHECK( fake.open_udp_calls == 4 ); // server + client again

    ctx.shutdown();
}

// -- config(true, _) on an uninitialised context returns NotInitialised ---

static void test_config_without_init_returns_not_initialised()
{
    NetworkContext ctx;
    auto           r = ctx.config( true, false );
    CHECK( !r.has_value() );
    if( !r.has_value() )
        CHECK( r.error() == NetError::NotInitialised );
}

int main()
{
    test_config_singleplayer_opens_nothing();
    test_config_multiplayer_opens_both_sockets();
    test_config_dedicated_opens_only_server_socket();
    test_config_off_then_on_reopens();
    test_config_without_init_returns_not_initialised();

    std::printf( "test_network_context_sockets: %d passed, %d failed\n",
                 g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
