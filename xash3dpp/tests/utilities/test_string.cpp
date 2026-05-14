// xash3dpp — string utility tests
// Covers: strncpy, stricmp, strnicmp, snprintf, atoi, atof, atov,
//         strip_colors, pretify_mem, match_pattern, parse_token, Tokenizer

#include <xash3dpp/utilities/string.hpp>
#include <cstring>
#include <cassert>
#include <cstdio>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::puts("FAIL: " #expr " (" __FILE__ ")"); } } while(0)

static void test_strncpy()
{
    char buf[8]{};
    xash::utilities::strncpy( buf, "hello", sizeof buf );
    // Must null-terminate even when src fits exactly.
    CHECK( buf[7] == '\0' );

    char tight[4]{};
    xash::utilities::strncpy( tight, "toolong", sizeof tight );
    // Must not write past buf.
    CHECK( tight[3] == '\0' );
}

static void test_stricmp()
{
    CHECK( xash::utilities::stricmp( "ABC", "abc" ) == 0 );
    CHECK( xash::utilities::stricmp( "abc", "abd" ) < 0 );
    CHECK( xash::utilities::stricmp( "abd", "abc" ) > 0 );
}

static void test_atoi()
{
    CHECK( xash::utilities::atoi( "42" )   == 42 );
    CHECK( xash::utilities::atoi( "-7" )   == -7 );
    CHECK( xash::utilities::atoi( "0x1F" ) == 31 );  // legacy: hex prefix support
    CHECK( xash::utilities::atoi( nullptr ) == 0 );
}

static void test_strnicmp()
{
    // legacy: Q_strnicmp in public/crtlib.c
    CHECK( xash::utilities::strnicmp( "ABCxyz", "abcXYZ", 3 ) == 0 );
    CHECK( xash::utilities::strnicmp( "abc", "abd", 2 ) == 0 );  // only "ab" compared
    CHECK( xash::utilities::strnicmp( "abc", "abd", 3 ) < 0 );
    CHECK( xash::utilities::strnicmp( "abc", "abc", 0 ) == 0 );  // n=0 is always equal
}

static void test_snprintf()
{
    // legacy: Q_snprintf in public/crtlib.c — always null-terminates.
    char buf[16]{};
    xash::utilities::snprintf( buf, sizeof buf, "%s", "hello!" );
    CHECK( std::strcmp( buf, "hello!" ) == 0 );

    // Truncation: result is null-terminated within the buffer.
    char tight[4]{};
    xash::utilities::snprintf( tight, sizeof tight, "%s", "toolong" );
    CHECK( tight[3] == '\0' );
    CHECK( std::strcmp( tight, "too" ) == 0 );
}

static void test_atof()
{
    // legacy: Q_atof in public/crtlib.c
    CHECK( xash::utilities::atof( "1.5"  ) == 1.5f  );
    CHECK( xash::utilities::atof( "-3.0" ) == -3.0f );
    CHECK( xash::utilities::atof( "0x40" ) == 64.0f );  // hex prefix → 64
    CHECK( xash::utilities::atof( "  2.5" ) == 2.5f  );  // leading whitespace
    CHECK( xash::utilities::atof( ""     ) == 0.0f  );
    CHECK( xash::utilities::atof( nullptr ) == 0.0f );
}

static void test_atov()
{
    // legacy: Q_atov in public/crtlib.c
    float out[3]{};
    xash::utilities::atov( std::span{ out }, "1.0 2.0 3.0" );
    CHECK( out[0] == 1.0f && out[1] == 2.0f && out[2] == 3.0f );

    // Fewer values than slots — remaining slots stay zero.
    float out2[3]{};
    xash::utilities::atov( std::span{ out2 }, "5.0" );
    CHECK( out2[0] == 5.0f && out2[1] == 0.0f && out2[2] == 0.0f );

    // More values than slots — extra values are silently dropped.
    float out3[2]{};
    xash::utilities::atov( std::span{ out3 }, "1.0 2.0 3.0" );
    CHECK( out3[0] == 1.0f && out3[1] == 2.0f );
}

static void test_strip_colors()
{
    // legacy: COM_StripColors in engine/common/common.c
    char out[32]{};
    xash::utilities::strip_colors( "^1red^0", out );
    CHECK( std::strcmp( out, "red" ) == 0 );

    xash::utilities::strip_colors( "plain", out );
    CHECK( std::strcmp( out, "plain" ) == 0 );

    // string_view overload.
    CHECK( xash::utilities::strip_colors( "^1hello ^2world" ) == "hello world" );
    CHECK( xash::utilities::strip_colors( "" ) == "" );
}

static void test_pretify_mem()
{
    // legacy: Q_pretifymem in public/crtlib.c
    CHECK( xash::utilities::pretify_mem( 100.0f,    0 ) == "100 bytes" );
    CHECK( xash::utilities::pretify_mem( 2048.0f,   0 ) == "2 Kb" );  // 2048 > 1024 → Kb
    CHECK( xash::utilities::pretify_mem( 1000.0f * 1024.0f, 0 ) == "1,000 Kb" );  // thousands sep
}

static void test_match_pattern()
{
    CHECK(  xash::utilities::match_pattern( "foo.bsp", "*.bsp", false ) );
    CHECK( !xash::utilities::match_pattern( "foo.bsp", "*.tga", false ) );
    CHECK(  xash::utilities::match_pattern( "dir/file.mdl", "dir/*.mdl", false ) );
}

static void test_parse_token()
{
    // legacy: COM_ParseFileSafe in public/crtlib.c
    char buf[64]{};
    int  len = 0;
    const char *data = "hello world";

    data = xash::utilities::parse_token( data, buf, sizeof buf,
                                         xash::utilities::TokenFlags::None, &len, nullptr );
    CHECK( len == 5 && std::strcmp( buf, "hello" ) == 0 );

    data = xash::utilities::parse_token( data, buf, sizeof buf,
                                         xash::utilities::TokenFlags::None, &len, nullptr );
    CHECK( len == 5 && std::strcmp( buf, "world" ) == 0 );

    // End of input → returns nullptr, len reset to 0.
    data = xash::utilities::parse_token( data, buf, sizeof buf,
                                         xash::utilities::TokenFlags::None, &len, nullptr );
    CHECK( data == nullptr && len == 0 );
}

static void test_tokenizer()
{
    // RAII wrapper around parse_token.
    xash::utilities::Tokenizer tok( "foo bar \"quoted\"" );

    const auto t1 = tok.next();
    CHECK( t1.has_value() && t1->text == "foo" && !t1->quoted );

    const auto t2 = tok.next();
    CHECK( t2.has_value() && t2->text == "bar" && !t2->quoted );

    const auto t3 = tok.next();
    CHECK( t3.has_value() && t3->text == "quoted" && t3->quoted );

    CHECK( !tok.next().has_value() );
}

int main()
{
    test_strncpy();
    test_stricmp();
    test_strnicmp();
    test_snprintf();
    test_atoi();
    test_atof();
    test_atov();
    test_strip_colors();
    test_pretify_mem();
    test_match_pattern();
    test_parse_token();
    test_tokenizer();

    std::printf( "string: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
