#pragma once
// xash3dpp — Input: keyboard/mouse/gamepad/gyro/touch/OSK engine core (Chunk 10)
// Legacy reference: engine/client/input/{input.c,in_keys.c,in_joy.c,in_gyro.c,
// in_touch.c,in_osk.c}.  Boundary spec: docs/boundaries/input-boundary.md.
//
// Q-22 pool-owned lifecycle: Input is constructed via create_input() into a
// caller-supplied memory pool (canonical shape — see File/ISearchBackend).
// xash3dpp_input is a dedicated-linkage LEAF static lib (Q-23): it depends on
// xash3dpp_cmd_cvar (command/cvar registration) and xash3dpp_memory only — no
// host/server/launcher edge, no EngineContext member (host wiring is Chunk 12).
//
// @thread-safety: T_Main only, full stop (no NetIO-style split exists or is
// planned for this subsystem — boundary spec "Threading"). Every mutating
// entry point asserts ThreadRole::Main.

#include <xash3dpp/input/bindings.hpp>
#include <xash3dpp/input/event.hpp>
#include <xash3dpp/input/event_source.hpp>
#include <xash3dpp/input/joy.hpp>
#include <xash3dpp/input/key.hpp>
#include <xash3dpp/input/stats.hpp>
#include <xash3dpp/input/touch.hpp>
#include <xash3dpp/input/window_controls.hpp>

#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::cmd_cvar { class CmdCvarContext; }

namespace xash::input {

// ---------------------------------------------------------------------------
// MoveCmd — minimal projection of the ABI usercmd_t fields IN_EngineAppendMove
// touches (viewangles, forwardmove, sidemove).  Not the vendored ABI struct
// itself (that lives in xash3dpp_abi, a non-dependency of this leaf lib,
// Q-23) — Chunk 12 bridges this view onto the real usercmd_t at the host seam.
// ---------------------------------------------------------------------------

struct MoveCmd
{
    float viewangles[3] = { 0.0f, 0.0f, 0.0f }; // [PITCH]=0 [YAW]=1 [ROLL]=2, matches legacy indexing
    float forwardmove = 0.0f;
    float sidemove    = 0.0f;
};
inline constexpr int k_move_pitch = 0;
inline constexpr int k_move_yaw   = 1;

// ---------------------------------------------------------------------------
// InputCallbacks — injectable seams standing in for the client-DLL/menu-DLL/
// VGUI/console ABI slots documented in the boundary spec's "External ABI
// contracts" section.  None of those ABIs are vendored in Chunk 10 (host
// wiring is Chunk 12) — every member here defaults to nullptr, in which case
// Input behaves as if no client/menu DLL / VGUI / console is present (routing
// continues past the corresponding first-refusal step, matching an
// unloaded-DLL legacy state). This is the seam S10.6's ordering tests drive.
// ---------------------------------------------------------------------------

struct InputCallbacks
{
    void *user = nullptr; // @lifetime: caller-owned; passed back to every callback below

    // pfnKey_Event first-refusal (cldll_func_t slot, in_keys.c:736-750).
    // Returning 0 (falsy) means "handled by the client DLL" (inverted sense,
    // preserved verbatim). nullptr => never claims the event.
    int (*pfn_key_event)(void *user, bool down, Key key, const char *binding) = nullptr;

    // IN_ClearStates (cldll_func_t slot) — Key_ClearStates' tail call,
    // gated `if (clgame.hInstance)` in legacy (in_keys.c:938-939).
    void (*in_clear_states)(void *user) = nullptr;

    // IN_ActivateMouse / IN_DeactivateMouse (cldll_func_t slots, optional —
    // both legacy call sites are themselves null-checked).
    void (*in_activate_mouse)(void *user)   = nullptr;
    void (*in_deactivate_mouse)(void *user) = nullptr;

    // VGui_* — fire unconditionally alongside key/mouse/touch routing
    // (in_keys.c step 8; in_touch.c:2172-2187).
    void (*vgui_key_event)(void *user, Key key, bool down)     = nullptr;
    void (*vgui_mouse_event)(void *user, Key key, bool down)   = nullptr;
    void (*vgui_mouse_move)(void *user, int x, int y)          = nullptr;
    void (*vgui_mwheel_event)(void *user, int y)                = nullptr;
    bool (*vgui_is_active)(void *user)                          = nullptr;

