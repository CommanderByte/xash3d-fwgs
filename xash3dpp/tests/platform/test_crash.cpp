// xash3dpp — platform crash handler tests
// Covers: install_handler (idempotent, no crash), print_trace (no crash).
//
// We can only exercise the safe paths: we cannot verify that the installed
// handler fires correctly without actually crashing the process.  The tests
// confirm that the API surface is callable without side-effects.
//
// print_trace() writes a stack trace to stderr — the test runner will capture
// it in the test log.  This is intentional and useful for verifying that the
// symbol resolution path does not break.

#include <xash3dpp/platform/crash.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// install_handler
// ---------------------------------------------------------------------------

static void test_install_handler_no_crash()
{
    xash::platform::crash::install_handler();
    ++g_pass;
}

static void test_install_handler_idempotent()
{
    // A second (and third) call must be a no-op — must not re-register or crash.
    xash::platform::crash::install_handler();
    xash::platform::crash::install_handler();
    ++g_pass;
}

// ---------------------------------------------------------------------------
// print_trace
// ---------------------------------------------------------------------------

static void test_print_trace_no_crash()
{
    // Writes a stack trace to stderr.  The only requirement tested here is
    // that the call completes without crashing or blocking.
    xash::platform::crash::print_trace();
    ++g_pass;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_install_handler_no_crash );
    RUN_TEST( test_install_handler_idempotent );
    RUN_TEST( test_print_trace_no_crash );

    std::printf("crash: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
