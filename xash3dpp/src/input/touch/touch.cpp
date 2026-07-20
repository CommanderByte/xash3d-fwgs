// xash3dpp — IN_TouchEvent routing orchestration, touch command census,
// and the touch/OSK public API forwarders.
// Legacy reference: engine/client/input/in_touch.c (EVENT MODEL ONLY).

#include <xash3dpp/private/input/input_impl.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <cstdlib>

namespace xash::input {

TouchTunables Input::Impl::touch_tunables() const noexcept
{
    TouchTunables t;
    auto v = [](::xash::cmd_cvar::Cvar *c, float def) { return (c != nullptr) ? c->abi.value : def; };
    t.enabled     = v(cv.touch_enable, 1.0f) != 0.0f;
    t.in_menu     = v(cv.touch_in_menu, 0.0f) != 0.0f;
    t.grid_enable = v(cv.touch_grid_enable, 1.0f) != 0.0f;
    t.grid_count  = v(cv.touch_grid_count, 50.0f);
    t.forwardzone = v(cv.touch_forwardzone, 0.06f);
    t.sidezone    = v(cv.touch_sidezone, 0.06f);
    t.pitch_sens  = v(cv.touch_pitch, 90.0f);
    t.yaw_sens    = v(cv.touch_yaw, 120.0f);
    t.nonlinear_look = v(cv.touch_nonlinear_look, 0.0f) != 0.0f;
    t.pow_factor  = v(cv.touch_pow_factor, 1.3f);
    t.pow_mult    = v(cv.touch_pow_mult, 400.0f);
    t.exp_mult    = v(cv.touch_exp_mult, 0.0f);
    t.joy_radius  = v(cv.touch_joy_radius, 1.0f);
    t.dpad_radius = v(cv.touch_dpad_radius, 1.0f);
    t.precise_amount = v(cv.touch_precise_amount, 0.5f);
    t.highlight_r = v(cv.touch_highlight_r, 1.0f);
    t.highlight_g = v(cv.touch_highlight_g, 1.0f);
    t.highlight_b = v(cv.touch_highlight_b, 1.0f);
    t.highlight_a = v(cv.touch_highlight_a, 1.0f);
    t.move_indicator = v(cv.touch_move_indicator, 0.0f);
    return t;
}

void Input::Impl::dispatch_touch_commands(std::vector<detail::TouchModel::CommandDispatch> &cmds) noexcept
{
    if (cvars == nullptr) { return; }
    for (auto &c : cmds) {
        if (c.unprivileged) { cvars->cbuf_stuff_text(c.text); } else { cvars->cbuf_add_text(c.text); }
    }
}

// IN_TouchEvent (in_touch.c:2080-2198).
int Input::Impl::touch_event(TouchEventType type, int finger_id, float x, float y, float dx, float dy) noexcept
{
    // [ref.rotation swap/invert omitted: renderer state not ported in Chunk 10.]

    TouchTunables t = touch_tunables();

    // Step 1: simulate menu mouse click when not key_game (and not touch_in_menu).
    if (key_dest != KeyDest::Game && !t.in_menu) {
        // D6: move/resize/look/wheel finger trackers reset to -1 FIRST
        // (in_touch.c:2105), before the console/message gesture hack below.
        touch.reset_fingers();

        bool is_console = (key_dest == KeyDest::Console);
        bool is_message = (key_dest == KeyDest::Message);
        if (is_console || is_message) {
            auto g = touch.gesture_event(type, x, y, dx, dy, is_console);
            if (g.open_text_input) { enable_text_input(true, true); }
            // [Con_Bottom/Con_PageUp/Con_PageDown omitted: console module not ported.]
            if (g.exit_console) {
                if (is_console) { if (callbacks.con_key_event != nullptr) { callbacks.con_key_event(callbacks.user, Key::Escape); } }
                else { if (callbacks.message_key_event != nullptr) { callbacks.message_key_event(callbacks.user, Key::Escape); } }
                return 0;
            }
        }

        // [UI_MouseMove(x*refState.width, y*refState.height) omitted: needs
        //  refState screen-pixel dimensions, renderer state not ported in
        //  Chunk 10 (in_touch.c:2159) — same gap as Step 4's aspect rescale
        //  below and VGui_MouseMove's omission in Step 2.]
        if (type == TouchEventType::Down) { key_event(Key::Mouse1, true); }
        if (type == TouchEventType::Up) { key_event(Key::Mouse1, false); }
        return 0;
    }

    // Step 2: VGui forwarding — non-consuming, falls through even when active.
    // [VGui_MouseMove(x*refState.width, y*refState.height) omitted: same
    //  refState screen-pixel gap as UI_MouseMove above (in_touch.c:2174).]
    if (callbacks.vgui_is_active != nullptr && callbacks.vgui_is_active(callbacks.user)) {
        if (type == TouchEventType::Down && callbacks.vgui_mouse_event != nullptr) { callbacks.vgui_mouse_event(callbacks.user, Key::Mouse1, true); }
        else if (type == TouchEventType::Up && callbacks.vgui_mouse_event != nullptr) { callbacks.vgui_mouse_event(callbacks.user, Key::Mouse1, false); }
    }

    // Step 3: touch enabled gate.
    if (!t.enabled && !touch.client_only()) { return false; }

    // [Step 4: y-rescale-by-aspect-ratio omitted: needs refState screen
    //  dimensions, renderer state not ported in Chunk 10 — the caller
    //  (Chunk 12 wiring) is expected to pre-scale y if real dims are known.]

    // Step 5: game-DLL pfnTouchEvent hook (can short-circuit).
    if (callbacks.pfn_touch_event != nullptr) {
        int handled = callbacks.pfn_touch_event(callbacks.user, static_cast<int>(type), finger_id, x, y, dx, dy);
        if (handled != 0) { return true; }
    }

    // Step 6: Touch_ControlsEvent precedence — ALWAYS returns true.
    std::vector<detail::TouchModel::CommandDispatch> cmds;

    if (touch.edit_state() == TouchEditState::EditMove) {
        touch.edit_move(type, finger_id, x, y, dx, dy);
    } else if (touch.edit_state() == TouchEditState::Edit && touch.edit_hit_test(type, finger_id, x, y)) {
        // consumed by the editor hit-test
    } else if (touch.button_press(touch.list_user(), type, finger_id, x, y, cmds)) {
        // consumed by a button
    } else if (type == TouchEventType::Motion) {
        auto fire = touch.motion(finger_id, x, y, dx, dy, t);
        if (fire == detail::TouchModel::WheelFire::Up && !touch.wheel_up_text().empty()) {
            cmds.push_back({ touch.wheel_up_text(), touch.wheel_unprivileged() });
        } else if (fire == detail::TouchModel::WheelFire::Down && !touch.wheel_down_text().empty()) {
            cmds.push_back({ touch.wheel_down_text(), touch.wheel_unprivileged() });
        }
    }

    dispatch_touch_commands(cmds);
    return true;
}

// touch_* command registration (~24 commands, 9 unprivileged + 15 restricted
// per direct source count — in_touch.c:1134-1157) + the touch/joy/gyro cvar
// census (S10.2/S10.3/S10.5).
namespace {

void cmd_touch_addbutton(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() < 4) { return; }
    const char *name = self->cvars->cmd_argv(1);
    const char *texture = self->cvars->cmd_argv(2);
    const char *command = self->cvars->cmd_argv(3);
    float x1 = 0.4f, y1 = 0.4f, x2 = 0.6f, y2 = 0.6f;
    std::uint8_t color[4] = { 255, 255, 255, 255 };
    int argc = self->cvars->cmd_argc();
    if (argc > 4) { x1 = static_cast<float>(std::atof(self->cvars->cmd_argv(4))); }
    if (argc > 5) { y1 = static_cast<float>(std::atof(self->cvars->cmd_argv(5))); }
    if (argc > 6) { x2 = static_cast<float>(std::atof(self->cvars->cmd_argv(6))); }
    if (argc > 7) { y2 = static_cast<float>(std::atof(self->cvars->cmd_argv(7))); }
    if (argc > 11) {
        color[0] = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(8)));
        color[1] = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(9)));
        color[2] = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(10)));
        color[3] = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(11)));
    }
    bool privileged = self->cvars->cmd_current_is_privileged();
    TouchButtonRecord *b = self->touch.add_button(self->touch.list_user(), name, texture, command, x1, y1, x2, y2, color, privileged);
    (void)b;
    self->touch.set_configchanged(true);
}

