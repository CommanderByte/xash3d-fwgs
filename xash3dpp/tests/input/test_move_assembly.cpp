// xash3dpp — S10.4: move assembly (IN_EngineAppendMove / IN_Commands /
// IN_JoyAppendMove +command synthesis / the pfnLookEvent bypass seam).

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
    xash::memory::PoolHandle handle = xash::memory::create_pool("test_input_move");
    ~ScopedPool() { xash::memory::destroy_pool(handle); }
};

struct Counters
{
    int plus_forward = 0, minus_forward = 0;
};
} // namespace

static void test_engine_append_move_scales_by_cl_forwardspeed()
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

    // Fwd axis raw -30000 -> forward = -(-30000)/32767 ~= 0.9155 (fw -= val/32767).
    source.push_event(JoyAxisEvent{ 1, -30000 });
    input->pump_events();

    MoveCmd cmd;
    input->engine_append_move(0.016f, cmd, true);

    CHECK(cmd.forwardmove > 300.0f); // ~0.9155 * 400 (cl_forwardspeed default)

    cvars.shutdown();
}

static void test_engine_append_move_noop_with_look_event()
{
    ScopedPool pool;
    MockEventSource source;
    InputInitParams params;
    params.event_source = &source;
    params.callbacks.look_event = [](void *, float, float) {};
    auto input = create_input(pool.handle, params);

    source.push_event(JoyAxisEvent{ 1, -30000 });
    input->pump_events();

    MoveCmd cmd;
    input->engine_append_move(0.016f, cmd, true);

    // pfnLookEvent bypass seam (Quirk/Dependencies): engine-side merge fully
    // no-ops when the client DLL implements pfnLookEvent.
    CHECK_EQ(cmd.forwardmove, 0.0f);
}

static void test_run_commands_drives_look_and_move_callbacks()
{
    ScopedPool pool;
    MockEventSource source;

    static float seen_yaw = -999.0f, seen_pitch = -999.0f;
    static bool  move_called = false;

    InputInitParams params;
    params.event_source = &source;
    params.callbacks.look_event = [](void *, float relyaw, float relpitch) { seen_yaw = relyaw; seen_pitch = relpitch; };
    params.callbacks.move_event = [](void *, float, float) { move_called = true; };
    auto input = create_input(pool.handle, params);

    source.push_event(JoyAxisEvent{ 2, -16384 }); // JOY_AXIS_PITCH: nonzero dpitch contribution
    input->pump_events();

    input->run_commands(0.016);

    CHECK(move_called);
    CHECK(seen_yaw != -999.0f || seen_pitch != -999.0f);
}

static void test_joy_append_move_synthesizes_plus_minus_forward()
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

    Counters counters;
    cvars.cmd_add("+forward", [](void *u) { ++static_cast<Counters *>(u)->plus_forward; }, &counters);
    cvars.cmd_add("-forward", [](void *u) { ++static_cast<Counters *>(u)->minus_forward; }, &counters);

    // Cross the +0.7 threshold forward. IN_JoyAppendMove dispatches the
    // synthesized +/-forward commands via Cmd_ExecuteString (immediate),
    // not the queued Cbuf_AddText path.
    source.push_event(JoyAxisEvent{ 1, -30000 });
    input->pump_events();
    MoveCmd cmd;
    input->engine_append_move(0.016f, cmd, true);
    CHECK_EQ(counters.plus_forward, 1);
    CHECK_EQ(counters.minus_forward, 0);

    // Drop back to neutral -> crosses back under threshold -> "-forward".
    // Legacy fires "-forward" from TWO independent, sequential branches when
    // forwardmove lands exactly on 0 with F still set: the T ("stopped")
    // safety-net branch (`else if (!(moveflags & T))` -> blanket "-back"/
    // "-forward") AND the F-threshold branch (`forwardmove < 0.7f && (moveflags
    // & F)` -> "-forward") both fire in the same call (input.c:485-527) — a
    // genuine double-dispatch quirk, not a bug in the port.
    source.push_event(JoyAxisEvent{ 1, 0 });
    input->pump_events();
    MoveCmd cmd2;
    input->engine_append_move(0.016f, cmd2, true);
    CHECK_EQ(counters.plus_forward, 1);
    CHECK_EQ(counters.minus_forward, 2);

    cvars.shutdown();
}

int main()
{
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    RUN_TEST(test_engine_append_move_scales_by_cl_forwardspeed);
    RUN_TEST(test_engine_append_move_noop_with_look_event);
    RUN_TEST(test_run_commands_drives_look_and_move_callbacks);
    RUN_TEST(test_joy_append_move_synthesizes_plus_minus_forward);

    std::printf("test_move_assembly: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
