// xash3dpp — Key_Event 13-step routing, Key_ClearStates, CL_CharEvent,
// Key_EnableTextInput.
// Legacy reference: engine/client/input/in_keys.c:709-990.

#include <xash3dpp/private/input/input_impl.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <cstdio>

namespace xash::input {

namespace {
[[nodiscard]] bool is_sep(char c) noexcept { return static_cast<unsigned char>(c) <= ' ' || c == ';'; }
} // namespace

// Key_AddKeyCommands (in_keys.c:595-632). Splits |kb| on runs of whitespace/
// ';'; '+'-prefixed tokens get the keynum appended ("+cmd N" / "-cmd N");
// non-'+' tokens fire down-only. Deviation: legacy's `if (!kb) return;` is a
// NULL-pointer check; an explicitly-unbound ("" binding) key is handled
// identically here via the same empty-string early return — observably
// equivalent (an empty command line, the only thing "" could ever produce,
// is inert either way).
void Input::Impl::dispatch_key_commands(Key key, std::string_view kb, bool down) noexcept
{
    if (kb.empty() || cvars == nullptr) { return; }

    std::size_t i = 0;
    while (i < kb.size()) {
        while (i < kb.size() && is_sep(kb[i])) { ++i; }
        std::size_t start = i;
        while (i < kb.size() && !is_sep(kb[i])) { ++i; }
        if (i == start) { break; }
        std::string_view token = kb.substr(start, i - start);

        if (!token.empty() && token.front() == '+') {
            char cmdbuf[1024];
            if (down) {
                std::snprintf(cmdbuf, sizeof(cmdbuf), "%.*s %d\n", static_cast<int>(token.size()), token.data(), key_index(key));
            } else {
                std::snprintf(cmdbuf, sizeof(cmdbuf), "-%.*s %d\n", static_cast<int>(token.size() - 1), token.data() + 1, key_index(key));
            }
            cvars->cbuf_add_text(cmdbuf);
            stats.commands_dispatched.fetch_add(1, std::memory_order_relaxed);
        } else if (down) {
            cvars->cbuf_add_text(token);
            cvars->cbuf_add_text("\n");
            stats.commands_dispatched.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// Key_Event's 13-step routing order (Quirk 1), verbatim.
void Input::Impl::key_event(Key key, bool down) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);

    // Step 1: Key_Rotate.
    float rotate_value = (cv.key_rotate != nullptr) ? cv.key_rotate->abi.value : 0.0f;
    key = keys.rotate(key, rotate_value);
    if (!key_in_range(key)) { return; }

    // Step 2: OSK absolute first refusal — before keys[key].down is even latched.
    bool osk_cvar = (cv.osk_enable != nullptr) ? (cv.osk_enable->abi.value != 0.0f) : false;
    auto osk_outcome = osk.key_event(key, down, osk_cvar);
    if (osk_outcome.consumed) {
        if (osk_outcome.reinject_enter) { key_event(Key::Enter, osk_outcome.reinject_enter_down); }
        if (osk_outcome.reinject_backspace) { key_event(Key::Backspace, osk_outcome.reinject_backspace_down); }
        if (osk_outcome.reinject_tab) { key_event(Key::Tab, osk_outcome.reinject_tab_down); }
        if (osk_outcome.char_code != 0) { char_event(osk_outcome.char_code); }
        return;
    }

    auto &rec = keys.record(key);

    // Step 3: stale key-up guard — key was pressed before engine was run.
    if (!rec.down && !down) { return; }

    // [HACKS_RELATED_HLMODS cinematic filter intentionally omitted: needs
    //  cls.state == ca_cinematic, a client-subsystem field not ported in
    //  Chunk 10 — compile-time-optional legacy hack, not default behaviour.]

    std::string kb(rec.binding); // snapshot BEFORE the down-state write (in_keys.c:718-723)
    rec.down = down;

    // Step 5: client-DLL first refusal (down || gamedown; up still routes if
    // the DLL claimed the matching down). Inverted-sense return (0 == handled).
    if (key_dest == KeyDest::Game && (down || rec.gamedown)) {
        if (callbacks.pfn_key_event != nullptr) {
            int handled_falsy = callbacks.pfn_key_event(callbacks.user, down, key, kb.c_str());
            if (handled_falsy == 0) {
                if (rec.repeats == 0 && down) { rec.gamedown = true; }
                if (!down) { rec.gamedown = false; rec.repeats = 0; }
                return; // "handled in client.dll"
            }
        }
    }

    // Step 6: autorepeat accounting/suppression.
    bool allow_autorepeat = (key_dest != KeyDest::Game);
    if (!allow_autorepeat) {
        switch (key) {
            case Key::Backspace: case Key::Pause:
            case Key::PgUp: case Key::KpPgUp:
            case Key::PgDn: case Key::KpPgDn:
                allow_autorepeat = true; break;
            default: break;
        }
    }

    if (down) {
        rec.repeats++;
        if (!allow_autorepeat && rec.repeats > 1) {
#if XASH_STATS
            stats.autorepeats_suppressed.fetch_add(1, std::memory_order_relaxed);
#endif
            return; // ignore most autorepeats
        }

        // Step 7: unbound-key console warning (keynum >= 200, down only).
        // Quirk (D2): legacy's `!kb` is a NULL-binding-POINTER check, true
        // only for keys with NO keynames[] row (never seeded by Key_Init /
        // never bound) — not for a key that was assigned an empty-string
        // binding (e.g. any keynames[] row whose default is ""). `rec.present`
        // reproduces that distinction; a plain `kb.empty()` check would fire
        // for every present-but-empty binding too, which is wrong.
        if (key_index(key) >= 200 && !rec.present) {
#if XASH_STATS
            stats.unbound_key_warnings.fetch_add(1, std::memory_order_relaxed);
#endif
            std::string name = keys.keynum_to_string(key);
            ::xash::core::logf(::xash::core::LogLevel::Info, "input", "%s is unbound.", name.c_str());
        }
    } else {
        rec.gamedown = false;
        rec.repeats  = 0;
    }

    // Step 8: unconditional VGui_KeyEvent.
    if (callbacks.vgui_key_event != nullptr) { callbacks.vgui_key_event(callbacks.user, key, down); }

    // Step 9: console-key hardcode (`/~), never reaches Key_AddKeyCommands.
    if (key == Key::Backtick || key == Key::Tilde) {
        if (key_dest == KeyDest::Message || !down) { return; }
        if (callbacks.con_toggle_console != nullptr) { callbacks.con_toggle_console(callbacks.user); }
        return;
    }

    // Step 10: ESC special-case, only inside key_game.
    // [r_showtextures texture-atlas-close branch omitted: needs a renderer
    //  debug cvar not ported in Chunk 10 — mouse_visible branch preserved.]
    // [cls.state != ca_cinematic sub-condition omitted: cls.state is
    //  client-connection state not ported in Chunk 10 (same class of gap as
    //  check_mouse_state's ca_active stand-in) — always takes the permissive
    //  (non-cinematic) branch, i.e. `mouse_visible` alone gates it here
    //  instead of `mouse_visible && cls.state != ca_cinematic`.]
    if (key == Key::Escape && down && key_dest == KeyDest::Game) {
        if (mouse_visible) {
            if (callbacks.pfn_key_event != nullptr) { (void)callbacks.pfn_key_event(callbacks.user, down, key, kb.c_str()); }
            return; // "handled in client.dll"
        }
        // else falls through to the generic dispatch below.
    }

    // Step 11: menu-dest char synthesis + UI_KeyEvent.
    if (key_dest == KeyDest::Menu) {
        // Classic Xash3D menus don't have an extension telling the engine
        // whether they want text input — enable it unconditionally
        // (gameui.use_extended_api is a menu-DLL-loaded flag not ported in
        // Chunk 10; this always takes the "classic" branch, which is the
        // conservative/safe default with no menu DLL wired).
        enable_text_input(true, false);

        if (!textmode && down && key_index(key) >= 32 && key_index(key) <= 'z') {
            int ch = key_index(key);
            if (keys.is_down(Key::Shift)) { ch += 'A' - 'a'; }
            if (callbacks.ui_char_event != nullptr) { callbacks.ui_char_event(callbacks.user, ch); }
        }

        if (callbacks.ui_key_event != nullptr) { callbacks.ui_key_event(callbacks.user, key, down); }
        return;
    }

    // Step 12: key-up-only short-circuit — only Key_AddKeyCommands runs,
    // regardless of key_dest (keeps a +action alive across a mode switch).
    if (!down) {
        dispatch_key_commands(key, kb, down);
        return;
    }

    // Step 13: final key-down dispatch by key_dest.
    if (key_dest == KeyDest::Game) {
        dispatch_key_commands(key, kb, down);
    } else if (key_dest == KeyDest::Console) {
        if (callbacks.con_key_event != nullptr) { callbacks.con_key_event(callbacks.user, key); }
    } else if (key_dest == KeyDest::Message) {
        if (callbacks.message_key_event != nullptr) { callbacks.message_key_event(callbacks.user, key); }
    }
}

// IN_MouseEvent (input.c:366-396).
void Input::Impl::mouse_button_event(int button, bool down) noexcept
{
    if (down) { in_mstate |= (1u << button); } else { in_mstate &= ~static_cast<unsigned>(1u << button); }

    Key key = key_from_index(key_index(Key::Mouse1) + button);
    bool touch_enable_on  = (cv.touch_enable  != nullptr) && (cv.touch_enable->abi.value  != 0.0f);
    bool touch_emulate_on = (cv.touch_emulate != nullptr) && (cv.touch_emulate->abi.value != 0.0f);
    bool touch_in_menu_on = (cv.touch_in_menu != nullptr) && (cv.touch_in_menu->abi.value != 0.0f);

    if (touch.want_visible_cursor(touch_enable_on, touch_emulate_on, touch_in_menu_on)) {
        // "touch emulation overrides all input" (input.c:376-379): Touch_KeyEvent
        // needs a normalized mouse position, which requires screen-pixel
        // dimensions (refState.width/height) — renderer-owned state that does
        // not exist in Chunk 10. Callers that want the mouse-emulates-touch
        // path wired end-to-end call Input::touch_key_event() directly with
        // caller-supplied normalized coordinates (Chunk 12 wiring); this ABI-
        // shaped IN_MouseEvent entry point only preserves the *gate* (real
        // mouse clicks are suppressed from VGui/client-DLL/Key_Event while
        // touch emulation owns input), consistent with the legacy comment.
    } else if (key_dest == KeyDest::Game) {
        if (callbacks.vgui_mouse_event != nullptr) { callbacks.vgui_mouse_event(callbacks.user, key, down); }
        // client-DLL IN_MouseEvent (cldll_func_t slot): not vendored in Chunk 10.
    } else {
        key_event(key, down);
    }
}

// Key_ClearStates (Quirk 5): skipped entirely during changelevel; else
// replays releases through the real event path before force-zeroing.
// compliance-allow(thread-assert): every mutation is delegated to key_event /
// mouse_button_event, which assert on the Main role themselves
void Input::Impl::clear_states() noexcept
{
    if (changelevel) { return; }

    for (int i = 0; i < k_key_count; ++i) {
        Key k = key_from_index(i);
        if (i >= key_index(Key::Mouse1) && i <= key_index(Key::Mouse5)) {
            mouse_button_event(i - key_index(Key::Mouse1), false);
        } else {
            key_event(k, false);
        }
        keys.zero_fields(k);
    }

    if (callbacks.in_clear_states != nullptr) { callbacks.in_clear_states(callbacks.user); }
}

// CL_CharEvent (in_keys.c:949-969).
void Input::Impl::char_event(int ch) noexcept
{
    if (ch == '`' || ch == '~') { return; }

    if (key_dest == KeyDest::Console && callbacks.con_visible != nullptr && !callbacks.con_visible(callbacks.user)) {
        if (static_cast<char>(ch) == '`' || static_cast<char>(ch) == '?') { return; }
    }

    if (key_dest == KeyDest::Console || key_dest == KeyDest::Message) {
        if (callbacks.con_char_event != nullptr) { callbacks.con_char_event(callbacks.user, ch); }
    } else if (key_dest == KeyDest::Menu) {
        if (callbacks.ui_char_event != nullptr) { callbacks.ui_char_event(callbacks.user, ch); }
    }
}

// Key_EnableTextInput (in_keys.c:863-876).
void Input::Impl::enable_text_input(bool enable, bool force) noexcept
{
    bool osk_cvar = (cv.osk_enable != nullptr) ? (cv.osk_enable->abi.value != 0.0f) : false;
    if (osk_cvar) {
        osk.enable_text_input(enable, force);
        return;
    }
    if (enable && (!textmode || force)) {
        if (source != nullptr) { source->enable_text_input(true); }
    } else if (!enable && (textmode || force)) {
        if (source != nullptr) { source->enable_text_input(false); }
    }
    textmode = enable;
}

} // namespace xash::input
