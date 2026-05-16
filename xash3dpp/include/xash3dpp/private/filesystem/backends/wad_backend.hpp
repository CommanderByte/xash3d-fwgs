#pragma once
// xash3dpp — WAD2 / WAD3 lump archive backend  (internal)
// Legacy reference: filesystem/wad.c

#include <xash3dpp/private/filesystem/i_search_backend.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem::backends {

class WadBackend final : public ISearchBackend {
public:
    WadBackend(xash::memory::PoolHandle pool,
               std::string_view wad_path, SearchPathFlags flags);

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
    struct Entry {
        std::string   name;         // lowercased, NUL-stripped, max 15 chars
        std::uint32_t offset      = 0;
        std::uint32_t disk_size   = 0;   // bytes on disk
        std::uint32_t uncompressed = 0;  // uncompressed size (== disk_size for WAD)
        std::uint8_t  attribs     = 0;   // ATTR_* flags from dlumpinfo_t
        std::uint8_t  type        = 0;   // TYP_* lump type
    };

    std::string     path_;
    std::string     stem_;  // lowercase WAD basename without extension
    SearchPathFlags flags_;
    std::vector<Entry> entries_;  // @pre-reserved: wad_max_lumps
    std::filesystem::file_time_type file_time_{};
    bool valid_ = false;

    // Find a sorted entry by lowercase name + type (k_TYP_ANY matches first of name).
    [[nodiscard]] const Entry* find_entry(std::string_view name,
                                          std::uint8_t     type) const noexcept;

    // Resolve a caller-supplied path (with optional WAD qualifier) to an entry.
    // Returns nullptr if not found or WAD qualifier does not match this archive.
    [[nodiscard]] const Entry* lookup(std::string_view path) const noexcept;

    // Read raw lump bytes from the WAD file (reopens per call; WAD is read-only).
    [[nodiscard]] std::vector<std::byte> read_lump_bytes(const Entry& e) const;
};

} // namespace xash::filesystem::backends
