// xash3dpp — atlas packer tests
// Covers: Atlas::Atlas, Atlas::clear, Atlas::alloc, Atlas::size, Atlas::max_height

#include <xash3dpp/utilities/atlas.hpp>
#include <cstdio>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static void test_constructor()
{
    // Normal size stored as-is.
    xash::utilities::Atlas a{ 64 };
    CHECK( a.size() == 64 );
    CHECK( a.max_height() == 0 );

    // Constructor clamps to ATLAS_MAX_SIZE (legacy: Atlas_Init did not clamp,
    // but the struct field width makes >1024 unsafe — clamping is the contract).
    xash::utilities::Atlas big{ 9999 };
    CHECK( big.size() == xash::utilities::ATLAS_MAX_SIZE );
}

static void test_alloc_single()
{
    // legacy: Atlas_AllocBlock in public/atlas.c — first alloc at (0,0).
    xash::utilities::Atlas a{ 64 };
    const auto b = a.alloc( 16, 16 );
    CHECK( b.has_value() );
    CHECK( b->x == 0 );
    CHECK( b->y == 0 );
    CHECK( a.max_height() == 16 );
}

static void test_alloc_adjacent()
{
    // Two equal-height blocks pack side-by-side on the first row.
    xash::utilities::Atlas a{ 64 };
    const auto b1 = a.alloc( 16, 16 );
    const auto b2 = a.alloc( 16, 16 );
    CHECK( b1.has_value() && b2.has_value() );
    CHECK( b2->x == 16 );
    CHECK( b2->y == 0 );
    CHECK( a.max_height() == 16 );
}

static void test_alloc_second_row()
{
    // Fill first row completely; the next block must start on the second row.
    xash::utilities::Atlas a{ 64 };
    for( int i = 0; i < 4; ++i )
        CHECK( a.alloc( 16, 8 ).has_value() );

    const auto b = a.alloc( 16, 8 );
    CHECK( b.has_value() );
    CHECK( b->y == 8 );  // placed on top of the first row
}

static void test_alloc_exact_fit()
{
    // A block exactly matching the atlas dimensions fits.
    xash::utilities::Atlas a{ 64 };
    const auto b = a.alloc( 64, 64 );
    CHECK( b.has_value() );
    CHECK( b->x == 0 );
    CHECK( b->y == 0 );
}

static void test_alloc_too_wide()
{
    // legacy: loop condition `i <= size - w` means w > size never finds a slot.
    xash::utilities::Atlas a{ 64 };
    CHECK( !a.alloc( 65, 1 ).has_value() );
}

static void test_alloc_too_tall()
{
    // legacy: Atlas_AllocBlock returns false when best + h > size.
    xash::utilities::Atlas a{ 64 };
    CHECK( !a.alloc( 1, 65 ).has_value() );
}

static void test_alloc_zero_dimensions()
{
    xash::utilities::Atlas a{ 64 };
    CHECK( !a.alloc( 0, 8 ).has_value() );
    CHECK( !a.alloc( 8, 0 ).has_value() );
}

static void test_alloc_full()
{
    // After filling the atlas entirely, any further alloc must fail.
    xash::utilities::Atlas a{ 64 };
    CHECK( a.alloc( 64, 64 ).has_value() );
    CHECK( !a.alloc( 1, 1 ).has_value() );
}

static void test_max_height()
{
    // legacy: atlas->max_height updated in Atlas_AllocBlock.
    xash::utilities::Atlas a{ 64 };
    a.alloc( 16, 8 );
    CHECK( a.max_height() == 8 );

    a.alloc( 16, 32 );
    CHECK( a.max_height() == 32 );

    // A shorter subsequent block must not reduce max_height.
    a.alloc( 16, 4 );
    CHECK( a.max_height() == 32 );
}

static void test_clear()
{
    xash::utilities::Atlas a{ 64 };
    a.alloc( 64, 64 );
    a.clear();
    CHECK( a.max_height() == 0 );

    // After clear the full surface must be available again.
    const auto b = a.alloc( 64, 64 );
    CHECK( b.has_value() );
    CHECK( b->x == 0 );
    CHECK( b->y == 0 );
}

static void test_best_fit_scan()
{
    // Verify the best-fit scan places a block in the shallowest available strip,
    // not just the first strip. Mirrors the strip-height comparison in Atlas_AllocBlock.
    //
    //   [0..32)  filled to y=16
    //   [32..64) filled to y=24
    //   alloc(32,8) must land at (0,16) — the shallower strip.
    xash::utilities::Atlas a{ 64 };
    const auto b1 = a.alloc( 32, 16 );
    const auto b2 = a.alloc( 32, 24 );
    CHECK( b1.has_value() && b2.has_value() );

    const auto b3 = a.alloc( 32, 8 );
    CHECK( b3.has_value() );
    CHECK( b3->x == 0 );
    CHECK( b3->y == 16 );
}

int main()
{
    test_constructor();
    test_alloc_single();
    test_alloc_adjacent();
    test_alloc_second_row();
    test_alloc_exact_fit();
    test_alloc_too_wide();
    test_alloc_too_tall();
    test_alloc_zero_dimensions();
    test_alloc_full();
    test_max_height();
    test_clear();
    test_best_fit_scan();

    std::printf( "%d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
