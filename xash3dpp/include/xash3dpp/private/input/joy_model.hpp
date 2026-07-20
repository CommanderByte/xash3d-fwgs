#pragma once
// xash3dpp — joystick/gamepad axis math + gyro (gamepad-integrated and
// built-in device) + the gyro calibration state machine.
// Legacy reference: engine/client/input/in_joy.c, in_gyro.c,
// engine/platform/sdl2/joy_sdl2.c (gyrocal state machine, Quirk 7).
//
// Deliberately decoupled from cmd_cvar — tunables are passed in by value
// each call (JoyTunables/DeviceGyroTunables), read fresh from the live cvars
// by Input's orchestration layer (src/input/joy/joy.cpp). This keeps the
// math testable without a CmdCvarContext.

#include <xash3dpp/input/joy.hpp>
#include <xash3dpp/input/key.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace xash::input::detail {

// ---------------------------------------------------------------------------
// JoyTunables — live cvar snapshot consumed by JoyModel each call.
// ---------------------------------------------------------------------------

struct JoyTunables
{
    bool  joy_enable = true;

    float pitch_sens = 100.0f;
    float yaw_sens   = 100.0f;
    float side_sens  = 1.0f;
    float forward_sens = 1.0f;

    float lt_threshold = 16384.0f;
    float rt_threshold = 16384.0f;
    float side_key_threshold    = 24576.0f;
    float forward_key_threshold = 24576.0f;

    float side_deadzone    = 4096.0f;
    float forward_deadzone = 4096.0f;
    float pitch_deadzone   = 4096.0f;
    float yaw_deadzone     = 4096.0f;

    // joy_axis_binding.string + FCVAR_CHANGED gate (Quirk 6: lazy re-parse,
    // once per frame inside Joy_FinalizeMove, never at init/via callback).
    const char *axis_binding = "sfpyrl";
    bool        axis_binding_changed = false;

    bool  gyro_enable  = true;  // joy_gyro_enable
    bool  have_gyro    = false; // joy_have_gyro
    GyroCalibrationState calibrated = GyroCalibrationState::NotCalibrated;
    float gyro_pitch_sens = 1.0f, gyro_yaw_sens = 1.0f, gyro_roll_sens = 0.0f;
    float gyro_pitch_deadzone = 0.5f, gyro_yaw_deadzone = 0.5f, gyro_roll_deadzone = 0.5f;
};

struct DeviceGyroTunables
{
    bool  enable   = false; // gyro_enable (OFF by default, unlike joy_gyro_enable=ON)
    bool  available = false; // gyro_available
    float pitch_sens = 1.0f, yaw_sens = 1.0f, roll_sens = 0.0f;
    float pitch_deadzone = 0.5f, yaw_deadzone = 0.5f, roll_deadzone = 0.5f;
};

// Key transitions produced by axis processing that must go through the FULL
// Key_Event routing pipeline (trigger presses, and the UI-mode hat synthesis
// side effect) — JoyModel cannot call back into Input's routing core
// directly, so it hands transitions back to the caller instead.
struct JoyKeyTransition { Key key; bool down; };

// ---------------------------------------------------------------------------
// JoyModel — axis state, deadzone/threshold math, hat synthesis.
// ---------------------------------------------------------------------------

class JoyModel
{
public:
    JoyModel() noexcept;

    // Joy_AxisMotionEvent (in_joy.c:273-290). |hw_axis| is the raw hardware
    // index (0..MAX_AXES-1); remapped internally via the axis-binding table.
    // |key_dest| gates the UI-mode hat-synthesis side effect (Joy_ProcessStick,
    // in_joy.c:254-263). Returns any Key_Event transitions the caller must route.
    [[nodiscard]] std::vector<JoyKeyTransition> on_axis_event(
        std::uint8_t hw_axis, std::int16_t value, const JoyTunables &t, bool ui_mode) noexcept;

