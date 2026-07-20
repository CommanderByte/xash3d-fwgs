// xash3dpp — S10.5: touch button/grid state machines + touch.cfg format +
// IN_TouchEvent routing + gesture accumulator + OSK first-refusal intercept.

#include <xash3dpp/input/input.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstring>

#include "../cmd_cvar/test_stubs.hpp"
#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::input;
using namespace xash::cmd_cvar::test;

namespace {
struct ScopedPool
{
    xash::memory::PoolHandle handle = xash::memory::create_pool("test_input_touch");
    ~ScopedPool() { xash::memory::destroy_pool(handle); }
};
} // namespace

static void test_touch_addbutton_and_press_dispatches_command()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("touch_addbutton \"jump\" \"tex\" \"+jump\" 0.4 0.4 0.6 0.6 255 255 255 255 0");

    auto buttons = input->touch_buttons();
    bool found = false;
    for (auto &b : buttons) { if (b.name == "jump") { found = true; } }
    CHECK(found);

    static int jump_count = 0;
    cvars.cmd_add("+jump", [](void *) { ++jump_count; }, nullptr);

    input->touch_set_client_only(false);
    // Must be in-game with touch enabled (off by default) for the internal
    // fallback to run — Step 3's gate (in_touch.c:2189-2190).
    cvars.cvar_set("touch_enable", "1");
    input->set_key_dest(KeyDest::Game);
    input->touch_event(TouchEventType::Down, 0, 0.5f, 0.5f, 0.0f, 0.0f);
    cvars.cbuf_execute();

    CHECK(jump_count == 1);

    cvars.shutdown();
}

static void test_touch_removebutton()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("touch_addbutton \"btn\" \"tex\" \"cmd\" 0.1 0.1 0.2 0.2 255 255 255 255 0");
    CHECK_EQ(input->touch_buttons().size(), static_cast<std::size_t>(1));

    cvars.cmd_execute_string("touch_removebutton \"btn\"");
    CHECK_EQ(input->touch_buttons().size(), static_cast<std::size_t>(0));

    cvars.shutdown();
}

static void test_touch_command_census_privilege_split()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);
    (void)input;

    static const char *const kUnprivileged[] = {
        "touch_addbutton", "touch_removebutton", "touch_settexture", "touch_setcolor",
        "touch_setcommand", "touch_setflags", "touch_show", "touch_hide", "touch_fade",
    };
    static const char *const kPrivileged[] = {
        "touch_enableedit", "touch_disableedit", "touch_list", "touch_removeall",
        "touch_loaddefaults", "touch_roundall", "touch_exportconfig", "touch_set_stroke",
        "touch_setclientonly", "touch_reloadconfig", "touch_writeconfig", "touch_deleteprofile",
        "touch_generate_code", "touch_toggleselection", "touch_aspectratio",
    };
    int unpriv_count = 0, priv_count = 0;
    for (auto *n : kUnprivileged) {
        CHECK(cvars.cmd_exists(n));
        CHECK((cvars.cmd_describe(n).flags & xash::cmd_cvar::FCMD_PRIVILEGED) == 0);
        ++unpriv_count;
    }
    for (auto *n : kPrivileged) {
        CHECK(cvars.cmd_exists(n));
        CHECK((cvars.cmd_describe(n).flags & xash::cmd_cvar::FCMD_PRIVILEGED) != 0);
        ++priv_count;
    }
    CHECK_EQ(unpriv_count, 9);
    CHECK_EQ(priv_count, 15);

    cvars.shutdown();
}

static void test_touch_edit_state_machine()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("touch_enableedit");
    cvars.cmd_execute_string("touch_disableedit");
    // No crash / state settles back to none — covered indirectly by not
    // asserting/crashing; edit_state() isn't part of the public P-4 surface
    // (touch_buttons() is), so this test exercises the command path only.

    cvars.shutdown();
}

static void test_osk_first_refusal_intercept()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cvar_set("osk_enable", "1");
    input->osk_enable_text_input(true, true);

    // With OSK enabled, the ENTER/A-button "arm" and subsequent navigation
    // are entirely consumed by OSK before reaching normal Key_Event routing.
    bool consumed = input->osk_key_event(Key::Enter, true);
    CHECK(consumed);

    auto state = input->osk_state();
    CHECK(state.enabled);

    cvars.shutdown();
}

