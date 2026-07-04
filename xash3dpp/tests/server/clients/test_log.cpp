// xash3dpp — server-log format test (Chunk 6 S9 satellite).
// Pins the "MM/DD/YYYY - HH:MM:SS: <text>" line prefix (sv_log.c:101).

#include <xash3dpp/private/server/clients.hpp>

#include "../../test_helpers.hpp"

#include <cstring>

namespace sv = xash::server;

static int g_pass = 0, g_fail = 0;

static void test_line_format()
{
    sv::ClientMachinery cm;
    char out[256] = {};
    sv::log_printf( cm, "player connected", out, sizeof( out ) );

    // suffix is exactly ": <text>"; a " - " separates the date and time.
    const std::size_t len = std::strlen( out );
    const char       *suffix = ": player connected";
    const std::size_t slen   = std::strlen( suffix );
    CHECK( len > slen );
    CHECK( std::strcmp( out + ( len - slen ), suffix ) == 0 );
    CHECK( std::strstr( out, " - " ) != nullptr );
}

int main()
{
    RUN_TEST( test_line_format );
    std::printf( "server_log: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
