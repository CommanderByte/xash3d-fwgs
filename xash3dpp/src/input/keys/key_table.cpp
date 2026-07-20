// xash3dpp — KeyTable implementation
// Legacy reference: engine/client/input/in_keys.c:22-556.

#include <xash3dpp/private/input/key_table.hpp>

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace xash::input::detail {

namespace {

[[nodiscard]] char ci_lower(char c) noexcept
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

[[nodiscard]] bool ci_equal(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ci_lower(a[i]) != ci_lower(b[i])) { return false; }
    }
    return true;
}

// Q_strnicmp(p, pBinding, len) semantics: does |haystack| start with |prefix|,
// case-insensitively, comparing exactly prefix.size() characters (a short
// haystack that is itself a prefix of |prefix| does NOT match, matching
// strncmp's behaviour of reading past a shorter string's NUL only up to the
// point of the first mismatch — replicated here via explicit bounds check).
[[nodiscard]] bool ci_starts_with(std::string_view haystack, std::string_view prefix) noexcept
{
    if (haystack.size() < prefix.size()) { return false; }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (ci_lower(haystack[i]) != ci_lower(prefix[i])) { return false; }
    }
    return true;
}

// Cmd_Escape (engine/common/cmd.c:1422-1445) — backslash-escapes double
// quotes for config.cfg compat (Key_WriteBindings, in_keys.c:476). The
// cmd_scripting '$' doubling is a cmd_cvar-owned opt-in feature outside this
// leaf lib's dependency surface (Q-23); omitted here (deviation, documented
// in the lane report).
[[nodiscard]] std::string escape_for_config(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') { out.push_back('\\'); }
        out.push_back(c);
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// g_keynames — the full keynames[] table, verbatim (in_keys.c:45-159).
// ---------------------------------------------------------------------------

const KeyNameEntry g_keynames[] = {
    { "TAB",            Key::Tab,                 ""                },
    { "ENTER",          Key::Enter,               ""                },
    { "ESCAPE",         Key::Escape,              "cancelselect"    }, // hardcoded
    { "SPACE",          Key::Space,               "+jump"           },
    { "BACKSPACE",      Key::Backspace,           ""                },
    { "UPARROW",        Key::UpArrow,             "+forward"        },
    { "DOWNARROW",      Key::DownArrow,           "+back"           },
    { "LEFTARROW",      Key::LeftArrow,           "+left"           },
    { "RIGHTARROW",     Key::RightArrow,          "+right"          },
    { "ALT",            Key::Alt,                 "+strafe"         },
    { "CTRL",           Key::Ctrl,                "+attack"         },
    { "SHIFT",          Key::Shift,               "+speed"          },
    { "CAPSLOCK",       key_from_index(175),      ""                }, // K_CAPSLOCK
    { "SCROLLOCK",      Key::ScrollLock,          ""                },
    { "F1",             key_from_index(135),      "cmd help"        },
    { "F2",             key_from_index(136),      "menu_savegame"   },
    { "F3",             key_from_index(137),      "menu_loadgame"   },
    { "F4",             key_from_index(138),      "menu_controls"   },
    { "F5",             key_from_index(139),      "menu_creategame" },
    { "F6",             key_from_index(140),      "savequick"       },
    { "F7",             key_from_index(141),      "loadquick"       },
    { "F8",             key_from_index(142),      "stop"            },
    { "F9",             key_from_index(143),      ""                },
    { "F10",            key_from_index(144),      "menu_main"       },
    { "F11",            key_from_index(145),      ""                },
    { "F12",            key_from_index(146),      "snapshot"        },
    { "INS",            key_from_index(147),      ""                },
    { "DEL",            key_from_index(148),      "+lookdown"       },
    { "PGDN",           Key::PgDn,                "+lookup"         },
    { "PGUP",           Key::PgUp,                ""                },
    { "HOME",           key_from_index(151),      ""                },
    { "END",            key_from_index(152),      "centerview"      },

    // mouse buttons
    { "MOUSE1",         Key::Mouse1,              "+attack"         },
    { "MOUSE2",         key_from_index(242),      "+attack2"        },
    { "MOUSE3",         key_from_index(243),      ""                },
    { "MOUSE4",         key_from_index(244),      ""                },
    { "MOUSE5",         Key::Mouse5,              ""                },
    { "MWHEELUP",       Key::MWheelUp,            ""                },
    { "MWHEELDOWN",     Key::MWheelDown,          ""                },

    // digital keyboard (numpad)
    { "KP_HOME",        key_from_index(160),      ""                },
    { "KP_UPARROW",     key_from_index(161),      "+forward"        },
    { "KP_PGUP",        Key::KpPgUp,              ""                },
    { "KP_LEFTARROW",   key_from_index(163),      "+left"           },
    { "KP_5",           key_from_index(164),      ""                },
    { "KP_RIGHTARROW",  key_from_index(165),      "+right"          },
    { "KP_END",         key_from_index(166),      "centerview"      },
    { "KP_DOWNARROW",   key_from_index(167),      "+back"           },
    { "KP_PGDN",        Key::KpPgDn,              "+lookup"         },
    { "KP_ENTER",       key_from_index(169),      ""                },
    { "KP_INS",         key_from_index(170),      ""                },
    { "KP_DEL",         key_from_index(171),      "+lookdown"       },
    { "KP_SLASH",       key_from_index(172),      ""                },
    { "KP_MINUS",       key_from_index(173),      ""                },
    { "KP_PLUS",        key_from_index(174),      ""                },
    { "PAUSE",          Key::Pause,               "pause"           },

    // Gamepad — A/B X/Y names match the Xbox controller layout
    { "A_BUTTON",       Key::AButton,             "+jump"           }, // K_AUX1
    { "B_BUTTON",       key_from_index(208),      "+use"            }, // K_AUX2
    { "X_BUTTON",       key_from_index(209),      "+reload"         }, // K_AUX3
    { "Y_BUTTON",       key_from_index(210),      "impulse 100"     }, // K_AUX4 (Flashlight)
    { "BACK",           key_from_index(213),      "pause"           }, // K_AUX7 (Menu)
    { "MODE",           key_from_index(214),      ""                }, // K_AUX8
    { "START",          Key::StartButton,         "cancelselect"    }, // K_AUX9
    { "STICK1",         key_from_index(216),      "+speed"          }, // K_AUX10
    { "STICK2",         key_from_index(217),      "+duck"           }, // K_AUX11
    { "L1_BUTTON",      key_from_index(211),      "+duck"           }, // K_AUX5
    { "R1_BUTTON",      key_from_index(212),      "+attack"         }, // K_AUX6
    { "DPAD_UP",        Key::DPadUp,              "impulse 201"     }, // Spray
    { "DPAD_DOWN",      Key::DPadDown,            "lastinv"         },
    { "DPAD_LEFT",      Key::DPadLeft,            "invprev"         },
    { "DPAD_RIGHT",     Key::DPadRight,           "invnext"         },
    { "L2_BUTTON",      key_from_index(218),      "+speed"          }, // K_AUX12
    { "R2_BUTTON",      key_from_index(219),      "+attack2"        }, // K_AUX13
    { "LTRIGGER",       Key::LTrigger,            "+speed"          }, // L2 in SDL2
    { "RTRIGGER",       Key::RTrigger,            "+attack2"        }, // R2 in SDL2
    { "JOY3",           key_from_index(205),      ""                },
    { "JOY4",           key_from_index(206),      ""                },
    { "C_BUTTON",       key_from_index(220),      ""                }, // K_AUX14
    { "Z_BUTTON",       key_from_index(221),      ""                }, // K_AUX15
    { "MISC_BUTTON",    key_from_index(226),      ""                }, // K_AUX20
    { "PADDLE1",        key_from_index(227),      ""                }, // K_AUX21
    { "PADDLE2",        key_from_index(228),      ""                }, // K_AUX22
    { "PADDLE3",        key_from_index(229),      ""                }, // K_AUX23
    { "PADDLE4",        key_from_index(230),      ""                }, // K_AUX24
    { "TOUCHPAD",       key_from_index(231),      ""                }, // K_AUX25
    { "AUX26",          key_from_index(232),      ""                }, // generic
    { "AUX27",          key_from_index(233),      ""                },
    { "AUX28",          key_from_index(234),      ""                },
    { "AUX29",          key_from_index(235),      ""                },
    { "AUX30",          key_from_index(236),      ""                },
    { "AUX31",          key_from_index(237),      ""                },
    { "AUX32",          key_from_index(238),      ""                },

    // raw semicolon separates commands
    { "SEMICOLON",      key_from_index(';'),      ""                },

    // extended keys set
    { "INTERNATIONAL1", Key::International,               ""        },
    { "INTERNATIONAL2", key_from_index(256 + 1),           ""        },
    { "INTERNATIONAL3", key_from_index(256 + 2),           ""        },
    { "INTERNATIONAL4", key_from_index(256 + 3),           ""        },
    { "INTERNATIONAL5", key_from_index(256 + 4),           ""        },
    { "INTERNATIONAL6", key_from_index(256 + 5),           ""        },
    { "INTERNATIONAL7", key_from_index(256 + 6),           ""        },
    { "INTERNATIONAL8", key_from_index(256 + 7),           ""        },
    { "INTERNATIONAL9", key_from_index(256 + 8),           ""        },
};
const std::size_t g_keynames_count = sizeof(g_keynames) / sizeof(g_keynames[0]);

// ---------------------------------------------------------------------------
// KeyTable
// ---------------------------------------------------------------------------

KeyTable::KeyTable()
{
    keys_.resize(static_cast<std::size_t>(k_key_count)); // @pre-reserved: input_key_count
}

bool KeyTable::is_down(Key key) const noexcept
{
    if (!key_in_range(key)) { return false; }
    return keys_[static_cast<std::size_t>(key_index(key))].down;
}

std::optional<Key> KeyTable::string_to_keynum(std::string_view str) const noexcept
{
    if (str.empty()) { return std::nullopt; }

    if (str.size() == 1) { return key_from_index(static_cast<unsigned char>(str[0])); }

    // hex code: "0x.." (in_keys.c:199-207) — bounds-checked against ARRAYSIZE(keys).
    if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') {
        char       *end = nullptr;
        std::string tmp(str);
        long        value = std::strtol(tmp.c_str(), &end, 0);
        if (value < 0 || value >= k_key_count) { return std::nullopt; }
        return key_from_index(static_cast<int>(value));
    }

    for (std::size_t i = 0; i < g_keynames_count; ++i) {
        if (ci_equal(str, g_keynames[i].name)) { return g_keynames[i].keynum; }
    }
    return std::nullopt;
}

