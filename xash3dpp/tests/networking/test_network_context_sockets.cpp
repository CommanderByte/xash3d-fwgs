// xash3dpp — NetworkContext::config/get_packet/send_packet wiring
// Verifies that config(multiplayer, change_port) drives the injected
// IPlatformSockets::open_udp() in the patterns required by Q-12 + the
// platform sockets boundary (docs/architecture/platform/sockets.md), and
// that send_packet/get_packet route through loopback vs. the real socket
// according to the rules documented in transport-layer.md and
// context-lifecycle.md.
//
// Notes:
//   • FakePlatformSockets returns OsSocket{ 0x42 } from open_udp so that
//     NetworkContext treats the slot as valid for send/recv routing.  The
//     ~OsSocket call to closesocket(0x42) fails silently on every platform.
//   • Other IPlatformSockets methods that are unreachable in this fixture
//     return NetError::NotInitialised.

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

#include "../test_helpers.hpp"

#include <algorithm>
#include <array>
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
    int sendto_calls   = 0;
    int recvfrom_calls = 0;

    // Last sendto destination + payload size for assertions.
    NetAddress  last_sendto_dest{};
    std::size_t last_sendto_size = 0;

    // Optional canned recvfrom reply.
    bool                       have_canned_rx = false;
    NetAddress                 canned_rx_from{};
    std::array<std::byte, 64>  canned_rx_buf{};
    std::size_t                canned_rx_len  = 0;

    Result<OsSocket> open_udp( IpFamily /*family*/,
                               std::uint16_t /*port*/,
                               std::string_view /*iface*/ ) noexcept override
    {
        ++open_udp_calls;
        // Returning an invalid-handle OsSocket would make NetworkContext
        // treat the slot as "not open" and refuse send/recv.  Tests that
        // exercise the real-socket path need a valid-looking handle, so we
        // hand out a synthetic non-zero handle.  ~OsSocket() will call
        // closesocket() on this bogus value; on Win32 that returns
        // SOCKET_ERROR/WSAENOTSOCK without side effects, and on POSIX it
        // returns EBADF — both are harmless for our process.
        return OsSocket{ static_cast<xash::platform::SocketHandle>( 0x42 ) };
    }
    Result<OsSocket> open_tcp( IpFamily ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
    Result<std::size_t> sendto( const OsSocket &,
                                std::span<const std::byte> data,
                                const NetAddress &to ) noexcept override
    {
        ++sendto_calls;
        last_sendto_dest = to;
        last_sendto_size = data.size();
        return data.size();
    }
    Result<std::size_t> recvfrom( const OsSocket &,
                                  std::span<std::byte> out,
                                  NetAddress &from ) noexcept override
    {
        ++recvfrom_calls;
        if( !have_canned_rx )
            return std::unexpected( NetError::WouldBlock );

        const std::size_t n = std::min( out.size(), canned_rx_len );
        for( std::size_t i = 0; i < n; ++i )
            out[ i ] = canned_rx_buf[ i ];
        from = canned_rx_from;
        have_canned_rx = false;
        return n;
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

// -- send_packet to 127.0.0.1 enqueues on the OPPOSITE-side loopback ring -

static void test_send_loopback_routes_to_other_side()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    const auto payload = std::array<std::byte, 4>{
        std::byte{ 0xDE }, std::byte{ 0xAD }, std::byte{ 0xBE }, std::byte{ 0xEF } };

    // Client sends to 127.0.0.1:27015 → server side receives.
    auto sent = ctx.send_packet( SocketKind::Client, payload,
                                 NetAddress::loopback_v4( 27015 ) );
    CHECK( sent.has_value() );
    // Loopback path must not have called sendto on the platform layer.
    CHECK( fake.sendto_calls == 0 );

    NetAddress              from{};
    std::array<std::byte, 64> buf{};
    auto got = ctx.get_packet( SocketKind::Server, from, std::span{ buf } );
    CHECK( got.has_value() );
    if( got.has_value() )
    {
        CHECK( *got == payload.size() );
        for( std::size_t i = 0; i < payload.size(); ++i )
            CHECK( buf[ i ] == payload[ i ] );
        CHECK( from == NetAddress::loopback_v4() );
    }
    // Server side should NOT have polled the OS — loopback short-circuit.
    CHECK( fake.recvfrom_calls == 0 );

    ctx.shutdown();
}

// -- send_packet to a non-loopback address calls IPlatformSockets::sendto -

static void test_send_real_address_uses_platform_sendto()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    NetAddress remote{};
    remote.family     = IpFamily::V4;
    remote.port       = 27015;
    remote.addr.v4[0] = 192;
    remote.addr.v4[1] = 168;
    remote.addr.v4[2] = 1;
    remote.addr.v4[3] = 1;

    const auto payload = std::array<std::byte, 3>{
        std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };
    auto sent = ctx.send_packet( SocketKind::Client, payload, remote );
    CHECK( sent.has_value() );
    CHECK( fake.sendto_calls == 1 );
    CHECK( fake.last_sendto_dest == remote );
    CHECK( fake.last_sendto_size == payload.size() );

    ctx.shutdown();
}

