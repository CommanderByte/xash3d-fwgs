#pragma once
// xash3dpp — Input::Impl — the full aggregate state behind the pimpl.
// Shared by every src/input/**/*.cpp translation unit that implements a
// piece of Input's behaviour (keys/, joy/, touch/, osk/, move assembly).
//
// @thread-safety: T_Main only.

#include <xash3dpp/input/event.hpp>
#include <xash3dpp/input/input.hpp>

#include <xash3dpp/private/input/joy_model.hpp>
#include <xash3dpp/private/input/key_table.hpp>
#include <xash3dpp/private/input/osk_model.hpp>
#include <xash3dpp/private/input/touch_model.hpp>

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>

#include <xash3dpp/memory/memory.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace xash::input {

// This header is private (src/input/**/*.cpp only).  Targeted
// using-DECLARATIONS (named types, not a namespace import — the
// no-using-namespace convention holds) keep Impl's member declarations
// readable without qualifying every helper type at every use.
using detail::DeviceGyro;
using detail::DeviceGyroTunables;
using detail::GyroCalibration;
using detail::JoyKeyTransition;
using detail::JoyModel;
using detail::JoyTunables;
using detail::KeyNameEntry;
using detail::KeyRecord;
using detail::KeyTable;
using detail::OskModel;
using detail::TouchButtonRecord;
using detail::TouchDefaultButtonRecord;
using detail::TouchModel;
using detail::TouchTunables;

// ---------------------------------------------------------------------------
// CvarRefs — every cvar Input registers, cached as a live pointer (mirrors
// the legacy CVAR_DEFINE_AUTO-then-Cvar_RegisterVariable-once pattern; math
// reads cv->abi.value / cv->abi.string fresh every call, same as legacy code
// reading a convar_t's .value/.string fields directly).
// ---------------------------------------------------------------------------

struct CvarRefs
{
    ::xash::cmd_cvar::Cvar *key_rotate = nullptr;
    ::xash::cmd_cvar::Cvar *m_pitch = nullptr;
    ::xash::cmd_cvar::Cvar *m_yaw = nullptr;
    ::xash::cmd_cvar::Cvar *m_ignore = nullptr;
    ::xash::cmd_cvar::Cvar *look_filter = nullptr;
    ::xash::cmd_cvar::Cvar *m_rawinput = nullptr;
    ::xash::cmd_cvar::Cvar *m_grab_debug = nullptr;
    ::xash::cmd_cvar::Cvar *cl_forwardspeed = nullptr;
    ::xash::cmd_cvar::Cvar *cl_backspeed = nullptr;
    ::xash::cmd_cvar::Cvar *cl_sidespeed = nullptr;
    ::xash::cmd_cvar::Cvar *touch_enable = nullptr;
    ::xash::cmd_cvar::Cvar *osk_enable = nullptr;

    ::xash::cmd_cvar::Cvar *joy_enable = nullptr;
    ::xash::cmd_cvar::Cvar *joy_pitch = nullptr;
    ::xash::cmd_cvar::Cvar *joy_yaw = nullptr;
    ::xash::cmd_cvar::Cvar *joy_side = nullptr;
    ::xash::cmd_cvar::Cvar *joy_forward = nullptr;
    ::xash::cmd_cvar::Cvar *joy_lt_threshold = nullptr;
    ::xash::cmd_cvar::Cvar *joy_rt_threshold = nullptr;
    ::xash::cmd_cvar::Cvar *joy_side_key_threshold = nullptr;
    ::xash::cmd_cvar::Cvar *joy_forward_key_threshold = nullptr;
    ::xash::cmd_cvar::Cvar *joy_side_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_forward_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_pitch_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_yaw_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_axis_binding = nullptr;
    ::xash::cmd_cvar::Cvar *joy_have_gyro = nullptr;
    ::xash::cmd_cvar::Cvar *joy_calibrated = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_enable = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_pitch = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_yaw = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_roll = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_pitch_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_yaw_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_gyro_roll_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *joy_debug = nullptr;

    ::xash::cmd_cvar::Cvar *gyro_enable = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_available = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_pitch = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_yaw = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_roll = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_pitch_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_yaw_deadzone = nullptr;
    ::xash::cmd_cvar::Cvar *gyro_roll_deadzone = nullptr;

    ::xash::cmd_cvar::Cvar *touch_in_menu = nullptr;
    ::xash::cmd_cvar::Cvar *touch_forwardzone = nullptr;
    ::xash::cmd_cvar::Cvar *touch_sidezone = nullptr;
    ::xash::cmd_cvar::Cvar *touch_pitch = nullptr;
    ::xash::cmd_cvar::Cvar *touch_yaw = nullptr;
    ::xash::cmd_cvar::Cvar *touch_nonlinear_look = nullptr;
    ::xash::cmd_cvar::Cvar *touch_pow_factor = nullptr;
    ::xash::cmd_cvar::Cvar *touch_pow_mult = nullptr;
    ::xash::cmd_cvar::Cvar *touch_exp_mult = nullptr;
    ::xash::cmd_cvar::Cvar *touch_grid_count = nullptr;
    ::xash::cmd_cvar::Cvar *touch_grid_enable = nullptr;
    ::xash::cmd_cvar::Cvar *touch_config_file = nullptr;
    ::xash::cmd_cvar::Cvar *touch_precise_amount = nullptr;
    ::xash::cmd_cvar::Cvar *touch_highlight_r = nullptr;
    ::xash::cmd_cvar::Cvar *touch_highlight_g = nullptr;
    ::xash::cmd_cvar::Cvar *touch_highlight_b = nullptr;
    ::xash::cmd_cvar::Cvar *touch_highlight_a = nullptr;
    ::xash::cmd_cvar::Cvar *touch_dpad_radius = nullptr;
    ::xash::cmd_cvar::Cvar *touch_joy_radius = nullptr;
    ::xash::cmd_cvar::Cvar *touch_move_indicator = nullptr;
    ::xash::cmd_cvar::Cvar *touch_joy_texture = nullptr;
    ::xash::cmd_cvar::Cvar *touch_emulate = nullptr;
};

