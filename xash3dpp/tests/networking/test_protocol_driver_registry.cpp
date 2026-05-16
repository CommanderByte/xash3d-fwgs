// xash3dpp — default protocol driver registry smoke test
// Verifies GoldSrcProtocolDriver (protocol 48) and XashProtocolDriver
// (protocol 49) are correctly registered, and that nullptr is returned for
// anything else.

#include <xash3dpp/private/networking/protocol_driver_default.hpp>
#include <xash3dpp/networking/message_buf.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstring>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_resolves_protocol_48()
{
    auto &reg = default_protocol_driver_registry();
    auto *drv = reg.resolve( 48 );
    REQUIRE( drv != nullptr );
    CHECK( std::strcmp( drv->name(), "goldsrc" ) == 0 );
    CHECK( drv->split_format() == SplitFormat::GoldSrc );
    CHECK( drv->delta_tables() == DeltaTableSet::GoldSrc );
}

static void test_resolves_protocol_49()
{
    auto &reg = default_protocol_driver_registry();
    auto *drv = reg.resolve( 49 );
    REQUIRE( drv != nullptr );
    CHECK( std::strcmp( drv->name(), "xash" ) == 0 );
    CHECK( drv->split_format() == SplitFormat::Xash );
    CHECK( drv->delta_tables() == DeltaTableSet::Xash );
    CHECK( drv->sends_qport()  == true );
}

static void test_unknown_protocol_returns_nullptr()
{
    auto &reg = default_protocol_driver_registry();
    CHECK( reg.resolve( 0 )    == nullptr );
    CHECK( reg.resolve( 47 )   == nullptr );
    CHECK( reg.resolve( 50 )   == nullptr );
    CHECK( reg.resolve( 9999 ) == nullptr );
}

static void test_singleton_identity_stable()
{
    auto *a = &default_protocol_driver_registry();
    auto *b = &default_protocol_driver_registry();
    CHECK( a == b );

    // Same driver instance across resolves of the same protocol.
    auto *d1 = a->resolve( 48 );
    auto *d2 = a->resolve( 48 );
    CHECK( d1 == d2 );

    // Protocols 48 and 49 resolve to distinct driver instances.
    auto *d48 = a->resolve( 48 );
    auto *d49 = a->resolve( 49 );
    CHECK( d48 != d49 );
}

// ---------- GoldSrc packet-header writer / reader -------------------------

static void test_goldsrc_sends_no_qport()
{
    auto *drv = default_protocol_driver_registry().resolve( 48 );
    REQUIRE( drv != nullptr );
    CHECK( drv->sends_qport() == false );
}