    // Menu (UI) ABI slots (menu_int.h UI_FUNCTIONS).
    void (*ui_key_event)(void *user, Key key, bool down) = nullptr;
    void (*ui_char_event)(void *user, int ch)             = nullptr;
    void (*ui_mouse_move)(void *user, int x, int y)       = nullptr;

    // Console.
    void (*con_char_event)(void *user, int ch)     = nullptr;
    void (*con_toggle_console)(void *user)          = nullptr;
    bool (*con_visible)(void *user)                 = nullptr;

    // Key_Console(key) / Key_Message(key) — raw console/chat line-editing
    // input handlers. Not defined in in_keys.c itself (extern to that file);
    // the actual line editor is a later client/console chunk. Stand-in seams
    // so the key_dest final-dispatch step (Quirk 1 step 13) stays structurally
    // complete and testable without a real console module.
    void (*con_key_event)(void *user, Key key)     = nullptr;
    void (*message_key_event)(void *user, Key key) = nullptr;

    // FWGS client-DLL extensions: pfnTouchEvent / pfnMoveEvent / pfnLookEvent.
    // look_event's mere presence is the legacy "modern path" opt-out switch
    // for IN_EngineAppendMove (input.c:587-591 — the Dependencies-section
    // "pfnLookEvent bypass seam").
    int  (*pfn_touch_event)(void *user, int type, int finger_id,
                             float x, float y, float dx, float dy) = nullptr;
    void (*move_event)(void *user, float forward, float side)      = nullptr;
    void (*look_event)(void *user, float relyaw, float relpitch)   = nullptr;

    // Touch profile persistence (touch.cfg) — the write-new/rotate-backup
    // file dance and FS_LoadFile/FS_FileExists probing are filesystem's I/O
    // surface (sibling-owned, input-boundary.md "Dependencies"); these
    // optional hooks are the seam. nullptr => the command-script TEXT is
    // still generated (the compat contract), just not persisted to disk.
    bool (*fs_write_file)(void *user, const char *path, const char *text) = nullptr;
    bool (*fs_delete_file)(void *user, const char *path)                  = nullptr;
    bool (*fs_rename_file)(void *user, const char *from, const char *to)  = nullptr;
    bool (*fs_file_exists)(void *user, const char *path)                  = nullptr;

    [[nodiscard]] bool has_look_event() const noexcept { return look_event != nullptr; }
};

// ---------------------------------------------------------------------------
// InputInitParams (Q-4 DI params)
// ---------------------------------------------------------------------------

struct InputInitParams
{
    // Required for bind/unbind/*_* command + cvar registration. nullptr =>
    // Input still constructs (tests may exercise the routing core without a
    // live command surface) but S10.2's command/cvar census is not registered.
    ::xash::cmd_cvar::CmdCvarContext *cvars = nullptr; // @lifetime: engine (borrowed; must outlive Input)

    // nullptr => pump_events() is a no-op (no live source); tests drive
    // key_event()/touch_event()/etc. directly instead.
    IEventSource *event_source = nullptr; // @lifetime: caller (borrowed; must outlive Input)

    // nullptr => an internally-owned NullWindowControls stands in
    // (XASH3DPP-STUB(chunk12), input-boundary.md "IWindowControls").
    IWindowControls *window = nullptr; // @lifetime: caller (borrowed; must outlive Input)

    InputCallbacks callbacks = {};

    // Host_IsDedicated() — suppresses mouse/gyro/OSK/joy/touch startup (IN_Init).
    bool dedicated = false;
};

// ---------------------------------------------------------------------------
// create_input — Q-22 pool-owned factory
// ---------------------------------------------------------------------------

class Input;
[[nodiscard]] std::unique_ptr<Input> create_input(memory::PoolHandle pool,
                                                   const InputInitParams &params) noexcept;

class Input
{
public:
    static void operator delete(void *p) noexcept;
    static void operator delete(void *p, std::size_t) noexcept;

