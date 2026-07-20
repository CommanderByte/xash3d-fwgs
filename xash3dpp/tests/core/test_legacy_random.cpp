#include <xash3dpp/core/legacy_random.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" {
void  legacy_oracle_set_seed( int seed );
int   legacy_oracle_random_long( int low, int high );
float legacy_oracle_random_float( float low, float high );
void  legacy_oracle_set_wall_seconds( std::int64_t seconds );
int   legacy_oracle_next_raw( void );
}

static int g_pass = 0, g_fail = 0;

namespace {

std::int64_t g_wall_seconds = 1700000123;

std::int64_t fixed_wall_seconds() noexcept
{
    return g_wall_seconds;
}

void seed_pair( ::xash::core::LegacyRandom &rng, int seed )
{
    legacy_oracle_set_wall_seconds( g_wall_seconds );
    legacy_oracle_set_seed( seed );
    rng.set_seed( seed );
}

void test_raw_sequence_matches_oracle()
{
    // Captured by running this executable's compiled, byte-verified C oracle
    // with --print-goldens. These values are not hand-derived.
    constexpr std::array<int, 5> captured{
        1672652053, 1932183681, 582342211, 826235900, 1619043826
    };

    ::xash::core::LegacyRandom rng( &fixed_wall_seconds );
    seed_pair( rng, 1 );

    for ( int i = 0; i < 128; ++i )
    {
        const int expected = legacy_oracle_next_raw();
        if ( i < static_cast<int>( captured.size() ))
            CHECK( expected == captured[static_cast<std::size_t>( i )] );
        const int actual = rng.random_long( 0, std::numeric_limits<int>::max() );
        CHECK( actual == expected );
    }
}

void test_captured_float_bits()
{
    constexpr std::array<std::uint32_t, 5> captured{
        0x3f47654au, 0x3f665591u, 0x3e8ad759u, 0x3ec4fd70u, 0x3f41014cu
    };

    ::xash::core::LegacyRandom rng( &fixed_wall_seconds );
    seed_pair( rng, 1 );
    for ( const std::uint32_t bits : captured )
    {
        const float expected = legacy_oracle_random_float( 0.0f, 1.0f );
        const float actual = rng.random_float( 0.0f, 1.0f );
        CHECK( std::bit_cast<std::uint32_t>( expected ) == bits );
        CHECK( std::bit_cast<std::uint32_t>( actual ) == bits );
    }
}

void test_seed_and_integer_range_matrix()
{
    constexpr std::array seeds{
        0, 1, -1, 999, 1000, 1001, -999, -1000, -1001,
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max()
    };
    constexpr std::array ranges{
        std::array{ 0, 0 },
        std::array{ 7, 7 },
        std::array{ 8, 7 },
        std::array{ -32, 31 },
        std::array{ 0, 255 },
        std::array{ 0, 1073741824 }, // rejection probability near one half
        std::array{ 0, std::numeric_limits<int>::max() },
        std::array{ std::numeric_limits<int>::min(), -1 }
    };

    for ( const int seed : seeds )
    {
        for ( const auto &range : ranges )
        {
            ::xash::core::LegacyRandom rng( &fixed_wall_seconds );
            seed_pair( rng, seed );
            for ( int draw = 0; draw < 32; ++draw )
            {
                const int expected =
                    legacy_oracle_random_long( range[0], range[1] );
                const int actual = rng.random_long( range[0], range[1] );
                CHECK( actual == expected );
            }
        }
    }
}

void test_float_bits_match_oracle()
{
    constexpr std::array seeds{ 0, 1, 999, 1000, 1001, -1000,
                                std::numeric_limits<int>::min() };
    constexpr std::array ranges{
        std::array{ 0.0f, 1.0f },
        std::array{ -1.0f, 1.0f },
        std::array{ 7.0f, 7.0f },
        std::array{ 5.0f, -3.0f },
        std::array{ -10000.25f, 20000.5f }
    };

    for ( const int seed : seeds )
    {
        for ( const auto &range : ranges )
        {
            ::xash::core::LegacyRandom rng( &fixed_wall_seconds );
            seed_pair( rng, seed );
            for ( int draw = 0; draw < 32; ++draw )
            {
                const float expected =
                    legacy_oracle_random_float( range[0], range[1] );
                const float actual = rng.random_float( range[0], range[1] );
                CHECK( std::bit_cast<std::uint32_t>( actual ) ==
                       std::bit_cast<std::uint32_t>( expected ) );
            }
        }
    }
}

void test_draw_consumption_quirks()
{
    ::xash::core::LegacyRandom rng( &fixed_wall_seconds );

    seed_pair( rng, 42 );
    CHECK( rng.random_long( 5, 5 ) == legacy_oracle_random_long( 5, 5 ) );
    CHECK( rng.random_long( 0, 1000 ) ==
           legacy_oracle_random_long( 0, 1000 ) );

    seed_pair( rng, 42 );
    CHECK( rng.random_long( 9, 8 ) == legacy_oracle_random_long( 9, 8 ) );
    CHECK( rng.random_long( 0, 1000 ) ==
           legacy_oracle_random_long( 0, 1000 ) );

    seed_pair( rng, 42 );
    CHECK( std::bit_cast<std::uint32_t>( rng.random_float( 3.0f, 3.0f )) ==
           std::bit_cast<std::uint32_t>(
               legacy_oracle_random_float( 3.0f, 3.0f )) );
    CHECK( rng.random_long( 0, 1000 ) ==
           legacy_oracle_random_long( 0, 1000 ) );
}

void print_oracle_goldens()
{
    legacy_oracle_set_seed( 1 );
    std::printf( "raw seed=1:" );
    for ( int i = 0; i < 5; ++i )
        std::printf( " %d", legacy_oracle_next_raw() );
    std::printf( "\n" );

    legacy_oracle_set_seed( 1 );
    std::printf( "float[0,1] seed=1 bits:" );
    for ( int i = 0; i < 5; ++i )
        std::printf( " %08x", std::bit_cast<std::uint32_t>(
            legacy_oracle_random_float( 0.0f, 1.0f )) );
    std::printf( "\n" );
}

} // namespace

int main( int argc, char **argv )
{
    if ( argc == 2 && std::strcmp( argv[1], "--print-goldens" ) == 0 )
    {
        print_oracle_goldens();
        return 0;
    }

    RUN_TEST( test_raw_sequence_matches_oracle );
    RUN_TEST( test_captured_float_bits );
    RUN_TEST( test_seed_and_integer_range_matrix );
    RUN_TEST( test_float_bits_match_oracle );
    RUN_TEST( test_draw_consumption_quirks );

    std::printf( "legacy_random: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
