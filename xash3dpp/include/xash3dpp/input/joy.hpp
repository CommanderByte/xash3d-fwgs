#pragma once
// xash3dpp — joystick/gamepad/gyro public types
// Legacy reference: engine/client/input.h (engineAxis_t, joy_calibration_state_t,
// JOY_HAT_* bitmask), in_joy.c, in_gyro.c.

#include <cstdint>

// @annotation-exempt: cold-value-type — JoyAxis/GyroCalibrationState/JoyHat
// are plain enums (JoyHat's operator|/has_hat are pure, stateless functions),
// and JoyAxisState is a POD snapshot struct returned BY VALUE from
// Input::joy_axis_state(); none carries shared mutable state, so the QN
// annotation matrix's @thread-safety requirement does not apply here.
namespace xash::input {

// engineAxis_t (client/input.h:127-136).
enum class JoyAxis : std::uint8_t
{
    Side = 0,
    Fwd,
    Pitch,
    Yaw,
    Rt,
    Lt,
    Count,
};
inline constexpr int k_joy_axis_count = static_cast<int>(JoyAxis::Count);

// joy_calibration_state_t (client/input.h:138-144).
enum class GyroCalibrationState : std::uint8_t
{
    NotCalibrated = 0,
    Calibrating,
    FailedToCalibrate,
    Calibrated,
};

// JOY_HAT_* bitmask (client/input.h:114-125) — combinable via OR.
enum class JoyHat : std::uint8_t
{
    Centered = 0,
    Up       = 1u << 0,
    Right    = 1u << 1,
    Down     = 1u << 2,
    Left     = 1u << 3,
};
[[nodiscard]] constexpr JoyHat operator|(JoyHat a, JoyHat b) noexcept
{
    return static_cast<JoyHat>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
[[nodiscard]] constexpr bool has_hat(JoyHat mask, JoyHat bit) noexcept
{
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(bit)) != 0;
}

// Snapshot of one logical axis's live state — P-4 typed introspection
// surface (mirrors joyaxis_s: val/prevval/rawval, in_joy.c:39-44).
struct JoyAxisState
{
    std::int16_t value    = 0;
    std::int16_t prev     = 0;
    std::int16_t raw      = 0; // pre-deadzone value, for debug display
};

} // namespace xash::input