void cmd_touch_removebutton(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2) { return; }
    self->touch.remove_button(self->cvars->cmd_argv(1), self->cvars->cmd_current_is_privileged());
}

void cmd_touch_enableedit(void *user) { static_cast<Input::Impl *>(user)->touch.enable_edit(); }

void cmd_touch_disableedit(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    bool should_write = false;
    self->touch.disable_edit(self->key_dest == KeyDest::Game, should_write);
    if (should_write) { self->touch.set_configchanged(true); }
}

void cmd_touch_settexture(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 3) { return; }
    if (auto *b = self->touch.find_first(self->touch.list_user(), self->cvars->cmd_argv(1), self->cvars->cmd_current_is_privileged())) {
        b->texture = self->cvars->cmd_argv(2);
    }
}

void cmd_touch_setcolor(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 6) { return; }
    std::uint8_t color[4] = {
        static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(2))), static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(3))),
        static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(4))), static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(5))) };
    // Touch_SetColor (in_touch.c:641-647) applies to EVERY matching button
    // (glob pattern support), not just the first.
    for (auto &b : self->touch.list_user()) {
        bool priv = self->cvars->cmd_current_is_privileged();
        if (!priv && !has_flag(b.flags, TouchButtonFlags::Unprivileged)) { continue; }
        if (b.name != self->cvars->cmd_argv(1)) { continue; }
        for (int i = 0; i < 4; ++i) { b.color[i] = color[i]; }
    }
}