    ~Input();

    Input(const Input &)            = delete;
    Input &operator=(const Input &) = delete;

    // Defined in input.cpp where Impl is complete (pimpl move rule).
    Input(Input &&) noexcept;
    Input &operator=(Input &&) noexcept;

    // ---- Event pump (S10.1) ------------------------------------------------
    // Drains event_source_->poll_events() and dispatches every InputEvent
    // through the matching *_event()/routing entry point below.  No-op if no
    // event source was injected.
    void pump_events() noexcept;

    // ---- Keys / key_dest (S10.1) --------------------------------------------
    [[nodiscard]] bool is_down(Key key) const noexcept;
    [[nodiscard]] KeyDest key_dest() const noexcept;
    void set_key_dest(KeyDest dest) noexcept;

    // Dependency-in per boundary spec's "Owned state" table (cls.changelevel):
    // Key_ClearStates skips entirely while a level transition is in flight.
    void set_changelevel(bool changelevel) noexcept;
    [[nodiscard]] bool changelevel() const noexcept;

    // Key_Event's 13-step routing (Quirk 1), verbatim order.
    void key_event(Key key, bool down) noexcept;

    // Key_ClearStates (Quirk 5).
    void clear_states() noexcept;

    // CL_CharEvent routing (console/message/menu dispatch by key_dest).
    void char_event(int ch) noexcept;

    // Key_EnableTextInput (delegates to OSK when osk_enable is set).
    void enable_text_input(bool enable, bool force) noexcept;

    // ---- Bindings (S10.2) ----------------------------------------------------
    [[nodiscard]] std::optional<Key> string_to_keynum(std::string_view name) const noexcept;
    [[nodiscard]] std::string        keynum_to_string(Key key) const noexcept;
    [[nodiscard]] bool               set_binding(Key key, std::string_view binding) noexcept;
    [[nodiscard]] std::string_view   get_binding(Key key) const noexcept;

    // Key_GetKey — case-insensitive PREFIX match (Quirk 14), first match wins
    // in keynum order.
    [[nodiscard]] std::optional<Key> get_key(std::string_view binding_prefix) const noexcept;
    [[nodiscard]] std::optional<std::string> lookup_binding(std::string_view binding_prefix) const noexcept;

    // bind/unbind/unbindall/resetkeys logic, callable directly (also reachable
    // via the registered commands when cvars != nullptr).
    void bind(Key key, std::string_view command) noexcept;
    [[nodiscard]] bool unbind(Key key) noexcept; // false: ESC refused (Quirk 2)
    void unbindall() noexcept;                   // Quirk 3
    void resetkeys() noexcept;                   // Quirk 3

    // P-4 typed introspection — replaces raw keys[] array access.
    [[nodiscard]] std::vector<BindingEntry> bindings_snapshot() const noexcept;

    // Key_WriteBindings text (Compat scope: always double-quoted, escaped).
    // Actual file I/O is the caller's job via Filesystem (sibling-owned).
    [[nodiscard]] std::string write_bindings_text() const noexcept;

    // Key_Bindlist_f text (Quirk 15: raw, unescaped, no unbindall header —
    // diverges from write_bindings_text()).
    [[nodiscard]] std::string bindlist_text() const noexcept;

    // ---- Joy / gyro (S10.3) ---------------------------------------------------
    void set_joy_capabilities(bool have_gyro) noexcept;
    [[nodiscard]] GyroCalibrationState joy_calibration_state() const noexcept;
    void set_joy_calibration_state(GyroCalibrationState state) noexcept;
    [[nodiscard]] JoyAxisState joy_axis_state(JoyAxis axis) const noexcept;
    [[nodiscard]] bool joy_active() const noexcept; // Joy_IsActive (joy_enable cvar)

    // Platform_CalibrateGamepadGyro trampoline + resets the internal
    // calibration state machine's accumulation window (Quirk 7).
    void start_gyro_calibration() noexcept;

    void lock_input_devices(bool lock) noexcept; // IN_LockInputDevices (Quirk 11)

