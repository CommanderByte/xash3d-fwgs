// xash3dpp — GameDll loader handshake pins (Chunk 6 S6)
// Drives the real dynlib path against the fake game DLL doubles (MODULE
// libraries built beside this test).  Covers: export resolution order
// (GiveFnptrsToDll FIRST), GetEntityAPI2 version negotiation, the
// legacy-fallback-with-mutated-version quirk, optional NEW_DLL_FUNCTIONS
// (success / version-reject-zeroing), missing-export failures,
// persist-across-load early-return, unload/reload, LINK_ENTITY dispatch
// by raw classname, and the pfnGetHullBounds ×4 → HullBoundsTable flow
// (absent hull leaves its slot zeroed).

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/private/server/game_dll.hpp>

#include "fake_dll_state.hpp"

#include "../../test_helpers.hpp"

#include <cstring>

namespace sv  = xash::server;
namespace abi = xash::abi;

static int g_pass = 0, g_fail = 0;

static fake_dll::State *state_of( sv::GameDll &dll )
{
    auto fn = reinterpret_cast<fake_dll::StateFn>( dll.symbol( "fake_state" ));
    return fn ? fn() : nullptr;
}

// ---------------------------------------------------------------------------
// full variant
// ---------------------------------------------------------------------------

static void test_full_handshake()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    REQUIRE( dll.load( FAKE_DLL_FULL, &table, &globals ));
    CHECK( dll.loaded() );
    CHECK( dll.extended_api() );
    CHECK( dll.has_new_api() );

    fake_dll::State *st = state_of( dll );
    REQUIRE( st != nullptr );

    // Handshake order is ABI: fnptrs first, then new-API, then EntityAPI2.
    CHECK_EQ( st->seq_len, 3 );
    CHECK_EQ( st->seq[0], fake_dll::k_seq_give_fnptrs );
    CHECK_EQ( st->seq[1], fake_dll::k_seq_new_api );
    CHECK_EQ( st->seq[2], fake_dll::k_seq_api2 );

    CHECK_EQ( st->api2_version_in, abi::k_interface_version );
    CHECK_EQ( st->newapi_version_in, abi::k_new_dll_functions_version );
    CHECK( st->engfuncs == &table );
    CHECK( st->globals == &globals );

    // Cross-DLL calls through the negotiated tables.
    REQUIRE( dll.funcs().pfnGetGameDescription != nullptr );
    CHECK( std::strcmp( dll.funcs().pfnGetGameDescription(), "Fake HL" ) == 0 );

    dll.funcs().pfnGameInit();
    CHECK_EQ( st->game_init_calls, 1 );

    REQUIRE( dll.new_funcs().pfnGameShutdown != nullptr );
    dll.new_funcs().pfnGameShutdown();
    CHECK_EQ( st->game_shutdown_calls, 1 );

    // The DLL persists across map changes: load() early-returns true and
    // performs NO second handshake.
    CHECK( dll.load( FAKE_DLL_FULL, &table, &globals ));
    CHECK_EQ( st->seq_len, 3 );

    // st is dangling after this point.
    dll.unload();
    CHECK( !dll.loaded() );
    CHECK( dll.funcs().pfnGameInit == nullptr );

    // Reload runs a fresh handshake.
    REQUIRE( dll.load( FAKE_DLL_FULL, &table, &globals ));
    fake_dll::State *st2 = state_of( dll );
    REQUIRE( st2 != nullptr );
    CHECK_EQ( st2->seq_len, 3 );
    dll.unload();
}

static void test_link_entity_dispatch()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    REQUIRE( dll.load( FAKE_DLL_FULL, &table, &globals ));

    abi::LINK_ENTITY_FUNC link = dll.entity_link( "fake_item" );
    REQUIRE( link != nullptr );

    abi::entvars_t ev{};
    link( &ev );
    CHECK_EQ( ev.health, 123.0f );

    fake_dll::State *st = state_of( dll );
    REQUIRE( st != nullptr );
    CHECK_EQ( st->link_calls, 1 );

    // Unknown classname → no export → no spawn function.
    CHECK( dll.entity_link( "monster_bogus" ) == nullptr );

    dll.unload();
}