void cmd_touch_setcommand(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 3) { return; }
    if (auto *b = self->touch.find_first(self->touch.list_user(), self->cvars->cmd_argv(1), self->cvars->cmd_current_is_privileged())) {
        b->command = self->cvars->cmd_argv(2);
    }
}

void cmd_touch_setflags(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 3) { return; }
    if (auto *b = self->touch.find_first(self->touch.list_user(), self->cvars->cmd_argv(1), self->cvars->cmd_current_is_privileged())) {
        b->flags = static_cast<TouchButtonFlags>(std::atoi(self->cvars->cmd_argv(2)));
    }
}

void cmd_touch_show(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2) { return; }
    self->touch.hide_buttons(self->cvars->cmd_argv(1), false, self->cvars->cmd_current_is_privileged());
}

void cmd_touch_hide(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2) { return; }
    self->touch.hide_buttons(self->cvars->cmd_argv(1), true, self->cvars->cmd_current_is_privileged());
}

void cmd_touch_removeall(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    self->touch.list_user().clear();
}

void cmd_touch_loaddefaults(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    self->touch.load_defaults(self->touch_tunables());
}

void cmd_touch_roundall(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    TouchTunables t = self->touch_tunables();
    if (!t.grid_enable) { return; }
    for (auto &b : self->touch.list_user()) { self->touch.check_coords(b.x1, b.y1, b.x2, b.y2, t); }
}

void cmd_touch_setclientonly(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2) { return; }
    self->touch.set_client_only(std::atoi(self->cvars->cmd_argv(1)) != 0);
}

