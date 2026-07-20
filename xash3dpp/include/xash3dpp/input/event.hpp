#pragma once
// xash3dpp — InputEvent: platform-delivered event stream
// Legacy reference: SDLash_EventHandler's switch (host_sdl2.c) and the
// individual IN_*Event entry points it fans out to (in_keys.c: Key_Event,
// input.c: IN_MouseEvent/IN_MWheelEvent, in_touch.c: IN_TouchEvent,
// in_joy.c: Joy_AxisMotionEvent, in_gyro.c: IN_GyroEvent).
//
// InputEvent is a trivially-copyable, exhaustively-visited std::variant of
// POD payload structs.  IEventSource::poll_events() returns a span of these;
// Input::pump_events() std::visits every element through overloaded lambdas
// with NO default case — adding a new alternative is a compile error at
// every visitor until handled (input-boundary.md S10.1 seam requirement).

#include <xash3dpp/input/key.hpp>

#include <cstdint>
#include <type_traits>
#include <variant>

namespace xash::input {

// event_down / event_up / event_motion — legacy touchEventType (client/input.h:61-66).
enum class TouchEventType : std::uint8_t { Down = 0, Up, Motion };

// A physical/virtual key transition.  Legacy: Key_Event(int key, int down).
struct KeyEvent
{
    Key  key;
    bool down;
};
static_assert(std::is_trivially_copyable_v<KeyEvent>);

// A mouse button transition.  Legacy: IN_MouseEvent(int key, int down) —
// |button| is the 0-based SDL button offset; K_MOUSE1 + button is the Key.
struct MouseButtonEvent
{
    std::uint8_t button; // 0..4
    bool         down;
};
static_assert(std::is_trivially_copyable_v<MouseButtonEvent>);

// Legacy: IN_MWheelEvent(int y) — direction sign only (y > 0 == wheel-up).
struct MouseWheelEvent
{
    int direction;
};
static_assert(std::is_trivially_copyable_v<MouseWheelEvent>);

// Legacy: IN_TouchEvent(touchEventType type, int fingerID, float x, float y,
// float dx, float dy) — mirrors the ABI signature exactly (in_touch.c:2080).
struct TouchRawEvent
{
    TouchEventType type;
    int            finger_id;
    float          x, y;
    float          dx, dy;
};
static_assert(std::is_trivially_copyable_v<TouchRawEvent>);

// Legacy: Joy_AxisMotionEvent(engineAxis_t engineAxis, short value).  |hw_axis|
// is the RAW hardware axis index (0..MAX_AXES-1) BEFORE the joy_axis_binding
// remap — Joy_AxisMotionEvent's own first step is `engineAxis = joyaxesmap[
// engineAxis]` (in_joy.c:278), so the wire-shape parameter is an index, not
// yet the logical axis its legacy type name implies.
struct JoyAxisEvent
{
    std::uint8_t hw_axis;
    std::int16_t value;
};
static_assert(std::is_trivially_copyable_v<JoyAxisEvent>);

// Gamepad-integrated gyro sample (rad/s).  Legacy: Joy_GyroEvent(vec3_t).
struct JoyGyroEvent
{
    float x, y, z;
};
static_assert(std::is_trivially_copyable_v<JoyGyroEvent>);

// Built-in device gyro sample (rad/s).  Legacy: IN_GyroEvent(vec3_t).
struct DeviceGyroEvent
{
    float x, y, z;
};
static_assert(std::is_trivially_copyable_v<DeviceGyroEvent>);

using InputEvent = std::variant<
    KeyEvent,
    MouseButtonEvent,
    MouseWheelEvent,
    TouchRawEvent,
    JoyAxisEvent,
    JoyGyroEvent,
    DeviceGyroEvent
>;
static_assert(std::is_trivially_copyable_v<InputEvent>);

} // namespace xash::input
