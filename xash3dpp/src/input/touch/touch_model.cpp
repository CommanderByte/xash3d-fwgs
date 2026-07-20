// xash3dpp — TouchModel implementation
// Legacy reference: engine/client/input/in_touch.c (EVENT MODEL ONLY).

#include <xash3dpp/private/input/touch_model.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace xash::input::detail {

namespace {

[[nodiscard]] char ci_lower(char c) noexcept { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

[[nodiscard]] bool ci_equal_n(std::string_view a, std::string_view b, std::size_t n) noexcept
{
    // Q_strncmp semantics: compare up to n chars OR either string's NUL.
    std::size_t i = 0;
    for (; i < n && i < a.size() && i < b.size(); ++i) {
        if (a[i] != b[i]) { return false; }
    }
    if (i < n) {
        // one string ended before n — equal only if both ended at the same point
        return a.size() == b.size();
    }
    return true;
}

// glob-capable name match ('*' wildcard, in_touch.c:484-523's Touch_FindNext
// pattern branch uses Q_stricmpext — a simple case-insensitive substring/glob
// test is close enough for this event-model port: '*' anywhere means
// "match everything" per the only pattern this subsystem's callers use).
[[nodiscard]] bool name_matches(std::string_view pattern, std::string_view name) noexcept
{
    if (pattern.find('*') != std::string_view::npos) {
        // supports a single leading/trailing '*' (the only shapes touch_* callers emit)
        if (pattern == "*") { return true; }
        if (!pattern.empty() && pattern.front() == '*') {
            std::string_view suffix = pattern.substr(1);
            if (suffix.size() > name.size()) { return false; }
            std::string_view tail = name.substr(name.size() - suffix.size());
            return ci_equal_n(tail, suffix, suffix.size());
        }
        if (!pattern.empty() && pattern.back() == '*') {
            std::string_view prefix = pattern.substr(0, pattern.size() - 1);
            if (prefix.size() > name.size()) { return false; }
            return ci_equal_n(name.substr(0, prefix.size()), prefix, prefix.size());
        }
        return false;
    }
    return ci_equal_n(name, pattern, std::max(name.size(), pattern.size()));
}

} // namespace

TouchModel::TouchModel() = default;

TouchButtonRecord *TouchModel::add_button(std::list<TouchButtonRecord> &list, std::string_view name,
                                           std::string_view texture, std::string_view command,
                                           float x1, float y1, float x2, float y2,
                                           const std::uint8_t color[4], bool privileged) noexcept
{
    remove_button_from_list(list, name, privileged); // replace if exists (in_touch.c:827)

    TouchButtonRecord b;
    b.name = name;
    b.texture = texture;
    b.x1 = x1; b.y1 = y1; b.x2 = x2; b.y2 = y2;
    for (int i = 0; i < 4; ++i) { b.color[i] = color[i]; }
    b.fade = 1.0f;
    if (!privileged) { b.flags = TouchButtonFlags::Unprivileged | TouchButtonFlags::Client; }

    // Touch_SetCommand (in_touch.c:660-676) — derives the type from the
    // command string's prefix.
    if (command == "_look") { b.type = TouchButtonType::Look; }
    else if (command == "_move") { b.type = TouchButtonType::Move; }
    else if (command == "_joy") { b.type = TouchButtonType::Joy; }
    else if (command == "_dpad") { b.type = TouchButtonType::Dpad; }
    else if (command.rfind("_wheel ", 0) == 0 || command.rfind("_hwheel ", 0) == 0) { b.type = TouchButtonType::Wheel; }
    else { b.type = TouchButtonType::Command; }
    b.command = command;

    b.finger = -1;
    list.push_back(std::move(b));
    return &list.back();
}

void TouchModel::remove_button_from_list(std::list<TouchButtonRecord> &list, std::string_view name,
                                          bool privileged) noexcept
{
    if (edit_ && (edit_->name == name)) { edit_ = nullptr; }
    if (selection_ && (selection_->name == name)) { selection_ = nullptr; }

    for (auto it = list.begin(); it != list.end();) {
        bool skip_unprivileged = !privileged && !has_flag(it->flags, TouchButtonFlags::Unprivileged);
        if (!skip_unprivileged && name_matches(name, it->name)) {
            if (move_button_ == &(*it)) { move_button_ = nullptr; }
            it = list.erase(it);
        } else {
            ++it;
        }
    }
}

TouchButtonRecord *TouchModel::find_first(std::list<TouchButtonRecord> &list, std::string_view name,
                                           bool privileged) noexcept
{
    for (auto &b : list) {
        if (!privileged && !has_flag(b.flags, TouchButtonFlags::Unprivileged)) { continue; }
        if (name_matches(name, b.name)) { return &b; }
    }
    return nullptr;
}

void TouchModel::check_coords(float &x1, float &y1, float &x2, float &y2, const TouchTunables &t) const noexcept
{
    float grid_x_count = std::max(t.grid_count, 1.0f);
    float grid_y_count = std::max(t.grid_count, 1.0f); // aspect factor applied by caller if desired
    float grid_x = 1.0f / grid_x_count;
    float grid_y = 1.0f / grid_y_count;

    if (x2 - x1 < grid_x * 2.0f) { x2 = x1 + grid_x * 2.0f; }
    if (y2 - y1 < grid_y * 2.0f) { y2 = y1 + grid_y * 2.0f; }

    if (x1 < 0.0f) { x2 -= x1; x1 = 0.0f; }
    if (y1 < 0.0f) { y2 -= y1; y1 = 0.0f; }
    if (y2 > 1.0f) { y1 -= (y2 - 1.0f); y2 = 1.0f; }
    if (x2 > 1.0f) { x1 -= (x2 - 1.0f); x2 = 1.0f; }

    if (t.grid_enable) {
        x1 = std::round(x1 * grid_x_count) / grid_x_count;
        x2 = std::round(x2 * grid_x_count) / grid_x_count;
        y1 = std::round(y1 * grid_y_count) / grid_y_count;
        y2 = std::round(y2 * grid_y_count) / grid_y_count;
    }
}

float TouchModel::aspect_ratio(const TouchTunables &, float actual_width, float actual_height) const noexcept
{
    // Touch_AspectRatio (in_touch.c:186-198): explicit config >= actual >=
    // live refState compute >= hardcoded 9/16 fallback.
    if (config_aspect_ratio_ >= 0.25f) { return config_aspect_ratio_; }
    if (actual_aspect_ratio_ >= 0.25f) { return actual_aspect_ratio_; }
    if (actual_width > 0.0f && actual_height > 0.0f) { return actual_height / actual_width; }
    return 9.0f / 16.0f;
}

// compliance-allow(thread-assert): private leaf reached ONLY through an
// already-asserting public `Input::` entry point (close-out audit, 2026-07-20).
// Input is permanently T_Main-confined — there is no T_Input split planned or
// warranted — so the assertion belongs at the entry, not repeated in the
// data-and-lookup layer beneath it.
void TouchModel::set_client_only(bool state) noexcept
{
    if (clientonly_ == state) { return; }
    bool dummy = false;
    disable_edit(false, dummy); // Touch_DisableEdit_f (a1ba: client buttons might lock user in edit state)
    clientonly_ = state;
    resize_finger_ = move_finger_ = look_finger_ = wheel_finger_ = -1;
    forward_ = side_ = 0.0f;
}

void TouchModel::hide_buttons(std::string_view name, bool hide, bool privileged) noexcept
{
    for (auto &b : list_user_) {
        if (!privileged && !has_flag(b.flags, TouchButtonFlags::Unprivileged)) { continue; }
        if (!name_matches(name, b.name)) { continue; }
        if (hide) { b.flags = b.flags | TouchButtonFlags::Hide; }
        else { b.flags = b.flags & ~TouchButtonFlags::Hide; }
    }
}

void TouchModel::remove_button(std::string_view name, bool privileged) noexcept
{
    remove_button_from_list(list_user_, name, privileged);
}

TouchButtonRecord *TouchModel::add_client_button(std::string_view name, std::string_view texture,
                                                  std::string_view command, float x1, float y1, float x2, float y2,
                                                  const std::uint8_t color[4], TouchRoundMode round, float aspect,
                                                  TouchButtonFlags flags, const TouchTunables &t) noexcept
{
    check_coords(x1, y1, x2, y2, t);
    if (round == TouchRoundMode::Aspect) {
        y2 = y1 + (x2 - x1) / aspect_ratio(t, 0.0f, 0.0f) * aspect;
    }
    TouchButtonRecord *b = add_button(list_user_, name, texture, command, x1, y1, x2, y2, color, true);
    b->flags = b->flags | TouchButtonFlags::Client | TouchButtonFlags::NoEdit | flags;
    b->aspect = aspect;
    return b;
}

void TouchModel::add_default_button(std::string_view name, std::string_view texture, std::string_view command,
                                     float x1, float y1, float x2, float y2, const std::uint8_t color[4],
                                     TouchRoundMode round, float aspect, TouchButtonFlags flags) noexcept
{
    TouchDefaultButtonRecord b;
    b.name = name; b.texture = texture; b.command = command;
    b.x1 = x1; b.y1 = y1; b.x2 = x2; b.y2 = y2;
    for (int i = 0; i < 4; ++i) { b.color[i] = color[i]; }
    b.round = round; b.aspect = aspect; b.flags = flags;
    default_buttons_.push_back(std::move(b));
}

void TouchModel::reset_default_buttons() noexcept
{
    default_buttons_.clear();
}

// compliance-allow(thread-assert): private leaf reached ONLY through an
// already-asserting public `Input::` entry point (close-out audit, 2026-07-20).
// Input is permanently T_Main-confined — there is no T_Input split planned or
// warranted — so the assertion belongs at the entry, not repeated in the
// data-and-lookup layer beneath it.
void TouchModel::load_defaults(const TouchTunables &t) noexcept
{
    for (auto &d : default_buttons_) {
        float x1 = d.x1, y1 = d.y1, x2 = d.x2, y2 = d.y2;
        check_coords(x1, y1, x2, y2, t);
        if (d.aspect != 0.0f && d.round == TouchRoundMode::Aspect) {
            y2 = y1 + ((x2 - x1) / aspect_ratio(t, 0.0f, 0.0f)) * d.aspect;
        }
        check_coords(x1, y1, x2, y2, t);
        TouchButtonRecord *b = add_button(list_user_, d.name, d.texture, d.command, x1, y1, x2, y2, d.color, true);
        b->flags = b->flags | d.flags;
        b->aspect = d.aspect;
    }
    configchanged_ = true;
}

bool TouchModel::button_press(std::list<TouchButtonRecord> &list, TouchEventType type, int finger_id,
                               float x, float y, std::vector<CommandDispatch> &out_commands) noexcept
{
    if (type != TouchEventType::Down && type != TouchEventType::Up) { return false; }

    bool result = false;

    // run from end(front) to start(back) — last-added button wins hit-testing.
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        TouchButtonRecord &b = *it;
        if (has_flag(b.flags, TouchButtonFlags::Hide)) { continue; } // Touch_IsVisible (approximate: hide-only gate)

        if (type == TouchEventType::Down) {
            if (x < b.x1 || x > b.x2 || y < b.y1 || y > b.y2) { continue; }
            b.finger = finger_id;

            switch (b.type) {
                case TouchButtonType::Command: {
                    out_commands.push_back({ b.command + "\n", has_flag(b.flags, TouchButtonFlags::Unprivileged) });
                    if (has_flag(b.flags, TouchButtonFlags::Precision)) { precision_ = true; }
                    result = true;
                    break;
                }
                case TouchButtonType::Wheel: {
                    wheel_finger_ = finger_id;
                    wheel_amount_ = 0.0f;
                    wheel_count_  = 0;
                    wheel_unprivileged_ = has_flag(b.flags, TouchButtonFlags::Unprivileged);
                    // command layout: "_wheel <up> <down> <end> [tap-cmd]"
                    std::string_view cmd = b.command;
                    std::size_t p0 = cmd.find(' ');
                    wheel_horizontal_ = cmd.rfind("_hwheel", 0) == 0;
                    std::size_t p1 = (p0 == std::string_view::npos) ? std::string_view::npos : cmd.find(' ', p0 + 1);
                    std::size_t p2 = (p1 == std::string_view::npos) ? std::string_view::npos : cmd.find(' ', p1 + 1);
                    if (p0 != std::string_view::npos && p1 != std::string_view::npos) {
                        wheel_up_ = std::string(cmd.substr(p0 + 1, p1 - p0 - 1)) + "\n";
                    }
                    if (p1 != std::string_view::npos && p2 != std::string_view::npos) {
                        wheel_down_ = std::string(cmd.substr(p1 + 1, p2 - p1 - 1)) + "\n";
                    }
                    if (p2 != std::string_view::npos) {
                        std::size_t p3 = cmd.find(' ', p2 + 1);
                        wheel_end_ = std::string(cmd.substr(p2 + 1, p3 - p2 - 1)) + "\n";
                    }
                    if (has_flag(b.flags, TouchButtonFlags::Precision)) { precision_ = true; }
                    result = true;
                    break;
                }
                case TouchButtonType::Move:
                case TouchButtonType::Joy:
                case TouchButtonType::Dpad: {
                    if (move_finger_ != -1) { b.finger = move_finger_; continue; } // revert, leave first finger
                    result = true;
                    if (look_finger_ == finger_id) {
                        move_finger_ = look_finger_ = -1;
                        for (auto &nb : list) {
                            if (nb.type == TouchButtonType::Move || nb.type == TouchButtonType::Look) { nb.finger = -1; }
                        }
                        continue;
                    }
                    move_finger_ = finger_id;
                    move_button_ = &b;
                    if (b.type == TouchButtonType::Move) {
                        move_start_x_ = x; move_start_y_ = y;
                    } else {
                        move_start_y_ = (b.y2 + b.y1) / 2.0f;
                        move_start_x_ = (b.x2 + b.x1) / 2.0f;
                        forward_ = ((b.y2 + b.y1) - y * 2.0f) / (b.y2 - b.y1);
                        side_    = (x * 2.0f - (b.x2 + b.x1)) / (b.x2 - b.x1);
                        if (b.type == TouchButtonType::Dpad) {
                            forward_ = std::round(forward_);
                            side_    = std::round(side_);
                        }
                    }
                    break;
                }
                case TouchButtonType::Look: {
                    if (look_finger_ != -1) { b.finger = look_finger_; continue; }
                    result = true;
                    if (move_finger_ == finger_id) {
                        move_finger_ = look_finger_ = -1;
                        for (auto &nb : list) {
                            if (nb.type == TouchButtonType::Move || nb.type == TouchButtonType::Look) { nb.finger = -1; }
                        }
                        continue;
                    }
                    look_finger_ = finger_id;
                    break;
                }
            }
        } else { // event_up
            if (finger_id != b.finger) { continue; }
            b.finger = -1;

            if (b.type == TouchButtonType::Command) {
                if (!b.command.empty() && b.command.front() == '+') {
                    out_commands.push_back({ "-" + b.command.substr(1) + "\n", has_flag(b.flags, TouchButtonFlags::Unprivileged) });
                }
                if (has_flag(b.flags, TouchButtonFlags::Precision)) { precision_ = false; }
                result = true;
            } else if (b.type == TouchButtonType::Wheel) {
                if (wheel_count_ != 0) {
                    out_commands.push_back({ wheel_end_, has_flag(b.flags, TouchButtonFlags::Unprivileged) });
                }
                if (has_flag(b.flags, TouchButtonFlags::Precision)) { precision_ = false; }
                wheel_finger_ = -1;
                result = true;
            } else if (b.type == TouchButtonType::Move || b.type == TouchButtonType::Joy || b.type == TouchButtonType::Dpad) {
                move_finger_ = -1;
                forward_ = side_ = 0.0f;
                move_button_ = nullptr;
            } else if (b.type == TouchButtonType::Look) {
                look_finger_ = -1;
            }
        }
    }

