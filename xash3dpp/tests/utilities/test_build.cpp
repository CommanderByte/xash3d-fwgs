// xash3dpp — build metadata tests
// Covers: number_from_date, COMPAT_NUMBER

#include <xash3dpp/utilities/build.hpp>
#include <cstdio>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static void test_number_from_date()
{
    // legacy: build_number_from_date in public/build.c — epoch is 2015-04-01 = 0.
    CHECK( xash::utilities::build::number_from_date( "2015-04-01" ) == 0 );
    CHECK( xash::utilities::build::number_from_date( "2015-04-02" ) == 1 );

    // April has 30 days → May 1 is day 30.
    CHECK( xash::utilities::build::number_from_date( "2015-05-01" ) == 30 );

    // 2016 is a leap year; 2016-04-01 is 366 days after 2015-04-01 (passes Feb 29).
    CHECK( xash::utilities::build::number_from_date( "2016-04-01" ) == 366 );

    // Dates before the epoch produce negative values.
    CHECK( xash::utilities::build::number_from_date( "2015-01-01" ) < 0 );

    // Feb 29 on a leap year is valid.
    CHECK( xash::utilities::build::number_from_date( "2016-02-29" ) >= 0 );

    // Feb 29 on a non-leap year is invalid.
    CHECK( xash::utilities::build::number_from_date( "2015-02-29" ) == -1 );

    // Invalid inputs — all must return -1.
    CHECK( xash::utilities::build::number_from_date( "" )           == -1 );  // too short
    CHECK( xash::utilities::build::number_from_date( "not-a-date" ) == -1 );  // non-digit year
    CHECK( xash::utilities::build::number_from_date( "2015-00-01" ) == -1 );  // month 0
    CHECK( xash::utilities::build::number_from_date( "2015-13-01" ) == -1 );  // month 13
    CHECK( xash::utilities::build::number_from_date( "2015/04/01" ) == -1 );  // wrong separator
    CHECK( xash::utilities::build::number_from_date( "2015-04-00" ) == -1 );  // day 0
}

static void test_compat_number()
{
    // Frozen at 4529 — some mods test against this value.
    CHECK( xash::utilities::build::COMPAT_NUMBER == 4529 );
}

int main()
{
    test_number_from_date();
    test_compat_number();

    std::printf( "build: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
