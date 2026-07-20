#pragma once
// xash3dpp — bindings introspection surface
// Legacy reference: in_keys.c keys[265]'s binding/down/gamedown/repeats
// fields (in_keys.c:22-43) and Key_Bindlist_f (in_keys.c:492-503).
//
// bindings_snapshot() is the P-4 typed introspection surface that replaces
// raw keys[] array access (input-boundary.md "Extension axes (Q-21)").

#include <xash3dpp/input/key.hpp>

#include <cstdint>
#include <string>

namespace xash::input {

// One row of a bindings_snapshot() result.  |key_name| is the legacy
// key-name string (Key_KeynumToString) — carried as a value, not the old
// static tinystr[16] reuse hazard (Owned state, "must not survive the port").
struct BindingEntry
{
    Key           key;
    std::string   key_name;  // Key_KeynumToString(key) — Compat scope name
    std::string   binding;   // empty == unbound
    bool          down       = false;
    bool          gamedown   = false;
    std::uint32_t repeats    = 0;
};

} // namespace xash::input
