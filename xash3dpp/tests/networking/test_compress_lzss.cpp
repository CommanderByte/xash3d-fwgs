// xash3dpp — LZSS compression round-trip test
// Validates the ported codec against itself and confirms decompress can
// reject malformed inputs.  Cross-checking against the legacy binary output
// will land in the integration test suite (item #49).

#include <xash3dpp/private/networking/compress.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking::lzss;
using xash::networking::NetError;

static void test_roundtrip_repeating()
{
    // Highly compressible input.
    std::vector<std::byte> src( 4096, std::byte{ 0x55 } );
    auto compressed = compress( src );
    CHECK( compressed.has_value() );
    CHECK( compressed->size() < src.size() );
    CHECK( is_compressed( *compressed ) );
    CHECK_EQ( static_cast<int>( actual_size( *compressed ) ),
              static_cast<int>( src.size() ) );

    std::vector<std::byte> out( src.size() );
    auto n = decompress( *compressed, out );
    CHECK( n.has_value() );
    CHECK_EQ( static_cast<int>( *n ), static_cast<int>( src.size() ) );
    CHECK( std::memcmp( out.data(), src.data(), src.size() ) == 0 );
}

static void test_roundtrip_mixed()
{
    std::vector<std::byte> src;
    src.reserve( 2048 );
    for( int i = 0; i < 256; ++i )
        for( int j = 0; j < 8; ++j )
            src.push_back( std::byte{ static_cast<std::uint8_t>( ( i + j ) & 0xFF ) } );

    auto compressed = compress( src );
    CHECK( compressed.has_value() );
    std::vector<std::byte> out( src.size() );
    auto n = decompress( *compressed, out );
    CHECK( n.has_value() );
    CHECK_EQ( static_cast<int>( *n ), static_cast<int>( src.size() ) );
    CHECK( std::memcmp( out.data(), src.data(), src.size() ) == 0 );
}

static void test_incompressible_input_returns_error()
{
    // Pseudo-random-ish input that LZSS can't shrink.  Use a small input
    // (just over the header threshold) so the bailout fires.
    std::vector<std::byte> src;
    src.reserve( 32 );
    for( int i = 0; i < 32; ++i )
        src.push_back( std::byte{ static_cast<std::uint8_t>( ( i * 73 + 11 ) & 0xFF ) } );
    auto compressed = compress( src );
    CHECK( !compressed.has_value() );
    CHECK( compressed.error() == NetError::BadAddress );
}

static void test_decompress_rejects_bad_magic()
{
    std::array<std::byte, 32> garbage{};
    std::vector<std::byte> out( 1024 );
    auto r = decompress( garbage, out );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BadAddress );
}

static void test_decompress_rejects_oversize_target()
{
    std::vector<std::byte> src( 4096, std::byte{ 0x42 } );
    auto compressed = compress( src );
    CHECK( compressed.has_value() );
    std::array<std::byte, 16> too_small{};
    auto r = decompress( *compressed, too_small );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::BufferTooSmall );
}

static void test_is_compressed_and_size_queries()
{
    std::vector<std::byte> src( 1024, std::byte{ 0xAA } );
    auto compressed = compress( src );
    CHECK( compressed.has_value() );
    CHECK( is_compressed( *compressed ) );
    CHECK_EQ( static_cast<int>( actual_size( *compressed ) ),
              static_cast<int>( src.size() ) );

    std::array<std::byte, 4> tiny{};
    CHECK( !is_compressed( tiny ) );
    CHECK_EQ( static_cast<int>( actual_size( tiny ) ), 0 );
}

int main()
{
    std::printf( "test_compress_lzss\n" );
    RUN_TEST( test_roundtrip_repeating );
    RUN_TEST( test_roundtrip_mixed );
    RUN_TEST( test_incompressible_input_returns_error );
    RUN_TEST( test_decompress_rejects_bad_magic );
    RUN_TEST( test_decompress_rejects_oversize_target );
    RUN_TEST( test_is_compressed_and_size_queries );
    std::printf( "test_compress_lzss: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
