// xash3dpp — split-packet encode/decode round-trip test

#include <xash3dpp/private/networking/wire/split_packet.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

namespace {

std::vector<std::byte> make_payload( std::size_t n, std::uint8_t seed )
{
    std::vector<std::byte> out( n );
    for( std::size_t i = 0; i < n; ++i )
        out[ i ] = std::byte{ static_cast<std::uint8_t>( seed + i ) };
    return out;
}

} // namespace

static void test_xash_split_roundtrip()
{
    const auto payload = make_payload( 2500, 0x10 );
    SplitProducerXash producer( payload, /*seq*/ 42, /*splitsize*/ 600 );
    CHECK_EQ( static_cast<int>( producer.total_fragments() ),
              static_cast<int>( ( 2500 + ( 600 - 10 ) - 1 ) / ( 600 - 10 ) ) );

    std::vector<std::byte> reassembled;
    reassembled.reserve( payload.size() );

    std::array<std::byte, 600> wire{};
    while( !producer.exhausted() )
    {
        auto n = producer.next( wire );
        CHECK( n.has_value() );
        if( !n.has_value() || *n == 0 )
            break;

        std::span<const std::byte> datagram( wire.data(), *n );
        auto info = decode_split_xash( datagram );
        CHECK( info.has_value() );
        CHECK_EQ( static_cast<int>( info->sequence_number ), 42 );
        // append in arrival order — producer emits sequentially
        reassembled.insert( reassembled.end(),
                            info->payload.begin(), info->payload.end() );
    }
    CHECK_EQ( reassembled.size(), payload.size() );
    CHECK( std::memcmp( reassembled.data(), payload.data(), payload.size() ) == 0 );
}

static void test_goldsrc_split_roundtrip()
{
    const auto payload = make_payload( 1200, 0xA0 );
    SplitProducerGoldSrc producer( payload, /*seq*/ 7, /*splitsize*/ 509 );
    CHECK( producer.total_fragments() > 0 );
    CHECK( producer.total_fragments() <= 15 );

    std::vector<std::byte> reassembled;
    reassembled.reserve( payload.size() );

    std::array<std::byte, 600> wire{};
    while( !producer.exhausted() )
    {
        auto n = producer.next( wire );
        CHECK( n.has_value() );
        if( !n.has_value() || *n == 0 )
            break;
        auto info = decode_split_goldsrc( { wire.data(), *n } );
        CHECK( info.has_value() );
        CHECK_EQ( static_cast<int>( info->sequence_number ), 7 );
        reassembled.insert( reassembled.end(),
                            info->payload.begin(), info->payload.end() );
    }
    CHECK_EQ( reassembled.size(), payload.size() );
    CHECK( std::memcmp( reassembled.data(), payload.data(), payload.size() ) == 0 );
}

static void test_decode_rejects_bad_magic()
{
    std::array<std::byte, 16> bogus{};
    auto r = decode_split_xash( bogus );
    CHECK( !r.has_value() );
}

static void test_decode_rejects_short_buffer()
{
    std::array<std::byte, 4> tiny{};
    auto r = decode_split_xash( tiny );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BufferTooSmall );
}

static void test_goldsrc_rejects_oversize_count()
{
    // 20 fragments would overflow nibble.
    const auto payload = make_payload( 20 * 100, 0x00 );
    SplitProducerGoldSrc producer( payload, 1, /*splitsize*/ 100 + 9 );
    CHECK_EQ( static_cast<int>( producer.total_fragments() ), 0 );
}

int main()
{
    std::printf( "test_split_packet\n" );
    RUN_TEST( test_xash_split_roundtrip );
    RUN_TEST( test_goldsrc_split_roundtrip );
    RUN_TEST( test_decode_rejects_bad_magic );
    RUN_TEST( test_decode_rejects_short_buffer );
    RUN_TEST( test_goldsrc_rejects_oversize_count );
    std::printf( "test_split_packet: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