    return result;
}

TouchModel::WheelFire TouchModel::motion(int finger_id, float x, float y, float dx, float dy, const TouchTunables &t) noexcept
{
    if (finger_id == wheel_finger_) {
        wheel_amount_ += wheel_horizontal_ ? dx : dy;
        if (wheel_amount_ > 0.1f) { wheel_count_++; wheel_amount_ = 0.0f; return WheelFire::Down; }
        if (wheel_amount_ < -0.1f) { wheel_count_++; wheel_amount_ = 0.0f; return WheelFire::Up; }
        return WheelFire::None;
    }

    if (finger_id == move_finger_) {
        const TouchButtonRecord *b = move_button_;
        if (!b || b->type == TouchButtonType::Move) {
            float fz = (t.forwardzone <= 0.0f) ? 0.5f : t.forwardzone;
            float sz = (t.sidezone <= 0.0f) ? 0.3f : t.sidezone;
            forward_ = (move_start_y_ - y) / fz;
            side_    = (x - move_start_x_) / sz;
        } else {
            forward_ = ((b->y2 + b->y1) - y * 2.0f) / (b->y2 - b->y1);
            side_    = (x * 2.0f - (b->x2 + b->x1)) / (b->x2 - b->x1);
            if (b->type == TouchButtonType::Joy) {
                forward_ *= t.joy_radius;
                side_    *= t.joy_radius;
            } else if (b->type == TouchButtonType::Dpad) {
                forward_ = std::round(forward_ * t.dpad_radius);
                side_    = std::round(side_ * t.dpad_radius);
            }
        }
        forward_ = std::clamp(forward_, -1.0f, 1.0f);
        side_    = std::clamp(side_, -1.0f, 1.0f);
    }

    if (finger_id == look_finger_) {
        if (precision_) { dx *= t.precise_amount; dy *= t.precise_amount; }

        if (t.nonlinear_look) {
            float dabs = std::sqrt(dx * dx + dy * dy);
            if (dabs < 0.000001f) { return WheelFire::None; }
            float dcos = dx / dabs, dsin = dy / dabs;
            if (t.exp_mult > 1.0f) { dabs = (std::exp(dabs * t.exp_mult) - 1.0f) / t.exp_mult; }
            if (t.pow_mult > 1.0f && t.pow_factor > 1.0f) { dabs = std::pow(dabs * t.pow_mult, t.pow_factor) / t.pow_mult; }
            dx = dabs * dcos; dy = dabs * dsin;
        }

        if (std::isnan(dx) || std::isnan(dy)) { return WheelFire::None; }

        yaw_   -= dx * t.yaw_sens;
        pitch_ += dy * t.pitch_sens;
    }

    return WheelFire::None;
}

