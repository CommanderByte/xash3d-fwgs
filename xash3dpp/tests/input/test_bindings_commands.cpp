// xash3dpp — S10.2: bindings + commands + cvar census.

#include <xash3dpp/input/input.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../cmd_cvar/test_stubs.hpp"
#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::input;
using namespace xash::cmd_cvar::test;

namespace {

struct ScopedPool
{
    xash::memory::PoolHandle handle = xash::memory::create_pool("test_input_bindings");
    ~ScopedPool() { xash::memory::destroy_pool(handle); }
};

// D1: log_set_callback capture helper, mirroring the sibling pattern in
// tests/platform/test_thread.cpp's test_realtime_logs_and_runs.
struct LogCapture
{
    int         call_count = 0;
    std::string last_tag;
    std::string last_text;
    static LogCapture *instance;

    static void callback(xash::core::LogLevel, std::string_view tag, std::string_view text) noexcept
    {
        if (!instance) { return; }
        instance->last_tag = std::string{ tag };
        instance->last_text = std::string{ text };
        ++instance->call_count;
    }

    void install() { instance = this; xash::core::log_set_callback(&LogCapture::callback); }
    void uninstall() { xash::core::log_set_callback(nullptr); instance = nullptr; }
};
LogCapture *LogCapture::instance = nullptr;

} // namespace

static void test_bind_unbind_via_commands()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("bind \"a\" \"+forward\"");
    std::string a_binding(input->get_binding(key_from_index('a'))); // keep alive across .c_str() below
    CHECK_STREQ(a_binding.c_str(), "+forward");

    cvars.cmd_execute_string("unbind \"a\"");
    CHECK(input->get_binding(key_from_index('a')).empty());

    cvars.shutdown();
}

static void test_unbind_refuses_escape()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    CHECK(!input->get_binding(Key::Escape).empty());
    cvars.cmd_execute_string("unbind \"ESCAPE\"");
    CHECK(!input->get_binding(Key::Escape).empty()); // Quirk 2: refused

    cvars.shutdown();
}

// D1: bind/unbind console messages, restored via the core logf surface
// (commands.cpp). Pins the exact legacy strings (in_keys.c:341-441).
static void test_bind_unbind_console_messages()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);
    (void)input;

    // unbind usage (argc != 2).
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("unbind");
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_tag == "input");
        CHECK(cap.last_text == "Usage: unbind <key> : remove commands from a key");
    }

    // bind usage (argc < 2).
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("bind");
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_text == "Usage: bind <key> [command] : attach a command to a key");
    }

    // "isn't a valid key" — unbind.
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("unbind \"NOTAREALKEY\"");
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_text == "\"NOTAREALKEY\" isn't a valid key");
    }

    // "isn't a valid key" — bind.
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("bind \"NOTAREALKEY\" \"+jump\"");
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_text == "\"NOTAREALKEY\" isn't a valid key");
    }

    // ESC refusal (Quirk 2, message form).
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("unbind \"ESCAPE\"");
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_text == "Can't unbind ESCAPE key");
        CHECK(!input->get_binding(Key::Escape).empty()); // still bound — the refusal actually held
    }

    // bind query, bound key.
    cvars.cmd_execute_string("bind \"a\" \"+forward\"");
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("bind \"a\"");
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_text == "\"a\" = \"+forward\"");
    }

    // bind query, a genuinely-never-bound key (present == false, D2) — "is
    // not bound", not a printed empty string.
    {
        LogCapture cap;
        cap.install();
        cvars.cmd_execute_string("bind \"0xC8\""); // 200 decimal: no keynames[] row
        cap.uninstall();
        CHECK_EQ(cap.call_count, 1);
        CHECK(cap.last_text == "\"0xC8\" is not bound");
    }

    cvars.shutdown();
}

// "makehelp" (in_keys.c:580 -> Key_EnumCmds_f) is registered unrestricted
// (Cmd_AddCommand, not Cmd_AddRestrictedCommand) with a chunk12-tagged stub
// body — registration parity is what's being pinned here, not real output.
static void test_makehelp_registered_unrestricted()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);
    (void)input;

    CHECK(cvars.cmd_exists("makehelp"));
    CHECK((cvars.cmd_describe("makehelp").flags & xash::cmd_cvar::FCMD_PRIVILEGED) == 0);

    LogCapture cap;
    cap.install();
    cvars.cmd_execute_string("makehelp");
    cap.uninstall();
    CHECK_EQ(cap.call_count, 1);
    CHECK(cap.last_text == "makehelp: not available until chunk 12");

    cvars.shutdown();
}

