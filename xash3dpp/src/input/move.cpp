// xash3dpp — move assembly: IN_EngineAppendMove / IN_Commands / IN_MouseMove
// / IN_CollectInput / IN_JoyAppendMove / the mouse activation state machine.
// Legacy reference: engine/client/input/input.c:174-654.

#include <xash3dpp/private/input/input_impl.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <algorithm>

namespace xash::input {

namespace {
// F/B/L/R/T/S bitflags (input.c:472-477) — IN_JoyAppendMove's persistent
// edge state, now Input::Impl::moveflags (S10.4: member state, not
// function-static).
constexpr std::uint32_t k_move_f = 1u << 0;
constexpr std::uint32_t k_move_b = 1u << 1;
constexpr std::uint32_t k_move_l = 1u << 2;
constexpr std::uint32_t k_move_r = 1u << 3;
constexpr std::uint32_t k_move_t = 1u << 4;
constexpr std::uint32_t k_move_s = 1u << 5;

[[nodiscard]] float cv_value(::xash::cmd_cvar::Cvar *c, float def) noexcept { return (c != nullptr) ? c->abi.value : def; }
} // namespace

// D3: read-only snapshot — does NOT clear joy_axis_binding's FCVAR_CHANGED
// gate. Joy_AxisMotionEvent (in_joy.c:273-290) never touches joy_axis_binding
// at all; only Joy_FinalizeMove (in_joy.c:317-337) may consume it. Callers
// outside the finalize_move call chain (e.g. pump_events()'s JoyAxisEvent ->
// JoyModel::on_axis_event) must use this variant so they cannot silently
// swallow a pending reparse before finalize_move ever observes it.
JoyTunables Input::Impl::joy_tunables_peek() const noexcept
{
    JoyTunables t;
    t.joy_enable = cv_value(cv.joy_enable, 1.0f) != 0.0f;
    t.pitch_sens = cv_value(cv.joy_pitch, 100.0f);
    t.yaw_sens   = cv_value(cv.joy_yaw, 100.0f);
    t.side_sens  = cv_value(cv.joy_side, 1.0f);
    t.forward_sens = cv_value(cv.joy_forward, 1.0f);
    t.lt_threshold = cv_value(cv.joy_lt_threshold, 16384.0f);
    t.rt_threshold = cv_value(cv.joy_rt_threshold, 16384.0f);
    t.side_key_threshold    = cv_value(cv.joy_side_key_threshold, 24576.0f);
    t.forward_key_threshold = cv_value(cv.joy_forward_key_threshold, 24576.0f);
    t.side_deadzone    = cv_value(cv.joy_side_deadzone, 4096.0f);
    t.forward_deadzone = cv_value(cv.joy_forward_deadzone, 4096.0f);
    t.pitch_deadzone   = cv_value(cv.joy_pitch_deadzone, 4096.0f);
    t.yaw_deadzone     = cv_value(cv.joy_yaw_deadzone, 4096.0f);
    t.axis_binding = (cv.joy_axis_binding != nullptr && cv.joy_axis_binding->abi.string != nullptr)
                         ? cv.joy_axis_binding->abi.string : "sfpyrl";
    t.axis_binding_changed = (cv.joy_axis_binding != nullptr)
                                  && ((cv.joy_axis_binding->abi.flags & ::xash::cmd_cvar::FCVAR_CHANGED) != 0);
    // NOTE: the gate is deliberately NOT cleared here — see joy_tunables().
    t.gyro_enable = cv_value(cv.joy_gyro_enable, 1.0f) != 0.0f;
    t.have_gyro   = cv_value(cv.joy_have_gyro, 0.0f) != 0.0f;
    t.calibrated  = static_cast<GyroCalibrationState>(static_cast<int>(cv_value(cv.joy_calibrated, 0.0f)));
    t.gyro_pitch_sens = cv_value(cv.joy_gyro_pitch, 1.0f);
    t.gyro_yaw_sens   = cv_value(cv.joy_gyro_yaw, 1.0f);
    t.gyro_roll_sens  = cv_value(cv.joy_gyro_roll, 0.0f);
    t.gyro_pitch_deadzone = cv_value(cv.joy_gyro_pitch_deadzone, 0.5f);
    t.gyro_yaw_deadzone   = cv_value(cv.joy_gyro_yaw_deadzone, 0.5f);
    t.gyro_roll_deadzone  = cv_value(cv.joy_gyro_roll_deadzone, 0.5f);
    return t;
}

// D3: the CONSUMING snapshot — test-and-clear FCVAR_CHANGED, exactly once
// per change. Reserved for the finalize_move call chain (collect_input() ->
// JoyModel::finalize_move); every other caller must use joy_tunables_peek().
JoyTunables Input::Impl::joy_tunables() const noexcept
{
    JoyTunables t = joy_tunables_peek();
    if (t.axis_binding_changed) {
        // Quirk 6: gate is cleared once consumed (in_joy.c:336) so the
        // re-parse happens exactly once per change, not every frame forever.
        cv.joy_axis_binding->abi.flags &= ~::xash::cmd_cvar::FCVAR_CHANGED;
    }
    return t;
}

DeviceGyroTunables Input::Impl::device_gyro_tunables() const noexcept
{
    DeviceGyroTunables t;
    t.enable    = cv_value(cv.gyro_enable, 0.0f) != 0.0f;
    t.available = cv_value(cv.gyro_available, 0.0f) != 0.0f;
    t.pitch_sens = cv_value(cv.gyro_pitch, 1.0f);
    t.yaw_sens   = cv_value(cv.gyro_yaw, 1.0f);
    t.roll_sens  = cv_value(cv.gyro_roll, 0.0f);
    t.pitch_deadzone = cv_value(cv.gyro_pitch_deadzone, 0.5f);
    t.yaw_deadzone   = cv_value(cv.gyro_yaw_deadzone, 0.5f);
    t.roll_deadzone  = cv_value(cv.gyro_roll_deadzone, 0.5f);
    return t;
}

void Input::Impl::apply_joy_key_transitions(std::vector<detail::JoyKeyTransition> &t) noexcept
{
    for (auto &tr : t) { key_event(tr.key, tr.down); }
}

// IN_CollectInput (input.c:552-578).
void Input::Impl::collect_input(float &forward, float &side, float &pitch, float &yaw, bool include_mouse,
                                 double frametime) noexcept
{
    if (include_mouse && source != nullptr) {
        Vec2 d = source->pointer_delta();
        pitch += d.y * cv_value(cv.m_pitch, 0.022f);
        yaw   -= d.x * cv_value(cv.m_yaw, 0.022f);

        float yy = yaw, pp = pitch;
        source->evdev_move(yy, pp);
        yaw = yy; pitch = pp;
    }

    bool landscape_flipped = (source != nullptr) && (source->display_orientation() == DisplayOrientation::LandscapeFlipped);
    device_gyro.finalize_move(forward, side, pitch, yaw, device_gyro_tunables(), frametime, landscape_flipped);
    joy.finalize_move(forward, side, pitch, yaw, joy_tunables(), frametime);
    touch.get_move(forward, side, pitch, yaw);

    if (cv_value(cv.look_filter, 0.0f) != 0.0f) {
        pitch = (inputstate_lastpitch + pitch) / 2.0f;
        yaw   = (inputstate_lastyaw + yaw) / 2.0f;
        inputstate_lastpitch = pitch;
        inputstate_lastyaw   = yaw;
    }
}

// IN_JoyAppendMove (input.c:478-550).
void Input::Impl::joy_append_move(MoveCmd &cmd, float forwardmove, float sidemove) noexcept
{
    float fwd_speed  = cv_value(cv.cl_forwardspeed, 400.0f);
    float side_speed = cv_value(cv.cl_sidespeed, 400.0f);

    if (forwardmove != 0.0f) { cmd.forwardmove = forwardmove * fwd_speed; }
    if (sidemove != 0.0f) { cmd.sidemove = sidemove * side_speed; }

    if (cvars == nullptr) { return; } // command synthesis needs a live command buffer

    if (forwardmove != 0.0f) { moveflags &= ~k_move_t; }
    else if ((moveflags & k_move_t) == 0) {
        cvars->cmd_execute_string("-back");
        cvars->cmd_execute_string("-forward");
        moveflags |= k_move_t;
    }

    if (sidemove != 0.0f) { moveflags &= ~k_move_s; }
    else if ((moveflags & k_move_s) == 0) {
        cvars->cmd_execute_string("-moveleft");
        cvars->cmd_execute_string("-moveright");
        moveflags |= k_move_s;
    }

    if (forwardmove > 0.7f && (moveflags & k_move_f) == 0) { moveflags |= k_move_f; cvars->cmd_execute_string("+forward"); }
    else if (forwardmove < 0.7f && (moveflags & k_move_f) != 0) { moveflags &= ~k_move_f; cvars->cmd_execute_string("-forward"); }

    if (forwardmove < -0.7f && (moveflags & k_move_b) == 0) { moveflags |= k_move_b; cvars->cmd_execute_string("+back"); }
    else if (forwardmove > -0.7f && (moveflags & k_move_b) != 0) { moveflags &= ~k_move_b; cvars->cmd_execute_string("-back"); }

    if (sidemove > 0.9f && (moveflags & k_move_r) == 0) { moveflags |= k_move_r; cvars->cmd_execute_string("+moveright"); }
    else if (sidemove < 0.9f && (moveflags & k_move_r) != 0) { moveflags &= ~k_move_r; cvars->cmd_execute_string("-moveright"); }

    if (sidemove < -0.9f && (moveflags & k_move_l) == 0) { moveflags |= k_move_l; cvars->cmd_execute_string("+moveleft"); }
    else if (sidemove > -0.9f && (moveflags & k_move_l) != 0) { moveflags &= ~k_move_l; cvars->cmd_execute_string("-moveleft"); }
}

// IN_EngineAppendMove (input.c:587-615).
void Input::Impl::engine_append_move(float frametime, MoveCmd &cmd, bool active) noexcept
{
    if (callbacks.has_look_event()) { return; } // modern-path opt-out switch (Dependencies: pfnLookEvent bypass seam)
    if (key_dest != KeyDest::Game) { return; }  // [cl.paused/cl.intermission gate omitted: client subsystem not ported]

    if (!active) { return; }

    float forward = 0.0f, side = 0.0f, pitch = 0.0f, yaw = 0.0f;
    collect_input(forward, side, pitch, yaw, false, frametime); // Quirk 12: legacy-path never reads mouse deltas
    joy_append_move(cmd, forward, side);

    if (pitch != 0.0f || yaw != 0.0f) {
        cmd.viewangles[k_move_yaw]   += yaw;
        cmd.viewangles[k_move_pitch] += pitch;
        cmd.viewangles[k_move_pitch]  = std::clamp(cmd.viewangles[k_move_pitch], -90.0f, 90.0f);
        // cl.viewangles mirroring omitted: cl.viewangles is client-subsystem
        // state that does not exist in Chunk 10 (documented gap).
    }
}

// IN_Commands (input.c:617-640).
void Input::Impl::run_commands(double frametime) noexcept
{
    if (source != nullptr) { source->evdev_frame(); }

    if (callbacks.has_look_event()) {
        float forward = 0.0f, side = 0.0f, pitch = 0.0f, yaw = 0.0f;
        bool  include_mouse = in_mouseinitialized && (cv_value(cv.m_ignore, 0.0f) == 0.0f);
        collect_input(forward, side, pitch, yaw, include_mouse, frametime);

        if (key_dest == KeyDest::Game) {
            callbacks.look_event(callbacks.user, yaw, pitch);
            if (callbacks.move_event != nullptr) { callbacks.move_event(callbacks.user, forward, side); }
        }
    }

    if (!in_mouseinitialized) { return; }
    check_mouse_state(in_mouseactive);
}

// IN_MouseMove (input.c:338-359).
void Input::Impl::mouse_move() noexcept
{
    if (!in_mouseinitialized) { return; }

    bool touch_enable_on  = (cv.touch_enable  != nullptr) && (cv.touch_enable->abi.value  != 0.0f);
    bool touch_emulate_on = (cv.touch_emulate != nullptr) && (cv.touch_emulate->abi.value != 0.0f);
    bool touch_in_menu_on = (cv.touch_in_menu != nullptr) && (cv.touch_in_menu->abi.value != 0.0f);
    if (touch.want_visible_cursor(touch_enable_on, touch_emulate_on, touch_in_menu_on)) {
        // Touch_KeyEvent(0,0) pure-motion path needs a normalized mouse
        // position — screen-dimension state Chunk 10 does not own (same gap
        // as mouse_button_event's touch branch; see Input::touch_key_event).
        return;
    }

    if (source == nullptr) { return; }
    Vec2 pos = source->mouse_pos();
    if (callbacks.vgui_mouse_move != nullptr) { callbacks.vgui_mouse_move(callbacks.user, static_cast<int>(pos.x), static_cast<int>(pos.y)); }
    if (callbacks.ui_mouse_move != nullptr) { callbacks.ui_mouse_move(callbacks.user, static_cast<int>(pos.x), static_cast<int>(pos.y)); }
}

// IN_CheckMouseState (input.c:271-293).
void Input::Impl::check_mouse_state(bool active) noexcept
{
    // Win32 raw-input gate `(m_rawinput.value && client_dll_uses_sdl) ||
    // pfnLookEvent != NULL` omitted: client_dll_uses_sdl is client-subsystem
    // state not ported in Chunk 10; always takes the non-Win32 legacy
    // default ("always use SDL code").
    bool use_raw_input = true;

    if (cv_value(cv.m_ignore, 0.0f) != 0.0f) { active = false; }

    // cls.state == ca_active stand-in: client connection state not ported
    // in Chunk 10; always true (documented gap — Chunk 12 wires the real value).
    bool ca_active = true;

    if (active && use_raw_input && !mouse_visible && ca_active) { set_relative_mouse_mode(true); }
    else { set_relative_mouse_mode(false); }

    if (active && !mouse_visible && ca_active) { set_mouse_grab(true); }
    else { set_mouse_grab(false); }
}

// IN_ToggleClientMouse (input.c:174-210).
void Input::Impl::toggle_client_mouse(KeyDest newstate, KeyDest oldstate) noexcept
{
    if (newstate == oldstate) { return; }

    if (newstate == KeyDest::Menu || newstate == KeyDest::Console) {
        window->set_cursor_type(CursorType::Arrow);
        if (source != nullptr) { source->evdev_set_grab(false); }
    } else {
        window->set_cursor_type(CursorType::None);
        if (source != nullptr) { source->evdev_set_grab(true); }
    }

    if (cv_value(cv.m_ignore, 0.0f) != 0.0f) { return; } // don't strand the user without a cursor

    if (oldstate == KeyDest::Game) { deactivate_mouse(); }
    else if (newstate == KeyDest::Game) { activate_mouse(); }
}

// IN_SetRelativeMouseMode (input.c:212-246) — edge-latched via member state
// (Threading table: was a function-local static, Race-static-buf shape).
void Input::Impl::set_relative_mouse_mode(bool set) noexcept
{
    if (set && !s_raw_input) {
        if (source != nullptr) { source->set_relative_mouse_mode(true); }
        s_raw_input = true;
    } else if (!set && s_raw_input) {
        if (source != nullptr) { source->set_relative_mouse_mode(false); }
        s_raw_input = false;
    }
}

// IN_SetMouseGrab (input.c:248-269).
void Input::Impl::set_mouse_grab(bool set) noexcept
{
    if (set && !s_mouse_grab) {
        window->set_mouse_grab(true);
        s_mouse_grab = true;
    } else if (!set && s_mouse_grab) {
        window->set_mouse_grab(false);
        s_mouse_grab = false;
    }
}

// IN_ActivateMouse / IN_DeactivateMouse (input.c:302-329).
void Input::Impl::activate_mouse() noexcept
{
    if (!in_mouseinitialized) { return; }
    check_mouse_state(true);
    if (callbacks.in_activate_mouse != nullptr) { callbacks.in_activate_mouse(callbacks.user); }
    in_mouseactive = true;
}

void Input::Impl::deactivate_mouse() noexcept
{
    if (!in_mouseinitialized) { return; }
    check_mouse_state(false);
    if (callbacks.in_deactivate_mouse != nullptr) { callbacks.in_deactivate_mouse(callbacks.user); }
    in_mouseactive = false;
}

// ---------------------------------------------------------------------------
// Input:: public forwarders
// ---------------------------------------------------------------------------

void Input::engine_append_move(float frametime, MoveCmd &cmd, bool active) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->engine_append_move(frametime, cmd, active);
}

void Input::run_commands(double frametime) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->run_commands(frametime);
}

void Input::mouse_move() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->mouse_move();
}

void Input::host_input_frame(double frametime) noexcept
{
    // Host_InputFrame (input.c:649-654).
    run_commands(frametime);
    mouse_move();
}

} // namespace xash::input