void cmd_touch_reloadconfig(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    bool dummy = false;
    self->touch.disable_edit(false, dummy);
    bool exists = (self->callbacks.fs_file_exists != nullptr)
                      && (self->cv.touch_config_file != nullptr) && (self->cv.touch_config_file->abi.string != nullptr)
                      && self->callbacks.fs_file_exists(self->callbacks.user, self->cv.touch_config_file->abi.string);
    if (exists && self->cvars != nullptr) {
        std::string exec = std::string("exec \"") + self->cv.touch_config_file->abi.string + "\"\n";
        self->cvars->cbuf_add_text(exec);
    } else {
        self->touch.load_defaults(self->touch_tunables());
        self->touch.set_configchanged(true);
    }
}

void cmd_touch_writeconfig(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->touch.list_user().empty() || !self->touch.configchanged() || !self->touch.config_loaded()) { return; }
    if (self->callbacks.fs_write_file == nullptr) { return; }
    const char *cfg_name = (self->cv.touch_config_file != nullptr && self->cv.touch_config_file->abi.string != nullptr)
                               ? self->cv.touch_config_file->abi.string : "touch.cfg";
    float aspect = self->touch.aspect_ratio(self->touch_tunables(), 0.0f, 0.0f);
    std::string text = self->touch.dump_config_text(cfg_name, self->touch_tunables(), aspect);
    self->callbacks.fs_write_file(self->callbacks.user, cfg_name, text.c_str());
}

void cmd_touch_deleteprofile(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2 || self->callbacks.fs_delete_file == nullptr) { return; }
    self->callbacks.fs_delete_file(self->callbacks.user, self->cvars->cmd_argv(1));
}

void cmd_touch_toggleselection(void *user) { (void)static_cast<Input::Impl *>(user); /* selection is drawing-adjacent chrome, no-op (chunk12) */ }

// Touch_Stroke_f (in_touch.c:472-482) — D5: ports the global overstroke
// state so dump_config_text() can round-trip "touch_set_stroke <w> <r> <g>
// <b> <a>". No console module in Chunk 10 to print the usage line through on
// a bad argc (silent no-op, consistent with every sibling touch_* handler
// here that lacks a Con_Printf surface).
void cmd_touch_set_stroke(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 6) { return; }
    int width = std::atoi(self->cvars->cmd_argv(1));
    auto r = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(2)));
    auto g = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(3)));
    auto b = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(4)));
    auto a = static_cast<std::uint8_t>(std::atoi(self->cvars->cmd_argv(5)));
    self->touch.set_stroke(width, r, g, b, a);
}

void cmd_touch_aspectratio(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2) { return; }
    self->touch.set_config_aspect_ratio(static_cast<float>(std::atof(self->cvars->cmd_argv(1))));
}

} // namespace