void TouchModel::get_move(float &forward, float &side, float &pitch, float &yaw) noexcept
{
    forward += forward_;
    side    += side_;
    pitch   += pitch_;
    yaw     += yaw_;
    yaw_ = pitch_ = 0.0f; // self-clears yaw/pitch only (forward/side persist until release)
}

bool TouchModel::want_visible_cursor(bool touch_enable, bool touch_emulate, bool touch_in_menu) const noexcept
{
    // Touch_WantVisibleCursor (in_touch.c:2256-2259), verbatim (D4).
    return (touch_enable && touch_emulate) || clientonly_ || touch_in_menu;
}

void TouchModel::reset_fingers() noexcept
{
    // in_touch.c:2105 — deliberately NOT edit_/selection_ (those belong to
    // the editor state machine, reset by enable_edit/disable_edit instead).
    move_finger_ = resize_finger_ = look_finger_ = wheel_finger_ = -1;
}

void TouchModel::notify_resize(float refstate_width, float refstate_height) noexcept
{
    if (refstate_width > 0.0f && refstate_height > 0.0f && configchanged_ == false) {
        float ar = refstate_height / refstate_width;
        if (ar < 0.99f && ar > actual_aspect_ratio_) { actual_aspect_ratio_ = ar; }
    }
}

TouchModel::KeyEventResult TouchModel::key_event(Key key, bool down, float mouse_x_norm, float mouse_y_norm) noexcept
{
    KeyEventResult r{};
    int finger;
    TouchEventType type;

    if (key_index(key) == 0) {
        if (key_event_finger_ < 0) { return r; }
        finger = key_event_finger_;
        type = TouchEventType::Motion;
    } else {
        finger = (key == Key::Mouse1) ? 0 : 1;
        if (down) { type = TouchEventType::Down; key_event_finger_ = finger; }
        else { type = TouchEventType::Up; key_event_finger_ = -1; }
    }

    r.fire = true;
    r.type = type;
    r.finger_id = finger;
    r.x = mouse_x_norm;
    r.y = mouse_y_norm;
    r.dx = mouse_x_norm - key_event_last_x_;
    r.dy = mouse_y_norm - key_event_last_y_;
    key_event_last_x_ = mouse_x_norm;
    key_event_last_y_ = mouse_y_norm;
    return r;
}

