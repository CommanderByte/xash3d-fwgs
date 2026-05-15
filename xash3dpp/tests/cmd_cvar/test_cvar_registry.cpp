// xash3dpp — cmd_cvar: cvar registry tests
// Covers: registration, find, set, unlink, auto-create, DLL registration

#include "test_stubs.hpp"

#include <cstdio>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

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

    // TODO: uncomment when cvar_get_or_create is implemented:
    // Cvar *cv = ctx.cvar_get_or_create("test_cvar", "42", 0);
    // CHECK( cv != nullptr );
    // CHECK( cv->abi.name != nullptr );
    // CHECK( ctx.cvar_find("test_cvar") == cv );
    // CHECK( ctx.cvar_find("TEST_CVAR") == cv ); // case-insensitive

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

    // TODO: uncomment when cvar_get_or_create + cvar_set_direct are implemented:
    // Cvar *cv = ctx.cvar_get_or_create("test_setval", "0", 0);
    // const auto gen_before = cv->generation.load();
    // ctx.cvar_set("test_setval", "1");
    // CHECK( cv->generation.load() > gen_before );
    // CHECK( cv->abi.value == 1.0f );

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

    // TODO: register a cvar with FCVAR_EXTDLL, call cvar_unlink(FCVAR_EXTDLL),
    //       then verify cvar_find returns nullptr.

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_init_shutdown();
    test_find_unknown();
    test_create_and_find();
    test_set_value();
    test_unlink();

    std::printf("cvar_registry: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
