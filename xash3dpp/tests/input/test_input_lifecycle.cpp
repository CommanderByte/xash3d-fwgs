// xash3dpp — Input lifecycle + Key_Event routing smoke tests (S10.1).

#include <xash3dpp/input/input.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "mock_event_source.hpp"
#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::input;
using namespace xash::input::test;

namespace {

struct ScopedPool
{
    xash::memory::PoolHandle handle = xash::memory::create_pool("test_input");
    ~ScopedPool() { xash::memory::destroy_pool(handle); }
};

// D2: log_set_callback capture helper (mirrors tests/platform/test_thread.cpp).
struct LogCapture
{
    int         call_count = 0;
    std::string last_text;
    static LogCapture *instance;

    static void callback(xash::core::LogLevel, std::string_view, std::string_view text) noexcept
    {
        if (!instance) { return; }
        instance->last_text = std::string{ text };
        ++instance->call_count;
    }

    void install() { instance = this; xash::core::log_set_callback(&LogCapture::callback); }
    void uninstall() { xash::core::log_set_callback(nullptr); instance = nullptr; }
};
LogCapture *LogCapture::instance = nullptr;

} // namespace

static void test_create_and_destroy()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);
    CHECK(input != nullptr);
    CHECK_EQ(static_cast<int>(input->key_dest()), static_cast<int>(KeyDest::Game));
}

static void test_key_down_up_basic()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    CHECK(!input->is_down(Key::Space));
    input->key_event(Key::Space, true);
    CHECK(input->is_down(Key::Space));
    input->key_event(Key::Space, false);
    CHECK(!input->is_down(Key::Space));
}

static void test_stale_key_up_ignored()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    // A key-up with no prior down is a no-op (Quirk: step 3 stale guard).
    input->key_event(Key::Tab, false);
    CHECK(!input->is_down(Key::Tab));
}

static void test_escape_cannot_be_unbound()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    CHECK(!input->get_binding(Key::Escape).empty());
    bool ok = input->unbind(Key::Escape);
    CHECK(!ok);
    CHECK(!input->get_binding(Key::Escape).empty());
}

// D2: the unbound-key warning (Step 7, key_routing.cpp) must key off the
// NULL-vs-"" distinction (KeyRecord::present), not text emptiness.
static void test_unbound_warning_only_for_absent_keynames_row()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    // MWHEELUP (keynum 240, >= 200) has a keynames[] row whose default
    // binding is "" — present-but-empty. No warning should fire.
    LogCapture cap_present_empty;
    cap_present_empty.install();
    input->key_event(Key::MWheelUp, true);
    cap_present_empty.uninstall();
    CHECK_EQ(cap_present_empty.call_count, 0);

    // keynum 200 has NO keynames[] row at all (genuinely never bound) — the
    // warning must fire.
    LogCapture cap_absent;
    cap_absent.install();
    input->key_event(key_from_index(200), true);
    cap_absent.uninstall();
    CHECK_EQ(cap_absent.call_count, 1);
    CHECK(cap_absent.last_text.find("is unbound.") != std::string::npos);
}

static void test_pump_events_key()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    source.push_event(KeyEvent{ Key::Enter, true });
    input->pump_events();
    CHECK(input->is_down(Key::Enter));

    source.push_event(KeyEvent{ Key::Enter, false });
    input->pump_events();
    CHECK(!input->is_down(Key::Enter));
}

static void test_keynum_string_roundtrip()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    auto k = input->string_to_keynum("ESCAPE");
    CHECK(k.has_value());
    CHECK(*k == Key::Escape);
    std::string escape_name = input->keynum_to_string(Key::Escape); // keep the std::string alive across the .c_str() use below
    CHECK_STREQ(escape_name.c_str(), "ESCAPE");

    auto ak = input->string_to_keynum("a");
    CHECK(ak.has_value());
    CHECK_EQ(xash::input::key_index(*ak), static_cast<int>('a'));
}

int main()
{
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    RUN_TEST(test_create_and_destroy);
    RUN_TEST(test_key_down_up_basic);
    RUN_TEST(test_stale_key_up_ignored);
    RUN_TEST(test_escape_cannot_be_unbound);
    RUN_TEST(test_unbound_warning_only_for_absent_keynames_row);
    RUN_TEST(test_pump_events_key);
    RUN_TEST(test_keynum_string_roundtrip);

    std::printf("test_input_lifecycle: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
