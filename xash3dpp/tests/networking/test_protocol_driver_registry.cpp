// xash3dpp — default protocol driver registry smoke test
// Verifies that default_protocol_driver_registry() exposes the GoldSrc
// driver for wire protocols 48 and 49, and returns nullptr for anything
// else.

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
    CHECK( std::strcmp( drv->name(), "goldsrc" ) == 0 );
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

    auto meta = drv->read_packet_header( buf );
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
    auto meta = drv->read_packet_header( buf );
    CHECK( !meta.has_value() );
    if( !meta.has_value() )
        CHECK( meta.error() == NetError::BufferTooSmall );
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

    std::printf( "test_protocol_driver_registry: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
