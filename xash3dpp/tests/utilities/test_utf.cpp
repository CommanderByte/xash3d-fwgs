// xash3dpp — UTF-8/16 conversion tests
// Covers: decode_utf8, decode_utf16, encode_utf8, Utf8Decoder, Utf16Decoder,
//         length, utf16_to_utf8, to_cp1251, to_cp1252

#include <xash3dpp/utilities/utf.hpp>

#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::puts("FAIL: " #expr " (" __FILE__ ")"); } } while(0)

// Cast to uint8_t for comparison against unsigned hex literals.
static constexpr uint8_t u8( char c ) noexcept { return static_cast<uint8_t>( c ); }

// ---------------------------------------------------------------------------
// decode_utf8
// ---------------------------------------------------------------------------

static void test_decode_utf8()
{
    // legacy: utflib_decodestate / utflib_decodechar in public/utflib.c

    // ASCII fast path — returns byte immediately, state unchanged.
    xash::utilities::utf::DecodeState s{};
    CHECK( xash::utilities::utf::decode_utf8( s, 0x41u ) == 0x41u );
    CHECK( s.remaining == 0 );

    // 2-byte sequence: U+00A9 © = 0xC2 0xA9
    xash::utilities::utf::DecodeState s2{};
    CHECK( xash::utilities::utf::decode_utf8( s2, 0xC2u ) == 0u );  // incomplete
    CHECK( s2.remaining == 1 );
    CHECK( xash::utilities::utf::decode_utf8( s2, 0xA9u ) == 0x00A9u );
    CHECK( s2.remaining == 0 );

    // 3-byte sequence: U+20AC € = 0xE2 0x82 0xAC
    xash::utilities::utf::DecodeState s3{};
    CHECK( xash::utilities::utf::decode_utf8( s3, 0xE2u ) == 0u );
    CHECK( xash::utilities::utf::decode_utf8( s3, 0x82u ) == 0u );
    CHECK( xash::utilities::utf::decode_utf8( s3, 0xACu ) == 0x20ACu );

    // 4-byte sequence: U+1F600 😀 = 0xF0 0x9F 0x98 0x80
    xash::utilities::utf::DecodeState s4{};
    CHECK( xash::utilities::utf::decode_utf8( s4, 0xF0u ) == 0u );
    CHECK( xash::utilities::utf::decode_utf8( s4, 0x9Fu ) == 0u );
    CHECK( xash::utilities::utf::decode_utf8( s4, 0x98u ) == 0u );
    CHECK( xash::utilities::utf::decode_utf8( s4, 0x80u ) == 0x1F600u );

    // Invalid lead byte (≥ 0xF8) → returns 0, state remains idle.
    xash::utilities::utf::DecodeState si{};
    CHECK( xash::utilities::utf::decode_utf8( si, 0xF9u ) == 0u );
    CHECK( si.remaining == 0 );

    // Bare continuation byte (0x80 with no prior lead) → returns 0.
    xash::utilities::utf::DecodeState sc{};
    CHECK( xash::utilities::utf::decode_utf8( sc, 0x80u ) == 0u );
    CHECK( sc.remaining == 0 );
}

// ---------------------------------------------------------------------------
// decode_utf16
// ---------------------------------------------------------------------------

static void test_decode_utf16()
{
    // BMP codepoint — returned directly.
    xash::utilities::utf::DecodeState s{};
    CHECK( xash::utilities::utf::decode_utf16( s, 0x0041u ) == 0x0041u );

    // Surrogate pair for U+1F600: high=0xD83D, low=0xDE00
    xash::utilities::utf::DecodeState sp{};
    CHECK( xash::utilities::utf::decode_utf16( sp, 0xD83Du ) == 0u );  // awaiting low surrogate
    CHECK( sp.remaining == 1 );
    CHECK( xash::utilities::utf::decode_utf16( sp, 0xDE00u ) == 0x1F600u );
    CHECK( sp.remaining == 0 );

    // Invalid low surrogate → state reset, returns 0.
    xash::utilities::utf::DecodeState si{};
    xash::utilities::utf::decode_utf16( si, 0xD83Du );   // high surrogate
    CHECK( xash::utilities::utf::decode_utf16( si, 0x0041u ) == 0u );  // wrong unit → error
    CHECK( si.remaining == 0 );
}