std::string KeyTable::keynum_to_string(Key key) const noexcept
{
    int idx = key_index(key);
    if (idx == -1) { return "<KEY NOT FOUND>"; }
    if (idx < 0 || idx >= k_key_count) { return "<OUT OF RANGE>"; }

    // printable ASCII fast path (don't use quote/semicolon/scrolllock, in_keys.c:238).
    if (idx > 32 && idx < 127 && idx != '"' && idx != ';' && idx != key_index(Key::ScrollLock)) {
        return std::string(1, static_cast<char>(idx));
    }

    for (std::size_t i = 0; i < g_keynames_count; ++i) {
        if (idx == key_index(g_keynames[i].keynum)) { return g_keynames[i].name; }
    }

    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%x", idx);
    return buf;
}

// compliance-allow(thread-assert): private leaf reached ONLY through an
// already-asserting public `Input::` entry point (close-out audit, 2026-07-20).
// Input is permanently T_Main-confined — there is no T_Input split planned or
// warranted — so the assertion belongs at the entry, not repeated in the
// data-and-lookup layer beneath it.
bool KeyTable::set_binding(Key key, std::string_view binding) noexcept
{
    if (!key_in_range(key)) { return false; }
    KeyRecord &rec = keys_[static_cast<std::size_t>(key_index(key))];
    rec.binding = binding;
    rec.present = true; // Key_SetBinding always leaves a non-NULL pointer (D2)
    return true;
}

