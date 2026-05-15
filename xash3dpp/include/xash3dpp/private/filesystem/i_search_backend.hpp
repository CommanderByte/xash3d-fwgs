#pragma once
// xash3dpp — polymorphic search-path backend interface  (internal)
// Legacy reference: filesystem/filesystem_internal.h  (7 raw fn-ptrs in searchpath_t)
// Modernization finding H-1: replaces hand-rolled vtable with virtual class.

#include <xash3dpp/filesystem/file.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem {

class ISearchBackend {
public:
    explicit ISearchBackend(xash::memory::PoolHandle pool) noexcept
        : pool_{pool} {}
    virtual ~ISearchBackend() = default;

    // Pool-aware deallocation — called by std::unique_ptr<ISearchBackend>'s
    // default deleter when *this was created via pool_new.
    static void operator delete(void* p) noexcept
    {
        xash::memory::mem_free(p);
    }
    static void operator delete(void* p, std::size_t) noexcept
    {
        xash::memory::mem_free(p);
    }

    // Human-readable description for debug/path-dump output.
    // Replaces the legacy pfnPrintInfo(char *dst, size_t size) out-buffer pattern.
    virtual std::string Info() const = 0;

    // Returns nullptr if the file does not exist in this backend.
    virtual std::unique_ptr<File> OpenFile(std::string_view path,
                                           std::string_view mode) = 0;

    // Returns nullopt if the file does not exist.
    virtual std::optional<std::filesystem::file_time_type>
        file_time(std::string_view path) = 0;

    // Case-insensitive name resolution.
    // Returns the canonical (exact on-disk) name, or nullopt if not found.
    virtual std::optional<std::string> FindFile(std::string_view path) = 0;

    // Glob search within this backend; returns all matching entry names.
    virtual std::vector<std::string> search(std::string_view pattern,
                                            bool case_insensitive) = 0;

    // Whole-file load.  Returns an empty vector if not found.
    virtual std::vector<std::byte> load_file(std::string_view path) = 0;

    // Invalidate the per-subdirectory name cache for `subdir` (relative to
    // this backend's root).  Called after write_file / rename so that
    // subsequent FindFile calls see newly created entries.
    // Default is a no-op — archive backends have immutable contents.
    virtual void InvalidateDirectory(std::string_view /*subdir*/) noexcept {}

protected:
    xash::memory::PoolHandle pool_;
};

// Returns true when 'mode' requests write or append access.
// All read-only backends (PAK, ZIP, WAD) use this to reject writes early.
inline bool is_write_mode( std::string_view mode ) noexcept
{
    return mode.find( 'w' ) != std::string_view::npos
        || mode.find( 'a' ) != std::string_view::npos;
}

} // namespace xash::filesystem
