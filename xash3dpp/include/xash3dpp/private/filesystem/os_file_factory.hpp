#pragma once
// xash3dpp — factory for OsFile instances  (internal)
//
// Backends that need streaming files call make_os_file() instead of
// constructing OsFile directly; OsFile is private to file.cpp.
//
// mode_flags() converts a C-style fopen mode string to a platform::OpenMode
// bitmask and is placed here so every backend can share it.

#include <xash3dpp/filesystem/file.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/platform/os_io.hpp>

#include <memory>
#include <string_view>

namespace xash::filesystem {

// Create a streaming OsFile wrapping `fd`.
// `pool`        — memory pool to allocate the OsFile from.
// `length`      — logical (uncompressed) size in bytes.
// `real_offset` — byte offset of the entry within an archive (0 for plain files).
// `deflated`    — true for zlib-compressed archive entries.
// Defined in src/filesystem/file.cpp.
[[nodiscard]] std::unique_ptr<File> make_os_file(xash::memory::PoolHandle pool,
                                   ::xash::platform::OsFd fd,
                                   FsOffset length,
                                   FsOffset real_offset = 0,
                                   bool     deflated    = false);

// Map a C-style fopen mode string to a platform::OpenMode bitmask.
// Handles: "r", "rb", "w", "wb", "a", "ab", "r+", "r+b", "w+", "w+b",
//          "a+", "a+b"  (and "b"-prefixed variants).
[[nodiscard]] inline ::xash::platform::OpenMode mode_flags(std::string_view mode) noexcept {
    using M = ::xash::platform::OpenMode;
    const bool has_w    = mode.find('w') != std::string_view::npos;
    const bool has_a    = mode.find('a') != std::string_view::npos;
    const bool has_plus = mode.find('+') != std::string_view::npos;

    if (has_w) {
        const M base = has_plus ? M::ReadWrite : M::WriteOnly;
        return base | M::create | M::Truncate;
    }
    if (has_a) {
        const M base = has_plus ? M::ReadWrite : M::WriteOnly;
        return base | M::create | M::Append;
    }
    // "r" or "r+"
    return has_plus ? M::ReadWrite : M::ReadOnly;
}

} // namespace xash::filesystem
