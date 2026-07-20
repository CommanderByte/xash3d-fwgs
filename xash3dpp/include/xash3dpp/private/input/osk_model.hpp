#pragma once
// xash3dpp — on-screen keyboard state machine
// Legacy reference: engine/client/input/in_osk.c.
// Drawing (OSK_Draw/OSK_DrawSymbolButton/OSK_DrawSpecialButton) is a Chunk 12
// fence (INP-OQ-2) — this is the state machine + key-event intercept only.

#include <xash3dpp/input/key.hpp>
#include <xash3dpp/input/touch.hpp>

namespace xash::input::detail {

inline constexpr int k_osk_rows  = 13;
inline constexpr int k_osk_lines = 4;

// OSK_TAB.. — special-key codes stored in the layout table (in_osk.c:56-63).
enum : char
{
    k_osk_tab        = 16,
    k_osk_shift      = 17,
    k_osk_backspace  = 18,
    k_osk_enter      = 19,
};

class OskModel
{
public:
    // OSK_KeyEvent (in_osk.c:94-204). Returns true if the raw key was
    // consumed (caller must swallow it — absolute first refusal in Key_Event).
    // |char_sink| receives decoded characters (CL_CharEvent stand-in, chosen
    // over a callback pointer here since the caller already owns routing);
    // |enter_key|/|backspace_key|/|tab_key| receive re-injected Key_Event
    // calls (osk.sending guard prevents re-entrant self-interception).
    struct KeyEventOutcome
    {
        bool consumed = false;
        bool reinject_enter = false;
        bool reinject_enter_down = false;
        bool reinject_backspace = false;
        bool reinject_backspace_down = false;
        bool reinject_tab = false;
        bool reinject_tab_down = false;
        int  char_code = 0; // nonzero => a CL_CharEvent(char_code) must fire
    };
    [[nodiscard]] KeyEventOutcome key_event(Key key, bool down, bool osk_enable_cvar) noexcept;

    // OSK_EnableTextInput (in_osk.c:213-224).
    void enable_text_input(bool enable, bool force) noexcept;

    [[nodiscard]] OskStateDesc state() const noexcept;
    [[nodiscard]] bool enabled() const noexcept { return enable_; }

private:
    bool enable_  = false;
    int  curlayout_ = 0;
    bool shift_    = false;
    bool sending_  = false; // re-entrancy guard against self-interception
    signed char cursor_x_ = 0, cursor_y_ = 0;
    char cursor_val_ = 0;
};

} // namespace xash::input::detail
