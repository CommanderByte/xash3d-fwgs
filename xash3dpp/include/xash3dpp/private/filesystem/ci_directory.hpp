#pragma once
// xash3dpp — case-insensitive directory name resolver  (internal)
// Legacy reference: filesystem/dir.c  (dir_t trie, listdirectory)
//
// At construction, probes whether the underlying volume already handles
// case-insensitivity natively (Windows, macOS, Linux CASEFOLD_FL).
// In Native mode all calls delegate to the OS.  In Emulated mode a
// per-subdirectory sorted name cache is built lazily on first access.

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xash::filesystem {

class CIDirectory {
public:
    explicit CIDirectory(std::string_view root_path);

    // Returns the canonical (exact on-disk) entry name for `name` inside
    // `subdir` (relative to root_path_), or nullopt if not found.
    std::optional<std::string> Resolve(std::string_view subdir,
                                       std::string_view name);

    // Returns all entries in `subdir` matching `pattern` (glob).
    std::vector<std::string> Glob(std::string_view subdir,
                                  std::string_view pattern,
                                  bool case_insensitive);

    // Invalidate the cache for `subdir` after a write operation.
    void Invalidate(std::string_view subdir);

private:
    enum class Mode { Native, Emulated };

    Mode        mode_;
    std::string root_;

    // Emulated mode only: map from subdir path → sorted list of entry names.
    mutable std::unordered_map<std::string, std::vector<std::string>> cache_;
    mutable std::mutex                                                 cache_mutex_;

    // Populate (or return cached) entry list for `dir`.
    const std::vector<std::string>& get_or_populate(const std::string& dir) const;
};

} // namespace xash::filesystem
