#pragma once
// xash3dpp — polymorphic search-path backend interface  (internal)
// Legacy reference: filesystem/filesystem_internal.h  (7 raw fn-ptrs in searchpath_t)
// Modernization finding H-1: replaces hand-rolled vtable with virtual class.

#include <xash3dpp/filesystem/file.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem {

class ISearchBackend {
public:
    virtual ~ISearchBackend() = default;

    // Human-readable description for debug/path-dump output.
    // Replaces the legacy pfnPrintInfo(char *dst, size_t size) out-buffer pattern.
    virtual std::string Info() const = 0;

    // Returns nullptr if the file does not exist in this backend.
    virtual std::unique_ptr<File> OpenFile(std::string_view path,
                                           std::string_view mode) = 0;

    // Returns nullopt if the file does not exist.
    virtual std::optional<std::filesystem::file_time_type>
        FileTime(std::string_view path) = 0;

    // Case-insensitive name resolution.
    // Returns the canonical (exact on-disk) name, or nullopt if not found.
    virtual std::optional<std::string> FindFile(std::string_view path) = 0;

    // Glob search within this backend; returns all matching entry names.
    virtual std::vector<std::string> Search(std::string_view pattern,
                                            bool case_insensitive) = 0;

    // Whole-file load.  Returns an empty vector if not found.
    virtual std::vector<std::byte> LoadFile(std::string_view path) = 0;
};

// Returns true when 'mode' requests write or append access.
// All read-only backends (PAK, ZIP, WAD) use this to reject writes early.
inline bool is_write_mode( std::string_view mode ) noexcept
{
    return mode.find( 'w' ) != std::string_view::npos
        || mode.find( 'a' ) != std::string_view::npos;
}

} // namespace xash::filesystem