// D4: Touch_WantVisibleCursor's 3-term gate — (touch_enable && touch_emulate)
// || clientonly || touch_in_menu (in_touch.c:2256-2259). Each disjunct is
// independently sufficient; the AND pair requires BOTH terms.
static void test_want_visible_cursor_three_term_gate()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    // Baseline: everything off -> false.
    cvars.cvar_set("touch_enable", "0");
    cvars.cvar_set("_touch_emulate", "0");
    cvars.cvar_set("touch_in_menu", "0");
    CHECK(!input->touch_want_visible_cursor());

    // touch_enable alone: not sufficient.
    cvars.cvar_set("touch_enable", "1");
    CHECK(!input->touch_want_visible_cursor());

    // touch_emulate alone (touch_enable back off): not sufficient — this is
    // exactly the D4 regression (the pre-fix code treated touch_emulate
    // alone as sufficient, ignoring touch_enable).
    cvars.cvar_set("touch_enable", "0");
    cvars.cvar_set("_touch_emulate", "1");
    CHECK(!input->touch_want_visible_cursor());

    // Both together: sufficient.
    cvars.cvar_set("touch_enable", "1");
    cvars.cvar_set("_touch_emulate", "1");
    CHECK(input->touch_want_visible_cursor());

    // Clear the AND pair; touch_in_menu alone must still be sufficient (the
    // other D4 regression — pre-fix code never read this cvar at all).
    cvars.cvar_set("touch_enable", "0");
    cvars.cvar_set("_touch_emulate", "0");
    cvars.cvar_set("touch_in_menu", "1");
    CHECK(input->touch_want_visible_cursor());
    cvars.cvar_set("touch_in_menu", "0");
    CHECK(!input->touch_want_visible_cursor());

    // clientonly alone (model state, independent of every cvar above).
    input->touch_set_client_only(true);
    CHECK(input->touch_want_visible_cursor());
    input->touch_set_client_only(false);
    CHECK(!input->touch_want_visible_cursor());

    cvars.shutdown();
}

// D6: every non-key_game touch event must reset the move/resize/look/wheel
// finger trackers FIRST (in_touch.c:2105), before anything else in that
// branch runs — including a finger that was never released via an Up event
// (e.g. a menu opened mid-drag). Observed indirectly: a stale look_finger_
// would otherwise keep attributing a later in-game motion event to yaw/pitch.
static void test_menu_dest_touch_resets_stale_look_finger()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    InputInitParams params;
    params.cvars = &cvars;
    auto input = create_input(pool.handle, params);

    cvars.cmd_execute_string("touch_addbutton \"look\" \"tex\" \"_look\" 0.0 0.0 1.0 1.0 255 255 255 255 0");
    cvars.cvar_set("touch_enable", "1");
    input->set_key_dest(KeyDest::Game);

    // Press the look button with finger 0 -> look_finger_ latches to 0.
    input->touch_event(TouchEventType::Down, 0, 0.5f, 0.5f, 0.0f, 0.0f);

    // A menu opens without an intervening Up event for finger 0 (e.g. the
    // pause key fired mid-drag). A stray touch event arrives while
    // key_dest != Game: this must reset the finger trackers FIRST.
    input->set_key_dest(KeyDest::Menu);
    input->touch_event(TouchEventType::Motion, 0, 0.5f, 0.5f, 0.05f, 0.05f);

    // Back in game: a motion event for finger 0 must NOT be attributed to
    // the (should-be-cleared) look_finger_.
    input->set_key_dest(KeyDest::Game);
    input->touch_event(TouchEventType::Motion, 0, 0.5f, 0.5f, 0.1f, 0.1f);

    float forward = 0.0f, side = 0.0f, pitch = 0.0f, yaw = 0.0f;
    input->touch_get_move(forward, side, pitch, yaw);
    CHECK_EQ(pitch, 0.0f);
    CHECK_EQ(yaw, 0.0f);

    cvars.shutdown();
}

