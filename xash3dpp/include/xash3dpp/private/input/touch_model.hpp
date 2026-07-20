#pragma once
// xash3dpp — touch button/grid data model + event routing + gesture state.
// Legacy reference: in_touch.c (EVENT MODEL ONLY — drawing is a Chunk 12
// fence, INP-OQ-2).
//
// @thread-safety: T_Main only.

#include <xash3dpp/input/event.hpp>
#include <xash3dpp/input/key.hpp>
#include <xash3dpp/input/touch.hpp>

#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::input::detail {

// touch_button_t (in_touch.c:48-70), minus the intrusive list links (a
// std::list gives pointer-stable storage without hand-rolled prev/next) and
// minus gl_texturenum (a renderer-owned cache, chunk12 concern).
struct TouchButtonRecord
{
    TouchButtonType  type = TouchButtonType::Command;
    float            x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
    std::uint8_t     color[4] = { 255, 255, 255, 255 };
    std::string      texture;
    std::string      command;
    std::string      name;
    int              finger = -1;
    TouchButtonFlags flags  = TouchButtonFlags::None;
    float            fade = 1.0f;
    float            fadespeed = 0.0f;
    float            fadeend   = 0.0f;
    float            aspect    = 0.0f;
};

// touchdefaultbutton_t (in_touch.c:72-82).
struct TouchDefaultButtonRecord
{
    std::string      name, texture, command;
    float            x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
    std::uint8_t     color[4] = { 255, 255, 255, 255 };
    TouchRoundMode   round = TouchRoundMode::None;
    float            aspect = 0.0f;
    TouchButtonFlags flags  = TouchButtonFlags::None;
};

// Tunables read fresh from cvars by the orchestration layer each call —
// mirrors the JoyTunables decoupling pattern.
struct TouchTunables
{
    bool  enabled = true;      // touch_enable
    bool  in_menu = false;     // touch_in_menu
    bool  grid_enable = true;  // touch_grid_enable
    float grid_count = 50.0f;  // touch_grid_count
    float forwardzone = 0.06f;
    float sidezone    = 0.06f;
    float pitch_sens  = 90.0f;
    float yaw_sens    = 120.0f;
    bool  nonlinear_look = false;
    float pow_factor = 1.3f;
    float pow_mult   = 400.0f;
    float exp_mult   = 0.0f;
    float joy_radius  = 1.0f;
    float dpad_radius = 1.0f;
    float precise_amount = 0.5f;
    float highlight_r = 1.0f, highlight_g = 1.0f, highlight_b = 1.0f, highlight_a = 1.0f;
    float move_indicator = 0.0f;
};

// touchEventType routing outcome — the ordering seam S10.6 tests drive
// (menu-mouse-sim -> VGui forward -> gate -> aspect rescale -> pfnTouchEvent
// hook -> internal fallback).
enum class TouchRouteStage : std::uint8_t
{
    MenuMouseSim,   // simulated menu click; consumed, don't continue
    VGuiForwarded,  // VGui saw it (non-consuming, falls through)
    Disabled,       // not initialized / touch_enable off and not clientonly
    InternalHandled,
};

class TouchModel
{
public:
    TouchModel();

    // ---- Button list management --------------------------------------
    TouchButtonRecord *add_button(std::list<TouchButtonRecord> &list, std::string_view name,
                                   std::string_view texture, std::string_view command,
                                   float x1, float y1, float x2, float y2,
                                   const std::uint8_t color[4], bool privileged) noexcept;
    void remove_button_from_list(std::list<TouchButtonRecord> &list, std::string_view name,
                                  bool privileged) noexcept;
    [[nodiscard]] TouchButtonRecord *find_first(std::list<TouchButtonRecord> &list,
                                                 std::string_view name, bool privileged) noexcept;

    // IN_TouchCheckCoords (in_touch.c:1274-1314): clamp min size -> clamp
    // [0,1] -> optional grid snap (Quirk: clamp-before-snap can push a
    // snapped button slightly off-grid at edges — acknowledged legacy TODO).
    void check_coords(float &x1, float &y1, float &x2, float &y2, const TouchTunables &t) const noexcept;
    [[nodiscard]] float aspect_ratio(const TouchTunables &t, float actual_width, float actual_height) const noexcept;
    void set_config_aspect_ratio(float v) noexcept { config_aspect_ratio_ = v; } // Touch_ConfigAspectRatio_f

