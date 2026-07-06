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
    [[nodiscard]] std::optional<std::string> resolve(std::string_view subdir,
                                       std::string_view name);

    // Returns all entries in `subdir` matching `pattern` (glob).
    [[nodiscard]] std::vector<std::string> glob(std::string_view subdir,
                                  std::string_view pattern,
                                  bool case_insensitive);

    // invalidate the cache for `subdir` after a write operation.
    void invalidate(std::string_view subdir);

private:
    enum class Mode { Native, Emulated };

    Mode        mode_;
    std::string root_;

    // Emulated mode only: map from subdir path → sorted list of entry names.
    mutable std::unordered_map<std::string, std::vector<std::string>> cache_; // @pre-reserved: warm lazily-populated per-directory listing cache; bounded by mounted directories, filled on first access, no pre-sizing (reserve N/A)
    mutable std::mutex                                                 cache_mutex_;

    // Populate (or return cached) entry list for `dir`.
    [[nodiscard]] const std::vector<std::string>& get_or_populate(const std::string& dir) const;
};

} // namespace xash::filesystem
