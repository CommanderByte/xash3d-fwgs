// xash3dpp — master-list satellite (heartbeat / shutdown wire I/O)
// Verifies that the built-in MasterListClient routes its packets through
// NetworkContext::send_packet on SocketKind::Server, respects lan_only(),
// gracefully tolerates an empty master_addresses() span, and emits the
// expected GoldSrc OOB byte sequences.
//
// We drive the satellite through a real NetworkContext wired to a
// FakePlatformSockets that captures every sendto() call so we can assert
// on destination address + payload bytes.

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/master_list.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

using xash::networking::create_master_list_client;
using xash::networking::IMasterListClient;
using xash::networking::IMasterListConfig;
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

namespace {

struct SendCapture
{
    NetAddress                from{};
    NetAddress                to{};
    std::vector<std::byte>    payload;
};

// Minimal fake sockets that records every sendto call.  open_udp returns
// a synthetic non-zero handle so NetworkContext treats the slot as live.
struct CapturingSockets final : IPlatformSockets
{
    std::vector<SendCapture> sends;
    int open_udp_calls = 0;

    Result<OsSocket> open_udp( IpFamily, std::uint16_t,
                               std::string_view ) noexcept override
    {
        ++open_udp_calls;
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
        SendCapture c;
        c.to      = to;
        c.payload.assign( data.begin(), data.end() );
        sends.emplace_back( std::move( c ) );
        return data.size();
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

// Fake config that stores its values + addresses as plain members so
// individual tests can mutate `lan_only_` / `addrs_` directly.
struct FakeConfig final : IMasterListConfig
{
    bool                     lan_only_ { false };
    bool                     nat_      { false };
    double                   hb_secs_  { 200.0 };
    std::vector<NetAddress>  addrs_;

    [[nodiscard]] bool   lan_only()   const noexcept override { return lan_only_; }
    [[nodiscard]] bool   nat_bypass() const noexcept override { return nat_; }
    [[nodiscard]] double heartbeat_interval_seconds() const noexcept override { return hb_secs_; }
    [[nodiscard]] std::span<const NetAddress> master_addresses() const noexcept override
    {
        return std::span<const NetAddress>{ addrs_.data(), addrs_.size() };
    }
};

[[nodiscard]] NetAddress make_v4( std::uint8_t a, std::uint8_t b,
                                  std::uint8_t c, std::uint8_t d,
                                  std::uint16_t port ) noexcept
{
    NetAddress addr{};
    addr.family     = IpFamily::V4;
    addr.port       = port;
    addr.addr.v4[0] = a;
    addr.addr.v4[1] = b;
    addr.addr.v4[2] = c;
    addr.addr.v4[3] = d;
    return addr;
}

NetworkInitParams make_params( CapturingSockets &fake, bool dedicated ) noexcept
{
    NetworkInitParams p{};
    p.sockets   = &fake;
    p.dedicated = dedicated;
    return p;
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_create_returns_non_null()
{
    CapturingSockets fake;
    NetworkContext   ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );
    FakeConfig cfg;
    auto client = create_master_list_client( ctx, cfg );
    CHECK( client != nullptr );
    ctx.shutdown();
}

static void test_heartbeat_sends_oob_q_packet_to_each_master()
{
    CapturingSockets fake;
    NetworkContext   ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );
    REQUIRE( ctx.config( /*multiplayer=*/ true, /*change_port=*/ false ).has_value() );

    FakeConfig cfg;
    cfg.addrs_.push_back( make_v4( 192, 168, 1,  10, 27010 ) );
    cfg.addrs_.push_back( make_v4(  10,  0, 0,   2, 27011 ) );

    auto client = create_master_list_client( ctx, cfg );
    REQUIRE( client != nullptr );

    client->heartbeat();

    REQUIRE( fake.sends.size() == 2u );
    // Each send must be the 6-byte OOB heartbeat: FF FF FF FF 'q' '\n'.
    for( const auto &s : fake.sends )
    {
        REQUIRE( s.payload.size() == 6u );
        CHECK( s.payload[ 0 ] == std::byte{ 0xFF } );
        CHECK( s.payload[ 1 ] == std::byte{ 0xFF } );
        CHECK( s.payload[ 2 ] == std::byte{ 0xFF } );
        CHECK( s.payload[ 3 ] == std::byte{ 0xFF } );
        CHECK( s.payload[ 4 ] == std::byte{ 'q'  } );
        CHECK( s.payload[ 5 ] == std::byte{ '\n' } );
    }
    CHECK( fake.sends[ 0 ].to == cfg.addrs_[ 0 ] );
    CHECK( fake.sends[ 1 ].to == cfg.addrs_[ 1 ] );

    ctx.shutdown();
}

static void test_send_shutdown_emits_b_packet()
{
    CapturingSockets fake;
    NetworkContext   ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );
    REQUIRE( ctx.config( /*multiplayer=*/ true, /*change_port=*/ false ).has_value() );

    FakeConfig cfg;
    cfg.addrs_.push_back( make_v4( 192, 168, 1, 10, 27010 ) );

    auto client = create_master_list_client( ctx, cfg );
    REQUIRE( client != nullptr );

    client->send_shutdown();

    REQUIRE( fake.sends.size() == 1u );
    REQUIRE( fake.sends[ 0 ].payload.size() == 6u );
    CHECK( fake.sends[ 0 ].payload[ 4 ] == std::byte{ 'b'  } );
    CHECK( fake.sends[ 0 ].payload[ 5 ] == std::byte{ '\n' } );

    ctx.shutdown();
}

static void test_lan_only_suppresses_all_sends()
{
    CapturingSockets fake;
    NetworkContext   ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    FakeConfig cfg;
    cfg.lan_only_ = true;
    cfg.addrs_.push_back( make_v4( 192, 168, 1, 10, 27010 ) );

    auto client = create_master_list_client( ctx, cfg );
    REQUIRE( client != nullptr );

    client->heartbeat();
    client->send_shutdown();
    CHECK( fake.sends.empty() );

    ctx.shutdown();
}

static void test_empty_address_list_is_a_noop()
{
    CapturingSockets fake;
    NetworkContext   ctx;
    REQUIRE( ctx.init( make_params( fake, /*dedicated=*/ true ) ) );
    REQUIRE( ctx.config( true, false ).has_value() );

    FakeConfig cfg; // addrs_ is empty
    auto client = create_master_list_client( ctx, cfg );
    REQUIRE( client != nullptr );

    client->heartbeat();
    client->send_shutdown();
    CHECK( fake.sends.empty() );

    ctx.shutdown();
}

int main()
{
    test_create_returns_non_null();
    test_heartbeat_sends_oob_q_packet_to_each_master();
    test_send_shutdown_emits_b_packet();
    test_lan_only_suppresses_all_sends();
    test_empty_address_list_is_a_noop();

    std::printf( "test_master_list: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
