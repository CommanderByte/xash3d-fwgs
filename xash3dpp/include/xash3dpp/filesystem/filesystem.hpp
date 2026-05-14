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

    // ---- Lifecycle --------------------------------------------------------

    // Scan rootdir / rodir for all game directories and add static paths.
    bool Init(std::string_view rootdir,
              std::string_view basedir,
              std::string_view gamedir,
              std::string_view rodir = {});

    void Shutdown();

    // Select a game directory by folder name and rebuild search paths.
    // The engine calls scan_game_directories() (gameinfo_parser.hpp) first,
    // presents the list to the user or reads it from config, then calls this.
    bool ActivateGame(std::string_view      gamefolder,
                      SearchPathFlags        mount_flags,
                      std::string_view       language = {});

    // Rebuild search paths for the current GameInfo without changing game selection.
    void Rescan(SearchPathFlags mount_flags, std::string_view language = {});

    // ---- Search-path management ------------------------------------------

    void AddGameDirectory(std::string_view dir, SearchPathFlags flags);

    // Mount the full game hierarchy (basedir → falldir → gamedir, plus
    // HD/LV/addon/l10n variants according to flags).
    void AddGameHierarchy(std::string_view dir, SearchPathFlags flags);

    // Remove all non-Static entries.
    void ClearPaths();

    // Toggle permission for absolute paths and "../" traversal.
    // Must always be restored to false by the caller after use.
    void AllowDirectPaths(bool enable);

    // Mount a single archive by absolute disk path.
    bool MountArchive(std::string_view path, SearchPathFlags flags);

    // ---- File I/O --------------------------------------------------------

    // Open a file for streaming.  Returns nullptr on failure.
    // mode follows fopen conventions ("r", "rb", "w", "wb", …).
    std::unique_ptr<File> Open(std::string_view path,
                               std::string_view mode,
                               bool gamedironly = false);

    // Whole-file load.  Returns an empty vector on failure.
    // The vector's size() reflects the exact on-disk byte count.
    std::vector<std::byte> LoadFile(std::string_view path, bool gamedironly = false);

    // Bypass VFS — read directly from a disk path.
    std::vector<std::byte> LoadDirectFile(std::string_view disk_path) const;

    bool WriteFile(std::string_view path, std::span<const std::byte> data);

    // ---- Queries ---------------------------------------------------------

    bool FileExists(std::string_view path, bool gamedironly = false) const;

    std::optional<FsOffset> FileSize(std::string_view path,
                                      bool gamedironly = false) const;

    std::optional<std::filesystem::file_time_type>
        FileTime(std::string_view path, bool gamedironly = false) const;

    // Returns the on-disk path if the file lives in a plain directory,
    // or nullopt if it is inside a packed archive.
    std::optional<std::string> DiskPath(std::string_view name,
                                         bool gamedironly = false) const;

    SearchResult Search(std::string_view pattern,
                        bool case_insensitive = true,
                        bool gamedironly      = false) const;

    bool Rename(std::string_view from, std::string_view to);
    bool Delete(std::string_view path);

    // ---- Hashing ---------------------------------------------------------

    std::optional<std::uint32_t>             CRC32File(std::string_view path);
    std::optional<std::array<std::byte, 16>> MD5File(std::string_view path);

    // ---- Library resolution ---------------------------------------------

    // Resolve a game library name to an absolute disk path.
    // TODO: move to a platform::dynlib layer once that subsystem exists.
    //       Currently here because it needs VFS path search.
    std::optional<std::string> FindLibrary(std::string_view name);

    // ---- Game info -------------------------------------------------------

    // Discover all candidate game directories under `root`.
    // Call this before ActivateGame(); present the result to the user or
    // read the selection from config, then call ActivateGame().
    std::vector<GameInfo> ScanGameDirectories(std::string_view root) const;

    std::string      Gamedir()     const;
    GameInfo          GetGameInfo() const;  // valid only after ActivateGame()

    // ---- Root directory --------------------------------------------------

    std::string_view GetRootDirectory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::filesystem
