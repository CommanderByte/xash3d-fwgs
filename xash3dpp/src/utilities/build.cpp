// xash3dpp — build number and VCS metadata
// Legacy reference: public/build.c + public/build_vcs.c
//
// build_vcs.cpp is generated at configure time.  This file provides the
// runtime helpers that parse and expose the metadata.

#include <xash3dpp/utilities/build.hpp>
#include <array>

namespace xash::utilities::build {

// These symbols are provided by the generated build_vcs.cpp.
// Declare as weak so the library links without the generated file during dev.
[[gnu::weak]] const std::string_view commit      = "(unknown)";
[[gnu::weak]] const std::string_view branch      = "(unknown)";
[[gnu::weak]] const std::string_view commit_date = "1970-01-01";

int number_from_date( std::string_view iso_date ) noexcept
{
    // "YYYY-MM-DD" is exactly 10 characters.
    if( iso_date.size() != 10 )
        return -1;

    const auto digit = []( char c ) noexcept -> int {
        const int value = c - '0';
        return static_cast<unsigned>( value ) <= 9 ? value : -1;
    };

    const int y0 = digit( iso_date[0] ), y1 = digit( iso_date[1] ),
              y2 = digit( iso_date[2] ), y3 = digit( iso_date[3] );
    if( y0 < 0 || y1 < 0 || y2 < 0 || y3 < 0 || iso_date[4] != '-' )
        return -1;

    const int m0 = digit( iso_date[5] ), m1 = digit( iso_date[6] );
    if( m0 < 0 || m1 < 0 || iso_date[7] != '-' )
        return -1;

    const int d0 = digit( iso_date[8] ), d1 = digit( iso_date[9] );
    if( d0 < 0 || d1 < 0 )
        return -1;

    const int year = y0 * 1000 + y1 * 100 + y2 * 10 + y3;
    const int month = m0 * 10 + m1;
    const int day = d0 * 10 + d1;

    if( month < 1 || month > 12 || day < 1 )
        return -1;

    const bool is_leap = ( year % 4 == 0 && ( year % 100 != 0 || year % 400 == 0 ) );
    constexpr std::array<int, 13> month_prefix
        = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    constexpr std::array<int, 12> month_days
        = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    const int max_day = month_days[month - 1] + ( is_leap && month == 2 );
    if( day > max_day )
        return -1;

    const long long y_1 = year - 1LL;
    const long long days = 365LL * y_1 + y_1 / 4LL - y_1 / 100LL + y_1 / 400LL
        + month_prefix[month - 1] + day - 1 + ( is_leap && month > 2 );

    return static_cast<int>( days - 735688LL );
}

int number() noexcept
{
    // Magic-static: evaluated once at first call, then a plain load.
    static const int cached = number_from_date( commit_date );
    return cached;
}

} // namespace xash::utilities::build
