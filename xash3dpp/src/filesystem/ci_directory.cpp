// xash3dpp — case-insensitive directory name resolver  (internal)
// Legacy reference: filesystem/dir.c  (FS_BuildTrie, FS_FixFileCase)

#include <xash3dpp/private/filesystem/ci_directory.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace xash::filesystem {

using ::xash::platform::OsFd;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CIDirectory::CIDirectory(std::string_view root_path)
    : root_{root_path}
{
    mode_ = ::xash::platform::is_case_insensitive(root_path)
                ? Mode::Native
                : Mode::Emulated;
}

// ---------------------------------------------------------------------------
// resolve — return canonical on-disk entry name for `name` inside `subdir`
// ---------------------------------------------------------------------------

std::optional<std::string> CIDirectory::resolve(std::string_view subdir,
                                                 std::string_view name)
{
    if (mode_ == Mode::Native) {
        // The OS already handles case — return the name as supplied.
        return std::string{name};
    }

    const std::string dir_key{subdir};
    std::lock_guard   lock{cache_mutex_};
    const auto& entries = get_or_populate(dir_key);

    // Binary search on the CI-sorted cache.
    auto it = std::lower_bound(entries.begin(), entries.end(), name,
        [](const std::string& a, std::string_view b) { return xash::utilities::ci_less(a, b); });

    if (it != entries.end() && xash::utilities::ci_equal(*it, name)) return *it;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// glob — list all entries in `subdir` matching `pattern`
// ---------------------------------------------------------------------------

std::vector<std::string> CIDirectory::glob(std::string_view subdir,
                                            std::string_view pattern,
                                            bool case_insensitive)
{
    const std::string dir_key{subdir};
    std::lock_guard   lock{cache_mutex_};
    const auto& entries = get_or_populate(dir_key);

    std::vector<std::string> results;
    for (const auto& e : entries) {
        if (xash::utilities::match_pattern(e, pattern, case_insensitive))
            results.push_back(e);
    }
    return results;
}

// ---------------------------------------------------------------------------
// invalidate
// ---------------------------------------------------------------------------

void CIDirectory::invalidate(std::string_view subdir) {
    std::lock_guard lock{cache_mutex_};
    cache_.erase(std::string{subdir});
}

// ---------------------------------------------------------------------------
// Internal: populate (or return cached) CI-sorted entry list for `dir`
// ---------------------------------------------------------------------------

const std::vector<std::string>&
CIDirectory::get_or_populate(const std::string& dir) const
{
    auto it = cache_.find(dir);
    if (it != cache_.end()) return it->second;

    const std::string full = xash::utilities::path_join(root_, dir);
    auto entries = ::xash::platform::list_directory(full);

    // Sort case-insensitively so resolve's binary search is correct.
    std::sort(entries.begin(), entries.end(),
        [](const std::string& a, const std::string& b) {
            return xash::utilities::ci_less(a, b);
        });

    auto [ins, _] = cache_.emplace(dir, std::move(entries));
    return ins->second;
}

} // namespace xash::filesystem
