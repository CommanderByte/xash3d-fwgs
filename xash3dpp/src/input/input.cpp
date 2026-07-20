// xash3dpp — Input lifecycle, event pump dispatch, and public API forwarding.
// Legacy reference: engine/client/input/input.c (IN_Init/IN_Shutdown), and
// the InputEvent dispatch table this file owns (S10.1 seam requirement).

#include <xash3dpp/private/input/input_impl.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <variant>

namespace xash::input {

// ---------------------------------------------------------------------------
// Impl::Impl
// ---------------------------------------------------------------------------

Input::Impl::Impl(memory::PoolHandle pool_, const InputInitParams &params) noexcept
    : pool(pool_)
    , callbacks(params.callbacks)
    , cvars(params.cvars)
    , source(params.event_source)
    , dedicated(params.dedicated)
{
    window = (params.window != nullptr) ? params.window : &null_window;

    // Default bindings (Key_Init's keynames[] replay, in_keys.c:582-584) —
    // unconditional, independent of cmd_cvar availability.
    keys.resetkeys();

    if (cvars != nullptr) {
        register_cvars_and_commands();
        register_touch_commands();
    }

    if (!dedicated) {
        in_mouseinitialized = true;
    }

    // IN_JoyAppendMove's initial moveflags == T | S (input.c:480).
    moveflags = (1u << 4) | (1u << 5);
}

// ---------------------------------------------------------------------------
// create_input / lifecycle
// ---------------------------------------------------------------------------

std::unique_ptr<Input> create_input(memory::PoolHandle pool, const InputInitParams &params) noexcept
{
    // Placement-new directly (rather than memory::pool_new<Input>) so that
    // Input's constructor can stay private — only this friend factory
    // constructs an Input; pool_new<T> is a free template with no standing
    // to name a private constructor. Mirrors pool_new<T>'s own body exactly
    // (mem_alloc + placement new), preserving the Q-22 pool-owned contract:
    // operator delete (defined below) frees via mem_free either way.
    void *raw = memory::mem_alloc(pool, sizeof(Input));
    if (raw == nullptr) { return nullptr; }
    Input *p = ::new (raw) Input(pool, params);
    return std::unique_ptr<Input>(p);
}

Input::Input(memory::PoolHandle pool, const InputInitParams &params) noexcept
    : impl_(std::make_unique<Impl>(pool, params))
{
}

Input::~Input() = default;
Input::Input(Input &&) noexcept            = default;
Input &Input::operator=(Input &&) noexcept = default;

void Input::operator delete(void *p) noexcept { memory::mem_free(p); }
void Input::operator delete(void *p, std::size_t) noexcept { memory::mem_free(p); }

// ---------------------------------------------------------------------------
// Event pump (S10.1)
// ---------------------------------------------------------------------------

void Input::pump_events() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    if (impl_->source == nullptr) { return; }

