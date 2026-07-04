// xash3dpp — PVS zero-RLE decompression goldens (Chunk 5, C7)
// Hand-derived from the Mod_DecompressPVS algorithm (mod_bmodel.c:1059-1086):
// nonzero byte = literal, zero byte = next byte is a zero-run length,
// run clamped to the output end, NULL input = all-visible fill.

#include <xash3dpp/map_loader/pvs.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <cstring>
#include <vector>

using xash::map_loader::decompress_pvs;

static int g_pass = 0, g_fail = 0;

namespace
{

std::vector<std::byte> bytes( std::initializer_list<unsigned char> v )
{
    std::vector<std::byte> out;
    for ( unsigned char c : v )
        out.push_back( static_cast<std::byte>( c ));
    return out;
}

std::vector<std::byte> run( const std::vector<std::byte> &in, std::size_t visbytes )
{
    std::vector<std::byte> out( visbytes, std::byte{ 0xEE } ); // poison
    decompress_pvs( in, visbytes, out );
    return out;
}

bool eq( const std::vector<std::byte> &a, std::initializer_list<unsigned char> b )
{
    if ( a.size() != b.size() )
        return false;
    std::size_t i = 0;
    for ( unsigned char c : b )
        if ( static_cast<unsigned char>( a[i++] ) != c )
            return false;
    return true;
}

} // namespace

static void test_literal_copy()
{
    CHECK( eq( run( bytes( { 0x03, 0x05 } ), 2 ), { 0x03, 0x05 } ));
}

static void test_zero_run()
{
    // 0xAA, then a run of 3 zero bytes, then 0xBB.
    CHECK( eq( run( bytes( { 0xAA, 0x00, 0x03, 0xBB } ), 5 ),
               { 0xAA, 0x00, 0x00, 0x00, 0xBB } ));
}

static void test_run_clamped_at_end()
{
    // Run of 255 clamped to the 3 remaining output bytes.
    CHECK( eq( run( bytes( { 0x00, 0xFF } ), 3 ), { 0x00, 0x00, 0x00 } ));
}

static void test_zero_length_run_progresses()
{
    // A zero-byte with run length 0 must not stall the decoder.
    CHECK( eq( run( bytes( { 0x00, 0x00, 0x07 } ), 1 ), { 0x07 } ));
}

static void test_null_input_all_visible()
{
    CHECK( eq( run( {}, 3 ), { 0xFF, 0xFF, 0xFF } ));
}

static void test_input_exhaustion_zero_fills()
{
    // Hardening: stream ends mid-output → remainder zero-filled.
    CHECK( eq( run( bytes( { 0x11 } ), 3 ), { 0x11, 0x00, 0x00 } ));
    // Trailing zero byte with no run length behaves as run 0 then exhausts.
    CHECK( eq( run( bytes( { 0x11, 0x00 } ), 3 ), { 0x11, 0x00, 0x00 } ));
}

static void test_output_span_limits()
{
    // visbytes larger than the out span: only out.size() bytes written.
    std::array<std::byte, 2> out{ std::byte{ 0xEE }, std::byte{ 0xEE } };
    decompress_pvs( {}, 8, out );
    CHECK_EQ( static_cast<unsigned>( out[0] ), 0xFFu );
    CHECK_EQ( static_cast<unsigned>( out[1] ), 0xFFu );
}

int main()
{
    RUN_TEST( test_literal_copy );
    RUN_TEST( test_zero_run );
    RUN_TEST( test_run_clamped_at_end );
    RUN_TEST( test_zero_length_run_progresses );
    RUN_TEST( test_null_input_all_visible );
    RUN_TEST( test_input_exhaustion_zero_fills );
    RUN_TEST( test_output_span_limits );

    std::printf( "pvs_decompress: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
