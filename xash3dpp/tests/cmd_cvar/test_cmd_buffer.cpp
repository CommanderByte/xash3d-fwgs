// xash3dpp — cmd_cvar: command buffer tests
// Covers: cbuf_add_text, cbuf_insert_text, cbuf_execute, cmd_wait

#include "test_stubs.hpp"

#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

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

    // TODO: uncomment when cbuf_execute is implemented:
    // CHECK( called == 1 );

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

    // TODO: register two commands and verify execution order

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

    // TODO: post "wait\n" then a command; verify command not executed until
    //       second cbuf_execute call.

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_add_and_execute();
    test_insert_ordering();
    test_cmd_wait();

    std::printf("cmd_buffer: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
