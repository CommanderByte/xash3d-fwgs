// xash3dpp — JoyModel / DeviceGyro / GyroCalibration implementation
// Legacy reference: in_joy.c, in_gyro.c, engine/platform/sdl2/joy_sdl2.c.

#include <xash3dpp/private/input/joy_model.hpp>

#include <cmath>

namespace xash::input::detail {

namespace {
constexpr float k_shrt_max = 32767.0f; // SHRT_MAX
constexpr float k_pi       = 3.14159265358979323846f;
} // namespace

// ---------------------------------------------------------------------------
// JoyModel
// ---------------------------------------------------------------------------

JoyModel::JoyModel() noexcept
{
    // joyaxesmap[] static-init default (in_joy.c:29-37) — index==value.
    axis_map_[static_cast<int>(JoyAxis::Side)]  = JoyAxis::Side;
    axis_map_[static_cast<int>(JoyAxis::Fwd)]   = JoyAxis::Fwd;
    axis_map_[static_cast<int>(JoyAxis::Pitch)] = JoyAxis::Pitch;
    axis_map_[static_cast<int>(JoyAxis::Yaw)]   = JoyAxis::Yaw;
    axis_map_[static_cast<int>(JoyAxis::Rt)]    = JoyAxis::Rt;
    axis_map_[static_cast<int>(JoyAxis::Lt)]    = JoyAxis::Lt;
}

std::vector<JoyKeyTransition> JoyModel::on_axis_event(
    std::uint8_t hw_axis, std::int16_t value, const JoyTunables &t, bool ui_mode) noexcept
{
    std::vector<JoyKeyTransition> out;

    if (hw_axis >= k_joy_axis_count) { return out; }
    JoyAxis axis = axis_map_[hw_axis];
    if (static_cast<int>(axis) >= k_joy_axis_count) { return out; } // MAX_AXES == "disabled" (Quirk 8's binding table)

    int ai = static_cast<int>(axis);
    if (value == axes_[ai].value) { return out; } // "it is not an update"

    if (axis == JoyAxis::Rt || axis == JoyAxis::Lt) {
        // Joy_ProcessTrigger (in_joy.c:154-187) — edge-detected via
        // prev/cur straddling the threshold, not a level check. |trigThreshold|
        // is declared `int` in legacy (in_joy.c:156) — the float cvar value
        // truncates toward zero on assignment (Minor 7); reproduced via an
        // explicit int cast rather than comparing against the raw float.
        int threshold = static_cast<int>((axis == JoyAxis::Rt) ? t.rt_threshold : t.lt_threshold);
        Key button    = (axis == JoyAxis::Rt) ? Key::RTrigger : Key::LTrigger;

        axes_[ai].prev  = axes_[ai].value;
        axes_[ai].value = value;

        if (axes_[ai].value > threshold && axes_[ai].prev <= threshold) {
            out.push_back({ button, true });
        } else if (axes_[ai].value < threshold && axes_[ai].prev >= threshold) {
            out.push_back({ button, false });
        }
        return out;
    }

    // Joy_ProcessStick (in_joy.c:229-264). |deadzone| is declared `int` in
    // legacy (in_joy.c:231) — truncated from the float cvar (Minor 7).
    int deadzone = 0;
    switch (axis) {
        case JoyAxis::Fwd:   deadzone = static_cast<int>(t.forward_deadzone); break;
        case JoyAxis::Side:  deadzone = static_cast<int>(t.side_deadzone);    break;
        case JoyAxis::Pitch: deadzone = static_cast<int>(t.pitch_deadzone);   break;
        case JoyAxis::Yaw:   deadzone = static_cast<int>(t.yaw_deadzone);     break;
        default: break;
    }

    axes_[ai].raw = value;

    std::int16_t deadzoned = value;
    if (deadzoned < deadzone && deadzoned > -deadzone) { deadzoned = 0; }

    axes_[ai].prev  = axes_[ai].value;
    axes_[ai].value = deadzoned;

    // fwd/side axes simulate hat movement while in menu/console (in_joy.c:254-263).
    if ((axis == JoyAxis::Side || axis == JoyAxis::Fwd) && ui_mode) {
        auto hat_for = [&](JoyAxis a, float threshold, Key negative, Key positive,
                            std::vector<JoyKeyTransition> &dst) {
            int i2 = static_cast<int>(a);
            if (axes_[i2].value > threshold && axes_[i2].prev <= threshold) {
                dst.push_back({ positive, true });
            } else if (axes_[i2].value < -threshold && axes_[i2].prev >= -threshold) {
                dst.push_back({ negative, true });
            }
        };
        std::vector<JoyKeyTransition> hat;
        hat_for(JoyAxis::Side, t.side_key_threshold, Key::LeftArrow, Key::RightArrow, hat);
        hat_for(JoyAxis::Fwd, t.forward_key_threshold, Key::UpArrow, Key::DownArrow, hat);
        // Joy_HatMotionEvent (in_joy.c:119-147): presses the arrows implied
        // by |val| and releases the other two — modelled here as: any arrow
        // not present this call whose axis matches gets released.
        bool want_up = false, want_down = false, want_left = false, want_right = false;
        for (auto &h : hat) {
            if (h.key == Key::UpArrow) { want_up = true; }
            if (h.key == Key::DownArrow) { want_down = true; }
            if (h.key == Key::LeftArrow) { want_left = true; }
            if (h.key == Key::RightArrow) { want_right = true; }
        }
        if (!want_up) { out.push_back({ Key::UpArrow, false }); }
        if (!want_down) { out.push_back({ Key::DownArrow, false }); }
        if (!want_left) { out.push_back({ Key::LeftArrow, false }); }
        if (!want_right) { out.push_back({ Key::RightArrow, false }); }
        for (auto &h : hat) { out.push_back(h); }
    }

    return out;
}

void JoyModel::finalize_move(float &fw, float &side, float &dpitch, float &dyaw,
                              const JoyTunables &t, double frametime) noexcept
{
    if (!t.joy_enable) { return; }

    if (t.axis_binding_changed) {
        // Quirk 6: lazy re-parse, once per frame, never at init/via callback.
        for (int i = 0; i < k_joy_axis_count && t.axis_binding[i] != '\0'; ++i) {
            switch (t.axis_binding[i]) {
                case 's': axis_map_[static_cast<std::size_t>(i)] = JoyAxis::Side;  break;
                case 'f': axis_map_[static_cast<std::size_t>(i)] = JoyAxis::Fwd;   break;
                case 'y': axis_map_[static_cast<std::size_t>(i)] = JoyAxis::Yaw;   break;
                case 'p': axis_map_[static_cast<std::size_t>(i)] = JoyAxis::Pitch; break;
                case 'r': axis_map_[static_cast<std::size_t>(i)] = JoyAxis::Rt;    break;
                case 'l': axis_map_[static_cast<std::size_t>(i)] = JoyAxis::Lt;    break;
                default:  axis_map_[static_cast<std::size_t>(i)] = static_cast<JoyAxis>(k_joy_axis_count); break;
            }
        }
    }

    fw   -= t.forward_sens * static_cast<float>(axes_[static_cast<int>(JoyAxis::Fwd)].value) / k_shrt_max;
    side += t.side_sens    * static_cast<float>(axes_[static_cast<int>(JoyAxis::Side)].value) / k_shrt_max;
    dpitch += t.pitch_sens * static_cast<float>(axes_[static_cast<int>(JoyAxis::Pitch)].value) / k_shrt_max
              * static_cast<float>(frametime);
    dyaw   -= t.yaw_sens   * static_cast<float>(axes_[static_cast<int>(JoyAxis::Yaw)].value) / k_shrt_max
              * static_cast<float>(frametime);

    if (t.gyro_enable && t.have_gyro && t.calibrated == GyroCalibrationState::Calibrated) {
        float pitch_speed = gyro_speed_[0] * (180.0f / k_pi);
        float yaw_speed   = gyro_speed_[1] * (180.0f / k_pi);
        float roll_speed  = gyro_speed_[2] * (180.0f / k_pi);

        if (std::fabs(pitch_speed) < t.gyro_pitch_deadzone) { pitch_speed = 0.0f; }
        if (std::fabs(yaw_speed) < t.gyro_yaw_deadzone) { yaw_speed = 0.0f; }
        if (std::fabs(roll_speed) < t.gyro_roll_deadzone) { roll_speed = 0.0f; }

        dpitch -= t.gyro_pitch_sens * pitch_speed * static_cast<float>(frametime);
        dyaw   += t.gyro_yaw_sens   * yaw_speed   * static_cast<float>(frametime);
        dyaw   += t.gyro_roll_sens  * roll_speed  * static_cast<float>(frametime);
    }

    gyro_speed_ = { 0.0f, 0.0f, 0.0f }; // unconditionally cleared every call (in_joy.c:362)
}

void JoyModel::on_gyro_sample(float x, float y, float z) noexcept
{
    gyro_speed_   = { x, y, z };
    gyro_display_ = { x, y, z }; // survives the per-frame clear
}

JoyAxisState JoyModel::axis_state(JoyAxis axis) const noexcept
{
    int i = static_cast<int>(axis);
    if (i < 0 || i >= k_joy_axis_count) { return {}; }
    return axes_[static_cast<std::size_t>(i)];
}

// ---------------------------------------------------------------------------
// DeviceGyro
// ---------------------------------------------------------------------------

void DeviceGyro::on_sample(float x, float y, float z) noexcept
{
    speed_ = { x, y, z };
}

void DeviceGyro::finalize_move(float &fw, float &side, float &dpitch, float &dyaw,
                                const DeviceGyroTunables &t, double frametime,
                                bool landscape_flipped) noexcept
{
    (void)fw;
    (void)side;
    if (!t.enable || !t.available) { return; }

    float orient_scale = landscape_flipped ? -1.0f : 1.0f;

    // Landscape axes are swapped relative to natural (Portrait) orientation:
    // Y rotation -> pitch, X rotation -> yaw (in_gyro.c:101-106).
    float pitch_speed = -orient_scale * speed_[1] * (180.0f / k_pi);
    float yaw_speed   =  orient_scale * speed_[0] * (180.0f / k_pi);
    float roll_speed  =  orient_scale * speed_[2] * (180.0f / k_pi);

    if (std::fabs(pitch_speed) < t.pitch_deadzone) { pitch_speed = 0.0f; }
    if (std::fabs(yaw_speed) < t.yaw_deadzone) { yaw_speed = 0.0f; }
    if (std::fabs(roll_speed) < t.roll_deadzone) { roll_speed = 0.0f; }

    dpitch -= t.pitch_sens * pitch_speed * static_cast<float>(frametime);
    dyaw   += t.yaw_sens   * yaw_speed   * static_cast<float>(frametime);
    dyaw   += t.roll_sens  * roll_speed  * static_cast<float>(frametime);

    speed_ = { 0.0f, 0.0f, 0.0f };
}

// ---------------------------------------------------------------------------
// GyroCalibration
// ---------------------------------------------------------------------------

void GyroCalibration::restart(double now, float data_rate) noexcept
{
    state_         = GyroCalibrationState::NotCalibrated;
    window_active_ = true;
    sum_[0] = sum_[1] = sum_[2] = 0.0f;
    samples_    = 0;
    continuous_ = false;
    data_rate_  = (data_rate != 0.0f) ? data_rate : 10.0f;
    window_end_ = now + 5.0; // CALIBRATION_TIME
}

void GyroCalibration::accumulate(float x, float y, float z) noexcept
{
    // Continuous background calibration only listens for noise close to the
    // already-known bias; first calibration accepts everything (device may
    // report an offset).
    if (continuous_) {
        float dx = x - bias_[0], dy = y - bias_[1], dz = z - bias_[2];
        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len > 0.1f) { return; }
    }

