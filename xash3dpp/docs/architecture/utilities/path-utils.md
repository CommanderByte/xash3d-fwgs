# Path utilities

> **Header**: `xash3dpp/include/xash3dpp/utilities/path.hpp`  
> **Source**: `xash3dpp/src/utilities/path.cpp`  
> **Namespace**: `xash::utilities`  
> **Legacy reference**: `public/crtlib.h`, `public/crtlib.c` (`COM_FileBase`,
> `COM_FileExtension`, `COM_DefaultExtension`, `COM_ExtractFilePath`, etc.)

## Purpose

Provides string-level path manipulation: extension handling, directory
extraction, basename extraction, slash normalisation, and path joining. No file
system access is performed; this is pure string manipulation.

## Dual API design

Every function that writes into a caller-provided buffer (`char *`, `size_t`)
also has an overload that returns a `std::string`. The raw form is for hot
paths, struct fields, and C API bridges; the `std::string` form is for config
code and diagnostics.

All raw-form functions always null-terminate the output buffer.

## Basename

```cpp
// Raw form: writes result into dst[0..size-1].
void file_base( const char *src, char *dst, std::size_t size ) noexcept;

// std::string form
std::string file_base( std::string_view path );
```

Returns the filename without directory or extension. Given `"maps/c1a0.bsp"`,
returns `"c1a0"`. Legacy: `COM_FileBase`.

## Extension

```cpp
// Returns a view into path (does not allocate).
std::string_view file_extension( std::string_view path ) noexcept;
```

Returns the extension including the `.` separator, or an empty view if there is
no extension. The returned view is a slice of the input — no allocation.
Legacy: `COM_FileExtension`.

## Default extension

```cpp
// Raw form: appends ext to path in-place if path has no extension.
void default_extension( char *path, const char *ext, std::size_t size ) noexcept;

// std::string form
std::string default_extension( std::string_view path, std::string_view ext );
```

Appends the given extension only when `path` does not already have one.
Legacy: `COM_DefaultExtension`.

## Replace extension

```cpp
void        replace_extension( char *path, const char *ext, std::size_t size ) noexcept;
std::string replace_extension( std::string_view path, std::string_view ext );
```

Replaces the existing extension (if any) with `ext`. If `path` has no
extension, the extension is appended. Legacy: `COM_ReplaceExtension`.

## Directory extraction

```cpp
void        extract_dir( const char *path, char *dst, std::size_t size ) noexcept;
std::string extract_dir( std::string_view path );
```

Returns the directory component including the trailing separator, or an empty
string/buffer for paths with no directory component. Given `"maps/c1a0.bsp"`,
returns `"maps/"`. Legacy: `COM_ExtractFilePath`.

## Filename (without directory)

```cpp
// Returns a view into path — no allocation.
std::string_view filename( std::string_view path ) noexcept;
```

Returns the filename component after the last `/` or `\` separator.
Given `"maps/c1a0.bsp"`, returns `"c1a0.bsp"`. Legacy: `COM_FileWithoutPath`.

## Strip extension

```cpp
// Returns a view into path — no allocation.
std::string_view strip_extension( std::string_view path ) noexcept;
```

Returns the path without its extension. The result is a slice of the input.
Legacy: `COM_StripExtension`.

## Slash normalisation

```cpp
void        fix_slashes( char *path, std::size_t size ) noexcept;
std::string fix_slashes( std::string_view path );
```

Replaces all `\` with `/` for internal canonical path form. The engine's
internal representation always uses forward slashes; backslashes appear only
when paths come from Windows OS APIs. Legacy: `COM_FixSlashes`.

## Whitespace and line-feed stripping

```cpp
void remove_line_feed( char *s ) noexcept;          // strips trailing \r\n
std::string_view trim_space( std::string_view s ) noexcept;  // trims leading and trailing whitespace
```

`remove_line_feed` modifies `s` in place. `trim_space` returns a view into its
input — no allocation. Legacy: `COM_RemoveLineFeed`, `COM_TrimSpace`.

## Path joining

```cpp
std::string path_join( std::string_view a, std::string_view b );
std::string path_join( std::string_view a, std::string_view b, std::string_view c );
```

Joins path components with a `/` separator, suppressing a double separator when
`a` already ends with `/` or `b` starts with `/`. The three-argument form joins
`a/b/c` in one call. There is no legacy equivalent; this replaces inline
`std::string + "/" + std::string` patterns throughout the codebase.

## Thread safety

All functions are pure transformations on their inputs. The raw-form functions
write into caller-provided memory. There is no shared mutable state.
