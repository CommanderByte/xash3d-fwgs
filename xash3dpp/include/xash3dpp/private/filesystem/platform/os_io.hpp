#pragma once
// xash3dpp — platform-neutral OS I/O declarations  (internal)
// Legacy reference: filesystem/filesystem.c (FS_SysOpen, FS_SysRead, …),
//                   filesystem/dir.c (listdirectory), filesystem/android.c
//
// Implementations:
//   platform/posix.cpp   — Linux / macOS
//   platform/win32.cpp   — Windows
//   platform/android.cpp — Android AAsset extensions (XASH_ANDROID only)
//
// No platform #ifdef blocks appear above this header.

#include <xash3dpp/private/filesystem/os_fd.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem::platform {

// ---- File open -----------------------------------------------------------

enum class OpenMode : unsigned {
    ReadOnly  = 0,
    WriteOnly = 1,
    ReadWrite = 2,
    Append    = 4,
    Create    = 8,
    Truncate  = 16,
    // In-memory (memfd_create on Linux; stub on other platforms)
    Memory    = 32,
};

constexpr OpenMode operator|(OpenMode a, OpenMode b) noexcept {
    return static_cast<OpenMode>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr OpenMode operator&(OpenMode a, OpenMode b) noexcept {
    return static_cast<OpenMode>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
constexpr bool any(OpenMode m) noexcept { return static_cast<unsigned>(m) != 0; }

// Open a file; returns an invalid OsFd on failure.
OsFd open_file(std::string_view path, OpenMode mode) noexcept;

// Create an anonymous in-memory file (Linux memfd_create; stub elsewhere).
OsFd open_memfd(std::string_view name) noexcept;

// ---- Read / write / seek -------------------------------------------------

std::int64_t read(OsFd& fd, void* buf, std::size_t size) noexcept;
std::int64_t write(OsFd& fd, const void* buf, std::size_t size) noexcept;
std::int64_t seek(OsFd& fd, std::int64_t offset, int whence) noexcept;
std::int64_t tell(OsFd& fd) noexcept;
void         flush(OsFd& fd) noexcept;

// Called by OsFd::close().
void close_fd(int raw_fd) noexcept;

// ---- File metadata -------------------------------------------------------

std::optional<std::int64_t>                    file_size(std::string_view path) noexcept;
std::optional<std::filesystem::file_time_type> file_time(std::string_view path) noexcept;

// ---- Directory operations ------------------------------------------------

// Returns all entries in `path` except "." and "..".
std::vector<std::string> list_directory(std::string_view path) noexcept;

// Returns true if the directory (or volume) at `path` already performs
// case-insensitive comparisons natively, so no in-process emulation trie
// is needed.  On Linux this checks for FS_CASEFOLD_FL on the inode.
bool is_case_insensitive(std::string_view path) noexcept;

// ---- Filesystem mutations ------------------------------------------------

bool make_directory(std::string_view path) noexcept;
bool rename_file(std::string_view from, std::string_view to) noexcept;
bool delete_file(std::string_view path) noexcept;

// ---- Android AAsset (XASH_ANDROID only) ----------------------------------
// Declared here so the android_backend can include a single header.
// Definitions live in platform/android.cpp and are excluded on other targets.

#if defined(XASH_ANDROID)
#include <jni.h>

struct AssetManagerHandle;  // opaque; full definition in platform/android.cpp

// Must be called once before any asset operations (from JNI_OnLoad or equivalent).
void android_init_jni(JNIEnv* env, jobject activity, jclass activity_class) noexcept;

// Retrieve the AAssetManager for the engine or app package via JNI.
// Returns nullptr if android_init_jni() has not been called or JNI fails.
AssetManagerHandle* get_asset_manager(bool engine_package) noexcept;

// List asset names directly under `path` (not recursive; no "." / "..").
std::vector<std::string> list_assets(AssetManagerHandle* mgr,
                                     std::string_view path) noexcept;

// Return true if an asset at `path` exists in this package.
bool asset_exists(AssetManagerHandle* mgr, std::string_view path) noexcept;

// Copy the asset at `path` into an anonymous in-memory fd (memfd).
// The returned fd is seeked to position 0; length = seek(fd, 0, SEEK_END).
// Returns an invalid OsFd if the asset does not exist.
OsFd open_asset(AssetManagerHandle* mgr, std::string_view path) noexcept;
#endif

} // namespace xash::filesystem::platform
