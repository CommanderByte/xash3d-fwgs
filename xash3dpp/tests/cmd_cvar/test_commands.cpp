// xash3dpp — cmd_cvar: CommandCtxFn (context command) tests
// Covers: campaign B5 decision — the non-breaking context overload of
// cmd_add() (docs/audits/2026-07-chunk8-10-campaign.md §Decision record).

#include "test_stubs.hpp"

#include <cstdio>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"
#include <xash3dpp/core/thread_role.hpp>

using namespace xash::cmd_cvar;
using namespace xash::cmd_cvar::test;

// ---------------------------------------------------------------------------
// A ctx command receives the exact 'user' pointer it was registered with,
// and can mutate state through it.
// ---------------------------------------------------------------------------

namespace {
struct Counter {
    int  value  = 0;
    void *seen  = nullptr; // records the pointer the callback was invoked with
};
} // namespace

static void test_ctx_command_receives_user_pointer()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    Counter counter;
    ctx.cmd_add("ctx_bump", [](void *user) noexcept {
        Counter *c = static_cast<Counter *>(user);
        c->seen = user;
        ++c->value;
    }, &counter);

    ctx.cmd_execute_string("ctx_bump");

    CHECK( counter.seen == static_cast<void *>(&counter) );
    CHECK( counter.value == 1 );

    // A second dispatch mutates the same struct again through the same
    // borrowed pointer.
    ctx.cmd_execute_string("ctx_bump");
    CHECK( counter.value == 2 );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// A legacy capture-less command registered alongside a ctx command still
// dispatches normally (non-breaking addition).
// ---------------------------------------------------------------------------

static void test_legacy_command_still_dispatches_alongside_ctx()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    Counter counter;
    ctx.cmd_add("ctx_cmd", [](void *user) noexcept {
        ++static_cast<Counter *>(user)->value;
    }, &counter);

    static int legacy_called = 0;
    ctx.cmd_add("legacy_cmd", [] { ++legacy_called; });

    ctx.cmd_execute_string("ctx_cmd");
    ctx.cmd_execute_string("legacy_cmd");

    CHECK( counter.value == 1 );
    CHECK( legacy_called == 1 );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cmd_exists / cmd_describe / cmd_remove all work on a ctx command.
// ---------------------------------------------------------------------------

static void test_ctx_command_exists_describe_remove()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    Counter counter;
    ctx.cmd_add("ctx_desc", [](void *user) noexcept {
        ++static_cast<Counter *>(user)->value;
    }, &counter, 0, "a ctx command");

    CHECK( ctx.cmd_exists("ctx_desc") );

    CommandDesc desc = ctx.cmd_describe("ctx_desc");
    CHECK( desc.name != nullptr );
    CHECK_STREQ( desc.name, "ctx_desc" );
    CHECK( desc.desc != nullptr );
    CHECK_STREQ( desc.desc, "a ctx command" );

    ctx.cmd_remove("ctx_desc");
    CHECK( !ctx.cmd_exists("ctx_desc") );

    // Removed command must no longer dispatch.
    ctx.cmd_execute_string("ctx_desc");
    CHECK( counter.value == 0 );

    ctx.shutdown();
}

// ---------------------------------------------------------------------------
// cmd_unlink(flags_mask) removes a flagged ctx command.
// ---------------------------------------------------------------------------

static void test_cmd_unlink_removes_flagged_ctx_command()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    Counter counter;
    ctx.cmd_add("ctx_extdll", [](void *user) noexcept {
        ++static_cast<Counter *>(user)->value;
    }, &counter, FCMD_EXTDLL);

    CHECK( ctx.cmd_exists("ctx_extdll") );

    ctx.cmd_unlink(FCMD_EXTDLL);

    CHECK( !ctx.cmd_exists("ctx_extdll") );

    // No longer dispatches once unlinked.
    ctx.cmd_execute_string("ctx_extdll");
    CHECK( counter.value == 0 );

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

    RUN_TEST( test_ctx_command_receives_user_pointer );
    RUN_TEST( test_legacy_command_still_dispatches_alongside_ctx );
    RUN_TEST( test_ctx_command_exists_describe_remove );
    RUN_TEST( test_cmd_unlink_removes_flagged_ctx_command );

    std::printf("commands: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