// -- dedicated build sends loopback addresses via the real socket --------

static void test_send_loopback_on_dedicated_uses_platform()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    const auto payload = std::array<std::byte, 1>{ std::byte{ 0xAA } };
    auto sent = ctx.send_packet( SocketKind::Server, payload,
                                 NetAddress::loopback_v4( 27015 ) );
    CHECK( sent.has_value() );
    CHECK( fake.sendto_calls == 1 );

    ctx.shutdown();
}

// -- get_packet polls the real socket when the loopback ring is empty ----

static void test_get_packet_falls_through_to_recvfrom()
{
    FakePlatformSockets fake;
    fake.have_canned_rx           = true;
    fake.canned_rx_from.family    = IpFamily::V4;
    fake.canned_rx_from.port      = 12345;
    fake.canned_rx_from.addr.v4[0] = 10;
    fake.canned_rx_buf[ 0 ]       = std::byte{ 0x11 };
    fake.canned_rx_buf[ 1 ]       = std::byte{ 0x22 };
    fake.canned_rx_len            = 2;

    NetworkContext ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    NetAddress                from{};
    std::array<std::byte, 32> buf{};
    auto got = ctx.get_packet( SocketKind::Client, from, std::span{ buf } );
    CHECK( got.has_value() );
    if( got.has_value() )
    {
        CHECK( *got == 2 );
        CHECK( buf[ 0 ] == std::byte{ 0x11 } );
        CHECK( buf[ 1 ] == std::byte{ 0x22 } );
        CHECK( from == fake.canned_rx_from );
    }
    CHECK( fake.recvfrom_calls == 1 );

    ctx.shutdown();
}

// -- get_packet with no loopback + closed socket returns WouldBlock ------

static void test_get_packet_without_socket_returns_wouldblock()
{
    FakePlatformSockets fake;
    NetworkContext      ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ false ) ) );
    // No config() — sockets remain closed.

    NetAddress                from{};
    std::array<std::byte, 16> buf{};
    auto got = ctx.get_packet( SocketKind::Client, from, std::span{ buf } );
    CHECK( !got.has_value() );
    if( !got.has_value() )
        CHECK( got.error() == NetError::WouldBlock );
    CHECK( fake.recvfrom_calls == 0 );

    ctx.shutdown();
}

int main()
{
    test_config_singleplayer_opens_nothing();
    test_config_multiplayer_opens_both_sockets();
    test_config_dedicated_opens_only_server_socket();
    test_config_off_then_on_reopens();
    test_config_without_init_returns_not_initialised();

    test_send_loopback_routes_to_other_side();
    test_send_real_address_uses_platform_sendto();
    test_send_loopback_on_dedicated_uses_platform();
    test_get_packet_falls_through_to_recvfrom();
    test_get_packet_without_socket_returns_wouldblock();

    std::printf( "test_network_context_sockets: %d passed, %d failed\n",
                 g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