// ---------------------------------------------------------------------------
// encode_utf8
// ---------------------------------------------------------------------------

static void test_encode_utf8()
{
    // ASCII
    {
        const auto [b, n] = xash::utilities::utf::encode_utf8( 0x41u );
        CHECK( n == 1 && u8( b[0] ) == 0x41u );
    }
    // U+00A9 © — 2-byte: C2 A9
    {
        const auto [b, n] = xash::utilities::utf::encode_utf8( 0x00A9u );
        CHECK( n == 2 && u8( b[0] ) == 0xC2u && u8( b[1] ) == 0xA9u );
    }
    // U+20AC € — 3-byte: E2 82 AC
    {
        const auto [b, n] = xash::utilities::utf::encode_utf8( 0x20ACu );
        CHECK( n == 3 && u8( b[0] ) == 0xE2u && u8( b[1] ) == 0x82u && u8( b[2] ) == 0xACu );
    }
    // U+1F600 😀 — 4-byte: F0 9F 98 80
    {
        const auto [b, n] = xash::utilities::utf::encode_utf8( 0x1F600u );
        CHECK( n == 4 );
        CHECK( u8( b[0] ) == 0xF0u && u8( b[1] ) == 0x9Fu );
        CHECK( u8( b[2] ) == 0x98u && u8( b[3] ) == 0x80u );
    }
}

// ---------------------------------------------------------------------------
// Utf8Decoder
// ---------------------------------------------------------------------------

static void test_utf8_decoder()
{
    xash::utilities::utf::Utf8Decoder dec;

    // ASCII single byte → immediate result.
    const auto r_ascii = dec.feed( 0x41u );
    CHECK( r_ascii.has_value() && *r_ascii == 0x41u );

    // 2-byte © — first byte incomplete, second complete.
    const auto r1 = dec.feed( 0xC2u );
    CHECK( !r1.has_value() );
    const auto r2 = dec.feed( 0xA9u );
    CHECK( r2.has_value() && *r2 == 0x00A9u );

    // Invalid standalone continuation byte → optional{0} (caller substitutes U+FFFD).
    dec.reset();
    const auto r_inv = dec.feed( 0x80u );
    CHECK( r_inv.has_value() && *r_inv == 0u );
}

// ---------------------------------------------------------------------------
// Utf16Decoder
// ---------------------------------------------------------------------------

static void test_utf16_decoder()
{
    xash::utilities::utf::Utf16Decoder dec;

    // BMP codepoint.
    const auto r_bmp = dec.feed( 0x0041u );
    CHECK( r_bmp.has_value() && *r_bmp == 0x0041u );

    // Surrogate pair for U+1F600.
    const auto r_hi = dec.feed( 0xD83Du );
    CHECK( !r_hi.has_value() );
    const auto r_lo = dec.feed( 0xDE00u );
    CHECK( r_lo.has_value() && *r_lo == 0x1F600u );

    // Invalid low surrogate (BMP unit after high surrogate) → optional{0}.
    dec.reset();
    dec.feed( 0xD83Du );
    const auto r_inv = dec.feed( 0x0041u );
    CHECK( r_inv.has_value() && *r_inv == 0u );
}

// ---------------------------------------------------------------------------
// length
// ---------------------------------------------------------------------------

static void test_length()
{
    // legacy: utflib_length in public/utflib.c
    CHECK( xash::utilities::utf::length( "ABC" )         == 3 );
    CHECK( xash::utilities::utf::length( "" )            == 0 );

    // 2-byte codepoint © = 2 bytes = 1 codepoint.
    CHECK( xash::utilities::utf::length( "\xC2\xA9" )    == 1 );

    // Mix: "A©" = 3 bytes = 2 codepoints.
    CHECK( xash::utilities::utf::length( "A\xC2\xA9" )   == 2 );

    // 4-byte 😀 = 4 bytes = 1 codepoint.
    CHECK( xash::utilities::utf::length( "\xF0\x9F\x98\x80" ) == 1 );
}

