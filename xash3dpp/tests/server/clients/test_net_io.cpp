// xash3dpp — server ↔ networking I/O bridge tests (Chunk 6 S9 seam).
// Drives read_packets() over a REAL NetworkContext backed by a fake
// IPlatformSockets: a connectionless "getchallenge" datagram arriving on the
// server socket must be routed into the connection state machine and its
// "challenge <N> 0" reply sent back out, framed with the -1 OOB magic word,
// to the source address (Netchan_OutOfBandPrint).  This pins the host-owns-
// NetworkContext / server-holds-a-handle wiring (decisions-architecture Q-2/Q-4).
//
// The bridge is exercised with a bare ServerRuntime — getchallenge only touches
// the challenge salt + realtime, so no game DLL / spawned map is needed.

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>
#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>

#include "../../test_helpers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

namespace sv  = xash::server;
namespace net = xash::networking;

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

// A fake platform-sockets layer with one canned inbound datagram and full
// capture of the outbound one.  Mirrors tests/networking's fixture (open_udp
// hands out a synthetic non-zero handle so NetworkContext treats the slot as
// open) plus a sendto that records the reply bytes.
struct FakeSockets final : IPlatformSockets
{
    // canned inbound (one packet, then WouldBlock)
    bool                       have_rx = false;
    NetAddress                 rx_from{};
    std::array<std::byte, 256> rx_buf{};
    std::size_t                rx_len = 0;

    // captured outbound
    int                         sendto_calls   = 0;
    int                         recvfrom_calls = 0;
    NetAddress                  last_dest{};
    std::array<std::byte, 2048> last_data{};
    std::size_t                 last_len = 0;

    void queue_oob( const NetAddress &from, const char *cmd ) noexcept
    {
        rx_buf[0] = std::byte{ 0xFF };
        rx_buf[1] = std::byte{ 0xFF };
        rx_buf[2] = std::byte{ 0xFF };
        rx_buf[3] = std::byte{ 0xFF };
        const std::size_t len = std::strlen( cmd );
        for ( std::size_t i = 0; i < len; ++i )
            rx_buf[4 + i] = static_cast<std::byte>( cmd[i] );
        rx_len  = 4 + len;
        rx_from = from;
        have_rx = true;
    }