    // ---- Client-facing surface -----------------------------------------
    void set_client_only(bool state) noexcept;
    [[nodiscard]] bool client_only() const noexcept { return clientonly_; }
    void hide_buttons(std::string_view name, bool hide, bool privileged) noexcept;
    void remove_button(std::string_view name, bool privileged) noexcept;
    TouchButtonRecord *add_client_button(std::string_view name, std::string_view texture,
                                          std::string_view command, float x1, float y1, float x2, float y2,
                                          const std::uint8_t color[4], TouchRoundMode round, float aspect,
                                          TouchButtonFlags flags, const TouchTunables &t) noexcept;
    void add_default_button(std::string_view name, std::string_view texture, std::string_view command,
                             float x1, float y1, float x2, float y2, const std::uint8_t color[4],
                             TouchRoundMode round, float aspect, TouchButtonFlags flags) noexcept;
    void reset_default_buttons() noexcept;
    void load_defaults(const TouchTunables &t) noexcept;

    // ---- Event routing --------------------------------------------------
    // Touch_ControlsEvent's edit-state precedence (in_touch.c:2063-2078) is
    // orchestrated by the caller (src/input/touch/touch.cpp), which threads
    // command dispatch through edit_hit_test/edit_move/button_press/motion
    // in that exact order.

    // Touch_ButtonPress (in_touch.c:1772-1990): returns true if a button
    // consumed the event; |out_commands| collects the console command
    // lines that must be dispatched (Cbuf_Add[Filtered]Text), tagged with
    // whether the FCMD-restricted (unprivileged/filtered) path applies.
    struct CommandDispatch { std::string text; bool unprivileged; };
    bool button_press(std::list<TouchButtonRecord> &list, TouchEventType type, int finger_id,
                       float x, float y, std::vector<CommandDispatch> &out_commands) noexcept;

    enum class WheelFire : std::uint8_t { None, Up, Down };
    // Touch_Motion (in_touch.c:1665-1770). Returns which wheel command line
    // (if any) crossed threshold this call — the caller dispatches
    // wheel_up_text()/wheel_down_text() through Cbuf_Add[Filtered]Text.
    [[nodiscard]] WheelFire motion(int finger_id, float x, float y, float dx, float dy, const TouchTunables &t) noexcept;
    [[nodiscard]] const std::string &wheel_up_text() const noexcept { return wheel_up_; }
    [[nodiscard]] const std::string &wheel_down_text() const noexcept { return wheel_down_; }
    [[nodiscard]] bool wheel_unprivileged() const noexcept { return wheel_unprivileged_; }

    void get_move(float &forward, float &side, float &pitch, float &yaw) noexcept; // Touch_GetMove

    // in_touch.c:2105 — move/resize/look/wheel finger trackers reset to -1.
    // Fired FIRST by the caller for every non-key_game touch event (D6),
    // before any menu-mouse-sim/console-gesture handling runs.
    void reset_fingers() noexcept;

    // Touch_WantVisibleCursor (in_touch.c:2256-2259): (touch_enable &&
    // touch_emulate) || clientonly || touch_in_menu. All three cvar reads are
    // threaded in by the caller (the model itself has no cvar access, D4).
    [[nodiscard]] bool want_visible_cursor(bool touch_enable, bool touch_emulate, bool touch_in_menu) const noexcept;
    void notify_resize(float refstate_width, float refstate_height) noexcept;

    // Touch_KeyEvent mouse-emulation adapter — returns the synthesized raw
    // touch event parameters for the caller to route through touch_event().
    struct KeyEventResult { bool fire; TouchEventType type; int finger_id; float x, y, dx, dy; };
    [[nodiscard]] KeyEventResult key_event(Key key, bool down, float mouse_x_norm, float mouse_y_norm) noexcept;

    // ---- Editor state machine (state_none/edit/edit_move) ----------------
    [[nodiscard]] TouchEditState edit_state() const noexcept { return state_; }
    void enable_edit() noexcept;
    void disable_edit(bool in_game, bool &out_should_write_config) noexcept;

    // Touch_ButtonEdit's list_user hit-test/select core (in_touch.c:1995-2061).
    // The editor toolbar (list_edit / showeditbuttons grid-cell) is drawing-
    // adjacent chrome — XASH3DPP-STUB(chunk12), omitted; this covers
    // select-into-edit-move + finger-release bookkeeping.
    bool edit_hit_test(TouchEventType type, int finger_id, float x, float y) noexcept;

    // Touch_EditMove (in_touch.c:1612-1663): drag (same finger) or resize
    // (second finger). Returns true when the caller should persist config
    // (mirrors Touch_DisableEdit_f's write-on-leave-via-key_game condition,
    // applied by the caller after a state transition, not from here).
    void edit_move(TouchEventType type, int finger_id, float x, float y, float dx, float dy) noexcept;

