// xash3dpp — Info-string helper tests (Chunk 6 S9).
// Pins Info_ValueForKey / SetValueForKey / RemoveKey / RemovePrefixedKeys /
// IsValid parity against engine/common/infostring.c.

#include <xash3dpp/private/server/info_string.hpp>

#include "../../test_helpers.hpp"

#include <cstring>

namespace sv = xash::server;

static int g_pass = 0, g_fail = 0;

static const char *val( const char *s, const char *key )
{
    static char buf[256];
    return sv::info_value_for_key( s, key, buf, sizeof( buf ) );
}

static void test_value_for_key()
{
    const char *s = "\\name\\Player\\rate\\9999\\model\\gordon";
    CHECK_STREQ( val( s, "name" ), "Player" );
    CHECK_STREQ( val( s, "rate" ), "9999" );
    CHECK_STREQ( val( s, "model" ), "gordon" );
    CHECK_STREQ( val( s, "absent" ), "" ); // missing key ⇒ empty
    CHECK_STREQ( val( "", "name" ), "" );
}

static void test_set_and_remove()
{
    char s[512] = "";
    sv::info_set_value_for_key( s, "name", "Bob", sizeof( s ) );
    sv::info_set_value_for_key( s, "rate", "5000", sizeof( s ) );
    CHECK_STREQ( val( s, "name" ), "Bob" );
    CHECK_STREQ( val( s, "rate" ), "5000" );

    // overwrite existing removes the old binding first (no duplicate).
    sv::info_set_value_for_key( s, "name", "Alice", sizeof( s ) );
    CHECK_STREQ( val( s, "name" ), "Alice" );

    // remove
    sv::info_remove_key( s, "rate" );
    CHECK_STREQ( val( s, "rate" ), "" );
    CHECK_STREQ( val( s, "name" ), "Alice" );

    // setting an empty value clears the key.
    sv::info_set_value_for_key( s, "name", "", sizeof( s ) );
    CHECK_STREQ( val( s, "name" ), "" );
}

static void test_star_key_protection()
{
    char s[256] = "";
    // '*' keys are engine-reserved unless star_allowed.
    sv::info_set_value_for_key( s, "*sid", "1", sizeof( s ), false );
    CHECK_STREQ( val( s, "*sid" ), "" );
    sv::info_set_value_for_key( s, "*sid", "1", sizeof( s ), true );
    CHECK_STREQ( val( s, "*sid" ), "1" );
}

static void test_reject_illegal()
{
    char s[256] = "\\name\\Bob";
    // keys/values with backslash or quote are rejected (s unchanged).
    sv::info_set_value_for_key( s, "bad\\key", "x", sizeof( s ) );
    sv::info_set_value_for_key( s, "quote", "a\"b", sizeof( s ) );
    CHECK_STREQ( val( s, "name" ), "Bob" );
    CHECK_STREQ( val( s, "bad\\key" ), "" );
    CHECK_STREQ( val( s, "quote" ), "" );
}

static void test_remove_prefixed()
{
    char s[256] = "";
    sv::info_set_value_for_key( s, "name", "Bob", sizeof( s ) );
    sv::info_set_value_for_key( s, "_hidden", "secret", sizeof( s ) );
    sv::info_set_value_for_key( s, "_also", "x", sizeof( s ) );
    sv::info_remove_prefixed_keys( s, '_' );
    CHECK_STREQ( val( s, "name" ), "Bob" );
    CHECK_STREQ( val( s, "_hidden" ), "" );
    CHECK_STREQ( val( s, "_also" ), "" );
}

static void test_is_valid()
{
    CHECK( sv::info_is_valid( "\\name\\Bob\\rate\\9999" ) );
    CHECK( sv::info_is_valid( "" ) );
    CHECK( !sv::info_is_valid( "\\name" ) );        // key with no value
    CHECK( !sv::info_is_valid( "\\name\\\\rate\\1" ) ); // empty value
}

int main()
{
    RUN_TEST( test_value_for_key );
    RUN_TEST( test_set_and_remove );
    RUN_TEST( test_star_key_protection );
    RUN_TEST( test_reject_illegal );
    RUN_TEST( test_remove_prefixed );
    RUN_TEST( test_is_valid );
    std::printf( "server_info_string: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
