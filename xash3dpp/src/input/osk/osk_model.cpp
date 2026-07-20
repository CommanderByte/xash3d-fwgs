// xash3dpp — OskModel implementation
// Legacy reference: engine/client/input/in_osk.c:20-224.

#include <xash3dpp/private/input/osk_model.hpp>

namespace xash::input::detail {

namespace {

// osk_keylayout[2][4] (in_osk.c:64-78) — 7-bit ASCII only; Russian variants
// dead-code-commented-out in legacy, omitted here.
const char *const g_osk_layout[2][4] = {
    {
        "`1234567890-=",
        "qwertyuiop[]\\",
        "\x10" "asdfghjkl;'" "\x12",
        "\x11" "zxcvbnm,./ " "\x13",
    },
    {
        "~!@#$%^&*()_+",
        "QWERTYUIOP{}|",
        "\x10" "ASDFGHJKL:\"" "\x12",
        "\x11" "ZXCVBNM<>? "  "\x13",
    },
};

} // namespace

OskModel::KeyEventOutcome OskModel::key_event(Key key, bool down, bool osk_enable_cvar) noexcept
{
    KeyEventOutcome out{};

    if (!enable_ || !osk_enable_cvar) { return out; }

    if (sending_) {
        sending_ = false;
        return out; // consumed == false — lets the re-injected event pass through
    }

    if (cursor_val_ == 0) {
        if (key == Key::Enter || key == Key::AButton) {
            cursor_val_ = g_osk_layout[curlayout_][cursor_y_][cursor_x_];
            out.consumed = true;
            return out;
        }
        return out;
    }

    switch (key) {
        case Key::AButton:
        case Key::Enter: {
            switch (cursor_val_) {
                case k_osk_enter:
                    sending_ = true;
                    out.reinject_enter = true;
                    out.reinject_enter_down = down;
                    break;
                case k_osk_shift:
                    if (!down) { break; }
                    if (curlayout_ & 1) { curlayout_--; } else { curlayout_++; }
                    shift_ = true;
                    cursor_val_ = g_osk_layout[curlayout_][cursor_y_][cursor_x_];
                    break;
                case k_osk_backspace:
                    out.reinject_backspace = true;
                    out.reinject_backspace_down = down;
                    break;
                case k_osk_tab:
                    out.reinject_tab = true;
                    out.reinject_tab_down = down;
                    break;
                default: {
                    if (!down) {
                        if (shift_ && (curlayout_ & 1)) { curlayout_--; }
                        shift_ = false;
                        cursor_val_ = g_osk_layout[curlayout_][cursor_y_][cursor_x_];
                        break;
                    }
                    // NOTE: cls.accept_utf8 / Con_UtfProcessCharForce folding
                    // is a client/console-owned surface not yet ported
                    // (chunk12); the raw byte is forwarded as char_code.
                    out.char_code = static_cast<unsigned char>(cursor_val_);
                    break;
                }
            }
            break;
        }
        case Key::UpArrow:
            if (down && --cursor_y_ < 0) {
                cursor_y_ = static_cast<signed char>(k_osk_lines - 1);
                cursor_val_ = 0;
                out.consumed = true;
                return out;
            }
            break;
        case Key::DownArrow:
            if (down && ++cursor_y_ >= k_osk_lines) {
                cursor_y_ = 0;
                cursor_val_ = 0;
                out.consumed = true;
                return out;
            }
            break;
        case Key::LeftArrow:
            if (down && --cursor_x_ < 0) { cursor_x_ = static_cast<signed char>(k_osk_rows - 1); }
            break;
        case Key::RightArrow:
            if (down && ++cursor_x_ >= k_osk_rows) { cursor_x_ = 0; }
            break;
        default:
            return out; // consumed == false
    }

    cursor_val_ = g_osk_layout[curlayout_][cursor_y_][cursor_x_];
    out.consumed = true;
    return out;
}

void OskModel::enable_text_input(bool enable, bool force) noexcept
{
    bool old = enable_;
    enable_ = enable;
    if (enable_ && (!old || force)) {
        curlayout_  = 0;
        cursor_val_ = g_osk_layout[curlayout_][cursor_y_][cursor_x_];
    }
}

OskStateDesc OskModel::state() const noexcept
{
    return OskStateDesc{ enable_, curlayout_, shift_, cursor_x_, cursor_y_, cursor_val_ };
}

} // namespace xash::input::detail