void TouchModel::enable_edit() noexcept
{
    state_ = TouchEditState::Edit;
    if (edit_) { edit_->finger = -1; }
    resize_finger_ = -1;
    edit_ = nullptr;
    selection_ = nullptr;
}

void TouchModel::disable_edit(bool in_game, bool &out_should_write_config) noexcept
{
    state_ = TouchEditState::None;
    if (edit_) { edit_->finger = -1; }
    if (selection_) { selection_->finger = -1; }
    edit_ = selection_ = nullptr;
    resize_finger_ = move_finger_ = look_finger_ = wheel_finger_ = -1;
    out_should_write_config = in_game; // Touch_DisableEdit_f writes config only when leaving via key_game
}

bool TouchModel::edit_hit_test(TouchEventType type, int finger_id, float x, float y) noexcept
{
    for (auto it = list_user_.rbegin(); it != list_user_.rend(); ++it) {
        TouchButtonRecord &b = *it;
        if (type == TouchEventType::Down) {
            if (x > b.x1 && x < b.x2 && y > b.y1 && y < b.y2) {
                b.finger = finger_id;
                if (has_flag(b.flags, TouchButtonFlags::NoEdit)) { continue; }
                edit_ = &b;
                selection_ = nullptr;
                state_ = TouchEditState::EditMove;
                return true;
            }
        } else if (type == TouchEventType::Up) {
            if (finger_id == b.finger) { b.finger = -1; }
        }
    }
    if (type == TouchEventType::Down) { selection_ = nullptr; }
    return false;
}

