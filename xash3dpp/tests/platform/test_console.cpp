// xash3dpp — platform console I/O tests
// Covers: write (various inputs, no crash), read_line (non-blocking, no hang).
//
// These are smoke tests.  The side-effects (bytes reaching stdout/logcat) are
// not verified programmatically — the test runner captures them in its log.
//
// When stdin is a pipe (as in automated CI), read_line() must return {} without
// blocking.  Win32 uses PeekConsoleInput/PeekNamedPipe; POSIX uses select(0).

#include <xash3dpp/platform/console.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------

static void test_write_empty()
{
    // Empty view must not crash.
    xash::platform::console::write({});
    ++g_pass;
}

static void test_write_normal()
{
    xash::platform::console::write("test_console: write OK\n");
    ++g_pass;
}

static void test_write_no_newline()
{
    // No trailing newline — implementation must not require one.
    xash::platform::console::write("no-newline");
    ++g_pass;
}

static void test_write_long()
{
    // ~300 chars: exercises any static internal buffer (e.g. Android logcat path).
    xash::platform::console::write(
        "LONG:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n");
    ++g_pass;
}

// ---------------------------------------------------------------------------
// read_line
// ---------------------------------------------------------------------------

static void test_read_line_nonblocking()
{
    // stdin is typically a pipe in test runners — read_line() must not block.
    // We cannot assert the returned value; any string_view (including {}) is
    // acceptable as long as the call returns.
    auto line = xash::platform::console::read_line();
    (void)line;
    ++g_pass;
}

static void test_read_line_idempotent()
{
    // Multiple calls with no pending input must not crash.
    for (int i = 0; i < 5; ++i)
        (void)xash::platform::console::read_line();
    ++g_pass;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_write_empty();
    test_write_normal();
    test_write_no_newline();
    test_write_long();
    test_read_line_nonblocking();
    test_read_line_idempotent();

    std::printf("console: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