    for (const InputEvent &ev : impl_->source->poll_events()) {
        std::visit(
            [this](auto &&e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, KeyEvent>) {
                    // key_events_routed is counted once, inside Input::key_event()
                    // itself (the single entry point every caller — pump_events
                    // included — routes through; avoids double-counting).
                    key_event(e.key, e.down);
                } else if constexpr (std::is_same_v<T, MouseButtonEvent>) {
                    impl_->stats.mouse_events_routed.fetch_add(1, std::memory_order_relaxed);
                    impl_->mouse_button_event(e.button, e.down);
                } else if constexpr (std::is_same_v<T, MouseWheelEvent>) {
                    impl_->stats.mouse_events_routed.fetch_add(1, std::memory_order_relaxed);
                    Key b = (e.direction > 0) ? Key::MWheelUp : Key::MWheelDown;
                    if (impl_->callbacks.vgui_mwheel_event != nullptr) { impl_->callbacks.vgui_mwheel_event(impl_->callbacks.user, e.direction); }
                    key_event(b, true);
                    key_event(b, false);
                } else if constexpr (std::is_same_v<T, TouchRawEvent>) {
                    // touch_events_routed is counted once, inside Input::touch_event().
                    touch_event(e.type, e.finger_id, e.x, e.y, e.dx, e.dy);
                } else if constexpr (std::is_same_v<T, JoyAxisEvent>) {
                    impl_->stats.joy_axis_events.fetch_add(1, std::memory_order_relaxed);
                    bool ui_mode = (impl_->key_dest == KeyDest::Menu || impl_->key_dest == KeyDest::Console);
                    // D3: peek — Joy_AxisMotionEvent never consumes
                    // joy_axis_binding's FCVAR_CHANGED gate (in_joy.c:273-290);
                    // only finalize_move may.
                    auto transitions = impl_->joy.on_axis_event(e.hw_axis, e.value, impl_->joy_tunables_peek(), ui_mode);
                    impl_->apply_joy_key_transitions(transitions);
                } else if constexpr (std::is_same_v<T, JoyGyroEvent>) {
                    impl_->stats.gyro_samples.fetch_add(1, std::memory_order_relaxed);
                    GyroCalibrationState before = impl_->gyro_cal.state();
                    auto sample = impl_->gyro_cal.on_sample(impl_->gyro_time_now, e.x, e.y, e.z);
                    if (impl_->gyro_cal.state() != before) { set_joy_calibration_state(impl_->gyro_cal.state()); }
                    if (sample.has_value()) { impl_->joy.on_gyro_sample((*sample)[0], (*sample)[1], (*sample)[2]); }
                } else if constexpr (std::is_same_v<T, DeviceGyroEvent>) {
                    impl_->stats.gyro_samples.fetch_add(1, std::memory_order_relaxed);
                    impl_->device_gyro.on_sample(e.x, e.y, e.z);
                }
                // NO default case (S10.1): a new InputEvent alternative is a
                // compile error here until handled.
            },
            ev);
    }
}

// ---------------------------------------------------------------------------
// Keys / key_dest (S10.1) — thin forwarders
// ---------------------------------------------------------------------------

bool Input::is_down(Key key) const noexcept { return impl_->keys.is_down(key); }
KeyDest Input::key_dest() const noexcept { return impl_->key_dest; }

void Input::set_key_dest(KeyDest dest) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    KeyDest old = impl_->key_dest;
    impl_->toggle_client_mouse(dest, old);
    impl_->enable_text_input(dest == KeyDest::Console || dest == KeyDest::Message, false);
    impl_->key_dest = dest;
}

void Input::set_changelevel(bool changelevel) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->changelevel = changelevel;
}
bool Input::changelevel() const noexcept { return impl_->changelevel; }

void Input::key_event(Key key, bool down) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->stats.key_events_routed.fetch_add(1, std::memory_order_relaxed);
    impl_->key_event(key, down);
}
void Input::clear_states() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->clear_states();
}
void Input::char_event(int ch) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->char_event(ch);
}
void Input::enable_text_input(bool enable, bool force) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->enable_text_input(enable, force);
}

// ---------------------------------------------------------------------------
// Bindings (S10.2)
// ---------------------------------------------------------------------------

std::optional<Key> Input::string_to_keynum(std::string_view name) const noexcept { return impl_->keys.string_to_keynum(name); }
std::string Input::keynum_to_string(Key key) const noexcept { return impl_->keys.keynum_to_string(key); }
bool Input::set_binding(Key key, std::string_view binding) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    return impl_->keys.set_binding(key, binding);
}
std::string_view Input::get_binding(Key key) const noexcept { return impl_->keys.get_binding(key); }

// get_key() walks the full keys_ table (KeyTable::get_key's linear
// prefix scan) rather than indexing a single record — same mutable-state
// iteration hazard as bindings_snapshot()/write_bindings_text()/
// bindlist_text() below, so it asserts too (FIX A: snapshot-shaped query).
std::optional<Key> Input::get_key(std::string_view binding_prefix) const noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    return impl_->keys.get_key(binding_prefix);
}

