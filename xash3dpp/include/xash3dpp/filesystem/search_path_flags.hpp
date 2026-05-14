#pragma once
// xash3dpp — search path flag type
// Legacy reference: filesystem/filesystem.h  (FS_STATIC_PATH, FS_NOWRITE_PATH, …)

#include <cstdint>

namespace xash::filesystem {

enum class SearchPathFlags : std::uint32_t {
    None       = 0,
    Static     = 1 << 0,  // survives ClearPaths()
    NoWrite    = 1 << 1,  // never selected as the write target
    GameDir    = 1 << 2,  // part of the active game hierarchy
    Exec       = 1 << 3,  // may serve native library files (.so / .dll)
    Custom     = 1 << 4,  // injected outside the normal game hierarchy
    SkipWads   = 1 << 5,  // do not auto-mount WADs found inside this archive
    MountHD    = 1 << 6,  // include _hd variant directories
    MountLV    = 1 << 7,  // include _lv (low-violence) directories
    MountAddon = 1 << 8,  // include addon directories
    MountL10n  = 1 << 9,  // include localisation directories
};

constexpr SearchPathFlags operator|(SearchPathFlags a, SearchPathFlags b) noexcept {
    return static_cast<SearchPathFlags>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
constexpr SearchPathFlags operator&(SearchPathFlags a, SearchPathFlags b) noexcept {
    return static_cast<SearchPathFlags>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
constexpr SearchPathFlags operator~(SearchPathFlags a) noexcept {
    return static_cast<SearchPathFlags>(~static_cast<std::uint32_t>(a));
}
constexpr SearchPathFlags& operator|=(SearchPathFlags& a, SearchPathFlags b) noexcept {
    return a = a | b;
}
constexpr SearchPathFlags& operator&=(SearchPathFlags& a, SearchPathFlags b) noexcept {
    return a = a & b;
}
constexpr bool any(SearchPathFlags f) noexcept {
    return static_cast<std::uint32_t>(f) != 0;
}

} // namespace xash::filesystem
