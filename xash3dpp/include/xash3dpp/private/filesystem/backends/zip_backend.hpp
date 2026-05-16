#pragma once
// xash3dpp — ZIP / PK3 archive backend  (internal)
// Legacy reference: filesystem/zip.c

#include <xash3dpp/private/filesystem/i_search_backend.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>

#include <memory>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem::backends {

class ZipBackend final : public ISearchBackend {
public:
    ZipBackend(xash::memory::PoolHandle pool,
               std::string_view zip_path, SearchPathFlags flags);

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

private:
    // Central-directory record — modernization finding M-8: std::vector replaces
    // the flexible-array TOC table.
    struct Entry {
        std::string   name;                // full path within archive (original case)
        std::uint32_t data_offset = 0;     // absolute byte offset of compressed data
        std::uint32_t comp_size   = 0;
        std::uint32_t uncomp_size = 0;
        std::uint16_t method      = 0;     // 0 = stored, 8 = deflate
    };

    std::string     path_;
    SearchPathFlags flags_;
    std::vector<Entry> entries_;  // @pre-reserved: zip_max_files
    std::filesystem::file_time_type file_time_{};
    bool valid_ = false;

    // Binary search (case-insensitive); returns nullptr if not found.
    [[nodiscard]] const Entry* find_entry(std::string_view name) const noexcept;
};

} // namespace xash::filesystem::backends