static void test_hull_bounds_flow()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    REQUIRE( dll.load( FAKE_DLL_FULL, &table, &globals ));

    const auto bounds = sv::query_hull_bounds( dll.funcs() );

    CHECK_EQ( bounds[0].mins.x, -16.0f );
    CHECK_EQ( bounds[0].mins.z, -36.0f );
    CHECK_EQ( bounds[0].maxs.z, 36.0f );

    // Hull 1: the fake returns 0 — the slot stays zeroed (legacy
    // host.player_mins parity), it is NOT defaulted.
    CHECK_EQ( bounds[1].mins.x, 0.0f );
    CHECK_EQ( bounds[1].maxs.z, 0.0f );

    CHECK_EQ( bounds[3].mins.y, -32.0f );
    CHECK_EQ( bounds[3].maxs.y, 32.0f );

    dll.unload();
}

// ---------------------------------------------------------------------------
// degraded variants
// ---------------------------------------------------------------------------

static void test_legacy_only_fallback()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    REQUIRE( dll.load( FAKE_DLL_LEGACY, &table, &globals ));
    CHECK( !dll.extended_api() );
    CHECK( !dll.has_new_api() );
    CHECK( dll.new_funcs().pfnGameShutdown == nullptr );

    fake_dll::State *st = state_of( dll );
    REQUIRE( st != nullptr );
    CHECK_EQ( st->seq_len, 2 );
    CHECK_EQ( st->seq[0], fake_dll::k_seq_give_fnptrs );
    CHECK_EQ( st->seq[1], fake_dll::k_seq_api );
    CHECK_EQ( st->api_version_in, abi::k_interface_version );

    REQUIRE( dll.funcs().pfnGetGameDescription != nullptr );
    CHECK( std::strcmp( dll.funcs().pfnGetGameDescription(), "Fake HL" ) == 0 );

    dll.unload();
}

static void test_api2_version_mismatch_quirk()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    // The DLL "was built against interface 138": GetEntityAPI2 writes 138
    // back and returns 0; the engine falls back to GetEntityAPI passing
    // `version` AS MUTATED (sv_game.c:5297-5313 quirk) — the fake must
    // see 138, not 140.
    REQUIRE( dll.load( FAKE_DLL_BADVER, &table, &globals ));
    CHECK( !dll.extended_api() );
    CHECK( !dll.has_new_api() ); // FAKE_NEWAPI_FAIL: version-reject zeroed

    fake_dll::State *st = state_of( dll );
    REQUIRE( st != nullptr );
    CHECK_EQ( st->api2_version_in, abi::k_interface_version );
    CHECK_EQ( st->api_version_in, 138 );
    CHECK_EQ( st->seq_len, 4 );
    CHECK_EQ( st->seq[3], fake_dll::k_seq_api );

    dll.unload();
}

static void test_missing_handshake_fails()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    CHECK( !dll.load( FAKE_DLL_NOHANDSHAKE, &table, &globals ));
    CHECK( !dll.loaded() );
    CHECK( dll.last_error() == sv::GameDll::LoadError::MissingGiveFnptrs );
}

static void test_missing_library_fails()
{
    sv::GameDll        dll;
    abi::enginefuncs_t table{};
    abi::globalvars_t  globals{};

    CHECK( !dll.load( "no/such/game_dll_xyz.dll", &table, &globals ));
    CHECK( !dll.loaded() );
    CHECK( dll.last_error() == sv::GameDll::LoadError::LibraryNotFound );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_full_handshake );
    RUN_TEST( test_link_entity_dispatch );
    RUN_TEST( test_hull_bounds_flow );
    RUN_TEST( test_legacy_only_fallback );
    RUN_TEST( test_api2_version_mismatch_quirk );
    RUN_TEST( test_missing_handshake_fails );
    RUN_TEST( test_missing_library_fails );

    std::printf( "game_dll: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