static void test_unbindall_leaves_only_two_defaults()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("unbindall");

    auto snap = input->bindings_snapshot();
    int bound_count = 0;
    for (auto &e : snap) {
        if (!e.binding.empty()) { ++bound_count; }
    }
    CHECK_EQ(bound_count, 2); // Quirk 3: ESC + START_BUTTON only

    std::string esc_binding(input->get_binding(Key::Escape));
    CHECK_STREQ(esc_binding.c_str(), "cancelselect");
    std::string start_binding(input->get_binding(Key::StartButton));
    CHECK_STREQ(start_binding.c_str(), "cancelselect");

    cvars.shutdown();
}

static void test_resetkeys_restores_full_table()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("unbindall");
    cvars.cmd_execute_string("resetkeys");

    std::string space_binding(input->get_binding(Key::Space));
    CHECK_STREQ(space_binding.c_str(), "+jump"); // full keynames[] replay, not just the 2-key unbindall subset

    cvars.shutdown();
}

static void test_prefix_match_lookup()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    // "+forward" is bound to UPARROW by default; "forward" (no leading '+')
    // should prefix-match against it once the '+' is stripped (Quirk 14).
    auto found = input->lookup_binding("forward");
    CHECK(found.has_value());
}

static void test_write_bindings_quoting()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("unbindall");
    // Bind a value containing a literal double-quote directly through the
    // C++ API — the cmd_cvar tokenizer itself has no backslash-escape
    // support (by design, its own doc comment), so a literal embedded quote
    // cannot survive a round trip through cmd_execute_string's command-line
    // parser; this test targets write_bindings_text()'s OWN escaping of
    // whatever binding string is already stored (Key_WriteBindings' job),
    // independent of how that string was set.
    input->bind(key_from_index('a'), "say \"hi\"");

    std::string text = input->write_bindings_text();
    CHECK(text.rfind("unbindall\n", 0) == 0); // header first
    // "a" is printable ASCII, so Key_KeynumToString returns the raw
    // (lowercase) character itself, not an uppercased key NAME.
    CHECK(text.find("bind \"a\" \"say \\\"hi\\\"\"") != std::string::npos);

    cvars.shutdown();
}

static void test_bindlist_diverges_from_write_bindings()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("unbindall");
    cvars.cmd_execute_string("bind \"a\" \"say hi\"");

    std::string bindlist = input->bindlist_text();
    std::string writebindings = input->write_bindings_text();

    CHECK(bindlist.find("unbindall") == std::string::npos); // Quirk 15: no header
    CHECK(writebindings.find("unbindall") != std::string::npos);

    cvars.shutdown();
}

static void test_cvar_census_registered()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);
    (void)input;

    static const char *const kExpected[] = {
        "key_rotate", "m_pitch", "m_yaw", "m_ignore", "joy_enable", "joy_axis_binding",
        "joy_have_gyro", "joy_calibrated", "gyro_enable", "gyro_available",
        "touch_enable", "osk_enable",
    };
    for (const char *name : kExpected) {
        CHECK(cvars.cvar_find(name) != nullptr);
    }

    // FCVAR_READ_ONLY per S10.3's "the cvar set with READ_ONLY flags".
    auto *have_gyro = cvars.cvar_find("joy_have_gyro");
    REQUIRE(have_gyro != nullptr);
    CHECK((have_gyro->abi.flags & xash::cmd_cvar::FCVAR_READ_ONLY) != 0);

    cvars.shutdown();
}

static void test_touch_command_census_and_privilege_split()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);
    (void)input;

    CHECK(cvars.cmd_exists("touch_addbutton"));
    CHECK(cvars.cmd_exists("touch_enableedit"));

    auto unpriv = cvars.cmd_describe("touch_addbutton");
    CHECK((unpriv.flags & xash::cmd_cvar::FCMD_PRIVILEGED) == 0);

    auto priv = cvars.cmd_describe("touch_enableedit");
    CHECK((priv.flags & xash::cmd_cvar::FCMD_PRIVILEGED) != 0);

    cvars.shutdown();
}

static void test_input_stats_track_key_events()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    input->key_event(Key::Tab, true);
    input->key_event(Key::Tab, false);

    CHECK(input->stats().key_events_routed.load() >= 2);
}

int main()
{
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    RUN_TEST(test_bind_unbind_via_commands);
    RUN_TEST(test_bind_unbind_console_messages);
    RUN_TEST(test_makehelp_registered_unrestricted);
    RUN_TEST(test_unbind_refuses_escape);
    RUN_TEST(test_unbindall_leaves_only_two_defaults);
    RUN_TEST(test_resetkeys_restores_full_table);
    RUN_TEST(test_prefix_match_lookup);
    RUN_TEST(test_write_bindings_quoting);
    RUN_TEST(test_bindlist_diverges_from_write_bindings);
    RUN_TEST(test_cvar_census_registered);
    RUN_TEST(test_touch_command_census_and_privilege_split);
    RUN_TEST(test_input_stats_track_key_events);

    std::printf("test_bindings_commands: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
