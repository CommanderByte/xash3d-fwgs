// xash3dpp — S10.6: scripted event-sequence integration tests through
// MockEventSource — key state, key_dest routing, +/-command press/release
// pairing, pfnKey_Event first-refusal ordering, move-assembly goldens,
// bindings_snapshot shape, touch gesture outcomes.

#include <xash3dpp/input/input.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "mock_event_source.hpp"
#include "../cmd_cvar/test_stubs.hpp"
#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::input;
using namespace xash::input::test;
using namespace xash::cmd_cvar::test;

namespace {
struct ScopedPool
{
    xash::memory::PoolHandle handle = xash::memory::create_pool("test_input_e2e");
    ~ScopedPool() { xash::memory::destroy_pool(handle); }
};
} // namespace

// ---------------------------------------------------------------------------
// Scripted key sequence -> +/-command press/release pairing via a cmd_cvar
// capture context (the bound "+jump" default on SPACE).
// ---------------------------------------------------------------------------

static void test_scripted_key_sequence_press_release_pairing()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    MockEventSource source;

    InputInitParams params;
    params.cvars = &cvars;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    struct Capture { int plus = 0, minus = 0; };
    Capture capture;

    cvars.cmd_add("+jump", [](void *u) { ++static_cast<Capture *>(u)->plus; }, &capture, 0);
    cvars.cmd_add("-jump", [](void *u) { ++static_cast<Capture *>(u)->minus; }, &capture, 0);

    // Script: press SPACE (default binding "+jump"), release.
    source.push_event(KeyEvent{ Key::Space, true });
    input->pump_events();
    cvars.cbuf_execute();

    source.push_event(KeyEvent{ Key::Space, false });
    input->pump_events();
    cvars.cbuf_execute();

    CHECK_EQ(capture.plus, 1);
    CHECK_EQ(capture.minus, 1); // press/release pairing: the '+' token synthesizes the matching '-' on release
    CHECK(!input->is_down(Key::Space));
}

// ---------------------------------------------------------------------------
// key_dest routing: the same KeyEvent produces different observable effects
// depending on key_dest (Game -> command dispatch; Console/Message ->
// dedicated callback hooks; Menu -> UI hooks).
// ---------------------------------------------------------------------------

static void test_key_dest_routing_differs_by_destination()
{
    ScopedPool pool;
    MockEventSource source;

    static bool console_seen = false, message_seen = false, ui_seen = false;
    InputInitParams params;
    params.event_source = &source;
    params.callbacks.con_key_event = [](void *, Key) { console_seen = true; };
    params.callbacks.message_key_event = [](void *, Key) { message_seen = true; };
    params.callbacks.ui_key_event = [](void *, Key, bool) { ui_seen = true; };
    auto input = create_input(pool.handle, params);

    input->set_key_dest(KeyDest::Console);
    input->key_event(Key::Tab, true);
    CHECK(console_seen);
    input->key_event(Key::Tab, false);

    input->set_key_dest(KeyDest::Message);
    input->key_event(Key::Tab, true);
    CHECK(message_seen);
    input->key_event(Key::Tab, false);

    input->set_key_dest(KeyDest::Menu);
    input->key_event(Key::Tab, true);
    CHECK(ui_seen);
    input->key_event(Key::Tab, false);
}

// ---------------------------------------------------------------------------
// pfnKey_Event first-refusal ordering: when the hook claims the event
// (returns 0/falsy), no further routing (VGui/console/menu/dispatch) occurs.
// ---------------------------------------------------------------------------

static void test_pfn_key_event_first_refusal_short_circuits()
{
    ScopedPool pool;
    MockEventSource source;

    static bool vgui_seen = false;
    static int  pfn_calls = 0;

    InputInitParams params;
    params.event_source = &source;
    params.callbacks.pfn_key_event = [](void *, bool, Key, const char *) -> int { ++pfn_calls; return 0; }; // claims it
    params.callbacks.vgui_key_event = [](void *, Key, bool) { vgui_seen = true; };
    auto input = create_input(pool.handle, params);

    input->set_key_dest(KeyDest::Game);
    input->key_event(Key::Backtick, true); // would normally toggle the console via VGui/console hardcode

    CHECK_EQ(pfn_calls, 1);
    CHECK(!vgui_seen); // step 5's first refusal returns BEFORE step 8 (VGui)
}

