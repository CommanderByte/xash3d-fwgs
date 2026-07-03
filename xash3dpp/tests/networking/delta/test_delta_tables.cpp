// xash3dpp — DeltaTables lifecycle and game-DLL hook tests
// Legacy reference: engine/common/net_encode.c Delta_Init/InitClient/Shutdown,
// Delta_AddEncoder, Delta_FindField, Delta_Set/UnsetField[ByIndex].

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>

#include "../../test_helpers.hpp"

#include <cstdint>

using namespace xash::networking;
using namespace xash::networking::delta;

static int g_pass = 0, g_fail = 0;

namespace
{

const char *k_script =
    "event_t gamedll Game_EventEncode\n"
    "{\n"
    "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 ),\n"
    "    DEFINE_DELTA( ducking, DT_INTEGER, 1, 1.0 ),\n"
    "    DEFINE_DELTA( iparam1, DT_INTEGER | DT_SIGNED, 18, 1.0 )\n"
    "}\n";

int  g_encoder_calls = 0;
void stub_encoder( DeltaField * /*fields*/,
                   const std::uint8_t * /*from*/, const std::uint8_t * /*to*/ )
{
    ++g_encoder_calls;
}

} // namespace

// -- lifecycle ---------------------------------------------------------------

static void test_lifecycle()
{
    DeltaTables tables;
    CHECK( !tables.is_initialized());
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 0 );

    REQUIRE( tables.init_from_script( k_script ));
    CHECK( tables.is_initialized());
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 3 );
    CHECK_EQ( tables.stats().tables_parsed.load(), 1u );

    // re-init (per map spawn) resets rather than accumulates
    REQUIRE( tables.init_from_script( k_script ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 3 );
    CHECK_EQ( tables.stats().tables_parsed.load(), 2u );

    tables.clear();
    CHECK( !tables.is_initialized());
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 0 );
    CHECK( tables.table_fields( DeltaStructId::Event ) == nullptr );

    // clear on a cleared instance is a no-op
    tables.clear();
    CHECK( !tables.is_initialized());
}

// -- init_client: only meaningful with populated tables -----------------------

static void test_init_client_empty_noop()
{
    DeltaTables tables;
    tables.init_client();
    CHECK( !tables.is_initialized()); // nothing received -> stays down
}

static void test_init_client_already_initialized_noop()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_script ));
    tables.init_client(); // local game: keep server tables untouched
    CHECK( tables.is_initialized());
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 3 );
}

// -- register_encoder ----------------------------------------------------------

static void test_register_encoder()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_script ));

    // unknown encoder name
    CHECK( !tables.register_encoder( "NoSuchEncoder", &stub_encoder ));

    // movevars fallback carries no custom encoder — funcName is empty, so
    // lookup by any name misses it; the "null" name of `none` sections is
    // rejected by the CUSTOM_NONE check.
    CHECK( !tables.register_encoder( "", &stub_encoder ));

    // correct name registers
    CHECK( tables.register_encoder( "Game_EventEncode", &stub_encoder ));
}

static void test_register_encoder_rejects_custom_none()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script(
        "event_t none\n{\n    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n}\n" ));

    // `none` sections get funcName "null" but CUSTOM_NONE — must reject.
    CHECK( !tables.register_encoder( "null", &stub_encoder ));
}

// -- field token helpers ---------------------------------------------------------

static void test_field_token_helpers()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script( k_script ));

    DeltaField *fields = tables.table_fields( DeltaStructId::Event );
    REQUIRE( fields != nullptr );

    CHECK_EQ( tables.find_field( fields, "entindex" ), 0 );
    CHECK_EQ( tables.find_field( fields, "iparam1" ), 2 );
    CHECK_EQ( tables.find_field( fields, "no_such" ), -1 );
    CHECK_EQ( tables.find_field( fields, "" ), -1 );
    CHECK_EQ( tables.find_field( nullptr, "entindex" ), -1 );

    // by name
    tables.unset_field( fields, "ducking" );
    CHECK( fields[1].inactive );
    tables.set_field( fields, "ducking" );
    CHECK( !fields[1].inactive );

    // by index (with bounds no-ops)
    tables.unset_field_by_index( fields, 2 );
    CHECK( fields[2].inactive );
    tables.set_field_by_index( fields, 2 );
    CHECK( !fields[2].inactive );
    tables.unset_field_by_index( fields, -1 );
    tables.unset_field_by_index( fields, 999 );
    CHECK( !fields[0].inactive && !fields[1].inactive && !fields[2].inactive );

    // foreign pointer is not a valid token — all helpers no-op / miss
    DeltaField foreign{};
    CHECK_EQ( tables.find_field( &foreign, "entindex" ), -1 );
    tables.unset_field( &foreign, "entindex" );
    CHECK( !fields[0].inactive );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_lifecycle );
    RUN_TEST( test_init_client_empty_noop );
    RUN_TEST( test_init_client_already_initialized_noop );
    RUN_TEST( test_register_encoder );
    RUN_TEST( test_register_encoder_rejects_custom_none );
    RUN_TEST( test_field_token_helpers );

    std::printf( "delta_tables: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