    // Joy_FinalizeMove (in_joy.c:312-363). Additive into fw/side/dpitch/dyaw.
    void finalize_move(float &fw, float &side, float &dpitch, float &dyaw,
                        const JoyTunables &t, double frametime) noexcept;

    // Joy_GyroEvent (in_joy.c:299-303) — stores into the live + display buffers.
    void on_gyro_sample(float x, float y, float z) noexcept;

    [[nodiscard]] JoyAxisState axis_state(JoyAxis axis) const noexcept;
    [[nodiscard]] std::array<float, 3> gyro_speed_display() const noexcept { return gyro_display_; }

private:
    // joyaxesmap[MAX_AXES] (in_joy.c:29-37) — hardware index -> logical axis.
    std::array<JoyAxis, k_joy_axis_count> axis_map_{};
    std::array<JoyAxisState, k_joy_axis_count> axes_{};
    std::array<float, 3> gyro_speed_{ 0.0f, 0.0f, 0.0f };  // live buffer, cleared every finalize_move
    std::array<float, 3> gyro_display_{ 0.0f, 0.0f, 0.0f }; // survives the per-frame clear
};

// ---------------------------------------------------------------------------
// DeviceGyro — built-in device gyroscope (in_gyro.c).
// ---------------------------------------------------------------------------

class DeviceGyro
{
public:
    void on_sample(float x, float y, float z) noexcept; // IN_GyroEvent

    // IN_GyroFinalizeMove (in_gyro.c:89-119). Landscape-mode axis swap +
    // sign flip per |orientation|.
    void finalize_move(float &fw, float &side, float &dpitch, float &dyaw,
                        const DeviceGyroTunables &t, double frametime,
                        bool landscape_flipped) noexcept;

private:
    std::array<float, 3> speed_{ 0.0f, 0.0f, 0.0f };
};

// ---------------------------------------------------------------------------
// GyroCalibration — the gamepad-gyro calibration state machine (Quirk 7).
// Legacy: joy_sdl2.c's `gyrocal` module-static + SDLash_RestartCalibration/
// AccumulateCalibrationData/FinalizeCalibration/the SensorUpdate gyrocal
// branch (joy_sdl2.c:55-138,268-294).
//
// Self-latching: every successful calibration re-arms another window and
// silently re-averages continuously; a failed CONTINUOUS re-cal is muted (no
// state change) — only the first calibration can transition to
// FailedToCalibrate.  Modernized as member state (was a module-static struct).
// ---------------------------------------------------------------------------

class GyroCalibration
{
public:
    // SDLash_RestartCalibration. |now| is the current time in seconds
    // (host.realtime equivalent); |data_rate| defaults to the legacy
    // pre-2.0.16 SDL fallback of 10.0 Hz.
    void restart(double now, float data_rate = 10.0f) noexcept;

    // The gyrocal branch of SDLash_GameControllerSensorUpdate
    // (joy_sdl2.c:279-292). Returns the bias-corrected sample IFF it should
    // be forwarded to Joy_GyroEvent (nullopt: suppressed during the initial,
    // non-continuous calibration window).
    [[nodiscard]] std::optional<std::array<float, 3>> on_sample(double now, float x, float y, float z) noexcept;

    [[nodiscard]] GyroCalibrationState state() const noexcept { return state_; }
    [[nodiscard]] bool window_active() const noexcept { return window_active_; } // gyrocal.time != 0

private:
    void finalize(double now) noexcept;
    void accumulate(float x, float y, float z) noexcept;

    bool   window_active_ = false;
    double window_end_    = 0.0;
    float  data_rate_     = 10.0f;
    float  sum_[3]        = { 0.0f, 0.0f, 0.0f };
    int    samples_       = 0;
    float  bias_[3]       = { 0.0f, 0.0f, 0.0f }; // gyrocal.calibrated_values
    bool   continuous_    = false;
    GyroCalibrationState state_ = GyroCalibrationState::NotCalibrated;
};

} // namespace xash::input::detail