void Input::Impl::register_touch_commands() noexcept
{
    if (cvars == nullptr) { return; }

    // 9 unprivileged.
    cvars->cmd_add("touch_addbutton", &cmd_touch_addbutton, this);
    cvars->cmd_add("touch_removebutton", &cmd_touch_removebutton, this);
    cvars->cmd_add("touch_settexture", &cmd_touch_settexture, this);
    cvars->cmd_add("touch_setcolor", &cmd_touch_setcolor, this);
    cvars->cmd_add("touch_setcommand", &cmd_touch_setcommand, this);
    cvars->cmd_add("touch_setflags", &cmd_touch_setflags, this);
    cvars->cmd_add("touch_show", &cmd_touch_show, this);
    cvars->cmd_add("touch_hide", &cmd_touch_hide, this);
    // touch_fade: fade animation state is drawing-adjacent (chunk12); command
    // registered as a structural no-op so the census/privilege split is complete.
    cvars->cmd_add("touch_fade", [](void *) {}, nullptr);

    // 15 restricted.
    constexpr std::uint32_t kPriv = ::xash::cmd_cvar::FCMD_PRIVILEGED;
    cvars->cmd_add("touch_enableedit", &cmd_touch_enableedit, this, kPriv);
    cvars->cmd_add("touch_disableedit", &cmd_touch_disableedit, this, kPriv);
    cvars->cmd_add("touch_list", [](void *) {}, nullptr, kPriv);
    cvars->cmd_add("touch_removeall", &cmd_touch_removeall, this, kPriv);
    cvars->cmd_add("touch_loaddefaults", &cmd_touch_loaddefaults, this, kPriv);
    cvars->cmd_add("touch_roundall", &cmd_touch_roundall, this, kPriv);
    cvars->cmd_add("touch_exportconfig", [](void *) {}, nullptr, kPriv);
    cvars->cmd_add("touch_set_stroke", &cmd_touch_set_stroke, this, kPriv);
    cvars->cmd_add("touch_setclientonly", &cmd_touch_setclientonly, this, kPriv);
    cvars->cmd_add("touch_reloadconfig", &cmd_touch_reloadconfig, this, kPriv);
    cvars->cmd_add("touch_writeconfig", &cmd_touch_writeconfig, this, kPriv);
    cvars->cmd_add("touch_deleteprofile", &cmd_touch_deleteprofile, this, kPriv);
    cvars->cmd_add("touch_generate_code", [](void *) {}, nullptr, kPriv);
    cvars->cmd_add("touch_toggleselection", &cmd_touch_toggleselection, this, kPriv);
    cvars->cmd_add("touch_aspectratio", &cmd_touch_aspectratio, this, kPriv);

    // touch_* cvars (direct-source census: 22, in_touch.c:1160-1187 + touch_enable in input.c).
    cv.touch_in_menu          = cvars->cvar_get_or_create("touch_in_menu", "0", ::xash::cmd_cvar::FCVAR_PRIVILEGED);
    cv.touch_forwardzone      = cvars->cvar_get_or_create("touch_forwardzone", "0.06", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_sidezone         = cvars->cvar_get_or_create("touch_sidezone", "0.06", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_pitch            = cvars->cvar_get_or_create("touch_pitch", "90", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_yaw              = cvars->cvar_get_or_create("touch_yaw", "120", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_nonlinear_look   = cvars->cvar_get_or_create("touch_nonlinear_look", "0", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_pow_factor       = cvars->cvar_get_or_create("touch_pow_factor", "1.3", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_pow_mult         = cvars->cvar_get_or_create("touch_pow_mult", "400.0", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_exp_mult         = cvars->cvar_get_or_create("touch_exp_mult", "0", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_grid_count       = cvars->cvar_get_or_create("touch_grid_count", "50", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_grid_enable      = cvars->cvar_get_or_create("touch_grid_enable", "1", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_config_file      = cvars->cvar_get_or_create("touch_config_file", "touch.cfg", ::xash::cmd_cvar::FCVAR_ARCHIVE | ::xash::cmd_cvar::FCVAR_PRIVILEGED);
    cv.touch_precise_amount   = cvars->cvar_get_or_create("touch_precise_amount", "0.5", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_highlight_r      = cvars->cvar_get_or_create("touch_highlight_r", "1.0", 0);
    cv.touch_highlight_g      = cvars->cvar_get_or_create("touch_highlight_g", "1.0", 0);
    cv.touch_highlight_b      = cvars->cvar_get_or_create("touch_highlight_b", "1.0", 0);
    cv.touch_highlight_a      = cvars->cvar_get_or_create("touch_highlight_a", "1.0", 0);
    cv.touch_dpad_radius      = cvars->cvar_get_or_create("touch_dpad_radius", "1.0", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_joy_radius       = cvars->cvar_get_or_create("touch_joy_radius", "1.0", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_move_indicator   = cvars->cvar_get_or_create("touch_move_indicator", "0.0", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_joy_texture      = cvars->cvar_get_or_create("touch_joy_texture", "touch_default/joy", ::xash::cmd_cvar::FCVAR_FILTERABLE);
    cv.touch_emulate          = cvars->cvar_get_or_create("_touch_emulate", "0", ::xash::cmd_cvar::FCVAR_PRIVILEGED);

    touch.set_config_loaded(true); // Chunk 10 has no host.config_executed gate; treat as always ready
}

// ---------------------------------------------------------------------------
// Input:: public forwarders (touch/OSK)
// ---------------------------------------------------------------------------

int Input::touch_event(TouchEventType type, int finger_id, float x, float y, float dx, float dy) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->stats.touch_events_routed.fetch_add(1, std::memory_order_relaxed);
    return impl_->touch_event(type, finger_id, x, y, dx, dy);
}

void Input::touch_get_move(float &forward, float &side, float &pitch, float &yaw) noexcept
{
    impl_->touch.get_move(forward, side, pitch, yaw);
}

void Input::touch_set_client_only(bool state) noexcept { impl_->touch.set_client_only(state); }

bool Input::touch_want_visible_cursor() const noexcept
{
    bool touch_enable_on  = (impl_->cv.touch_enable  != nullptr) && (impl_->cv.touch_enable->abi.value  != 0.0f);
    bool touch_emulate_on = (impl_->cv.touch_emulate != nullptr) && (impl_->cv.touch_emulate->abi.value != 0.0f);
    bool touch_in_menu_on = (impl_->cv.touch_in_menu != nullptr) && (impl_->cv.touch_in_menu->abi.value != 0.0f);
    return impl_->touch.want_visible_cursor(touch_enable_on, touch_emulate_on, touch_in_menu_on);
}

void Input::touch_key_event(Key key, bool down, float mouse_x_norm, float mouse_y_norm) noexcept
{
    if (!touch_want_visible_cursor()) { return; }
    auto r = impl_->touch.key_event(key, down, mouse_x_norm, mouse_y_norm);
    if (r.fire) { impl_->touch_event(r.type, r.finger_id, r.x, r.y, r.dx, r.dy); }
}

void Input::touch_notify_resize(float refstate_width, float refstate_height) noexcept
{
    impl_->touch.notify_resize(refstate_width, refstate_height);
}

void Input::touch_remove_button(std::string_view name, bool privileged) noexcept { impl_->touch.remove_button(name, privileged); }
void Input::touch_hide_buttons(std::string_view name, bool hide, bool privileged) noexcept { impl_->touch.hide_buttons(name, hide, privileged); }

std::vector<TouchButtonDesc> Input::touch_buttons() const noexcept
{
    std::vector<TouchButtonDesc> out;
    out.reserve(impl_->touch.list_user().size());
    for (const auto &b : impl_->touch.list_user()) {
        TouchButtonDesc d;
        d.name = b.name; d.texture = b.texture; d.command = b.command;
        d.type = b.type;
        d.x1 = b.x1; d.y1 = b.y1; d.x2 = b.x2; d.y2 = b.y2;
        for (int i = 0; i < 4; ++i) { d.color[i] = b.color[i]; }
        d.flags = b.flags;
        d.finger = b.finger;
        d.aspect = b.aspect;
        out.push_back(std::move(d));
    }
    return out;
}

bool Input::osk_key_event(Key key, bool down) noexcept
{
    bool osk_cvar = (impl_->cv.osk_enable != nullptr) ? (impl_->cv.osk_enable->abi.value != 0.0f) : false;
    auto outcome = impl_->osk.key_event(key, down, osk_cvar);
    if (outcome.reinject_enter) { impl_->key_event(Key::Enter, outcome.reinject_enter_down); }
    if (outcome.reinject_backspace) { impl_->key_event(Key::Backspace, outcome.reinject_backspace_down); }
    if (outcome.reinject_tab) { impl_->key_event(Key::Tab, outcome.reinject_tab_down); }
    if (outcome.char_code != 0) { impl_->char_event(outcome.char_code); }
    return outcome.consumed;
}

void Input::osk_enable_text_input(bool enable, bool force) noexcept { impl_->osk.enable_text_input(enable, force); }
OskStateDesc Input::osk_state() const noexcept { return impl_->osk.state(); }

} // namespace xash::input
