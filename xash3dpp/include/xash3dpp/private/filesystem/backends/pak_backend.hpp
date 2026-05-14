#pragma once
// xash3dpp — Quake PAK archive backend  (internal)
// Legacy reference: filesystem/pak.c

#include <xash3dpp/private/filesystem/i_search_backend.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>

#include <memory>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem::backends {

class PakBackend final : public ISearchBackend {
public:
    PakBackend(std::string_view pak_path, SearchPathFlags flags);

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
    struct Entry {
        std::string   name;    // full path within the PAK (original case)
        std::uint32_t offset = 0;
        std::uint32_t size   = 0;
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
