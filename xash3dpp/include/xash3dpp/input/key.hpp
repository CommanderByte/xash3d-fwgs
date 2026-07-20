#pragma once
// xash3dpp — Key: strong typedef over the legacy keynum space
// Legacy reference: engine/keydefs.h (K_* numeric key codes), client.h (keydest_t)
//
// Key wraps the raw int keynum used to index the keys[] binding table.
// Printable ASCII characters are their own keynum (Key_StringToKeynum:
// "single ascii characters return themselves", in_keys.c:195-196) — only the
// non-ASCII / device keys that engine *logic* branches on by name get a named
// enumerator here.  The full ~130-row name table (keynames[]) is DATA, not
// code, and lives in the private key_table implementation (Compat scope —
// input-boundary.md "Compat scope (Q-12)").
//
// @thread-safety: value type; no shared state.

#include <xash3dpp/limits.hpp>

#include <cstdint>

namespace xash::input {

enum class Key : int
{
    // -- ASCII punctuation referenced by exact-value comparison in routing --
    Backtick   = '`',
    Tilde      = '~',

    // -- keydefs.h named constants that engine logic branches on by name --
    Tab          = 9,     // K_TAB
    Enter        = 13,    // K_ENTER
    Escape       = 27,    // K_ESCAPE
    Space        = 32,    // K_SPACE
    ScrollLock   = 70,    // K_SCROLLLOCK
    Backspace    = 127,   // K_BACKSPACE
    UpArrow      = 128,   // K_UPARROW
    DownArrow    = 129,   // K_DOWNARROW
    LeftArrow    = 130,   // K_LEFTARROW
    RightArrow   = 131,   // K_RIGHTARROW
    Alt          = 132,   // K_ALT
    Ctrl         = 133,   // K_CTRL
    Shift        = 134,   // K_SHIFT
    PgDn         = 149,   // K_PGDN
    PgUp         = 150,   // K_PGUP

    KpPgUp       = 162,   // K_KP_PGUP
    KpPgDn       = 168,   // K_KP_PGDN

    // -- joystick / gamepad (keydefs.h K_JOY1/2 == K_LTRIGGER/RTRIGGER) --
    LTrigger     = 203,   // K_JOY1 / K_LTRIGGER
    RTrigger     = 204,   // K_JOY2 / K_RTRIGGER

    AButton      = 207,   // K_AUX1 / K_A_BUTTON
    StartButton  = 215,   // K_AUX9 / K_START_BUTTON
    DPadUp       = 222,   // K_AUX16 / K_DPAD_UP
    DPadDown     = 223,   // K_AUX17 / K_DPAD_DOWN
    DPadLeft     = 224,   // K_AUX18 / K_DPAD_LEFT
    DPadRight    = 225,   // K_AUX19 / K_DPAD_RIGHT

    // -- mouse (virtual keys synthesized from button index) --
    MWheelDown  = 239,  // K_MWHEELDOWN
    MWheelUp    = 240,  // K_MWHEELUP
    Mouse1      = 241,  // K_MOUSE1
    Mouse2      = 242,
    Mouse3      = 243,
    Mouse4      = 244,
    Mouse5      = 245,  // K_MOUSE5

    Pause        = 255,   // K_PAUSE

    International = 256,  // K_INTERNATIONAL — first of 9 intl slots (256..264)
};

// ARRAYSIZE(keys) in legacy in_keys.c:43 — 265 = ~255 real keys + 9
// international keyboard slots. Mirrors limits::input_key_count exactly
// (static_assert below); kept as a separate int-typed constant because
// limits::input_key_count is std::size_t and this value indexes into Key's
// int-based range checks throughout this header.
inline constexpr int k_key_count = 265;
static_assert(static_cast<std::size_t>(k_key_count) == ::xash::limits::input_key_count,
              "xash::input::k_key_count must track xash::limits::input_key_count");

[[nodiscard]] constexpr int key_index(Key k) noexcept { return static_cast<int>(k); }
[[nodiscard]] constexpr bool key_in_range(Key k) noexcept
{
    return key_index(k) >= 0 && key_index(k) < k_key_count;
}
[[nodiscard]] constexpr Key key_from_index(int i) noexcept { return static_cast<Key>(i); }

// --- static_assert pins against engine/keydefs.h (frozen porting contract) ---
static_assert(key_index(Key::Tab) == 9);
static_assert(key_index(Key::Enter) == 13);
static_assert(key_index(Key::Escape) == 27);
static_assert(key_index(Key::Space) == 32);
static_assert(key_index(Key::ScrollLock) == 70);
static_assert(key_index(Key::Backspace) == 127);
static_assert(key_index(Key::UpArrow) == 128);
static_assert(key_index(Key::DownArrow) == 129);
static_assert(key_index(Key::LeftArrow) == 130);
static_assert(key_index(Key::RightArrow) == 131);
static_assert(key_index(Key::Alt) == 132);
static_assert(key_index(Key::Ctrl) == 133);
static_assert(key_index(Key::Shift) == 134);
static_assert(key_index(Key::PgDn) == 149);
static_assert(key_index(Key::PgUp) == 150);
static_assert(key_index(Key::KpPgUp) == 162);
static_assert(key_index(Key::KpPgDn) == 168);
static_assert(key_index(Key::LTrigger) == 203);
static_assert(key_index(Key::RTrigger) == 204);
static_assert(key_index(Key::AButton) == 207);
static_assert(key_index(Key::StartButton) == 215);
static_assert(key_index(Key::DPadUp) == 222);
static_assert(key_index(Key::DPadDown) == 223);
static_assert(key_index(Key::DPadLeft) == 224);
static_assert(key_index(Key::DPadRight) == 225);
static_assert(key_index(Key::MWheelDown) == 239);
static_assert(key_index(Key::MWheelUp) == 240);
static_assert(key_index(Key::Mouse1) == 241);
static_assert(key_index(Key::Mouse5) == 245);
static_assert(key_index(Key::Pause) == 255);
static_assert(key_index(Key::International) == 256);
static_assert(k_key_count == 265);

// KeyModifiers — bitmask returned by IEventSource::key_modifiers().
// Legacy: Platform_GetKeyModifiers (SDL_GetModState() -> key_modifier_t,
// in_sdl2.c:261-290; R10.2 notes no confirmed input-core call site today).
enum class KeyModifiers : std::uint32_t
{
    None  = 0,
    Shift = 1u << 0,
    Ctrl  = 1u << 1,
    Alt   = 1u << 2,
    Super = 1u << 3,
    Caps  = 1u << 4,
    Num   = 1u << 5,
};
[[nodiscard]] constexpr KeyModifiers operator|(KeyModifiers a, KeyModifiers b) noexcept
{
    return static_cast<KeyModifiers>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
[[nodiscard]] constexpr bool has_modifier(KeyModifiers mask, KeyModifiers bit) noexcept
{
    return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0;
}

// keydest_t (client.h) — routes a key event to the game, console, chat/message
// entry line, or menu.  Owned by the broader client state in legacy (cls.key_dest);
// input-boundary.md "Owned state" treats it as dependency-in for the class design,
// but Chunk 10 has no client subsystem yet to own it, so Input owns it directly
// (single authoritative source until Chunk 12 wiring; door-debt noted in report).
enum class KeyDest
{
    Game,
    Console,
    Message,
    Menu,
};

} // namespace xash::input