std::string_view KeyTable::get_binding(Key key) const noexcept
{
    if (!key_in_range(key)) { return {}; }
    return keys_[static_cast<std::size_t>(key_index(key))].binding;
}

std::optional<Key> KeyTable::get_key(std::string_view binding_prefix) const noexcept
{
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i].binding.empty()) { continue; }

        std::string_view p = keys_[i].binding;
        if (!p.empty() && p.front() == '+') { p.remove_prefix(1); }

        if (ci_starts_with(p, binding_prefix)) { return key_from_index(static_cast<int>(i)); }
    }
    return std::nullopt;
}

Key KeyTable::rotate(Key key, float key_rotate_value) const noexcept
{
    // Key_Rotate (in_keys.c:660-699) — exact float == comparisons, verbatim.
    if (key_rotate_value == 1.0f) { // CW
        if (key == Key::UpArrow) { return Key::LeftArrow; }
        if (key == Key::LeftArrow) { return Key::DownArrow; }
        if (key == Key::RightArrow) { return Key::UpArrow; }
        if (key == Key::DownArrow) { return Key::RightArrow; }
    } else if (key_rotate_value == 3.0f) { // CCW
        if (key == Key::UpArrow) { return Key::RightArrow; }
        if (key == Key::LeftArrow) { return Key::UpArrow; }
        if (key == Key::RightArrow) { return Key::DownArrow; }
        if (key == Key::DownArrow) { return Key::LeftArrow; }
    } else if (key_rotate_value == 2.0f) {
        if (key == Key::UpArrow) { return Key::DownArrow; }
        if (key == Key::LeftArrow) { return Key::RightArrow; }
        if (key == Key::RightArrow) { return Key::LeftArrow; }
        if (key == Key::DownArrow) { return Key::UpArrow; }
    }
    return key;
}

