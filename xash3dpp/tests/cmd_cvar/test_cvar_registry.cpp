// xash3dpp — cmd_cvar: cvar registry tests
// Covers: registration, find, set, unlink, auto-create, DLL registration

#include "test_stubs.hpp"

#include <cstdio>

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
    RUN_TEST( test_set_value );
    RUN_TEST( test_unlink );

    std::printf("cvar_registry: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