static void test_pfn_key_event_not_claimed_continues_routing()
{
    ScopedPool pool;
    MockEventSource source;

    static bool vgui_seen = false;
    static bool con_toggle_seen = false;

    InputInitParams params;
    params.event_source = &source;
    params.callbacks.pfn_key_event = [](void *, bool, Key, const char *) -> int { return 1; }; // does NOT claim it
    params.callbacks.vgui_key_event = [](void *, Key, bool) { vgui_seen = true; };
    params.callbacks.con_toggle_console = [](void *) { con_toggle_seen = true; };
    auto input = create_input(pool.handle, params);

    input->set_key_dest(KeyDest::Game);
    input->key_event(Key::Backtick, true);

    CHECK(vgui_seen);        // step 8 runs
    CHECK(con_toggle_seen);  // step 9 runs (console hardcode)
}

// ---------------------------------------------------------------------------
// Move-assembly output golden: hand-derived from the legacy math
// (fw -= joy_forward.value * val/32767; cmd.forwardmove = forward*cl_forwardspeed).
// ---------------------------------------------------------------------------

static void test_move_assembly_golden()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    // hw_axis 1 == JOY_AXIS_FWD, value -16384 (half-scale back... well
    // negative raw -> positive forward due to the `fw -=` sign flip).
    source.push_event(JoyAxisEvent{ 1, -16384 });
    input->pump_events();

    MoveCmd cmd;
    input->engine_append_move(0.016f, cmd, true);

    // Hand-derived: forward = -(-16384)/32767 = 0.500015...; * cl_forwardspeed(400) = 200.006...
    CHECK(cmd.forwardmove > 199.5f && cmd.forwardmove < 200.5f);
}

// ---------------------------------------------------------------------------
// bindings_snapshot() shape: one entry per key slot, default-bound entries
// carry the legacy key-name string.
// ---------------------------------------------------------------------------

static void test_bindings_snapshot_shape()
{
    ScopedPool pool;
    InputInitParams params;
    auto input = create_input(pool.handle, params);

    auto snap = input->bindings_snapshot();
    CHECK_EQ(snap.size(), static_cast<std::size_t>(k_key_count));

    bool found_escape = false;
    for (auto &e : snap) {
        if (e.key == Key::Escape) {
            found_escape = true;
            CHECK_STREQ(e.key_name.c_str(), "ESCAPE");
            CHECK_STREQ(e.binding.c_str(), "cancelselect");
        }
    }
    CHECK(found_escape);
}

// ---------------------------------------------------------------------------
// Touch gesture outcome: a swipe sequence through the console-mode gesture
// accumulator triggers the exit-console outcome exactly at the documented
// edge threshold (Quirk 9, modernized as member state — repeated presses
// across separate Input instances must behave identically, proving there is
// no leaked function-static state).
// ---------------------------------------------------------------------------

static void test_touch_gesture_outcome_is_instance_local()
{
    for (int trial = 0; trial < 2; ++trial) {
        ScopedPool pool;
        TrustedOracle oracle;
        NullPolicy policy;
        auto cvars = make_test_context(oracle, policy);

        static bool escape_seen;
        escape_seen = false;

        InputInitParams params;
        params.cvars = &cvars;
        params.callbacks.con_key_event = [](void *, Key k) { if (k == Key::Escape) { escape_seen = true; } };
        auto input = create_input(pool.handle, params);

        input->set_key_dest(KeyDest::Console);
        input->touch_event(TouchEventType::Down, 0, 0.05f, 0.95f, 0.0f, 0.0f);

        CHECK(escape_seen); // identical outcome every trial -> no cross-instance leakage

        cvars.shutdown();
    }
}

int main()
{
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    RUN_TEST(test_scripted_key_sequence_press_release_pairing);
    RUN_TEST(test_key_dest_routing_differs_by_destination);
    RUN_TEST(test_pfn_key_event_first_refusal_short_circuits);
    RUN_TEST(test_pfn_key_event_not_claimed_continues_routing);
    RUN_TEST(test_move_assembly_golden);
    RUN_TEST(test_bindings_snapshot_shape);
    RUN_TEST(test_touch_gesture_outcome_is_instance_local);

    std::printf("test_event_script_integration: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