bool KeyTable::unbind(Key key) noexcept
{
    // Quirk 2: ESC is the only permanently-protected key.
    if (key == Key::Escape) { return false; }
    (void)set_binding(key, ""); // intentional: key already validated
    return true;
}

void KeyTable::unbindall() noexcept
{
    // Quirk 3: unbindall clears then re-applies exactly two hardcoded defaults.
    for (auto &rec : keys_) { rec.binding.clear(); }
    (void)set_binding(Key::Escape, "cancelselect");      // hardcoded defaults —
    (void)set_binding(Key::StartButton, "cancelselect");  // keys are always valid
}

// compliance-allow(thread-assert): owned key-table leaf, mutated only through
// already-asserting Input:: entry points and the T_Main-only ctor
void KeyTable::resetkeys() noexcept
{
    // Quirk 3: resetkeys clears then replays the ENTIRE keynames[] table.
    for (auto &rec : keys_) { rec.binding.clear(); }
    for (std::size_t i = 0; i < g_keynames_count; ++i) {
        (void)set_binding(g_keynames[i].keynum, g_keynames[i].default_binding); // table replay — all rows valid
    }
}

void KeyTable::zero_fields(Key key) noexcept
{
    if (!key_in_range(key)) { return; }
    KeyRecord &rec = keys_[static_cast<std::size_t>(key_index(key))];
    rec.down     = false;
    rec.repeats  = 0;
    rec.gamedown = false;
}

const KeyRecord &KeyTable::record(Key key) const noexcept
{
    static const KeyRecord k_empty{};
    if (!key_in_range(key)) { return k_empty; }
    return keys_[static_cast<std::size_t>(key_index(key))];
}

KeyRecord &KeyTable::record(Key key) noexcept
{
    // Pre: key_in_range(key) — callers (Input's routing core) only ever reach
    // here with keys handed back from string_to_keynum/rotate/event dispatch,
    // all of which are range-checked at the boundary.
    return keys_[static_cast<std::size_t>(key_index(key))];
}

std::vector<BindingEntry> KeyTable::bindings_snapshot() const noexcept
{
    std::vector<BindingEntry> out;
    out.reserve(keys_.size());
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        Key k = key_from_index(static_cast<int>(i));
        out.push_back(BindingEntry{
            k, keynum_to_string(k), keys_[i].binding, keys_[i].down, keys_[i].gamedown, keys_[i].repeats });
    }
    return out;
}

// compliance-allow(thread-assert): private leaf reached ONLY through an
// already-asserting public `Input::` entry point (close-out audit, 2026-07-20).
// Input is permanently T_Main-confined — there is no T_Input split planned or
// warranted — so the assertion belongs at the entry, not repeated in the
// data-and-lookup layer beneath it.
std::string KeyTable::write_bindings_text() const noexcept
{
    // Key_WriteBindings (in_keys.c:462-484): unbindall header, then one
    // `bind "<name>" "<escaped>"` line per bound key, always double-quoted
    // (Compat scope: mod-regex-parsing compat, not incidental style).
    std::string out = "unbindall\n";
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i].binding.empty()) { continue; }
        Key k = key_from_index(static_cast<int>(i));
        out += "bind \"";
        out += keynum_to_string(k);
        out += "\" \"";
        out += escape_for_config(keys_[i].binding);
        out += "\"\n";
    }
    return out;
}

std::string KeyTable::bindlist_text() const noexcept
{
    // Key_Bindlist_f (in_keys.c:492-503): Quirk 15 — raw, unescaped, no
    // unbindall header; diverges from write_bindings_text() deliberately.
    std::string out;
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i].binding.empty()) { continue; }
        Key k = key_from_index(static_cast<int>(i));
        out += keynum_to_string(k);
        out += " \"";
        out += keys_[i].binding;
        out += "\"\n";
    }
    return out;
}

} // namespace xash::input::detail
