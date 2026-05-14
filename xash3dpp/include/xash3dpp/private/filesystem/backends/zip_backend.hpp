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
    ZipBackend(std::string_view zip_path, SearchPathFlags flags);

    static std::unique_ptr<ISearchBackend>
        Create(std::string_view path, SearchPathFlags flags);

    std::string Info() const override;

    std::unique_ptr<File> OpenFile(std::string_view path,
                                   std::string_view mode) override;

    std::optional<std::filesystem::file_time_type>
        FileTime(std::string_view path) override;

    std::optional<std::string> FindFile(std::string_view path) override;

    std::vector<std::string> Search(std::string_view pattern,
                                    bool case_insensitive) override;

    std::vector<std::byte> LoadFile(std::string_view path) override;

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
    std::vector<Entry> entries_;
    std::filesystem::file_time_type file_time_{};
    bool valid_ = false;

    // Binary search (case-insensitive); returns nullptr if not found.
    const Entry* find_entry(std::string_view name) const noexcept;
};

} // namespace xash::filesystem::backends
