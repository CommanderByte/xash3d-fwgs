#pragma once
// xash3dpp — IEventSource: typed events + polled device queries
// Legacy reference: docs/boundaries/input-boundary.md "IEventSource" table
// (R10.2's two-column partition). Backing real SDL2/evdev implementation is
// a Chunk-12 vendoring target; MockEventSource (tests/input/mock_event_source.hpp)
// is the only concrete implementation shipped in Chunk 10.
//
// @thread-safety: T_Main only (ratified thread model — input has no NetIO-
// style split, docs/boundaries/input-boundary.md "Threading"). poll_events()
// must be drained by the same thread before the next call — the returned
// span is only valid until the next mutating call on the same source.

#include <xash3dpp/input/event.hpp>
#include <xash3dpp/input/key.hpp>

#include <span>

namespace xash::input {

// Plain 2-float pair.  Deliberately not utilities::Vec2 — xash3dpp_input is a
// dedicated-linkage leaf lib (Q-23) with no dependency on xash3dpp_utilities.
struct Vec2 { float x = 0.0f; float y = 0.0f; };

// Platform_JoyInit's return value is "count of enumerable devices at init,
// NOT a success bool, NOT opened-controller count" (joy_sdl2.c:371-398,
// in_joy.c:614-618 discards it).
struct JoyInitResult { int device_count = 0; };

// platform_orientation_t — needed by IN_GyroFinalizeMove's landscape axis
// swap/sign-flip (in_gyro.c:97-106). Not itemised in the boundary spec's
// IEventSource table (R10.3's fragment did not enumerate it); added here as
// a device-state poll analogous to key_modifiers() — no window dependency,
// same "device probe" shape. Documented as a resolved spec gap in the S10.3
// lane report.
enum class DisplayOrientation
{
    Unknown,
    Landscape,
    LandscapeFlipped,
    Portrait,
    PortraitFlipped,
};

class IEventSource
{
public:
    virtual ~IEventSource() = default;

    // Drains the platform event queue (Platform_RunEvents / SDL_PollEvent
    // loop, host_sdl2.c:426-436) and returns every InputEvent produced since
    // the previous call.  Invalidated by the next call to poll_events().
    [[nodiscard]] virtual std::span<const InputEvent> poll_events() noexcept = 0;

    // POLLED mouse delta (Platform_MouseMove / SDL_GetRelativeMouseState).
    // Quirk 12: the legacy-path engine move-merge never calls this
    // (IN_CollectInput(..., includeMouse=false), input.c:599-614) — mouse
    // look for that path is entirely client-DLL-internal via the ABI
    // exports.  Only the modern (pfnLookEvent) path reads it.
    [[nodiscard]] virtual Vec2 pointer_delta() noexcept = 0;

    // Platform_GetMousePos — the only other GAME_EXPORT ABI slot besides
    // SetMousePos; refState-scaled by the real backend (Chunk 12 contract).
    [[nodiscard]] virtual Vec2 mouse_pos() const noexcept = 0;

    [[nodiscard]] virtual KeyModifiers key_modifiers() const noexcept = 0;

    // Platform_JoyInit / Platform_JoyShutdown.
    [[nodiscard]] virtual JoyInitResult joy_init() noexcept = 0;
    virtual void joy_shutdown() noexcept = 0;

    // Platform_CalibrateGamepadGyro -> SDLash_RestartCalibration trampoline.
    virtual void calibrate_gamepad_gyro() noexcept = 0;

    // Platform_Vibrate / Platform_Vibrate2.  Negative values randomize
    // rather than disable (joy_sdl2.c:334-352) — that convention belongs to
    // the caller, this seam just forwards the values.
    virtual void vibrate(float left, float right) noexcept = 0;
    virtual void vibrate2(int left, int right) noexcept = 0;

    // Platform_EnableTextInput (SDL_StartTextInput/StopTextInput).
    virtual void enable_text_input(bool enable) noexcept = 0;

    // Evdev alt-backend (Linux raw input) — runs ALONGSIDE SDL, not a
    // replacement (input.c:186,194,562,620). No-op on non-evdev backends.
    virtual void evdev_move(float &yaw, float &pitch) noexcept = 0;
    virtual void evdev_frame() noexcept = 0;
    virtual void evdev_set_grab(bool grab) noexcept = 0; // Evdev_SetGrab, called from IN_ToggleClientMouse

    // R10.2 "A leak" finding: legacy's IN_SetRelativeMouseMode calls raw SDL
    // directly (SDL_SetRelativeMouseMode et al.), bypassing Platform_*
    // entirely (input.c:212-246). The rewrite closes that leak by folding it
    // into this seam instead — not a behavioural deviation, a boundary fix.
    virtual void set_relative_mouse_mode(bool enable) noexcept = 0;

    // Device-orientation probe for IN_GyroFinalizeMove's landscape swap.
    [[nodiscard]] virtual DisplayOrientation display_orientation() const noexcept = 0;
};

} // namespace xash::input