static void test_goldsrc_write_packet_header_minimal()
{
    auto *drv = default_protocol_driver_registry().resolve( 48 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 32> storage {};
    MessageBuf out{ storage, "test-out" };

    PacketHeaderInput in{};
    in.outgoing_sequence = 0x1234;
    in.incoming_sequence = 0x5678;
    in.is_client         = true;
    in.qport             = 0xABCD;

    auto r = drv->write_packet_header( out, in );
    CHECK( r.has_value() );
    // GoldSrc never writes qport, so exactly 8 bytes regardless of is_client.
    CHECK( out.real_bytes_written() == 8u );

    const auto bytes = out.data();
    const std::uint32_t w1 =
        static_cast<std::uint32_t>( bytes[ 0 ] ) |
        ( static_cast<std::uint32_t>( bytes[ 1 ] ) <<  8 ) |
        ( static_cast<std::uint32_t>( bytes[ 2 ] ) << 16 ) |
        ( static_cast<std::uint32_t>( bytes[ 3 ] ) << 24 );
    const std::uint32_t w2 =
        static_cast<std::uint32_t>( bytes[ 4 ] ) |
        ( static_cast<std::uint32_t>( bytes[ 5 ] ) <<  8 ) |
        ( static_cast<std::uint32_t>( bytes[ 6 ] ) << 16 ) |
        ( static_cast<std::uint32_t>( bytes[ 7 ] ) << 24 );
    CHECK( w1 == 0x1234u );
    CHECK( w2 == 0x5678u );
}

static void test_goldsrc_write_sets_reliable_bits()
{
    auto *drv = default_protocol_driver_registry().resolve( 48 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 32> storage {};
    MessageBuf out{ storage, "test-out" };

    PacketHeaderInput in{};
    in.outgoing_sequence          = 7;
    in.incoming_sequence          = 9;
    in.incoming_reliable_sequence = 1;
    in.send_reliable              = true;
    in.send_reliable_fragment     = true;

    auto r = drv->write_packet_header( out, in );
    CHECK( r.has_value() );

    const auto bytes = out.data();
    const std::uint32_t w1 =
        static_cast<std::uint32_t>( bytes[ 0 ] ) |
        ( static_cast<std::uint32_t>( bytes[ 1 ] ) <<  8 ) |
        ( static_cast<std::uint32_t>( bytes[ 2 ] ) << 16 ) |
        ( static_cast<std::uint32_t>( bytes[ 3 ] ) << 24 );
    const std::uint32_t w2 =
        static_cast<std::uint32_t>( bytes[ 4 ] ) |
        ( static_cast<std::uint32_t>( bytes[ 5 ] ) <<  8 ) |
        ( static_cast<std::uint32_t>( bytes[ 6 ] ) << 16 ) |
        ( static_cast<std::uint32_t>( bytes[ 7 ] ) << 24 );
    CHECK( ( w1 & 0x80000000u ) != 0u ); // reliable bit
    CHECK( ( w1 & 0x40000000u ) != 0u ); // reliable-fragment bit
    CHECK( ( w1 & ~0xC0000000u ) == 7u );
    CHECK( ( w2 & 0x80000000u ) != 0u ); // incoming_reliable bit
    CHECK( ( w2 & ~0x80000000u ) == 9u );
}

static void test_goldsrc_write_overflows_on_tiny_buffer()
{
    auto *drv = default_protocol_driver_registry().resolve( 48 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 4> tiny {}; // only room for w1, not w2
    MessageBuf out{ tiny, "tiny" };

    PacketHeaderInput in{};
    auto r = drv->write_packet_header( out, in );
    CHECK( !r.has_value() );
    if( !r.has_value() )
        CHECK( r.error() == NetError::Overflow );
}

static void test_goldsrc_read_round_trip()
{
    auto *drv = default_protocol_driver_registry().resolve( 48 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 32> storage {};
    MessageBuf buf{ storage, "rt" };

    PacketHeaderInput in{};
    in.outgoing_sequence          = 0x0010'0001u;
    in.incoming_sequence          = 0x0020'0002u;
    in.incoming_reliable_sequence = 1u;
    in.send_reliable              = true;
    auto wr = drv->write_packet_header( buf, in );
    REQUIRE( wr.has_value() );

    // Rewind to bit 0 to start reading.
    REQUIRE( buf.seek_to_bit( 0, SeekOrigin::Begin ) );

    // GoldSrc round-trip: reading back as a server (direction matches the
    // write side, though GoldSrc has no qport so is_server_socket is moot).
    auto meta = drv->read_packet_header( buf, /*is_server_socket=*/true );
    REQUIRE( meta.has_value() );
    CHECK( meta->sequence     == 0x0010'0001u );
    CHECK( meta->sequence_ack == 0x0020'0002u );
    CHECK( meta->is_reliable  == true );
    CHECK( meta->is_split     == false );
    CHECK( meta->is_oob       == false );
}

static void test_goldsrc_read_rejects_truncated()
{
    auto *drv = default_protocol_driver_registry().resolve( 48 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 4> tiny {}; // only 4 bytes available
    MessageBuf buf{ tiny, "trunc" };
    auto meta = drv->read_packet_header( buf, /*is_server_socket=*/true );
    CHECK( !meta.has_value() );
    if( !meta.has_value() )
        CHECK( meta.error() == NetError::BufferTooSmall );
}

// ---------- Xash packet-header writer / reader ----------------------------

static void test_xash_sends_qport()
{
    auto *drv = default_protocol_driver_registry().resolve( 49 );
    REQUIRE( drv != nullptr );
    CHECK( drv->sends_qport() == true );
}

static void test_xash_write_includes_qport_for_client()
{
    auto *drv = default_protocol_driver_registry().resolve( 49 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 32> storage {};
    MessageBuf out{ storage, "test-out" };

    PacketHeaderInput in{};
    in.outgoing_sequence = 0x1234;
    in.incoming_sequence = 0x5678;
    in.is_client         = true;
    in.qport             = 0x1ABC;

    auto r = drv->write_packet_header( out, in );
    CHECK( r.has_value() );
    // Xash client: 8-byte header + 2-byte qport = 10 bytes total.
    CHECK( out.real_bytes_written() == 10u );

    const auto bytes = out.data();
    const std::uint16_t qport_wire =
        static_cast<std::uint16_t>( bytes[ 8 ] ) |
        ( static_cast<std::uint16_t>( bytes[ 9 ] ) << 8 );
    CHECK( qport_wire == 0x1ABCu );
}

static void test_xash_write_no_qport_for_server()
{
    auto *drv = default_protocol_driver_registry().resolve( 49 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 32> storage {};
    MessageBuf out{ storage, "test-out" };

    PacketHeaderInput in{};
    in.outgoing_sequence = 5;
    in.incoming_sequence = 3;
    in.is_client         = false; // server side: qport field ignored
    in.qport             = 0xDEAD;

    auto r = drv->write_packet_header( out, in );
    CHECK( r.has_value() );
    CHECK( out.real_bytes_written() == 8u ); // no qport on server path
}

static void test_xash_read_round_trip_with_qport()
{
    auto *drv = default_protocol_driver_registry().resolve( 49 );
    REQUIRE( drv != nullptr );

    std::array<std::byte, 32> storage {};
    MessageBuf buf{ storage, "rt" };

    PacketHeaderInput in{};
    in.outgoing_sequence = 42;
    in.incoming_sequence = 17;
    in.is_client         = true;
    in.qport             = 0x7777;

    REQUIRE( drv->write_packet_header( buf, in ).has_value() );
    REQUIRE( buf.seek_to_bit( 0, SeekOrigin::Begin ) );

    // Server reading a client packet: is_server_socket=true so the 2-byte
    // qport word is consumed from the stream.
    auto meta = drv->read_packet_header( buf, /*is_server_socket=*/true );
    REQUIRE( meta.has_value() );
    CHECK( meta->sequence     == 42u );
    CHECK( meta->sequence_ack == 17u );
    CHECK( meta->is_reliable  == false );
    // Read cursor must have advanced past the 10-byte (80-bit) header.
    CHECK( buf.tell_bit() == 80u );
}

static void test_xash_read_rejects_truncated_without_qport()
{
    auto *drv = default_protocol_driver_registry().resolve( 49 );
    REQUIRE( drv != nullptr );

    // 8 bytes: enough for w1+w2 but missing the 2-byte qport.
    std::array<std::byte, 8> storage {};
    MessageBuf buf{ storage, "trunc" };

    // Server reading a client packet: driver expects the qport and must
    // return BufferTooSmall when it is absent.
    auto meta = drv->read_packet_header( buf, /*is_server_socket=*/true );
    CHECK( !meta.has_value() );
    if( !meta.has_value() )
        CHECK( meta.error() == NetError::BufferTooSmall );
}

static void test_xash_read_client_side_no_qport()
{
    // Client reading a server->client packet: the server never writes a
    // qport, so is_server_socket=false means the driver must NOT try to
    // consume one.  An 8-byte buffer (w1+w2 only) must succeed.
    auto *drv = default_protocol_driver_registry().resolve( 49 );
    REQUIRE( drv != nullptr );

    // Hand-craft an 8-byte server->client header (sequence=7, ack=3).
    std::array<std::byte, 8> storage {};
    auto emit = []( std::array<std::byte, 8> &a, int offset, std::uint32_t v )
    {
        a[offset + 0] = static_cast<std::byte>( v & 0xFFu );
        a[offset + 1] = static_cast<std::byte>( ( v >> 8  ) & 0xFFu );
        a[offset + 2] = static_cast<std::byte>( ( v >> 16 ) & 0xFFu );
        a[offset + 3] = static_cast<std::byte>( ( v >> 24 ) & 0xFFu );
    };
    emit( storage, 0, 7u );   // w1: sequence=7, no reliable
    emit( storage, 4, 3u );   // w2: sequence_ack=3

    MessageBuf buf{ storage, "srv-pkt" };
    auto meta = drv->read_packet_header( buf, /*is_server_socket=*/false );
    REQUIRE( meta.has_value() );
    CHECK( meta->sequence     == 7u );
    CHECK( meta->sequence_ack == 3u );
    CHECK( meta->is_reliable  == false );
    // Read cursor must be at exactly 64 bits (8 bytes) — no qport consumed.
    CHECK( buf.tell_bit() == 64u );
}

int main()
{
    test_resolves_protocol_48();
    test_resolves_protocol_49();
    test_unknown_protocol_returns_nullptr();
    test_singleton_identity_stable();

    test_goldsrc_sends_no_qport();
    test_goldsrc_write_packet_header_minimal();
    test_goldsrc_write_sets_reliable_bits();
    test_goldsrc_write_overflows_on_tiny_buffer();
    test_goldsrc_read_round_trip();
    test_goldsrc_read_rejects_truncated();

    test_xash_sends_qport();
    test_xash_write_includes_qport_for_client();
    test_xash_write_no_qport_for_server();
    test_xash_read_round_trip_with_qport();
    test_xash_read_rejects_truncated_without_qport();
    test_xash_read_client_side_no_qport();

    std::printf( "test_protocol_driver_registry: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