// ---------------------------------------------------------------------------
// utf16_to_utf8
// ---------------------------------------------------------------------------

static void test_utf16_to_utf8()
{
    // ASCII string.
    const std::uint16_t ascii_src[] = { 0x41u, 0x42u, 0x43u, 0x0000u };
    char dst[32]{};
    const std::size_t n = xash::utilities::utf::utf16_to_utf8(
        std::span{ dst }, std::span{ ascii_src } );
    CHECK( n == 3 );
    CHECK( std::strcmp( dst, "ABC" ) == 0 );

    // Empty source.
    const std::uint16_t empty_src[] = { 0x0000u };
    char dst2[8]{};
    CHECK( xash::utilities::utf::utf16_to_utf8( std::span{ dst2 }, std::span{ empty_src } ) == 0 );
    CHECK( dst2[0] == '\0' );

    // Surrogate pair: U+1F600 😀
    const std::uint16_t emoji_src[] = { 0xD83Du, 0xDE00u, 0x0000u };
    char dst3[8]{};
    const std::size_t m = xash::utilities::utf::utf16_to_utf8(
        std::span{ dst3 }, std::span{ emoji_src } );
    CHECK( m == 4 );
    CHECK( static_cast<uint8_t>( dst3[0] ) == 0xF0u );
    CHECK( static_cast<uint8_t>( dst3[1] ) == 0x9Fu );
    CHECK( static_cast<uint8_t>( dst3[2] ) == 0x98u );
    CHECK( static_cast<uint8_t>( dst3[3] ) == 0x80u );
}

// ---------------------------------------------------------------------------
// to_cp1251
// ---------------------------------------------------------------------------

static void test_to_cp1251()
{
    // legacy: utflib_encode1251 in public/utflib.c
    // ASCII passthrough.
    CHECK( xash::utilities::utf::to_cp1251( 0x41u ) == 0x41u );

    // Cyrillic capital А (U+0410) → 0xC0.
    CHECK( xash::utilities::utf::to_cp1251( 0x0410u ) == 0xC0u );

    // Cyrillic small а (U+0430) → 0xE0.
    CHECK( xash::utilities::utf::to_cp1251( 0x0430u ) == 0xE0u );

    // Ё (U+0401) is in the 64-entry table at index 40 → 0xA8.
    CHECK( xash::utilities::utf::to_cp1251( 0x0401u ) == 0xA8u );

    // Unknown codepoint → '?'.
    CHECK( xash::utilities::utf::to_cp1251( 0x1F600u ) == static_cast<uint32_t>( '?' ) );
}

// ---------------------------------------------------------------------------
// to_cp1252
// ---------------------------------------------------------------------------

static void test_to_cp1252()
{
    // legacy: utflib_encode1252 in public/utflib.c
    // Strict passthrough for cp < 0xFF.
    CHECK( xash::utilities::utf::to_cp1252( 0x41u ) == 0x41u );
    CHECK( xash::utilities::utf::to_cp1252( 0x00u ) == 0x00u );
    CHECK( xash::utilities::utf::to_cp1252( 0xFEu ) == 0xFEu );

    // 0xFF is NOT strictly less than 0xFF → returns '?'.
    CHECK( xash::utilities::utf::to_cp1252( 0xFFu ) == static_cast<uint32_t>( '?' ) );

    // Any codepoint above 0xFF → '?'.
    CHECK( xash::utilities::utf::to_cp1252( 0x100u ) == static_cast<uint32_t>( '?' ) );
}

int main()
{
    test_decode_utf8();
    test_decode_utf16();
    test_encode_utf8();
    test_utf8_decoder();
    test_utf16_decoder();
    test_length();
    test_utf16_to_utf8();
    test_to_cp1251();
    test_to_cp1252();

    std::printf( "utf: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
