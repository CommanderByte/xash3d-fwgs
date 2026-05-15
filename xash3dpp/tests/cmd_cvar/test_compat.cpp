// xash3dpp — cmd_cvar: compatibility policy tests
// Covers: cvar redirect (HL25), filterable exemption table, overridable commands

#include "test_stubs.hpp"
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>

#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

using namespace xash::cmd_cvar;
using namespace xash::cmd_cvar::test;

// ---------------------------------------------------------------------------
// NullPolicy returns no-quirk sentinels for everything
// ---------------------------------------------------------------------------

static void test_null_policy()
{
    NullPolicy p;
    CHECK( p.redirect_cvar_name("gl_widescreen_yfov") == nullptr );
    CHECK( p.is_filterable_exempt("slot1")             == false );
    CHECK( p.is_overridable_command("pause")           == false );
}

// ---------------------------------------------------------------------------
// Context with NullPolicy: cvar_find does not redirect
// ---------------------------------------------------------------------------

static void test_no_redirect_with_null_policy()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    // TODO: once registry is implemented, register "r_adjust_fov" and verify
    //       that cvar_find("gl_widescreen_yfov") returns nullptr (no redirect
    //       when NullPolicy is active).

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_null_policy();
    test_no_redirect_with_null_policy();

    std::printf("compat: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