    Result<OsSocket> open_udp( IpFamily, std::uint16_t,
                               std::string_view ) noexcept override
    {
        return OsSocket{ static_cast<xash::platform::SocketHandle>( 0x42 ) };
    }
    Result<OsSocket> open_tcp( IpFamily ) noexcept override
    {
        return std::unexpected( NetError::NotInitialised );
    }
    Result<std::size_t> sendto( const OsSocket &, std::span<const std::byte> data,
                                const NetAddress &to ) noexcept override
    {
        ++sendto_calls;
        last_dest          = to;
        const std::size_t n = std::min( data.size(), last_data.size() );
        for ( std::size_t i = 0; i < n; ++i )
            last_data[i] = data[i];
        last_len = n;
        return data.size();
    }
    Result<std::size_t> recvfrom( const OsSocket &, std::span<std::byte> out,
                                  NetAddress &from ) noexcept override
    {
        ++recvfrom_calls;
        if ( !have_rx )
            return std::unexpected( NetError::WouldBlock );
        const std::size_t n = std::min( out.size(), rx_len );
        for ( std::size_t i = 0; i < n; ++i )
            out[i] = rx_buf[i];
        from    = rx_from;
        have_rx = false;
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

NetworkInitParams make_params( FakeSockets &fake )
{
    NetworkInitParams p{};
    p.sockets   = &fake;
    p.dedicated = true; // dedicated server: one server socket, no loopback ring
    return p;
}

// A non-loopback source address so the reply routes through sendto (a loopback
// destination would be short-circuited into the in-process ring instead).
NetAddress remote_adr()
{
    NetAddress a{};
    a.family     = IpFamily::V4;
    a.port       = 27005;
    a.addr.v4[0] = 1;
    a.addr.v4[1] = 2;
    a.addr.v4[2] = 3;
    a.addr.v4[3] = 4;
    return a;
}

} // namespace

// ---------------------------------------------------------------------------
// getchallenge arrives on the server socket → challenge reply leaves via sendto,
// OOB-framed, to the source address, with the value the state machine computes.
// ---------------------------------------------------------------------------
static void test_getchallenge_bridge()
{
    FakeSockets fake;
    fake.queue_oob( remote_adr(), "getchallenge" );

    NetworkContext ctx;
    REQUIRE( ctx.init( make_params( fake ) ) );
    REQUIRE( ctx.config( /*multiplayer=*/true, /*change_port=*/false ).has_value() );

    sv::ServerRuntime rt;
    rt.net              = &ctx;
    rt.clients.realtime = 100.0;

    sv::read_packets( rt );

    // exactly one OOB reply, to the source address.
    REQUIRE( fake.sendto_calls == 1 );
    CHECK( fake.last_dest == remote_adr() );

    // reply = FF FF FF FF "challenge <N> 0"
    REQUIRE( fake.last_len > 4 );
    CHECK( fake.last_data[0] == std::byte{ 0xFF } );
    CHECK( fake.last_data[1] == std::byte{ 0xFF } );
    CHECK( fake.last_data[2] == std::byte{ 0xFF } );
    CHECK( fake.last_data[3] == std::byte{ 0xFF } );

    char        reply[256] = {};
    std::size_t body       = fake.last_len - 4;
    if ( body > sizeof( reply ) - 1 )
        body = sizeof( reply ) - 1;
    std::memcpy( reply, fake.last_data.data() + 4, body );

    int chal = 0;
    REQUIRE( std::sscanf( reply, "challenge %d", &chal ) == 1 );

    // the reply challenge reproduces the stateless SV_GetChallenge value.
    const std::uint32_t window =
        static_cast<std::uint32_t>( rt.clients.realtime / 5 );
    CHECK_EQ( chal, sv::compute_challenge( rt.persistent.challenge_salt,
                                           remote_adr(), window ) );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// ping → ack (a second connectionless path through the same bridge).
// ---------------------------------------------------------------------------
static void test_ping_bridge()
{
    FakeSockets fake;
    fake.queue_oob( remote_adr(), "ping" );

    NetworkContext ctx;
    REQUIRE( ctx.init( make_params( fake ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    sv::ServerRuntime rt;
    rt.net              = &ctx;
    rt.clients.realtime = 100.0;

    sv::read_packets( rt );

    REQUIRE( fake.sendto_calls == 1 );
    REQUIRE( fake.last_len == 4 + 3 ); // FFx4 + "ack"
    CHECK( fake.last_data[4] == static_cast<std::byte>( 'a' ) );
    CHECK( fake.last_data[5] == static_cast<std::byte>( 'c' ) );
    CHECK( fake.last_data[6] == static_cast<std::byte>( 'k' ) );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// A non-OOB (in-session) datagram from an unknown peer is silently dropped —
// the netchan demux is the Slice-C seam; nothing is sent back.
// ---------------------------------------------------------------------------
static void test_insession_packet_dropped()
{
    FakeSockets fake;
    // leading dword != -1 ⇒ a sequenced netchan packet, not connectionless.
    fake.rx_buf[0] = std::byte{ 0x01 };
    fake.rx_buf[1] = std::byte{ 0x00 };
    fake.rx_buf[2] = std::byte{ 0x00 };
    fake.rx_buf[3] = std::byte{ 0x00 };
    fake.rx_len    = 8;
    fake.rx_from   = remote_adr();
    fake.have_rx   = true;

    NetworkContext ctx;
    REQUIRE( ctx.init( make_params( fake ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    sv::ServerRuntime rt;
    rt.net              = &ctx;
    rt.clients.realtime = 100.0;

    sv::read_packets( rt );

    CHECK( fake.recvfrom_calls >= 1 ); // polled the socket
    CHECK_EQ( fake.sendto_calls, 0 );  // no reply — dropped at the Slice-C seam

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// A null NetworkContext (offline / scaffold server) makes read_packets inert.
// ---------------------------------------------------------------------------
static void test_null_net_is_noop()
{
    sv::ServerRuntime rt; // rt.net == nullptr
    sv::read_packets( rt );
    CHECK( rt.net == nullptr ); // reached here without touching the network
}

int main()
{
    // read_packets + the OOB sink assert the main-thread role (OQ-9).
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_getchallenge_bridge );
    RUN_TEST( test_ping_bridge );
    RUN_TEST( test_insession_packet_dropped );
    RUN_TEST( test_null_net_is_noop );

    std::printf( "server_net_io: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