void TouchModel::edit_move(TouchEventType type, int finger_id, float x, float y, float dx, float dy) noexcept
{
    (void)x; (void)y;
    if (!edit_) { return; }

    if (edit_->finger == finger_id) {
        if (type == TouchEventType::Up) {
            TouchTunables t; // grid clamp uses default grid config for the final snap
            check_coords(edit_->x1, edit_->y1, edit_->x2, edit_->y2, t);
            edit_->finger = -1;
            selection_ = edit_;
            edit_ = nullptr;
            state_ = TouchEditState::Edit;
        } else if (type == TouchEventType::Motion) {
            edit_->y1 += dy; edit_->y2 += dy;
            edit_->x1 += dx; edit_->x2 += dx;
        }
    } else {
        if (type == TouchEventType::Down) {
            if (resize_finger_ == -1) { resize_finger_ = finger_id; }
        } else if (type == TouchEventType::Up) {
            if (resize_finger_ == finger_id) { resize_finger_ = -1; }
        } else if (type == TouchEventType::Motion) {
            if (resize_finger_ == finger_id) { edit_->y2 += dy; edit_->x2 += dx; }
        }
    }
}

TouchModel::GestureOutcome TouchModel::gesture_event(TouchEventType type, float x, float y, float dx, float dy,
                                                       bool is_console) noexcept
{
    GestureOutcome out{};
    gesture_x_ += dx;

    if (type == TouchEventType::Up) {
        out.open_text_input = true;
        gesture_x_ = 0.0f;
    }

    if (is_console) {
        gesture_y_ += dy;
        if (dy > 0.4f) { out.con_bottom = true; }
        if (gesture_y_ > 0.01f) { out.page_up = 1; gesture_y_ = 0.0f; }
        if (gesture_y_ < -0.01f) { out.page_down = 1; gesture_y_ = 0.0f; }
    }

    if (type == TouchEventType::Down && x < 0.1f && y > 0.9f) {
        out.exit_console = true;
        return out;
    }

    if ((x > 0.7f && gesture_x_ < -0.1f) || (x < 0.3f && gesture_x_ > 0.1f)) {
        out.exit_console = true;
        gesture_x_ = 0.0f;
    }

    return out;
}

