// xash3dpp — S10.3: joystick/gamepad axis math + gyro + calibration state machine.

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
    xash::memory::PoolHandle handle = xash::memory::create_pool("test_input_joy");
    ~ScopedPool() { xash::memory::destroy_pool(handle); }
};
} // namespace

static void test_axis_deadzone_zeroes_small_values()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    // hw_axis 1 == JOY_AXIS_FWD under the default "sfpyrl" binding.
    source.push_event(JoyAxisEvent{ 1, 2000 }); // within default deadzone (4096)
    input->pump_events();

    CHECK_EQ(input->joy_axis_state(JoyAxis::Fwd).value, 0);
}

static void test_axis_beyond_deadzone_is_kept()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    source.push_event(JoyAxisEvent{ 1, 20000 });
    input->pump_events();

    CHECK_EQ(input->joy_axis_state(JoyAxis::Fwd).value, 20000);
}

static void test_trigger_threshold_synthesizes_key()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    CHECK(!input->is_down(Key::RTrigger));
    // hw_axis 4 == JOY_AXIS_RT; default threshold 16384.
    source.push_event(JoyAxisEvent{ 4, 20000 });
    input->pump_events();
    CHECK(input->is_down(Key::RTrigger));

    source.push_event(JoyAxisEvent{ 4, 1000 });
    input->pump_events();
    CHECK(!input->is_down(Key::RTrigger));
}

static void test_hat_synthesis_only_in_ui_mode()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    auto input = create_input(pool.handle, params);

    // Gameplay mode: side-axis motion never synthesizes arrow keys.
    source.push_event(JoyAxisEvent{ 0, 30000 }); // hw_axis 0 == JOY_AXIS_SIDE
    input->pump_events();
    CHECK(!input->is_down(Key::RightArrow));

    // Joy_AxisMotionEvent no-ops on a repeated identical value ("it is not
    // an update", in_joy.c:283-284) — reset to neutral before re-pushing so
    // the menu-mode push below is observed as a real transition.
    source.push_event(JoyAxisEvent{ 0, 0 });
    input->pump_events();

    input->set_key_dest(KeyDest::Menu);
    source.push_event(JoyAxisEvent{ 0, 30000 });
    input->pump_events();
    CHECK(input->is_down(Key::RightArrow));
}

static void test_lazy_axis_binding_reparse()
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

    // Rebind hw_axis 0 (normally SIDE) to feed FWD instead ("f" first char).
    cvars.cvar_set("joy_axis_binding", "fsypll");

    MoveCmd cmd;
    input->engine_append_move(0.016f, cmd, true); // consumes the FCVAR_CHANGED gate once

    source.push_event(JoyAxisEvent{ 0, 20000 });
    input->pump_events();

    CHECK_EQ(input->joy_axis_state(JoyAxis::Fwd).value, 20000);
    CHECK_EQ(input->joy_axis_state(JoyAxis::Side).value, 0);

    cvars.shutdown();
}

// D3: an axis event that fires BETWEEN a cvar change and the next
// finalize_move must not swallow the pending reparse. Joy_AxisMotionEvent
// never touches joy_axis_binding in legacy (in_joy.c:273-290) — only
// Joy_FinalizeMove may consume/clear FCVAR_CHANGED (in_joy.c:317-337).
static void test_axis_event_before_finalize_does_not_swallow_reparse()
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

    // Rebind hw_axis 0 (normally SIDE) to feed FWD instead ("f" first char).
    cvars.cvar_set("joy_axis_binding", "fsypll");

    // An axis event on an UNRELATED axis (hw_axis 3 == JOY_AXIS_YAW under the
    // still-default mapping) fires before finalize_move ever runs. Under the
    // bug, on_axis_event's tunables snapshot would consume-and-clear
    // FCVAR_CHANGED itself, silently swallowing the pending reparse.
    source.push_event(JoyAxisEvent{ 3, 5000 });
    input->pump_events();

    // finalize_move must still observe the change and reparse hw_axis 0 to FWD.
    MoveCmd cmd;
    input->engine_append_move(0.016f, cmd, true);

    source.push_event(JoyAxisEvent{ 0, 20000 });
    input->pump_events();

    CHECK_EQ(input->joy_axis_state(JoyAxis::Fwd).value, 20000);
    CHECK_EQ(input->joy_axis_state(JoyAxis::Side).value, 0);

    cvars.shutdown();
}

static void test_gyro_calibration_state_machine()
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

    CHECK(static_cast<int>(input->joy_calibration_state()) == static_cast<int>(GyroCalibrationState::NotCalibrated));

    input->set_clock_now(100.0);
    input->start_gyro_calibration();
    CHECK(static_cast<int>(input->joy_calibration_state()) == static_cast<int>(GyroCalibrationState::NotCalibrated));

    // Feed enough samples within the 5s window at ~10Hz (default data rate).
    for (int i = 0; i < 60; ++i) {
        input->set_clock_now(100.0 + static_cast<double>(i) * 0.05);
        source.push_event(JoyGyroEvent{ 0.01f, 0.02f, 0.03f });
        input->pump_events();
    }
    CHECK(static_cast<int>(input->joy_calibration_state()) == static_cast<int>(GyroCalibrationState::Calibrating));

    // Cross the 5s window boundary with one more sample to trigger finalize.
    input->set_clock_now(105.5);
    source.push_event(JoyGyroEvent{ 0.01f, 0.02f, 0.03f });
    input->pump_events();

    CHECK(static_cast<int>(input->joy_calibration_state()) == static_cast<int>(GyroCalibrationState::Calibrated));

    cvars.shutdown();
}

static void test_gyro_calibration_failure_only_on_first_run()
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

    input->set_clock_now(0.0);
    input->start_gyro_calibration();

    // Too few samples before the window expires -> FailedToCalibrate.
    source.push_event(JoyGyroEvent{ 0.0f, 0.0f, 0.0f });
    input->pump_events();

    input->set_clock_now(5.5);
    source.push_event(JoyGyroEvent{ 0.0f, 0.0f, 0.0f });
    input->pump_events();

    CHECK(static_cast<int>(input->joy_calibration_state()) == static_cast<int>(GyroCalibrationState::FailedToCalibrate));

    cvars.shutdown();
}

static void test_lock_input_devices_flips_read_only()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    auto *joy_enable = cvars.cvar_find("joy_enable");
    REQUIRE(joy_enable != nullptr);
    CHECK((joy_enable->abi.flags & xash::cmd_cvar::FCVAR_READ_ONLY) == 0);

    input->lock_input_devices(true);
    CHECK((joy_enable->abi.flags & xash::cmd_cvar::FCVAR_READ_ONLY) != 0);

    input->lock_input_devices(false);
    CHECK((joy_enable->abi.flags & xash::cmd_cvar::FCVAR_READ_ONLY) == 0);

    cvars.shutdown();
}

int main()
{
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    RUN_TEST(test_axis_deadzone_zeroes_small_values);
    RUN_TEST(test_axis_beyond_deadzone_is_kept);
    RUN_TEST(test_trigger_threshold_synthesizes_key);
    RUN_TEST(test_hat_synthesis_only_in_ui_mode);
    RUN_TEST(test_lazy_axis_binding_reparse);
    RUN_TEST(test_axis_event_before_finalize_does_not_swallow_reparse);
    RUN_TEST(test_gyro_calibration_state_machine);
    RUN_TEST(test_gyro_calibration_failure_only_on_first_run);
    RUN_TEST(test_lock_input_devices_flips_read_only);

    std::printf("test_joy_gyro: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
