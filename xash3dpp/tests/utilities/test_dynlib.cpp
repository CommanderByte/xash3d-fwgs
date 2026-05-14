// xash3dpp — dynlib export table tests
// Covers: clear_exports, validate_exports

#include <xash3dpp/utilities/dynlib.hpp>
#include <cstdio>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::puts("FAIL: " #expr " (" __FILE__ ")"); } } while(0)

static void test_clear_exports()
{
    // After clear_exports all slots must be null.
    void *a = reinterpret_cast<void *>( 1 );
    void *b = reinterpret_cast<void *>( 2 );
    void *c = reinterpret_cast<void *>( 3 );

    xash::utilities::ExportEntry table[] = {
        { "fn_a", &a },
        { "fn_b", &b },
        { "fn_c", &c },
    };

    xash::utilities::clear_exports( table );

    CHECK( a == nullptr );
    CHECK( b == nullptr );
    CHECK( c == nullptr );
}

static void test_validate_exports()
{
    // All non-null → valid.
    void *f1 = reinterpret_cast<void *>( 1 );
    void *f2 = reinterpret_cast<void *>( 2 );

    xash::utilities::ExportEntry full[] = {
        { "fn1", &f1 },
        { "fn2", &f2 },
    };

    CHECK( xash::utilities::validate_exports( full ) == true );

    // One null slot → invalid.
    void *g1 = reinterpret_cast<void *>( 1 );
    void *g2 = nullptr;

    xash::utilities::ExportEntry partial[] = {
        { "fn1", &g1 },
        { "fn2", &g2 },
    };

    CHECK( xash::utilities::validate_exports( partial ) == false );

    // Empty table → all slots filled (vacuous truth) → valid.
    CHECK( xash::utilities::validate_exports( {} ) == true );
}

int main()
{
    test_clear_exports();
    test_validate_exports();

    std::printf( "dynlib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
