// xash3dpp — bind/unbind/unbindall/resetkeys/bindlist/makehelp commands +
// the key/mouse/joy/gyro/osk cvar census (S10.2, S10.3).
// Legacy reference: engine/client/input/in_keys.c:341-503,570-588,
// input.c:437-459 (IN_Init registration order), in_joy.c:48-72,573-623,
// in_gyro.c:20-48, engine/common/con_utils.c:1559 (Key_EnumCmds_f, the
// makehelp handler).

#include <xash3dpp/private/input/input_impl.hpp>

#include <xash3dpp/core/log.hpp>

namespace xash::input {

namespace {

// D1: bind/unbind console messages, restored via the same core logf surface
// key_routing.cpp:128 already uses (no console module in Chunk 10 to
// Con_Printf through — logf's "input" tag is the equivalent seam). KeyTable
// itself stays silent (pure data-and-lookup layer, per its header doc); this
// COMMAND layer logs, matching legacy structure (Key_Bind_f/Key_Unbind_f,
// in_keys.c:341-453). Messages omit the trailing '\n' legacy uses — logf()
// appends one automatically.

// bind (Quirk 4: FCMD_PRIVILEGED). Query form (argc==2) prints the current
// binding (in_keys.c:435-441); get_binding()/bindings_snapshot() remain the
// typed P-4 surface for programmatic access.
void cmd_bind(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    int argc = self->cvars->cmd_argc();
    if (argc < 2) {
        ::xash::core::logf(::xash::core::LogLevel::Info, "input",
                            "Usage: bind <key> [command] : attach a command to a key");
        return;
    }

    const char *keyname = self->cvars->cmd_argv(1);
    auto key = self->keys.string_to_keynum(keyname);
    if (!key.has_value()) {
        ::xash::core::logf(::xash::core::LogLevel::Info, "input", "\"%s\" isn't a valid key", keyname);
        return;
    }

    if (argc == 2) { // query form: print the current binding, or "not bound"
        const auto &rec = self->keys.record(*key);
        if (rec.present) { // D2: present-but-empty ("") still prints, matching legacy's non-NULL pointer check
            ::xash::core::logf(::xash::core::LogLevel::Info, "input", "\"%s\" = \"%s\"", keyname, rec.binding.c_str());
        } else {
            ::xash::core::logf(::xash::core::LogLevel::Info, "input", "\"%s\" is not bound", keyname);
        }
        return;
    }

    std::string cmd;
    for (int i = 2; i < argc; ++i) {
        cmd += self->cvars->cmd_argv(i);
        if (i != argc - 1) { cmd += ' '; }
    }
    (void)self->keys.set_binding(*key, cmd); // key validated above
}

void cmd_unbind(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    if (self->cvars->cmd_argc() != 2) {
        ::xash::core::logf(::xash::core::LogLevel::Info, "input",
                            "Usage: unbind <key> : remove commands from a key");
        return;
    }
    const char *keyname = self->cvars->cmd_argv(1);
    auto key = self->keys.string_to_keynum(keyname);
    if (!key.has_value()) {
        ::xash::core::logf(::xash::core::LogLevel::Info, "input", "\"%s\" isn't a valid key", keyname);
        return;
    }
    if (*key == Key::Escape) { // Quirk 2 refusal (in_keys.c:359-363), message form
        ::xash::core::logf(::xash::core::LogLevel::Info, "input", "Can't unbind ESCAPE key");
        return;
    }
    (void)self->keys.unbind(*key); // key validated + ESC already excluded above
}

void cmd_unbindall(void *user) { static_cast<Input::Impl *>(user)->keys.unbindall(); }
void cmd_resetkeys(void *user) { static_cast<Input::Impl *>(user)->keys.resetkeys(); }

// bindlist (unrestricted, in_keys.c:492-503). No console module in Chunk 10
// to print through — registered for completeness of the command census;
// Input::bindlist_text() is the typed P-4 surface a future console consumes.
void cmd_bindlist(void *) {}

// makehelp (unrestricted, in_keys.c:580 -> Key_EnumCmds_f, engine/common/
// con_utils.c:1559 -- writes help.txt via the filesystem, listing every
// console cvar/cmd). Neither a console-output surface nor a filesystem
// write path is available to this leaf lib in Chunk 10; registered as a
// chunk12-tagged stub so the command CENSUS (registration parity) is
// complete even though the body cannot do real work yet.
void cmd_makehelp(void *)
{
    ::xash::core::logf(::xash::core::LogLevel::Info, "input", "makehelp: not available until chunk 12");
}

void cmd_joy_calibrate_gyro(void *user)
{
    auto *self = static_cast<Input::Impl *>(user);
    bool have_gyro = (self->cv.joy_have_gyro != nullptr) && (self->cv.joy_have_gyro->abi.value != 0.0f);
    if (!have_gyro) { return; }
    if (self->source != nullptr) { self->source->calibrate_gamepad_gyro(); }
}

} // namespace

void Input::Impl::register_cvars_and_commands() noexcept
{
    if (cvars == nullptr) { return; }

    constexpr std::uint32_t kPriv = ::xash::cmd_cvar::FCMD_PRIVILEGED;
    constexpr std::uint32_t kArchiveFilterable = ::xash::cmd_cvar::FCVAR_ARCHIVE | ::xash::cmd_cvar::FCVAR_FILTERABLE;

    // bind/unbind/unbindall/resetkeys are trust-gated; bindlist is not (Quirk 4).
    cvars->cmd_add("bind", &cmd_bind, this, kPriv);
    cvars->cmd_add("unbind", &cmd_unbind, this, kPriv);
    cvars->cmd_add("unbindall", &cmd_unbindall, this, kPriv);
    cvars->cmd_add("resetkeys", &cmd_resetkeys, this, kPriv);
    cvars->cmd_add("bindlist", &cmd_bindlist, this);
    cvars->cmd_add("makehelp", &cmd_makehelp, this); // unrestricted (Cmd_AddCommand, not Cmd_AddRestrictedCommand)

    cvars->cmd_add("joy_calibrate_gyro", &cmd_joy_calibrate_gyro, this, kPriv);

    cv.key_rotate = cvars->cvar_get_or_create("key_rotate", "0", kArchiveFilterable);

    // Mouse (input.c:42-53).
    cv.m_pitch      = cvars->cvar_get_or_create("m_pitch", "0.022", kArchiveFilterable);
    cv.m_yaw        = cvars->cvar_get_or_create("m_yaw", "0.022", kArchiveFilterable);
    cv.m_ignore     = cvars->cvar_get_or_create("m_ignore", "0", kArchiveFilterable); // DEFAULT_M_IGNORE (defaults.h) approximated as "0"
    cv.look_filter  = cvars->cvar_get_or_create("look_filter", "0", kArchiveFilterable);
    cv.m_rawinput   = cvars->cvar_get_or_create("m_rawinput", "1", kArchiveFilterable);
    cv.m_grab_debug = cvars->cvar_get_or_create("m_grab_debug", "0", ::xash::cmd_cvar::FCVAR_PRIVILEGED);
    cv.touch_enable = cvars->cvar_get_or_create("touch_enable", "0", kArchiveFilterable); // DEFAULT_TOUCH_ENABLE approximated as "0"

    cv.cl_forwardspeed = cvars->cvar_get_or_create("cl_forwardspeed", "400", kArchiveFilterable | ::xash::cmd_cvar::FCVAR_CLIENTDLL);
    cv.cl_backspeed    = cvars->cvar_get_or_create("cl_backspeed", "400", kArchiveFilterable | ::xash::cmd_cvar::FCVAR_CLIENTDLL);
    cv.cl_sidespeed    = cvars->cvar_get_or_create("cl_sidespeed", "400", kArchiveFilterable | ::xash::cmd_cvar::FCVAR_CLIENTDLL);

    // Gamepad (in_joy.c:48-72) — 24 cvars.
    cv.joy_pitch   = cvars->cvar_get_or_create("joy_pitch", "100.0", kArchiveFilterable);
    cv.joy_yaw     = cvars->cvar_get_or_create("joy_yaw", "100.0", kArchiveFilterable);
    cv.joy_side    = cvars->cvar_get_or_create("joy_side", "1.0", kArchiveFilterable);
    cv.joy_forward = cvars->cvar_get_or_create("joy_forward", "1.0", kArchiveFilterable);
    cv.joy_lt_threshold = cvars->cvar_get_or_create("joy_lt_threshold", "16384", kArchiveFilterable);
    cv.joy_rt_threshold = cvars->cvar_get_or_create("joy_rt_threshold", "16384", kArchiveFilterable);
    cv.joy_side_key_threshold    = cvars->cvar_get_or_create("joy_side_key_threshold", "24576", kArchiveFilterable);
    cv.joy_forward_key_threshold = cvars->cvar_get_or_create("joy_forward_key_threshold", "24576", kArchiveFilterable);
    cv.joy_side_deadzone    = cvars->cvar_get_or_create("joy_side_deadzone", "4096", kArchiveFilterable);
    cv.joy_forward_deadzone = cvars->cvar_get_or_create("joy_forward_deadzone", "4096", kArchiveFilterable);
    cv.joy_pitch_deadzone   = cvars->cvar_get_or_create("joy_pitch_deadzone", "4096", kArchiveFilterable);
    cv.joy_yaw_deadzone     = cvars->cvar_get_or_create("joy_yaw_deadzone", "4096", kArchiveFilterable);
    cv.joy_axis_binding = cvars->cvar_get_or_create("joy_axis_binding", "sfpyrl", kArchiveFilterable);
    cv.joy_enable       = cvars->cvar_get_or_create("joy_enable", "1", kArchiveFilterable);
    cv.joy_have_gyro    = cvars->cvar_get_or_create("joy_have_gyro", "0", ::xash::cmd_cvar::FCVAR_READ_ONLY);
    cv.joy_calibrated   = cvars->cvar_get_or_create("joy_calibrated", "0", ::xash::cmd_cvar::FCVAR_READ_ONLY);
    cv.joy_gyro_enable  = cvars->cvar_get_or_create("joy_gyro_enable", "1", kArchiveFilterable);
    cv.joy_gyro_pitch = cvars->cvar_get_or_create("joy_gyro_pitch", "1.0", kArchiveFilterable);
    cv.joy_gyro_yaw   = cvars->cvar_get_or_create("joy_gyro_yaw", "1.0", kArchiveFilterable);
    cv.joy_gyro_roll  = cvars->cvar_get_or_create("joy_gyro_roll", "0.0", kArchiveFilterable);
    cv.joy_gyro_pitch_deadzone = cvars->cvar_get_or_create("joy_gyro_pitch_deadzone", "0.5", kArchiveFilterable);
    cv.joy_gyro_yaw_deadzone   = cvars->cvar_get_or_create("joy_gyro_yaw_deadzone", "0.5", kArchiveFilterable);
    cv.joy_gyro_roll_deadzone  = cvars->cvar_get_or_create("joy_gyro_roll_deadzone", "0.5", kArchiveFilterable);
    cv.joy_debug = cvars->cvar_get_or_create("joy_debug", "0", 0);

    // Built-in device gyro (in_gyro.c:20-27) — 8 cvars.
    cv.gyro_enable    = cvars->cvar_get_or_create("gyro_enable", "0", kArchiveFilterable); // OFF by default, unlike joy_gyro_enable
    cv.gyro_available = cvars->cvar_get_or_create("gyro_available", "0", ::xash::cmd_cvar::FCVAR_READ_ONLY);
    cv.gyro_pitch = cvars->cvar_get_or_create("gyro_pitch", "1.0", kArchiveFilterable);
    cv.gyro_yaw   = cvars->cvar_get_or_create("gyro_yaw", "1.0", kArchiveFilterable);
    cv.gyro_roll  = cvars->cvar_get_or_create("gyro_roll", "0.0", kArchiveFilterable);
    cv.gyro_pitch_deadzone = cvars->cvar_get_or_create("gyro_pitch_deadzone", "0.5", kArchiveFilterable);
    cv.gyro_yaw_deadzone   = cvars->cvar_get_or_create("gyro_yaw_deadzone", "0.5", kArchiveFilterable);
    cv.gyro_roll_deadzone  = cvars->cvar_get_or_create("gyro_roll_deadzone", "0.5", kArchiveFilterable);

    cv.osk_enable = cvars->cvar_get_or_create("osk_enable", "0", kArchiveFilterable);

    // "-noenginejoy" (renamed from "-nojoy" to avoid colliding with the game
    // DLL's own joystick flag, in_joy.c:612-618) is a launcher command-line
    // argument — argv access is host/launcher-owned and not wired in Chunk
    // 10 (no EngineContext member, leaf static lib); Chunk 12 host wiring is
    // expected to call cvar_full_set("joy_enable","0",FCVAR_READ_ONLY) itself
    // before Input's cvars are read, if the flag is present.
}

} // namespace xash::input