// D5: dump_config_text() golden — Touch_DumpConfig's exact emission order
// (in_touch.c:263-303), including the ported stroke state and the
// previously-missing highlight/move_indicator lines. Captured via the
// fs_write_file callback seam (touch_writeconfig's real I/O path).
static void test_touch_dump_config_matches_legacy_order()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);

    static std::string captured_path, captured_text;
    captured_path.clear();
    captured_text.clear();

    InputInitParams params;
    params.cvars = &cvars;
    params.callbacks.fs_write_file = [](void *, const char *path, const char *text) -> bool {
        captured_path = path;
        captured_text = text;
        return true;
    };
    auto input = create_input(pool.handle, params);
    (void)input;

    cvars.cmd_execute_string("touch_addbutton \"jump\" \"touch_default/jump\" \"+jump\" 0.1 0.1 0.2 0.2 255 255 255 255 0");
    cvars.cmd_execute_string("touch_set_stroke 2 10 20 30 40");
    cvars.cmd_execute_string("touch_writeconfig");

    REQUIRE(!captured_text.empty());
    CHECK(captured_path == "touch.cfg");

    // 4-line generated-by header, first.
    CHECK(captured_text.rfind("//=======================================================================\n", 0) == 0);
    CHECK(captured_text.find("Generated by") != std::string::npos);
    CHECK(captured_text.find("touchscreen config") != std::string::npos);

    // Full cvar emission order (D5) — each marker present, in this relative order.
    static const char *const kOrderedMarkers[] = {
        "touch_config_file",
        "touch_pitch",
        "touch_yaw",
        "touch_forwardzone",
        "touch_sidezone",
        "touch_nonlinear_look",
        "touch_pow_factor",
        "touch_pow_mult",
        "touch_exp_mult",
        "touch_grid_count",
        "touch_grid_enable",
        "touch_set_stroke",
        "touch_highlight_r",
        "touch_highlight_g",
        "touch_highlight_b",
        "touch_highlight_a",
        "touch_dpad_radius",
        "touch_joy_radius",
        "touch_precise_amount",
        "touch_move_indicator",
        "touch_setclientonly 0",
        "touch_removeall",
        "touch_aspectratio",
        "touch_addbutton \"jump\"",
    };
    std::size_t cursor = 0;
    for (auto *marker : kOrderedMarkers) {
        std::size_t pos = captured_text.find(marker, cursor);
        CHECK(pos != std::string::npos);
        if (pos != std::string::npos) { cursor = pos + std::strlen(marker); }
    }

    // The ported global-stroke state (Touch_SetStroke) round-trips exactly.
    CHECK(captured_text.find("touch_set_stroke 2 10 20 30 40") != std::string::npos);

    cvars.shutdown();
}

static void test_gesture_console_swipe_exits_console()
{
    ScopedPool pool;
    TrustedOracle oracle;
    NullPolicy policy;
    auto cvars = make_test_context(oracle, policy);
    InputInitParams params;
    params.cvars = &cvars;

    static bool escape_seen = false;
    params.callbacks.con_key_event = [](void *, Key k) { if (k == Key::Escape) { escape_seen = true; } };
    auto input = create_input(pool.handle, params);

    input->set_key_dest(KeyDest::Console);

    // Edge-swipe-to-exit-console: down near the bottom-left corner.
    input->touch_event(TouchEventType::Down, 0, 0.05f, 0.95f, 0.0f, 0.0f);

    CHECK(escape_seen);

    cvars.shutdown();
}

int main()
{
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    RUN_TEST(test_touch_addbutton_and_press_dispatches_command);
    RUN_TEST(test_touch_removebutton);
    RUN_TEST(test_touch_command_census_privilege_split);
    RUN_TEST(test_touch_edit_state_machine);
    RUN_TEST(test_osk_first_refusal_intercept);
    RUN_TEST(test_want_visible_cursor_three_term_gate);
    RUN_TEST(test_menu_dest_touch_resets_stale_look_finger);
    RUN_TEST(test_touch_dump_config_matches_legacy_order);
    RUN_TEST(test_gesture_console_swipe_exits_console);

    std::printf("test_touch_osk: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
