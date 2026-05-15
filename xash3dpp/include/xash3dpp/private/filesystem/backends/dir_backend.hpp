#pragma once
// xash3dpp — plain-directory search backend  (internal)
// Legacy reference: filesystem/filesystem.c  (dir_backend_t / pfnSearch dispatch)

#include <xash3dpp/private/filesystem/i_search_backend.hpp>
#include <xash3dpp/private/filesystem/ci_directory.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace xash::filesystem::backends {

class DirBackend final : public ISearchBackend {
public:
    DirBackend(xash::memory::PoolHandle pool,
               std::string_view root_path, SearchPathFlags flags);

    static std::unique_ptr<ISearchBackend>
        Create(xash::memory::PoolHandle pool,
               std::string_view path, SearchPathFlags flags);

    std::string Info() const override;

    std::unique_ptr<File> OpenFile(std::string_view path,
                                   std::string_view mode) override;

    std::optional<std::filesystem::file_time_type>
        file_time(std::string_view path) override;

    std::optional<std::string> FindFile(std::string_view path) override;

    std::vector<std::string> search(std::string_view pattern,
                                    bool case_insensitive) override;

    std::vector<std::byte> load_file(std::string_view path) override;

    void InvalidateDirectory(std::string_view subdir) noexcept override;

private:
    std::string     root_;
    SearchPathFlags flags_;
    CIDirectory     ci_;

    // Walk each component of `rel_path` through the CI resolver.
    // Returns the canonical relative path, or empty string if any component
    // is not found (i.e. the path does not exist under root_).
    std::string resolve_path(std::string_view rel_path);
};

} // namespace xash::filesystem::backends
