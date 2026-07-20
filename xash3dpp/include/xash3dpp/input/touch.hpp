#pragma once
// xash3dpp — touch/OSK public types
// Legacy reference: in_touch.c, in_osk.c, engine/mobility_int.h (TOUCH_FL_*).
//
// Drawing (ref.dllFuncs.*, Con_DrawString, CL_DrawCharacter) is fenced to
// Chunk 12 (INP-OQ-2) — everything here is the data model: button/layout
// state, iterable read-only, XASH3DPP-STUB(chunk12) for the actual draw calls.

#include <cstdint>
#include <string>

namespace xash::input {

// touchButtonType (in_touch.c:24-32) — derived from the command string's
// prefix ("_look"/"_move"/"_joy"/"_dpad"/"_wheel "/"_hwheel "), default
// touch_command.
enum class TouchButtonType : std::uint8_t
{
    Command = 0,
    Move,
    Joy,
    Dpad,
    Look,
    Wheel,
};

// touchState (in_touch.c:34-39).
enum class TouchEditState : std::uint8_t
{
    None = 0,
    Edit,
    EditMove,
};

// touchRound (in_touch.c:41-46).
enum class TouchRoundMode : std::uint8_t
{
    None = 0,
    Grid,
    Aspect,
};

// TOUCH_FL_* (engine/mobility_int.h:43-52) + the in_touch.c-local
// TOUCH_FL_UNPRIVILEGED (in_touch.c:140, bit 10 verbatim — private to the
// engine, not part of the client-facing mobility_int.h set).
enum class TouchButtonFlags : std::uint32_t
{
    None          = 0,
    Hide          = 1u << 0,  // TOUCH_FL_HIDE
    NoEdit        = 1u << 1,  // TOUCH_FL_NOEDIT
    Client        = 1u << 2,  // TOUCH_FL_CLIENT
    Mp            = 1u << 3,  // TOUCH_FL_MP
    Sp            = 1u << 4,  // TOUCH_FL_SP
    DefShow       = 1u << 5,  // TOUCH_FL_DEF_SHOW
    DefHide       = 1u << 6,  // TOUCH_FL_DEF_HIDE
    DrawAdditive  = 1u << 7,  // TOUCH_FL_DRAW_ADDITIVE
    Stroke        = 1u << 8,  // TOUCH_FL_STROKE
    Precision     = 1u << 9,  // TOUCH_FL_PRECISION
    Unprivileged  = 1u << 10, // TOUCH_FL_UNPRIVILEGED (in_touch.c:140)
};
[[nodiscard]] constexpr TouchButtonFlags operator|(TouchButtonFlags a, TouchButtonFlags b) noexcept
{
    return static_cast<TouchButtonFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
[[nodiscard]] constexpr TouchButtonFlags operator&(TouchButtonFlags a, TouchButtonFlags b) noexcept
{
    return static_cast<TouchButtonFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
[[nodiscard]] constexpr TouchButtonFlags operator~(TouchButtonFlags a) noexcept
{
    return static_cast<TouchButtonFlags>(~static_cast<std::uint32_t>(a));
}
[[nodiscard]] constexpr bool has_flag(TouchButtonFlags mask, TouchButtonFlags bit) noexcept
{
    return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0;
}

// Read-only snapshot of one touch button — P-4 iteration surface
// (INP-OQ-2: input owns the data model, Chunk 12 owns the draw calls).
// Mirrors touch_button_t's fields minus the intrusive list links and the
// gl_texturenum cache (a renderer-owned handle, chunk12 concern).
struct TouchButtonDesc
{
    std::string      name;
    std::string      texture;
    std::string      command;
    TouchButtonType  type = TouchButtonType::Command;
    float            x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
    std::uint8_t     color[4] = { 255, 255, 255, 255 };
    TouchButtonFlags flags = TouchButtonFlags::None;
    int              finger = -1;
    float            aspect = 0.0f;
};

// osk_s (in_osk.c:80-92) snapshot — P-4 introspection for a future OSK overlay.
struct OskStateDesc
{
    bool enabled     = false;
    int  layout      = 0;
    bool shift       = false;
    int  cursor_x    = 0;
    int  cursor_y    = 0;
    char cursor_val  = 0;
};

} // namespace xash::input
