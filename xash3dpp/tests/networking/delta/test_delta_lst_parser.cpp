// xash3dpp — delta.lst parser tests
// Legacy reference: engine/common/net_encode.c Delta_InitFields/ParseTable/
// ParseField grammar, including the movevars_t fallback quirk.
//
// The Filesystem-backed init() is a thin load_file wrapper over
// init_from_script(); the VFS path is covered by the filesystem suite, so
// these tests exercise the script parse boundary directly.

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>

#include "../../test_helpers.hpp"

#include <cstring>

using namespace xash::networking;
using namespace xash::networking::delta;

static int g_pass = 0, g_fail = 0;

namespace
{

// Finds a field by name on a table's token array; nullptr when absent.
const DeltaField *field_by_name( DeltaTables &tables, DeltaStructId id,
                                 const char *name )
{
    const DeltaField *fields = tables.table_fields( id );
    if( !fields )
        return nullptr;

    const int count = tables.table_field_count( id );
    for( int i = 0; i < count; ++i )
    {
        if( std::strcmp( fields[ i ].name, name ) == 0 )
            return &fields[ i ];
    }
    return nullptr;
}

} // namespace

// -- good script: flags, bits, multipliers, DEFINE_DELTA_POST, gamedll ------

static void test_good_script()
{
    static const char *script =
        "event_t gamedll Game_EventEncode\n"
        "{\n"
        "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 ),\n"
        "    DEFINE_DELTA( origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0 ),\n"
        "    DEFINE_DELTA_POST( fparam1, DT_SIGNED | DT_FLOAT, 20, 100.0, 0.1 )\n"
        "}\n";

    DeltaTables tables;
    REQUIRE( tables.init_from_script( script ));
    CHECK( tables.is_initialized());
    CHECK( tables.table_initialized( DeltaStructId::Event ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 3 );

    const DeltaField *entindex = field_by_name( tables, DeltaStructId::Event, "entindex" );
    REQUIRE( entindex != nullptr );
    CHECK_EQ( entindex->flags, k_dt_integer );
    CHECK_EQ( entindex->bits, 11 );
    CHECK( entindex->multiplier == 1.0f );
    CHECK( entindex->post_multiplier == 1.0f );
    CHECK_EQ( entindex->offset, 4 ); // event_args_t::entindex

    const DeltaField *origin0 = field_by_name( tables, DeltaStructId::Event, "origin[0]" );
    REQUIRE( origin0 != nullptr );
    CHECK_EQ( origin0->flags, k_dt_signed | k_dt_float );
    CHECK_EQ( origin0->bits, 16 );
    CHECK( origin0->multiplier == 8.0f );

    const DeltaField *fparam1 = field_by_name( tables, DeltaStructId::Event, "fparam1" );
    REQUIRE( fparam1 != nullptr );
    CHECK( fparam1->multiplier == 100.0f );
    CHECK( fparam1->post_multiplier == 0.1f );
}

// -- structural failures fail the whole parse -------------------------------

static void test_unknown_struct_fails()
{
    DeltaTables tables;
    CHECK( !tables.init_from_script( "bogus_t none\n{\n}\n" ));
    CHECK( !tables.is_initialized());
}

static void test_missing_brace_fails()
{
    DeltaTables tables;
    CHECK( !tables.init_from_script(
        "event_t none\nDEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n" ));
    CHECK( !tables.is_initialized());
}

// -- per-field errors skip the field, keep the table --------------------------

static void test_malformed_field_skipped()
{
    static const char *script =
        "event_t none\n"
        "{\n"
        "    DEFINE_DELTA( no_such_field, DT_INTEGER, 11, 1.0 ),\n"
        "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
        "}\n";

    DeltaTables tables;
    REQUIRE( tables.init_from_script( script ));
    CHECK( tables.table_initialized( DeltaStructId::Event ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 1 );
    CHECK( field_by_name( tables, DeltaStructId::Event, "entindex" ) != nullptr );
}

// -- unknown flag names are ignored (legacy behaviour) ------------------------

static void test_unknown_flag_ignored()
{
    static const char *script =
        "event_t none\n"
        "{\n"
        "    DEFINE_DELTA( entindex, DT_INTEGER | DT_BOGUS, 11, 1.0 )\n"
        "}\n";

    DeltaTables tables;
    REQUIRE( tables.init_from_script( script ));

    const DeltaField *entindex = field_by_name( tables, DeltaStructId::Event, "entindex" );
    REQUIRE( entindex != nullptr );
    CHECK_EQ( entindex->flags, k_dt_integer );
}

// -- movevars fallback: applied only when the script lacks the section --------

static void test_movevars_fallback_applied()
{
    DeltaTables tables;
    REQUIRE( tables.init_from_script(
        "event_t none\n{\n    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n}\n" ));

    CHECK( tables.table_initialized( DeltaStructId::Movevars ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Movevars ), 27 ); // 31 - 4

    // zmax quirk: unsigned 24-bit float (3D-skybox fix)
    const DeltaField *zmax = field_by_name( tables, DeltaStructId::Movevars, "zmax" );
    REQUIRE( zmax != nullptr );
    CHECK_EQ( zmax->flags, k_dt_float );
    CHECK_EQ( zmax->bits, 24 );
    CHECK( zmax->multiplier == 1.0f );

    const DeltaField *gravity = field_by_name( tables, DeltaStructId::Movevars, "gravity" );
    REQUIRE( gravity != nullptr );
    CHECK_EQ( gravity->flags, k_dt_float | k_dt_signed );
    CHECK_EQ( gravity->bits, 16 );
    CHECK( gravity->multiplier == 8.0f );
}

static void test_movevars_fallback_not_applied_when_specified()
{
    static const char *script =
        "movevars_t none\n"
        "{\n"
        "    DEFINE_DELTA( gravity, DT_FLOAT | DT_SIGNED, 12, 4.0 )\n"
        "}\n";

    DeltaTables tables;
    REQUIRE( tables.init_from_script( script ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Movevars ), 1 );

    const DeltaField *gravity = field_by_name( tables, DeltaStructId::Movevars, "gravity" );
    REQUIRE( gravity != nullptr );
    CHECK_EQ( gravity->bits, 12 );
    CHECK( gravity->multiplier == 4.0f );
}

// -- optional trailing comma after ')' both ways -------------------------------

static void test_trailing_comma_optional()
{
    static const char *script =
        "event_t none\n"
        "{\n"
        "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n" // no comma
        "    DEFINE_DELTA( ducking, DT_INTEGER, 1, 1.0 ),\n"  // comma
        "    DEFINE_DELTA( iparam1, DT_INTEGER | DT_SIGNED, 18, 1.0 )\n"
        "}\n";

    DeltaTables tables;
    REQUIRE( tables.init_from_script( script ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 3 );
}

// -- multiple sections in one script -------------------------------------------

static void test_multiple_sections()
{
    static const char *script =
        "event_t none\n"
        "{\n"
        "    DEFINE_DELTA( entindex, DT_INTEGER, 11, 1.0 )\n"
        "}\n"
        "usercmd_t none\n"
        "{\n"
        "    DEFINE_DELTA( msec, DT_BYTE, 8, 1.0 ),\n"
        "    DEFINE_DELTA( buttons, DT_SHORT, 16, 1.0 )\n"
        "}\n";

    DeltaTables tables;
    REQUIRE( tables.init_from_script( script ));
    CHECK_EQ( tables.table_field_count( DeltaStructId::Event ), 1 );
    CHECK_EQ( tables.table_field_count( DeltaStructId::Usercmd ), 2 );
    CHECK( tables.table_initialized( DeltaStructId::Usercmd ));
    CHECK( !tables.table_initialized( DeltaStructId::ClientData ));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_good_script );
    RUN_TEST( test_unknown_struct_fails );
    RUN_TEST( test_missing_brace_fails );
    RUN_TEST( test_malformed_field_skipped );
    RUN_TEST( test_unknown_flag_ignored );
    RUN_TEST( test_movevars_fallback_applied );
    RUN_TEST( test_movevars_fallback_not_applied_when_specified );
    RUN_TEST( test_trailing_comma_optional );
    RUN_TEST( test_multiple_sections );

    std::printf( "delta_lst_parser: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
