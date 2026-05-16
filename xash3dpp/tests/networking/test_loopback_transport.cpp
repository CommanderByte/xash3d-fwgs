// xash3dpp — LoopbackTransport tests

#include <xash3dpp/private/networking/loopback_transport.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_empty_receive_would_block()
{
    LoopbackTransport t;
    std::array<std::byte, 64> buf{};
    auto r = t.receive( SocketKind::Client, buf );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::WouldBlock );
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Client ) ), 0 );
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Server ) ), 0 );
}

static void test_client_to_server_roundtrip()
{
    LoopbackTransport t;
    const std::byte payload[] = { std::byte{ 0xDE }, std::byte{ 0xAD },
                                  std::byte{ 0xBE }, std::byte{ 0xEF } };
    auto s = t.send( SocketKind::Client, payload );
    CHECK( s.has_value() );
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Server ) ), 1 );
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Client ) ), 0 );

    std::array<std::byte, 64> buf{};
    auto r = t.receive( SocketKind::Server, buf );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 4 );
    CHECK( std::memcmp( buf.data(), payload, sizeof( payload ) ) == 0 );
    CHECK( t.pending( SocketKind::Server ) == 0 );
}

static void test_server_to_client_roundtrip()
{
    LoopbackTransport t;
    const std::byte payload[] = { std::byte{ 0x42 } };
    CHECK( t.send( SocketKind::Server, payload ).has_value() );
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Client ) ), 1 );

    std::array<std::byte, 8> buf{};
    auto r = t.receive( SocketKind::Client, buf );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 1 );
    CHECK( buf[0] == std::byte{ 0x42 } );
}

static void test_payload_too_large_returns_overflow()
{
    LoopbackTransport t;
    std::vector<std::byte> giant( loopback_slot_capacity + 1, std::byte{ 0 } );
    auto s = t.send( SocketKind::Client, giant );
    CHECK( !s.has_value() );
    CHECK( s.error() == NetError::Overflow );
}

static void test_receive_buffer_too_small()
{
    LoopbackTransport t;
    const std::byte payload[8] = {};
    CHECK( t.send( SocketKind::Client, payload ).has_value() );
    std::array<std::byte, 4> tiny{};
    auto r = t.receive( SocketKind::Server, tiny );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BufferTooSmall );
    // Packet remains queued.
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Server ) ), 1 );
}

static void test_ring_overflow_drops_oldest()
{
    LoopbackTransport t;
    // Send well past the ring capacity.
    for( int i = 0; i < static_cast<int>( ::xash::limits::net_max_loopback ) + 4; ++i )
    {
        const std::byte payload[1] = { std::byte{ static_cast<std::uint8_t>( i ) } };
        CHECK( t.send( SocketKind::Client, payload ).has_value() );
    }
    // After clamping, exactly net_max_loopback packets remain.
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Server ) ),
              static_cast<int>( ::xash::limits::net_max_loopback ) );

    // Drain.  Oldest should have been dropped — first packet observed is not 0.
    std::array<std::byte, 8> buf{};
    auto first = t.receive( SocketKind::Server, buf );
    CHECK( first.has_value() );
    CHECK( buf[0] != std::byte{ 0 } );
}

static void test_clear()
{
    LoopbackTransport t;
    const std::byte payload[2] = {};
    CHECK( t.send( SocketKind::Client, payload ).has_value() );
    CHECK( t.send( SocketKind::Server, payload ).has_value() );
    t.clear();
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Client ) ), 0 );
    CHECK_EQ( static_cast<int>( t.pending( SocketKind::Server ) ), 0 );
}

static void test_zero_length_packet_roundtrips()
{
    LoopbackTransport t;
    CHECK( t.send( SocketKind::Client, std::span<const std::byte>{} ).has_value() );
    std::array<std::byte, 4> buf{};
    auto r = t.receive( SocketKind::Server, buf );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 0 );
}

int main()
{
    std::printf( "test_loopback_transport\n" );
    RUN_TEST( test_empty_receive_would_block );
    RUN_TEST( test_client_to_server_roundtrip );
    RUN_TEST( test_server_to_client_roundtrip );
    RUN_TEST( test_payload_too_large_returns_overflow );
    RUN_TEST( test_receive_buffer_too_small );
    RUN_TEST( test_ring_overflow_drops_oldest );
    RUN_TEST( test_clear );
    RUN_TEST( test_zero_length_packet_roundtrips );
    std::printf( "test_loopback_transport: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
