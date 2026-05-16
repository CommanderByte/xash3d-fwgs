// xash3dpp — default protocol driver registry smoke test
// Verifies that default_protocol_driver_registry() exposes the GoldSrc
// driver for wire protocols 48 and 49, and returns nullptr for anything
// else.

#include <xash3dpp/private/networking/protocol_driver_default.hpp>

#include "../test_helpers.hpp"

#include <cstring>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_resolves_protocol_48()
{
    auto &reg = default_protocol_driver_registry();
    auto *drv = reg.resolve( 48 );
    REQUIRE( drv != nullptr );
    CHECK( std::strcmp( drv->name(), "goldsrc" ) == 0 );
    CHECK( drv->split_format() == SplitFormat::GoldSrc );
    CHECK( drv->delta_tables() == DeltaTableSet::GoldSrc );
}

static void test_resolves_protocol_49()
{
    auto &reg = default_protocol_driver_registry();
    auto *drv = reg.resolve( 49 );
    REQUIRE( drv != nullptr );
    CHECK( std::strcmp( drv->name(), "goldsrc" ) == 0 );
}

static void test_unknown_protocol_returns_nullptr()
{
    auto &reg = default_protocol_driver_registry();
    CHECK( reg.resolve( 0 )    == nullptr );
    CHECK( reg.resolve( 47 )   == nullptr );
    CHECK( reg.resolve( 50 )   == nullptr );
    CHECK( reg.resolve( 9999 ) == nullptr );
}

static void test_singleton_identity_stable()
{
    auto *a = &default_protocol_driver_registry();
    auto *b = &default_protocol_driver_registry();
    CHECK( a == b );

    // Same driver instance across resolves of the same protocol.
    auto *d1 = a->resolve( 48 );
    auto *d2 = a->resolve( 48 );
    CHECK( d1 == d2 );
}

int main()
{
    test_resolves_protocol_48();
    test_resolves_protocol_49();
    test_unknown_protocol_returns_nullptr();
    test_singleton_identity_stable();

    std::printf( "test_protocol_driver_registry: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