std::optional<std::string> Input::lookup_binding(std::string_view binding_prefix) const noexcept
{
    // Walks keys_ via KeyTable::get_key() — same reasoning as get_key() above.
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    auto key = impl_->keys.get_key(binding_prefix);
    if (!key.has_value()) { return std::nullopt; }
    return impl_->keys.keynum_to_string(*key);
}

void Input::bind(Key key, std::string_view command) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    (void)impl_->keys.set_binding(key, command);
}
bool Input::unbind(Key key) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    return impl_->keys.unbind(key);
}
void Input::unbindall() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->keys.unbindall();
}
void Input::resetkeys() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->keys.resetkeys();
}

// bindings_snapshot()/write_bindings_text()/bindlist_text() are const but
// each WALKS the full mutable keys_ vector to build their result (a loop,
// not a single-record index like is_down()/get_binding()) — FIX A treats
// that as needing the assert too (a query "built by walking mutable state").
std::vector<BindingEntry> Input::bindings_snapshot() const noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    return impl_->keys.bindings_snapshot();
}
std::string Input::write_bindings_text() const noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    return impl_->keys.write_bindings_text();
}
std::string Input::bindlist_text() const noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    return impl_->keys.bindlist_text();
}

// ---------------------------------------------------------------------------
// Joy / gyro (S10.3)
// ---------------------------------------------------------------------------

void Input::set_joy_capabilities(bool have_gyro) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    if (impl_->cvars != nullptr && impl_->cv.joy_have_gyro != nullptr) {
        impl_->cvars->cvar_full_set(impl_->cv.joy_have_gyro->abi.name, have_gyro ? "1" : "0", impl_->cv.joy_have_gyro->abi.flags);
    }
}

GyroCalibrationState Input::joy_calibration_state() const noexcept { return impl_->gyro_cal.state(); }

void Input::set_joy_calibration_state(GyroCalibrationState state) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    if (impl_->cvars != nullptr && impl_->cv.joy_calibrated != nullptr) {
        int v = static_cast<int>(state);
        char buf[4];
        buf[0] = static_cast<char>('0' + v);
        buf[1] = '\0';
        impl_->cvars->cvar_full_set(impl_->cv.joy_calibrated->abi.name, buf, impl_->cv.joy_calibrated->abi.flags);
    }
}

JoyAxisState Input::joy_axis_state(JoyAxis axis) const noexcept { return impl_->joy.axis_state(axis); }

bool Input::joy_active() const noexcept
{
    return (impl_->cv.joy_enable != nullptr) ? (impl_->cv.joy_enable->abi.value != 0.0f) : true;
}

void Input::start_gyro_calibration() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    if (impl_->source != nullptr) { impl_->source->calibrate_gamepad_gyro(); }
    impl_->gyro_cal.restart(impl_->gyro_time_now);
    set_joy_calibration_state(GyroCalibrationState::NotCalibrated);
}

void Input::set_clock_now(double seconds) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->gyro_time_now = seconds;
}

void Input::lock_input_devices(bool lock) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    // IN_LockInputDevices (Quirk 11) — pure FCVAR_READ_ONLY bitfield flip,
    // fully reentrant/idempotent (input.c:92-108).
    constexpr std::uint32_t k_read_only = ::xash::cmd_cvar::FCVAR_READ_ONLY;
    auto flip = [lock](::xash::cmd_cvar::Cvar *cv) {
        if (cv == nullptr) { return; }
        if (lock) { cv->abi.flags |= k_read_only; } else { cv->abi.flags &= ~k_read_only; }
    };
    flip(impl_->cv.m_ignore);
    flip(impl_->cv.joy_enable);
    flip(impl_->cv.touch_enable);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

const InputStats &Input::stats() const noexcept { return impl_->stats; }

} // namespace xash::input
