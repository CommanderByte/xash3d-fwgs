#pragma once
// xash3dpp — file path utilities
// Legacy reference: public/crtlib.h  (COM_FileBase, COM_FileExtension, …)
//
// @thread-safety: pure functions — safe from any thread.

#include <cstddef>
#include <string>
#include <string_view>

namespace xash::utilities {

// "path/to/file.ext"  →  "file"   (strips directory and extension)
// Legacy: COM_FileBase
void file_base( const char *path, char *out, std::size_t size ) noexcept;

// "path/to/file.ext"  →  ".ext"   (pointer into 'path', never nullptr)
// Legacy: COM_FileExtension
[[nodiscard]] std::string_view file_extension( std::string_view path ) noexcept;

// Append 'ext' if 'path' has no extension yet.
// Legacy: COM_DefaultExtension
void default_extension( char *path, const char *ext, std::size_t size ) noexcept;

// Replace (or add) the extension on 'path'.
// Legacy: COM_ReplaceExtension
void replace_extension( char *path, const char *ext, std::size_t size ) noexcept;

// Remove the filename, leaving only the directory part (including trailing slash).
// Legacy: COM_ExtractFilePath
void extract_dir( const char *path, char *out ) noexcept;

// Returns a pointer to the filename component inside 'path'.
// Legacy: COM_FileWithoutPath
[[nodiscard]] std::string_view filename( std::string_view path ) noexcept;

// Remove the extension in-place.
// Legacy: COM_StripExtension
void strip_extension( char *path ) noexcept;

// Normalise all '\\' to '/'.
// Legacy: COM_PathSlashFix
void fix_slashes( char *path ) noexcept;

// Strip trailing '\n' / '\r'.
// Legacy: COM_RemoveLineFeed
void        remove_line_feed( char *str, std::size_t size ) noexcept;
[[nodiscard]] std::string remove_line_feed( std::string_view s );

// Trim leading and trailing whitespace into 'dst'.
// Legacy: COM_TrimSpace
void trim_space( char *dst, const char *src, std::size_t size ) noexcept;

// ---------------------------------------------------------------------------
// std::string-returning overloads — convenient for cold / config code.
// The raw (char*, size_t) overloads above remain for hot paths.
// ---------------------------------------------------------------------------

[[nodiscard]] std::string file_base( std::string_view path );
[[nodiscard]] std::string strip_extension( std::string_view path );
[[nodiscard]] std::string fix_slashes( std::string_view path );
[[nodiscard]] std::string extract_dir( std::string_view path );
[[nodiscard]] std::string default_extension( std::string_view path, std::string_view ext );
[[nodiscard]] std::string replace_extension( std::string_view path, std::string_view ext );
[[nodiscard]] std::string trim_space( std::string_view src );

// Join a directory and a relative path with a single '/' separator.
// If 'dir' already ends with '/' or '\\', no extra separator is added.
[[nodiscard]] std::string path_join( std::string_view dir, std::string_view rel );

// Three-segment convenience overload: path_join(a, b, c) == path_join(path_join(a,b), c).
[[nodiscard]] std::string path_join( std::string_view a, std::string_view b, std::string_view c );

} // namespace xash::utilities
