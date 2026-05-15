#pragma once
// xash3dpp — platform OS file I/O contract
// Legacy reference: filesystem/filesystem.c (FS_SysOpen, FS_SysRead, …),
//                   filesystem/dir.c (listdirectory),
//                   filesystem/android.c (AAsset bridge)
//
// Implementations:
//   src/platform/win32/os_io.cpp   — Windows
//   src/platform/posix/os_io.cpp   — Linux / macOS / FreeBSD
//   src/platform/android/os_io.cpp — Android (extends POSIX with AAsset)
//
// Porting contract (mandatory, see docs/boundaries/platform-boundary.md):
//   All functions must be implemented for every new platform target.
//   Platform #ifdef blocks are permitted only for optional ABI extensions
//   (e.g. the Android AAsset bridge below).

#include <xash3dpp/platform/os_fd.hpp>

// jni.h must be included at global scope — JNI types are not in any namespace.
#if defined(XASH_ANDROID)
#  include <jni.h>
#endif

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::platform {

// ---------------------------------------------------------------------------
// File open mode flags
// ---------------------------------------------------------------------------

enum class OpenMode : unsigned {
    ReadOnly  = 0,
    WriteOnly = 1,
    ReadWrite = 2,
    Append    = 4,
    Create    = 8,
    Truncate  = 16,
    // In-memory backing: memfd_create on Linux; temp-file fallback elsewhere.
    Memory    = 32,
};

constexpr OpenMode operator|( OpenMode a, OpenMode b ) noexcept {
    return static_cast<OpenMode>( static_cast<unsigned>( a ) | static_cast<unsigned>( b ) );
}
constexpr OpenMode operator&( OpenMode a, OpenMode b ) noexcept {
    return static_cast<OpenMode>( static_cast<unsigned>( a ) & static_cast<unsigned>( b ) );
}
constexpr bool any( OpenMode m ) noexcept { return static_cast<unsigned>( m ) != 0; }

// ---------------------------------------------------------------------------
// File open
// ---------------------------------------------------------------------------

// open a file at |path| (UTF-8) with the given mode flags.
// Returns an invalid OsFd on any error.
// Win32: converts the path to UTF-16 internally.
[[nodiscard]] OsFd open_file( std::string_view path, OpenMode mode ) noexcept;

// Create an anonymous in-memory file backed by the OS.
// Linux: memfd_create.  Win32 / others: self-deleting temporary file.
// Returns an invalid OsFd on failure.
[[nodiscard]] OsFd open_memfd( std::string_view name ) noexcept;

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

[[nodiscard]] std::int64_t read ( OsFd &fd, void       *buf, std::size_t size ) noexcept;
[[nodiscard]] std::int64_t write( OsFd &fd, const void *buf, std::size_t size ) noexcept;

// whence follows POSIX semantics: SEEK_SET=0, SEEK_CUR=1, SEEK_END=2.
[[nodiscard]] std::int64_t seek ( OsFd &fd, std::int64_t offset, int whence ) noexcept;
[[nodiscard]] std::int64_t tell ( OsFd &fd ) noexcept;

void flush( OsFd &fd ) noexcept;

// Close a raw integer fd.  Called by OsFd::close(); do not call directly
// unless you have a raw fd obtained from OsFd::release().
void close_fd( int raw_fd ) noexcept;

// ---------------------------------------------------------------------------
// File metadata
// ---------------------------------------------------------------------------

// Returns the file size in bytes, or nullopt if the path does not exist or
// cannot be stat'd.
[[nodiscard]] std::optional<std::int64_t> file_size( std::string_view path ) noexcept;

// Returns the last-write time, or nullopt on failure.
[[nodiscard]] std::optional<std::filesystem::file_time_type> file_time( std::string_view path ) noexcept;

// ---------------------------------------------------------------------------
// Directory operations
// ---------------------------------------------------------------------------

// Returns the names (not full paths) of all entries under |path|, excluding
// "." and "..".  Returns an empty vector on error or empty directory.
[[nodiscard]] std::vector<std::string> list_directory( std::string_view path ) noexcept;

// Returns true if the directory (or volume) at |path| performs case-insensitive
// name comparisons natively.  When true, the filesystem layer skips its
// in-process trie-based CI search.
//
// Win32:   always true (NTFS default).
// macOS:   always true (HFS+/APFS default).
// Linux:   checks FS_CASEFOLD_FL on the inode (kernel ≥ 5.2).
// Others:  false (conservative default).
[[nodiscard]] bool is_case_insensitive( std::string_view path ) noexcept;

// ---------------------------------------------------------------------------
// Filesystem mutations
// ---------------------------------------------------------------------------

// Create the directory at |path|.  Returns true if it was created or already
// exists.  Does not create intermediate directories.
[[nodiscard]] bool make_directory( std::string_view path ) noexcept;

[[nodiscard]] bool rename_file( std::string_view from, std::string_view to ) noexcept;
[[nodiscard]] bool delete_file( std::string_view path ) noexcept;

// ---------------------------------------------------------------------------
// Android AAsset bridge (XASH_ANDROID builds only)
// ---------------------------------------------------------------------------
// Declared here so android_backend.cpp can include a single header.
// Definitions live in src/platform/android/os_io.cpp.
// On non-Android builds this section is compiled away entirely.

#if defined(XASH_ANDROID)
// Opaque handle; full definition in src/platform/android/os_io.cpp.
struct AssetManagerHandle;

// Must be called once at JNI_OnLoad time before any asset operations.
void android_init_jni( JNIEnv *env, jobject activity,
                        jclass activity_class ) noexcept;

// Obtain the AAssetManager for the engine or app package via JNI.
// Returns nullptr if android_init_jni() has not been called.
AssetManagerHandle *get_asset_manager( bool engine_package ) noexcept;

// List asset names directly under |path| (non-recursive; no "." / "..").
[[nodiscard]] std::vector<std::string> list_assets( AssetManagerHandle *mgr,
                                       std::string_view path ) noexcept;

// Return true if an asset at |path| exists in this package.
[[nodiscard]] bool asset_exists( AssetManagerHandle *mgr, std::string_view path ) noexcept;

// Copy the asset at |path| into an anonymous in-memory fd.
// The returned OsFd is seeked to position 0; its length = seek(fd, 0, SEEK_END).
// Returns an invalid OsFd if the asset does not exist.
[[nodiscard]] OsFd open_asset( AssetManagerHandle *mgr, std::string_view path ) noexcept;
#endif // XASH_ANDROID

} // namespace xash::platform
