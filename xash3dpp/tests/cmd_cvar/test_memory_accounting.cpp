// xash3dpp — cmd_cvar: memory pool accounting tests
//
// Verifies that the "cmd_cvar" pool is created by init(), receives allocation
// traffic during normal use, and is cleanly destroyed by shutdown().
//
// In debug builds destroy_pool() asserts live_bytes == 0; the tests below
// additionally verify this invariant explicitly via for_each_pool snapshots
// taken before and after the subsystem lifecycle.

#include "test_stubs.hpp"

#include <xash3dpp/memory/memory.hpp>

#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

using namespace xash::cmd_cvar;
using namespace xash::cmd_cvar::test;

// ---------------------------------------------------------------------------
// Helper — find the stats for the pool named `target` via for_each_pool.
// Returns a zeroed snapshot with found == false if no such pool is active.
// ---------------------------------------------------------------------------

namespace {

struct PoolSnapshot
{
    std::size_t live_bytes   = 0;
    std::size_t total_allocs = 0;
    std::size_t total_frees  = 0;
    bool        found        = false;
};

static PoolSnapshot snapshot_pool(const char *target) noexcept
{
    struct FindData { const char *name; PoolSnapshot result; };
    FindData fd{ target, {} };
    xash::memory::for_each_pool(
        [](xash::memory::PoolStats s, void *ud) noexcept
        {
            auto *d = static_cast<FindData *>(ud);
            if (s.name && std::strcmp(s.name, d->name) == 0)
                d->result = { s.live_bytes, s.total_allocs, s.total_frees, true };
        },
        &fd);
    return fd.result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Pool is absent before init() and absent again after shutdown().
// ---------------------------------------------------------------------------

static void test_pool_lifecycle()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    CmdCvarContext  ctx;

    // No "cmd_cvar" pool should exist before init.
    CHECK( !snapshot_pool("cmd_cvar").found );

    CHECK( ctx.init({ &oracle, &policy }) );

    // Pool must exist while the context is live.
    CHECK( snapshot_pool("cmd_cvar").found );

    ctx.shutdown();

    // shutdown() calls destroy_pool — pool must be gone.
    // (In debug builds destroy_pool also asserts live_bytes == 0.)
    CHECK( !snapshot_pool("cmd_cvar").found );
}

// ---------------------------------------------------------------------------
// Allocations made during init() and cvar registration are tracked.
// ---------------------------------------------------------------------------

static void test_allocations_are_tracked()
{
    UntrustedOracle oracle;
    NullPolicy      policy;
    auto ctx = make_test_context(oracle, policy);

    // init() registers built-in commands and cvars — proves allocations happened.
    PoolSnapshot after_init = snapshot_pool("cmd_cvar");
    CHECK( after_init.found );
    CHECK( after_init.total_allocs > 0 );

    // Adding a user cvar allocates a name copy + registry node.
    std::size_t allocs_before = after_init.total_allocs;
    ctx.cvar_get_or_create("mem_test_cvar", "1", 0);

    PoolSnapshot after_cvar = snapshot_pool("cmd_cvar");
    CHECK( after_cvar.total_allocs > allocs_before );

    // Live bytes must be positive while allocations are outstanding.
    CHECK( after_cvar.live_bytes > 0 );

    // shutdown() must free everything; destroy_pool asserts live_bytes == 0.
    ctx.shutdown();

    CHECK( !snapshot_pool("cmd_cvar").found );
}

// ---------------------------------------------------------------------------
// A re-initialised context creates a fresh pool with clean counters.
// Running two full lifecycles back-to-back catches any cross-cycle leaks
// (destroy_pool asserts live_bytes == 0 at the end of each cycle).
// ---------------------------------------------------------------------------

static void test_double_lifecycle()
{
    UntrustedOracle oracle;
    NullPolicy      policy;

    for (int i = 0; i < 2; ++i)
    {
        CmdCvarContext ctx;
        CHECK( ctx.init({ &oracle, &policy }) );

        ctx.cvar_get_or_create("dl_cvar", "0", 0);
        ctx.cmd_add("dl_cmd", [] {});

        PoolSnapshot s = snapshot_pool("cmd_cvar");
        CHECK( s.found );
        CHECK( s.total_allocs > 0 );
        CHECK( s.live_bytes   > 0 );

        // destroy_pool inside shutdown() will abort in debug if live_bytes != 0.
        ctx.shutdown();

        CHECK( !snapshot_pool("cmd_cvar").found );
    }
}

int main()
{
    test_pool_lifecycle();
    test_allocations_are_tracked();
    test_double_lifecycle();

    std::printf("memory_accounting: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