    sum_[0] += x; sum_[1] += y; sum_[2] += z;
    ++samples_;

    if (!continuous_) { state_ = GyroCalibrationState::Calibrating; }
}

void GyroCalibration::finalize(double now) noexcept
{
    int min_samples = static_cast<int>(5.0f * data_rate_ * 0.5f + 0.5f); // CALIBRATION_TIME * rate * 0.5, rounded

    if (samples_ <= min_samples) {
        if (!continuous_) {
            state_         = GyroCalibrationState::FailedToCalibrate;
            window_active_ = false;
            window_end_    = 0.0;
            return;
        }
        // Continuous re-cal failure: muted, no state change, silently retried.
    } else {
        bias_[0] = sum_[0] / static_cast<float>(samples_);
        bias_[1] = sum_[1] / static_cast<float>(samples_);
        bias_[2] = sum_[2] / static_cast<float>(samples_);
        state_      = GyroCalibrationState::Calibrated;
        continuous_ = true;
    }

    // Self-latching: re-arm another window regardless of outcome once
    // continuous (calibration runs forever in the background, Quirk 7).
    sum_[0] = sum_[1] = sum_[2] = 0.0f;
    samples_    = 0;
    window_end_ = now + 5.0;
}

std::optional<std::array<float, 3>> GyroCalibration::on_sample(double now, float x, float y, float z) noexcept
{
    if (!window_active_) {
        std::array<float, 3> out{ x - bias_[0], y - bias_[1], z - bias_[2] };
        return out;
    }

    if (now > window_end_) {
        finalize(now);
    } else {
        accumulate(x, y, z);
    }

    // Block gyro delivery only during the initial (non-continuous) calibration.
    if (!continuous_) { return std::nullopt; }

    std::array<float, 3> out{ x - bias_[0], y - bias_[1], z - bias_[2] };
    return out;
}

} // namespace xash::input::detail
