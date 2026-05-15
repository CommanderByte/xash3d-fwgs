#pragma once
// xash3dpp — virtual filesystem public API
// Legacy reference: filesystem/filesystem.h  (fs_api_t), filesystem/fscallback.h
//
// No C ABI.  No const char * overloads.  All paths are std::string_view.
// The Filesystem class uses pimpl so no internal type (SearchPath,
// ISearchBackend, …) leaks into this header.

#include <xash3dpp/filesystem/file.hpp>
#include <xash3dpp/gameinfo.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem {

// Import GameInfo into this namespace so filesystem-internal code can use
// the unqualified name.  External callers should use xash::GameInfo directly.
using GameInfo = ::xash::GameInfo;

// Result of a glob search across all active search paths.
struct SearchResult {
    std::vector<std::string> files;
};

class Filesystem {
public:
    Filesystem();
    ~Filesystem();

    Filesystem(const Filesystem&)            = delete;
    Filesystem& operator=(const Filesystem&) = delete;
    Filesystem(Filesystem&&) noexcept;
    Filesystem& operator=(Filesystem&&) noexcept;

    // ---- Lifecycle --------------------------------------------------------

    // Scan rootdir / rodir for all game directories and add static paths.
    [[nodiscard]] bool init(std::string_view rootdir,
              std::string_view basedir,
              std::string_view gamedir,
              std::string_view rodir = {});

    void shutdown();

    // Select a game directory by folder name and rebuild search paths.
    // The engine calls scan_game_directories() (gameinfo_parser.hpp) first,
    // presents the list to the user or reads it from config, then calls this.
    [[nodiscard]] bool activate_game(std::string_view      gamefolder,
                      SearchPathFlags        mount_flags,
                      std::string_view       language = {});

    // Rebuild search paths for the current GameInfo without changing game selection.
    void rescan(SearchPathFlags mount_flags, std::string_view language = {});

    // ---- search-path management ------------------------------------------

    void add_game_directory(std::string_view dir, SearchPathFlags flags);

    // Mount the full game hierarchy (basedir → falldir → gamedir, plus
    // HD/LV/addon/l10n variants according to flags).
    void add_game_hierarchy(std::string_view dir, SearchPathFlags flags);

    // Remove all non-Static entries.
    void clear_paths();

    // Toggle permission for absolute paths and "../" traversal.
    // Must always be restored to false by the caller after use.
    void allow_direct_paths(bool enable);

    // Mount a single archive by absolute disk path.
    [[nodiscard]] bool mount_archive(std::string_view path, SearchPathFlags flags);

    // ---- File I/O --------------------------------------------------------

    // open a file for streaming.  Returns nullptr on failure.
    // mode follows fopen conventions ("r", "rb", "w", "wb", …).
    [[nodiscard]] std::unique_ptr<File> open(std::string_view path,
                               std::string_view mode,
                               bool gamedironly = false);

    // Whole-file load.  Returns an empty vector on failure.
    // The vector's size() reflects the exact on-disk byte count.
    [[nodiscard]] std::vector<std::byte> load_file(std::string_view path, bool gamedironly = false);

    // Bypass VFS — read directly from a disk path.
    [[nodiscard]] std::vector<std::byte> load_direct_file(std::string_view disk_path) const;

    [[nodiscard]] bool write_file(std::string_view path, std::span<const std::byte> data);

    // ---- Queries ---------------------------------------------------------

    [[nodiscard]] bool file_exists(std::string_view path, bool gamedironly = false) const;

    [[nodiscard]] std::optional<FsOffset> file_size(std::string_view path,
                                      bool gamedironly = false) const;

    [[nodiscard]] std::optional<std::filesystem::file_time_type>
        file_time(std::string_view path, bool gamedironly = false) const;

    // Returns the on-disk path if the file lives in a plain directory,
    // or nullopt if it is inside a packed archive.
    [[nodiscard]] std::optional<std::string> disk_path(std::string_view name,
                                         bool gamedironly = false) const;

    [[nodiscard]] SearchResult search(std::string_view pattern,
                        bool case_insensitive = true,
                        bool gamedironly      = false) const;

    [[nodiscard]] bool rename(std::string_view from, std::string_view to);
    [[nodiscard]] bool remove(std::string_view path);

    // ---- Hashing ---------------------------------------------------------

    [[nodiscard]] std::optional<std::uint32_t>             crc32_file(std::string_view path);
    [[nodiscard]] std::optional<std::array<std::byte, 16>> md5_file(std::string_view path);

    // ---- Library resolution ---------------------------------------------

    // Resolve a game library name to an absolute disk path.
    // TODO: move to a platform::dynlib layer once that subsystem exists.
    //       Currently here because it needs VFS path search.
    [[nodiscard]] std::optional<std::string> find_library(std::string_view name);

    // ---- Game info -------------------------------------------------------

    // Discover all candidate game directories under `root`.
    // Call this before activate_game(); present the result to the user or
    // read the selection from config, then call activate_game().
    [[nodiscard]] std::vector<GameInfo> scan_game_directories(std::string_view root) const;

    [[nodiscard]] std::string      gamedir()     const;
    [[nodiscard]] GameInfo  get_game_info() const;  // valid only after activate_game()

    // ---- Root directory --------------------------------------------------

    [[nodiscard]] std::string_view get_root_directory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::filesystem
