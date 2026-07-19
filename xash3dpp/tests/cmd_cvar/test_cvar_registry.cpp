// xash3dpp — cmd_cvar: cvar registry tests
// Covers: registration, find, set, unlink, auto-create, DLL registration

#include "test_stubs.hpp"

#include <xash3dpp/utilities/string.hpp>

#include <cstdio>
#include <string_view>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"
#include <xash3dpp/core/thread_role.hpp>

using namespace xash::cmd_cvar;
using namespace xash::cmd_cvar::test;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    CmdCvarContext  ctx;
    CHECK( ctx.init({ &oracle, &policy }) );
    ctx.shutdown();
    // Re-init must succeed (idempotent lifecycle).
    CHECK( ctx.init({ &oracle, &policy }) );
    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cvar_find returns nullptr for unknown names
// ---------------------------------------------------------------------------

static void test_find_unknown()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    CHECK( ctx.cvar_find("nonexistent_cvar_xyz") == nullptr );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cvar_get_or_create creates a new cvar and find returns it
// ---------------------------------------------------------------------------

static void test_create_and_find()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    Cvar *cv = ctx.cvar_get_or_create("test_cvar", "42", 0);
    CHECK( cv != nullptr );
    CHECK( cv->abi.name != nullptr );
    CHECK( ctx.cvar_find("test_cvar") == cv );
    CHECK( ctx.cvar_find("TEST_CVAR") == cv ); // case-insensitive

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// HB-1/M-5: lookups and creation are bounded — a non-NUL-terminated
// string_view slice of a larger buffer must behave identically to the
// terminated form and never read past view.size().
// ---------------------------------------------------------------------------

static void test_unterminated_view_lookup()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    // "hb1_cvarXXX" — the slice stops before the XXX garbage tail.
    const char raw[] = { 'h','b','1','_','c','v','a','r','X','X','X' };
    const std::string_view slice( raw, 8 ); // "hb1_cvar", no NUL at data()+8

    Cvar *cv = ctx.cvar_get_or_create( slice, "1", 0 );
    CHECK( cv != nullptr );
    // The stored name must be exactly the 8 sliced bytes, NUL-terminated.
    CHECK( cv->abi.name != nullptr );
    CHECK( xash::utilities::strcmp( cv->abi.name, "hb1_cvar" ) == 0 );
    // Same slice finds it; the terminated spelling finds the same cvar.
    CHECK( ctx.cvar_find( slice ) == cv );
    CHECK( ctx.cvar_find( "hb1_cvar" ) == cv );
    CHECK( ctx.cvar_find( "HB1_CVAR" ) == cv ); // case-insensitive

    // Command path: bounded add/exists/describe/remove.
    const char rawc[] = { 'h','b','1','_','c','m','d','Z','Z' };
    const std::string_view cslice( rawc, 7 ); // "hb1_cmd"
    ctx.cmd_add( cslice, nullptr, 0, "hb1 test" );
    CHECK( ctx.cmd_exists( cslice ) );
    CHECK( ctx.cmd_exists( "hb1_cmd" ) );
    ctx.cmd_remove( cslice );
    CHECK( !ctx.cmd_exists( "hb1_cmd" ) );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cvar_set updates value and bumps generation
// ---------------------------------------------------------------------------

static void test_set_value()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    Cvar *cv = ctx.cvar_get_or_create("test_setval", "0", 0);
    CHECK( cv != nullptr );
    const auto gen_before = cv->generation.load();
    ctx.cvar_set("test_setval", "1");
    CHECK( cv->generation.load() > gen_before );
    CHECK( cv->abi.value == 1.0f );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cvar_unlink removes DLL-owned cvars
// ---------------------------------------------------------------------------

static void test_unlink()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    // Simulate a DLL-registered cvar using a stack-allocated CvarAbi struct.
    // (In production, the DLL owns the struct; we just need the shape right.)
    static char cv_name[] = "test_extdll_cvar";
    static char cv_val[]  = "0";
    CvarAbi dll_cv {};
    dll_cv.name   = cv_name;
    dll_cv.string = cv_val;
    dll_cv.flags  = FCVAR_EXTDLL;
    dll_cv.value  = 0.0f;
    dll_cv.next   = nullptr;

    Cvar *registered = ctx.cvar_register_dll(&dll_cv);
    CHECK( registered != nullptr );
    CHECK( ctx.cvar_find("test_extdll_cvar") == registered );

    ctx.cvar_unlink(FCVAR_EXTDLL);
    CHECK( ctx.cvar_find("test_extdll_cvar") == nullptr );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // 6B: cmd_cvar mutators assert ThreadRole::Main; the test thread
    // must register the role (mirrors the server test harnesses).
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_shutdown );
    RUN_TEST( test_find_unknown );
    RUN_TEST( test_create_and_find );
    RUN_TEST( test_unterminated_view_lookup );
    RUN_TEST( test_set_value );
    RUN_TEST( test_unlink );

    std::printf("cvar_registry: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
