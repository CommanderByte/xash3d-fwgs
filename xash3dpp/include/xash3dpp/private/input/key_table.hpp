#pragma once
// xash3dpp — KeyTable: keys[265] + keynames[] + the Key_* primitives
// Legacy reference: engine/client/input/in_keys.c:22-334, 341-556.
//
// Deliberately decoupled from cmd_cvar / callbacks — KeyTable is the pure
// data-and-lookup layer; Input (input.cpp) owns the 13-step Key_Event
// routing orchestration and the command/cvar registration.
//
// @thread-safety: T_Main only (owning Input's contract); no internal sync.

#include <xash3dpp/input/bindings.hpp>
#include <xash3dpp/input/key.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::input::detail {

struct KeyRecord
{
    std::string   binding;   // text value; "" is a valid PRESENT binding (see |present|)
    // NULL-vs-"" distinction (D2, in_keys.c:266-274,583-584): legacy's
    // keys[keynum].binding is a pointer — NULL means "Key_SetBinding was
    // never called for this key" (no keynames[] row, custom scancode never
    // bound), while copystring("") is a non-NULL pointer to an empty string
    // (any keynames[] row, even ones whose default binding is ""). |present|
    // reproduces that: true once set_binding() has ever run for this key.
    bool          present  = false;
    bool          down     = false;
    bool          gamedown = false;
    std::uint32_t repeats  = 0;
};

// One row of the legacy keynames[] table (in_keys.c:45-159) — DATA, not code
// (Compat scope: input-boundary.md §"Compat scope (Q-12)").
struct KeyNameEntry
{
    const char *name;
    Key         keynum;
    const char *default_binding;
};

// The full ~103-row keynames[] table (in_keys.c:47-158), verbatim. Defined in
// key_table.cpp.
extern const KeyNameEntry g_keynames[];
extern const std::size_t  g_keynames_count;

class KeyTable
{
public:
    KeyTable();

    [[nodiscard]] bool is_down(Key key) const noexcept;

    // Key_StringToKeynum (in_keys.c:188-217).
    [[nodiscard]] std::optional<Key> string_to_keynum(std::string_view str) const noexcept;

    // Key_KeynumToString (in_keys.c:227-254) — returns an owned std::string,
    // NOT the legacy static tinystr[16] reuse hazard (Owned state: "must not
    // survive the port as a shared mutable buffer").
    [[nodiscard]] std::string keynum_to_string(Key key) const noexcept;

    [[nodiscard]] bool             set_binding(Key key, std::string_view binding) noexcept;
    [[nodiscard]] std::string_view get_binding(Key key) const noexcept;

    // Key_GetKey (in_keys.c:294-318) — case-insensitive PREFIX match
    // (Quirk 14), first match wins in keynum (ascending index) order.
    [[nodiscard]] std::optional<Key> get_key(std::string_view binding_prefix) const noexcept;

    // Key_Rotate (in_keys.c:660-699) — exact float == against key_rotate.value.
    [[nodiscard]] Key rotate(Key key, float key_rotate_value) const noexcept;

    // unbind (Quirk 2: refuses K_ESCAPE), unbindall (Quirk 3), resetkeys (Quirk 3).
    [[nodiscard]] bool unbind(Key key) noexcept;
    void unbindall() noexcept;
    void resetkeys() noexcept;

    // Key_ClearStates field-zero pass (the force-zero half; the "replay
    // releases through the real event path" half is Input's orchestration —
    // in_keys.c:918-940).
    void zero_fields(Key key) noexcept;

    [[nodiscard]] const KeyRecord &record(Key key) const noexcept;
    [[nodiscard]] KeyRecord       &record(Key key) noexcept;

    [[nodiscard]] std::vector<BindingEntry> bindings_snapshot() const noexcept;
    [[nodiscard]] std::string                write_bindings_text() const noexcept;
    [[nodiscard]] std::string                bindlist_text() const noexcept;

private:
    std::vector<KeyRecord> keys_; // size == k_key_count (@pre-reserved: input_key_count)
};

} // namespace xash::input::detail
