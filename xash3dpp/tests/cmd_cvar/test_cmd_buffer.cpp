// xash3dpp — cmd_cvar: command buffer tests
// Covers: cbuf_add_text, cbuf_insert_text, cbuf_execute, cmd_wait

#include "test_stubs.hpp"

#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"
#include <xash3dpp/core/thread_role.hpp>

using namespace xash::cmd_cvar;
using namespace xash::cmd_cvar::test;

// ---------------------------------------------------------------------------
// Commands posted via cbuf_add_text are executed by cbuf_execute
// ---------------------------------------------------------------------------

static void test_add_and_execute()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    static int called = 0;
    ctx.cmd_add("test_cmd_exec", [] { ++called; });

    ctx.cbuf_add_text("test_cmd_exec\n");
    ctx.cbuf_execute();

    CHECK( called == 1 );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cbuf_insert_text runs before cbuf_add_text
// ---------------------------------------------------------------------------

static void test_insert_ordering()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    static int order[2];
    static int order_idx = 0;
    ctx.cmd_add("cmd_a", [] { order[order_idx++] = 0; });
    ctx.cmd_add("cmd_b", [] { order[order_idx++] = 1; });

    ctx.cbuf_add_text("cmd_b\n");
    ctx.cbuf_insert_text("cmd_a\n");  // cmd_a should execute first
    ctx.cbuf_execute();

    CHECK( order[0] == 0 );  // cmd_a ran first
    CHECK( order[1] == 1 );  // then cmd_b

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cmd_wait skips one cbuf_execute call
// ---------------------------------------------------------------------------

static void test_cmd_wait()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    static int called2 = 0;
    ctx.cmd_add("cmd_after_wait", [] { ++called2; });

    ctx.cbuf_add_text("wait\ncmd_after_wait\n");
    ctx.cbuf_execute();  // drains "wait" — wait decrements, cmd_after_wait stays queued
    CHECK( called2 == 0 );

    ctx.cbuf_execute();  // now cmd_after_wait runs
    CHECK( called2 == 1 );

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

    RUN_TEST( test_add_and_execute );
    RUN_TEST( test_insert_ordering );
    RUN_TEST( test_cmd_wait );

    std::printf("cmd_buffer: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
