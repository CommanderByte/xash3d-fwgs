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

    [[nodiscard]] static std::unique_ptr<ISearchBackend>
        create(xash::memory::PoolHandle pool,
               std::string_view path, SearchPathFlags flags);

    [[nodiscard]] std::string info() const override;

    [[nodiscard]] std::unique_ptr<File> open_file(std::string_view path,
                                   std::string_view mode) override;

    [[nodiscard]] std::optional<std::filesystem::file_time_type>
        file_time(std::string_view path) override;

    [[nodiscard]] std::optional<std::string> find_file(std::string_view path) override;

    [[nodiscard]] std::vector<std::string> search(std::string_view pattern,
                                    bool case_insensitive) override;

    [[nodiscard]] std::vector<std::byte> load_file(std::string_view path) override;

    void invalidate_directory(std::string_view subdir) noexcept override;

private:
    std::string     root_;
    SearchPathFlags flags_;
    CIDirectory     ci_;

    // Walk each component of `rel_path` through the CI resolver.
    // Returns the canonical relative path, or empty string if any component
    // is not found (i.e. the path does not exist under root_).
    [[nodiscard]] std::string resolve_path(std::string_view rel_path);
};

} // namespace xash::filesystem::backends