// ---------------------------------------------------------------------------
// Input::Impl
// ---------------------------------------------------------------------------

struct Input::Impl
{
    explicit Impl(memory::PoolHandle pool, const InputInitParams &params) noexcept;

    memory::PoolHandle pool;
    InputCallbacks     callbacks;
    ::xash::cmd_cvar::CmdCvarContext *cvars  = nullptr; // @lifetime: engine (borrowed)
    IEventSource       *source = nullptr;                // @lifetime: caller (borrowed)
    NullWindowControls  null_window;
    IWindowControls     *window = nullptr;                // @lifetime: caller (borrowed) or &null_window
    bool                dedicated = false;

    CvarRefs cv;

    detail::KeyTable        keys;
    detail::TouchModel      touch;
    detail::OskModel        osk;
    detail::JoyModel        joy;
    detail::DeviceGyro      device_gyro;
    detail::GyroCalibration gyro_cal;

    KeyDest key_dest = KeyDest::Game;
    bool    changelevel = false;
    bool    mouse_visible = false; // host.mouse_visible stand-in (Owned state, no host module yet)
    bool    textmode = false;      // host.textmode stand-in (Key_EnableTextInput's own state)

    InputStats stats;

    // Mouse activation state machine (input.c:29-40).
    bool in_mouseactive = false;
    bool in_mouseinitialized = false;
    int  in_lastvalidpos_x = 0, in_lastvalidpos_y = 0;
    bool in_mouse_savedpos = false;
    int  in_mstate = 0;
    float inputstate_lastpitch = 0.0f, inputstate_lastyaw = 0.0f;

    // Edge latches — member state per Threading table (were function-locals,
    // Race-static-buf shape; S10.1/S10.4 require member state).
    bool s_raw_input = false;
    bool s_mouse_grab = false;

    // IN_JoyAppendMove's persistent frame-to-frame edge state (was
    // `static uint moveflags` — S10.4 requires member state).
    std::uint32_t moveflags = 0;

    double gyro_time_now = 0.0; // monotonic clock fed by the caller each frame (calibration window driver)

    // ---- registration (S10.2) ----
    void register_cvars_and_commands() noexcept;

    // ---- key routing (S10.1) ----
    void key_event(Key key, bool down) noexcept;
    void clear_states() noexcept;
    void char_event(int ch) noexcept;
    void enable_text_input(bool enable, bool force) noexcept;
    void dispatch_key_commands(Key key, std::string_view binding, bool down) noexcept;
    void mouse_button_event(int button, bool down) noexcept;

    // ---- move assembly (S10.4) ----
    void engine_append_move(float frametime, MoveCmd &cmd, bool active) noexcept;
    void run_commands(double frametime) noexcept;
    void mouse_move() noexcept;
    void collect_input(float &forward, float &side, float &pitch, float &yaw, bool include_mouse, double frametime) noexcept;
    void joy_append_move(MoveCmd &cmd, float forwardmove, float sidemove) noexcept;
    void check_mouse_state(bool active) noexcept;
    void toggle_client_mouse(KeyDest newstate, KeyDest oldstate) noexcept;
    void set_relative_mouse_mode(bool set) noexcept;
    void set_mouse_grab(bool set) noexcept;
    void activate_mouse() noexcept;
    void deactivate_mouse() noexcept;

    // ---- joy/gyro (S10.3) ----
    // D3: joy_axis_binding's FCVAR_CHANGED gate may only be consumed (test-
    // and-clear) by the finalize_move call chain (Joy_FinalizeMove,
    // in_joy.c:317-337) — Joy_AxisMotionEvent never touches it
    // (in_joy.c:273-290). joy_tunables() is the consuming variant, reserved
    // for collect_input() -> JoyModel::finalize_move; joy_tunables_peek() is
    // the read-only variant every other caller (e.g. on_axis_event) must use.
    JoyTunables        joy_tunables() const noexcept;
    JoyTunables        joy_tunables_peek() const noexcept;
    DeviceGyroTunables device_gyro_tunables() const noexcept;
    void               apply_joy_key_transitions(std::vector<detail::JoyKeyTransition> &t) noexcept;

    // ---- touch (S10.5) ----
    TouchTunables touch_tunables() const noexcept;
    int           touch_event(TouchEventType type, int finger_id, float x, float y, float dx, float dy) noexcept;
    void          dispatch_touch_commands(std::vector<detail::TouchModel::CommandDispatch> &cmds) noexcept;
    void          register_touch_commands() noexcept;

    // ---- osk ----
    // (osk_key_event/osk_enable_text_input implemented inline via osk member)
};

} // namespace xash::input