    // Feeds the gyro calibration state machine's clock (host.realtime
    // equivalent — Chunk 10 has no host frame pump; Chunk 12 wiring drives
    // this from the real clock each frame; tests drive it directly to
    // exercise the calibration window, Quirk 7).
    void set_clock_now(double seconds) noexcept;

    // ---- Move assembly (S10.4) -------------------------------------------------
    // IN_EngineAppendMove. No-ops if callbacks_.has_look_event() (the
    // "modern path" opt-out switch, input.c:587-591).
    void engine_append_move(float frametime, MoveCmd &cmd, bool active) noexcept;

    // IN_Commands: drives the modern (pfnLookEvent) path's move/look callback
    // dispatch, then re-syncs mouse grab/relative-mode state every frame.
    // |frametime| feeds the gyro/joy integration math (legacy: the implicit
    // global host.realframetime — Chunk 10 has no host frame pump, so it is
    // an explicit parameter here; Chunk 12 wiring passes the real value).
    void run_commands(double frametime) noexcept;

    // IN_MouseMove: forwards the polled mouse position to VGUI/UI, or to the
    // touch-emulation cursor path if touch_want_visible_cursor().
    void mouse_move() noexcept;

    // Host_InputFrame = run_commands(frametime) + mouse_move(), once per host frame.
    void host_input_frame(double frametime) noexcept;

    // ---- Touch / OSK (S10.5) --------------------------------------------------
    // IN_TouchEvent full routing (menu-mouse-sim -> VGui forward -> touch
    // enabled gate -> aspect rescale -> pfnTouchEvent hook -> internal
    // fallback). Reachable directly or via pump_events(TouchRawEvent).
    int touch_event(TouchEventType type, int finger_id, float x, float y, float dx, float dy) noexcept;

    // Touch_GetMove — additive into the caller's accumulators; self-clears
    // yaw/pitch only (forward/side persist until the owning finger releases).
    void touch_get_move(float &forward, float &side, float &pitch, float &yaw) noexcept;

    void touch_set_client_only(bool state) noexcept;
    [[nodiscard]] bool touch_want_visible_cursor() const noexcept;

    // Touch_KeyEvent mouse-emulation adapter. |mouse_x_norm|/|mouse_y_norm|
    // are the current mouse position normalized to [0,1] — legacy derives
    // these from Platform_GetMousePos()/refState.width/height (screen-pixel
    // renderer state that does not exist in Chunk 10); the caller supplies
    // them directly (Chunk 12's real host loop; tests pass synthetic values).
    void touch_key_event(Key key, bool down, float mouse_x_norm, float mouse_y_norm) noexcept;

    void touch_notify_resize(float refstate_width, float refstate_height) noexcept;
    void touch_remove_button(std::string_view name, bool privileged) noexcept;
    void touch_hide_buttons(std::string_view name, bool hide, bool privileged) noexcept;

    // P-4 iteration surface (INP-OQ-2: input owns the data model, Chunk 12
    // owns the XASH3DPP-STUB(chunk12) draw calls).
    [[nodiscard]] std::vector<TouchButtonDesc> touch_buttons() const noexcept;

    [[nodiscard]] bool osk_key_event(Key key, bool down) noexcept; // OSK_KeyEvent first-refusal
    void osk_enable_text_input(bool enable, bool force) noexcept;
    [[nodiscard]] OskStateDesc osk_state() const noexcept;

    // ---- Stats ------------------------------------------------------------
    [[nodiscard]] const InputStats &stats() const noexcept;

public:
    // Opaque nested type; its full definition is confined to
    // include/xash3dpp/private/input/input_impl.hpp (src/input/**/*.cpp only)
    // — the private header is the real encapsulation boundary, matching this
    // codebase's private-header convention. Public consumers of this header
    // can name Input::Impl but never complete/use it (incomplete type).
    struct Impl;

private:
    friend std::unique_ptr<Input> create_input(memory::PoolHandle, const InputInitParams &) noexcept;
    Input(memory::PoolHandle pool, const InputInitParams &params) noexcept;

    std::unique_ptr<Impl> impl_; // pimpl (std::make_unique sanctioned exception; Input itself is the pool-owned object)
};

} // namespace xash::input
