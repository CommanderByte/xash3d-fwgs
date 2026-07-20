#pragma once
// xash3dpp — MockEventSource: the S10.1 IEventSource test fixture.
// Per the ratified mock-event-source decision (FACTBASE.md — "mock event
// source, no window"): a synthetic-event injection path for tests (and, per
// extension-goals G-5/G-1, a future scripting/MCP driver) that feeds events
// through the exact same Key_Event/IN_TouchEvent dispatch path a real SDL
// backend would use.
//
// @thread-safety: T_Main only, same contract as IEventSource itself — tests
// must call push_event()/poll_events() from the thread that registered
// ThreadRole::Main.

#include <xash3dpp/input/event_source.hpp>

#include <vector>

namespace xash::input::test {

class MockEventSource final : public IEventSource
{
public:
    // Queue an event for the next poll_events() call to return.
    void push_event(const InputEvent &ev) { pending_.push_back(ev); }

    [[nodiscard]] std::span<const InputEvent> poll_events() noexcept override
    {
        drained_ = std::move(pending_);
        pending_.clear();
        return drained_;
    }

    // Test knobs — set the value the next call should observe.
    void set_pointer_delta(Vec2 d) noexcept { pointer_delta_ = d; }
    void set_mouse_pos(Vec2 p) noexcept { mouse_pos_ = p; }
    void set_key_modifiers(KeyModifiers m) noexcept { modifiers_ = m; }
    void set_display_orientation(DisplayOrientation o) noexcept { orientation_ = o; }

    [[nodiscard]] Vec2 pointer_delta() noexcept override { return pointer_delta_; }
    [[nodiscard]] Vec2 mouse_pos() const noexcept override { return mouse_pos_; }
    [[nodiscard]] KeyModifiers key_modifiers() const noexcept override { return modifiers_; }

    [[nodiscard]] JoyInitResult joy_init() noexcept override { ++joy_init_calls; return { joy_device_count }; }
    void joy_shutdown() noexcept override { ++joy_shutdown_calls; }

    void calibrate_gamepad_gyro() noexcept override { ++calibrate_calls; }

    void vibrate(float left, float right) noexcept override { last_vibrate_left = left; last_vibrate_right = right; }
    void vibrate2(int left, int right) noexcept override { last_vibrate2_left = left; last_vibrate2_right = right; }

    void enable_text_input(bool enable) noexcept override { text_input_enabled = enable; }

    void evdev_move(float &, float &) noexcept override { ++evdev_move_calls; }
    void evdev_frame() noexcept override { ++evdev_frame_calls; }
    void evdev_set_grab(bool grab) noexcept override { evdev_grab = grab; }

    void set_relative_mouse_mode(bool enable) noexcept override { relative_mouse_mode = enable; }

    [[nodiscard]] DisplayOrientation display_orientation() const noexcept override { return orientation_; }

    // Observable call counters / last-seen values for assertions.
    int  joy_init_calls = 0;
    int  joy_shutdown_calls = 0;
    int  calibrate_calls = 0;
    float last_vibrate_left = 0.0f, last_vibrate_right = 0.0f;
    int  last_vibrate2_left = 0, last_vibrate2_right = 0;
    bool text_input_enabled = false;
    int  evdev_move_calls = 0;
    int  evdev_frame_calls = 0;
    bool evdev_grab = false;
    bool relative_mouse_mode = false;
    int  joy_device_count = 0;

private:
    std::vector<InputEvent> pending_;
    std::vector<InputEvent> drained_;
    Vec2 pointer_delta_{};
    Vec2 mouse_pos_{};
    KeyModifiers modifiers_ = KeyModifiers::None;
    DisplayOrientation orientation_ = DisplayOrientation::Unknown;
};

} // namespace xash::input::test