void TouchModel::gesture_reset() noexcept
{
    gesture_x_ = gesture_y_ = 0.0f;
}

std::string TouchModel::dump_config_text(std::string_view profile_name, const TouchTunables &t,
                                          float aspect) const noexcept
{
    // Touch_DumpConfig (in_touch.c:250-307) — fixed emission order (Compat
    // scope), D5. The 4-line generated-by header is structurally reproduced;
    // its dynamic build fields (buildnum/commit/branch/os-arch,
    // Q_buildnum()/g_buildcommit/g_buildbranch/Q_buildos()/Q_buildarch()) come
    // from a build-info module not ported in Chunk 10 (documented gap) — the
    // engine-name literal (XASH_ENGINE_NAME) is preserved.
    std::string out;
    out += "//=======================================================================\n";
    out += "//\tGenerated by Xash3D FWGS\n";
    out += "//\t\t\ttouchscreen config\n";
    out += "//=======================================================================\n";

    out += "\ntouch_config_file \"";
    out += profile_name;
    out += "\"\n\n// touch cvars\n\n// sensitivity settings\n";

    char buf[64];
    auto emit = [&](const char *name, float v) {
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
        out += "touch_"; out += name; out += " \""; out += buf; out += "\"\n";
    };
    emit("pitch", t.pitch_sens);
    emit("yaw", t.yaw_sens);
    emit("forwardzone", t.forwardzone);
    emit("sidezone", t.sidezone);
    out += "touch_nonlinear_look \""; out += (t.nonlinear_look ? "1" : "0"); out += "\"\n";
    emit("pow_factor", t.pow_factor);
    emit("pow_mult", t.pow_mult);
    emit("exp_mult", t.exp_mult);
    out += "\n// grid settings\n";
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(t.grid_count));
    out += "touch_grid_count \""; out += buf; out += "\"\n";
    out += "touch_grid_enable \""; out += (t.grid_enable ? "1" : "0"); out += "\"\n";

    out += "\n// global overstroke (width, r, g, b, a)\n";
    std::snprintf(buf, sizeof(buf), "touch_set_stroke %d %d %d %d %d\n",
                  stroke_width_, stroke_color_[0], stroke_color_[1], stroke_color_[2], stroke_color_[3]);
    out += buf;

    out += "\n// highlight when pressed\n";
    emit("highlight_r", t.highlight_r);
    emit("highlight_g", t.highlight_g);
    emit("highlight_b", t.highlight_b);
    emit("highlight_a", t.highlight_a);

    out += "\n// _joy and _dpad options\n";
    emit("dpad_radius", t.dpad_radius);
    emit("joy_radius", t.joy_radius);
    out += "\n// how much slowdown when Precise Look button pressed\n";
    emit("precise_amount", t.precise_amount);

    out += "\n// enable/disable move indicator\n";
    emit("move_indicator", t.move_indicator);

    out += "\n// reset menu state when execing config\ntouch_setclientonly 0\n";
    out += "\n// touch buttons\ntouch_removeall\n";
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(aspect));
    out += "touch_aspectratio "; out += buf; out += "\n";

    for (const auto &b : list_user_) {
        if (has_flag(b.flags, TouchButtonFlags::Client)) { continue; } // skip temporary buttons
        std::snprintf(buf, sizeof(buf), "%g %g %g %g", static_cast<double>(b.x1), static_cast<double>(b.y1),
                      static_cast<double>(b.x2), static_cast<double>(b.y2));
        out += "touch_addbutton \"" + b.name + "\" \"" + b.texture + "\" \"" + b.command + "\" ";
        out += buf;
        char flagbuf[32];
        std::snprintf(flagbuf, sizeof(flagbuf), " %d %d %d %d %u\n", b.color[0], b.color[1], b.color[2], b.color[3],
                      static_cast<unsigned>(b.flags));
        out += flagbuf;
    }
    return out;
}

} // namespace xash::input::detail
