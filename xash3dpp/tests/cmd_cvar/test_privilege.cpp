// xash3dpp — cmd_cvar: privilege and trust-oracle tests
// Covers: stuffcmd queue privilege gating, FCMD_PRIVILEGED commands,
//         FCVAR_PRIVILEGED cvars, cl_filterstuffcmd bypass

#include "test_stubs.hpp"

#include <cstdio>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"
#include <xash3dpp/core/thread_role.hpp>

using namespace xash::cmd_cvar;
using namespace xash::cmd_cvar::test;

// ---------------------------------------------------------------------------
// Stuffed privileged command is blocked when oracle returns untrusted
// ---------------------------------------------------------------------------

static void test_stuffcmd_blocked_when_untrusted()
{
    UntrustedOracle oracle; // stuffcmd_is_trusted() == false
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    static int called = 0;
    ctx.cmd_add("privileged_cmd", [] { ++called; }, FCMD_PRIVILEGED);

    ctx.cbuf_stuff_text("privileged_cmd\n");
    ctx.cbuf_execute();

    CHECK( called == 0 );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// Stuffed privileged command executes when oracle returns trusted
// ---------------------------------------------------------------------------

static void test_stuffcmd_allowed_when_trusted()
{
    TrustedOracle oracle; // stuffcmd_is_trusted() == true
    NullPolicy    policy;
    auto ctx = make_test_context(oracle, policy);

    static int called = 0;
    ctx.cmd_add("privileged_cmd2", [] { ++called; }, FCMD_PRIVILEGED);

    ctx.cbuf_stuff_text("privileged_cmd2\n");
    ctx.cbuf_execute();

    CHECK( called == 1 );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// Non-privileged stuffed command always executes regardless of oracle
// ---------------------------------------------------------------------------

static void test_stuffcmd_unprivileged_always_runs()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    static int called = 0;
    ctx.cmd_add("safe_cmd", [] { ++called; }); // no FCMD_PRIVILEGED

    ctx.cbuf_stuff_text("safe_cmd\n");
    ctx.cbuf_execute();

    CHECK( called == 1 );

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

    RUN_TEST( test_stuffcmd_blocked_when_untrusted );
    RUN_TEST( test_stuffcmd_allowed_when_trusted );
    RUN_TEST( test_stuffcmd_unprivileged_always_runs );

    std::printf("privilege: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
