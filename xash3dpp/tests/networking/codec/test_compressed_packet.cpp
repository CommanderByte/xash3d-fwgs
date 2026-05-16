// xash3dpp — Compressed-packet wrapper tests

#include <xash3dpp/private/networking/codec/compressed_packet.hpp>

#include "../../test_helpers.hpp"

#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_encode_decode_roundtrip()
{
    std::vector<std::byte> payload( 2048, std::byte{ 0x33 } );
    auto wrapped = compressed_packet::encode( payload );
    CHECK( wrapped.has_value() );
    CHECK( compressed_packet::is_compressed_packet( *wrapped ) );

    // First four bytes are the compressed-packet magic (0xFFFFFFFD little-endian).
    CHECK( ( *wrapped )[0] == std::byte{ 0xFD } );
    CHECK( ( *wrapped )[1] == std::byte{ 0xFF } );
    CHECK( ( *wrapped )[2] == std::byte{ 0xFF } );
    CHECK( ( *wrapped )[3] == std::byte{ 0xFF } );

    CHECK_EQ( static_cast<int>( compressed_packet::inflated_size( *wrapped ) ),
              static_cast<int>( payload.size() ) );

    std::vector<std::byte> out( payload.size() );
    auto                   n = compressed_packet::decode( *wrapped, out );
    CHECK( n.has_value() );
    CHECK_EQ( static_cast<int>( *n ), static_cast<int>( payload.size() ) );
    CHECK( std::memcmp( out.data(), payload.data(), payload.size() ) == 0 );
}

static void test_decode_rejects_wrong_magic()
{
    std::vector<std::byte> bogus( 16 );
    bogus[0] = std::byte{ 0xFF }; bogus[1] = std::byte{ 0xFF };
    bogus[2] = std::byte{ 0xFF }; bogus[3] = std::byte{ 0xFF }; // OOB magic
    std::vector<std::byte> out( 32 );
    auto r = compressed_packet::decode( bogus, out );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BadAddress );
    CHECK( !compressed_packet::is_compressed_packet( bogus ) );
}

static void test_decode_rejects_truncated_packet()
{
    std::vector<std::byte> tiny( 2 );
    std::vector<std::byte> out( 32 );
    auto r = compressed_packet::decode( tiny, out );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BadAddress );
}

static void test_encode_propagates_lzss_error()
{
    // Tiny pseudo-random input — LZSS bails out as "incompressible".
    std::vector<std::byte> src;
    for( int i = 0; i < 32; ++i )
        src.push_back( std::byte{ static_cast<std::uint8_t>( i * 73 + 11 ) } );
    auto wrapped = compressed_packet::encode( src );
    CHECK( !wrapped.has_value() );
}

static void test_inflated_size_zero_on_garbage()
{
    std::vector<std::byte> garbage( 16, std::byte{ 0x00 } );
    CHECK_EQ( static_cast<int>( compressed_packet::inflated_size( garbage ) ), 0 );
}

int main()
{
    std::printf( "test_compressed_packet\n" );
    RUN_TEST( test_encode_decode_roundtrip );
    RUN_TEST( test_decode_rejects_wrong_magic );
    RUN_TEST( test_decode_rejects_truncated_packet );
    RUN_TEST( test_encode_propagates_lzss_error );
    RUN_TEST( test_inflated_size_zero_on_garbage );
    std::printf( "test_compressed_packet: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