    // ---- Gesture accumulator (Quirk 9, modernized as member state) -------
    // The console/message-mode swipe accumulator — was two function-static
    // floats in IN_TouchEvent (in_touch.c:2107-2157, "absolutely horrible").
    struct GestureOutcome
    {
        bool open_text_input = false;  // Key_EnableTextInput(true, true) on release
        bool con_bottom = false;       // Con_Bottom() big-swipe
        int  page_up = 0;              // Con_PageUp(1) repeat-fire count
        int  page_down = 0;            // Con_PageDown(1) repeat-fire count
        bool exit_console = false;     // Key_Console(K_ESCAPE) / Key_Message(K_ESCAPE)
    };
    [[nodiscard]] GestureOutcome gesture_event(TouchEventType type, float x, float y, float dx, float dy,
                                                bool is_console) noexcept;
    void gesture_reset() noexcept;

    // ---- Config text (Compat scope) --------------------------------------
    [[nodiscard]] std::string dump_config_text(std::string_view profile_name, const TouchTunables &t,
                                                float aspect) const noexcept;

    [[nodiscard]] bool configchanged() const noexcept { return configchanged_; }
    void set_configchanged(bool v) noexcept { configchanged_ = v; }
    [[nodiscard]] bool config_loaded() const noexcept { return config_loaded_; }
    void set_config_loaded(bool v) noexcept { config_loaded_ = v; }

    [[nodiscard]] std::list<TouchButtonRecord> &list_user() noexcept { return list_user_; }
    [[nodiscard]] const std::list<TouchButtonRecord> &list_user() const noexcept { return list_user_; }

    void set_precision(bool v) noexcept { precision_ = v; }
    [[nodiscard]] bool precision() const noexcept { return precision_; }

    // ---- Global overstroke state (Touch_SetStroke, in_touch.c:472-482) ----
    // Ported for D5 (dump_config_text's "touch_set_stroke" line): ownerless
    // module-static in legacy (touch.swidth/touch.scolor), member state here.
    void set_stroke(int width, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
    {
        stroke_width_ = width;
        stroke_color_[0] = r; stroke_color_[1] = g; stroke_color_[2] = b; stroke_color_[3] = a;
    }
    [[nodiscard]] int stroke_width() const noexcept { return stroke_width_; }
    [[nodiscard]] const std::uint8_t *stroke_color() const noexcept { return stroke_color_; }

private:
    std::list<TouchButtonRecord> list_user_;
    std::list<TouchButtonRecord> list_edit_; // editor chrome — XASH3DPP-STUB(chunk12), left empty (drawing-adjacent)
    std::vector<TouchDefaultButtonRecord> default_buttons_; // g_DefaultButtons[]

    TouchEditState state_ = TouchEditState::None;

    int look_finger_ = -1, move_finger_ = -1, wheel_finger_ = -1, resize_finger_ = -1;
    TouchButtonRecord *move_button_ = nullptr; // @lifetime: self (borrowed into list_user_; stable across std::list mutation of other elements)
    float move_start_x_ = 0.0f, move_start_y_ = 0.0f;

    float wheel_amount_ = 0.0f;
    std::string wheel_up_, wheel_down_, wheel_end_;
    int  wheel_count_ = 0;
    bool wheel_horizontal_ = false;
    bool wheel_unprivileged_ = false;

    float forward_ = 0.0f, side_ = 0.0f, yaw_ = 0.0f, pitch_ = 0.0f;

    TouchButtonRecord *edit_ = nullptr;      // @lifetime: self (borrowed into list_user_)
    TouchButtonRecord *selection_ = nullptr; // @lifetime: self (borrowed into list_user_)

    bool clientonly_ = false;
    bool precision_ = false;
    bool configchanged_ = false;
    bool config_loaded_ = false;
    float actual_aspect_ratio_ = 0.0f;
    float config_aspect_ratio_ = 0.0f;

    // Touch_Init defaults (in_touch.c:1102-1103): swidth=1, scolor=opaque white.
    int          stroke_width_ = 1;
    std::uint8_t stroke_color_[4] = { 255, 255, 255, 255 };

    // Gesture accumulator (Quirk 9).
    float gesture_x_ = 0.0f;
    float gesture_y_ = 0.0f;

    // Mouse-emulation "named finger" tracking for Touch_KeyEvent.
    float key_event_last_x_ = 0.0f, key_event_last_y_ = 0.0f;
    int   key_event_finger_ = -1;
};

} // namespace xash::input::detail
