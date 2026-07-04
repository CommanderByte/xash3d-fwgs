// xash3dpp — server string pool behaviour pins (Chunk 6 S4, OQ-6)
// Covers: offset 0 == empty string, alloc/get round-trip, escape
// processing (\n plus the FWGS \r/\t extension; unknown escapes copy
// through), deduplication, the wrap-on-overflow overwrite quirk, the
// static/dynamic phase switch, SV_MakeString's INT-range test (synthetic
// pointers, never dereferenced), and the in-range make_string path.

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/string_pool.hpp>
#include <xash3dpp/utilities/string.hpp>

#include "../../test_helpers.hpp"

#include <cstdint>

namespace sv = xash::server;

static int g_pass = 0, g_fail = 0;

namespace {

struct PoolFixture
{
    xash::memory::PoolHandle pool;
    sv::StringPool           strings;

    explicit PoolFixture( std::size_t arena_size )
    {
        pool = xash::memory::create_pool( "test_string_pool" );
        REQUIRE( static_cast<bool>( pool ));
        REQUIRE( strings.init( pool, /*max_edicts=*/0, arena_size ));
    }

    ~PoolFixture()
    {
        strings.shutdown();
        xash::memory::destroy_pool( pool );
    }
};

} // namespace

static void test_offset_zero_is_empty_string()
{
    PoolFixture f( 64 );
    CHECK_STREQ( f.strings.get_string( 0 ), "" );
}

static void test_alloc_roundtrip_and_dedup()
{
    PoolFixture f( 64 );

    const auto s1 = f.strings.alloc_string( "hello" );
    CHECK_EQ( s1, 1 ); // first string lands right after the empty slot
    CHECK_STREQ( f.strings.get_string( s1 ), "hello" );

    const auto s2 = f.strings.alloc_string( "hello" );
    CHECK_EQ( s2, s1 ); // dedup returns the existing offset
    CHECK_EQ( f.strings.stats().num_dups, 1u );

    const auto s3 = f.strings.alloc_string( "world" );
    CHECK( s3 != s1 );
    CHECK_STREQ( f.strings.get_string( s3 ), "world" );

    // -str64dup equivalent: duplicates get their own storage.
    f.strings.set_allow_dup( true );
    const auto s4 = f.strings.alloc_string( "hello" );
    CHECK( s4 != s1 );
    CHECK_STREQ( f.strings.get_string( s4 ), "hello" );
}

static void test_escape_processing()
{
    PoolFixture f( 64 );

    CHECK_EQ( sv::StringPool::process_string( nullptr, "abc" ), 4u );

    const auto sn = f.strings.alloc_string( "a\\nb" );
    CHECK_STREQ( f.strings.get_string( sn ), "a\nb" );

    const auto sr = f.strings.alloc_string( "a\\rb" );
    CHECK_STREQ( f.strings.get_string( sr ), "a\rb" );

    const auto st = f.strings.alloc_string( "a\\tb" );
    CHECK_STREQ( f.strings.get_string( st ), "a\tb" );

    // Unknown escape: both characters copy through untouched.
    const auto sx = f.strings.alloc_string( "a\\xb" );
    CHECK_STREQ( f.strings.get_string( sx ), "a\\xb" );
}

static void test_overflow_wraps_and_overwrites()
{
    PoolFixture f( 16 );

    const auto s1 = f.strings.alloc_string( "0123456789" ); // len 11 → fits
    CHECK_EQ( s1, 1 );
    CHECK_STREQ( f.strings.get_string( s1 ), "0123456789" );

    // 12 used + 9 + 1 > 16 → wrap to the arena start and overwrite
    // (legacy numoverflows quirk: the old string_t now reads new text).
    const auto s2 = f.strings.alloc_string( "abcdefgh" );
    CHECK_EQ( s2, 1 );
    CHECK_EQ( f.strings.stats().num_overflows, 1u );
    CHECK_STREQ( f.strings.get_string( s1 ), "abcdefgh" );
}

static void test_phase_switch_uses_static_half()
{
    PoolFixture f( 32 );

    // Boot state writes into the dynamic (first) half.
    const auto s1 = f.strings.alloc_string( "boot" );
    CHECK_EQ( s1, 1 );

    // Static phase: cursor moves to the second half.
    f.strings.set_dynamic( true );  // no cursor move (dynamic branch)
    f.strings.set_dynamic( false ); // static half selected
    const auto s2 = f.strings.alloc_string( "static" );
    CHECK_EQ( static_cast<std::size_t>( s2 ), f.strings.arena_size() + 1 );
    CHECK_STREQ( f.strings.get_string( s2 ), "static" );
}

static void test_make_string_in_range()
{
    PoolFixture f( 64 );

    const auto s1 = f.strings.alloc_string( "direct" );
    // A pointer inside the pool is trivially within INT range of base.
    const auto s2 = f.strings.make_string( f.strings.get_string( s1 ));
    CHECK_EQ( s2, s1 );
}

static void test_offset_int_range_check()
{
    // Synthetic pointers only — never dereferenced.
    const char *base = reinterpret_cast<const char *>(
        static_cast<std::uintptr_t>( 0x100000000ull ));

    CHECK( sv::StringPool::offset_in_int_range( base, base + 100 ));
    CHECK( sv::StringPool::offset_in_int_range( base, base - 100 ));
    CHECK( sv::StringPool::offset_in_int_range( base, base + 0x7FFFFFFF ));
    CHECK( !sv::StringPool::offset_in_int_range( base, base + 0x90000000ull ));
    CHECK( !sv::StringPool::offset_in_int_range(
        base + 0x90000000ull, base )); // negative side past INT_MIN
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_offset_zero_is_empty_string );
    RUN_TEST( test_alloc_roundtrip_and_dedup );
    RUN_TEST( test_escape_processing );
    RUN_TEST( test_overflow_wraps_and_overwrites );
    RUN_TEST( test_phase_switch_uses_static_half );
    RUN_TEST( test_make_string_in_range );
    RUN_TEST( test_offset_int_range_check );

    std::printf( "string_pool: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
