// xash3dpp — file path utilities implementation
// Legacy reference: public/crtlib.c  (COM_FileBase, COM_FileExtension, …)

#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>
#include <cstring>
#include <cctype>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// file_extension (internal helper shared by several functions)
// ---------------------------------------------------------------------------

// Returns a pointer into 'path' to the char after the last '.', or the
// null terminator when there is no extension.  Never returns nullptr.
static const char *find_extension( const char *path ) noexcept
{
    if( !path ) return "";

    const char *dot = nullptr;
    for( const char *s = path; *s; ++s )
    {
        if( *s == '/' || *s == '\\' )
            dot = nullptr;  // dots in directory names don't count
        else if( *s == '.' )
            dot = s;
    }
    return ( dot && dot[1] ) ? dot + 1 : path + std::strlen( path );
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void file_base( const char *path, char *out, std::size_t size ) noexcept
{
    if( !out || !size ) return;
    out[0] = '\0';
    if( !path || !*path ) return;

    // Find the last directory separator.
    const char *slash = path;
    const char *dot   = nullptr;
    for( const char *s = path; *s; ++s )
    {
        if( *s == '/' || *s == '\\' ) slash = s + 1;
        if( *s == '.' )               dot   = s;
    }

    // If the dot is before the last slash (or absent), treat end as the dot.
    const char *end = path + std::strlen( path );
    if( !dot || dot < slash ) dot = end;

    const std::size_t len = std::min( size - 1, static_cast<std::size_t>( dot - slash ) );
    std::memcpy( out, slash, len );
    out[len] = '\0';
}

std::string_view file_extension( std::string_view path ) noexcept
{
    // Work on a temporary null-terminated pointer for find_extension.
    // string_view is not guaranteed null-terminated, so we check manually.
    const char *p   = path.data();
    const std::size_t n = path.size();

    const char *dot = nullptr;
    for( std::size_t i = 0; i < n; ++i )
    {
        if( p[i] == '/' || p[i] == '\\' )
            dot = nullptr;
        else if( p[i] == '.' )
            dot = p + i;
    }

    if( dot && ( dot + 1 ) < ( p + n ) )
        return { dot, static_cast<std::size_t>( ( p + n ) - dot ) };

    return {};
}

void default_extension( char *path, const char *ext, std::size_t size ) noexcept
{
    if( !path || !ext || !size ) return;

    // Only append if the path has no extension yet.
    if( *find_extension( path ) == '\0' )
    {
        const std::size_t len = std::strlen( path );
        strncpy( path + len, ext, size - len );
    }
}

void replace_extension( char *path, const char *ext, std::size_t size ) noexcept
{
    if( !path || !size ) return;
    strip_extension( path );
    default_extension( path, ext, size );
}

void extract_dir( const char *path, char *out ) noexcept
{
    if( !out ) return;
    out[0] = '\0';
    if( !path ) return;

    // Walk to the end then back up to the last separator.
    const char *end = path + std::strlen( path );
    const char *src = end;

    while( src > path && src[-1] != '/' && src[-1] != '\\' )
        --src;

    if( src > path )
    {
        const std::size_t len = static_cast<std::size_t>( src - path ) - 1; // drop the slash
        std::memcpy( out, path, len );
        out[len] = '\0';
    }
}

std::string_view filename( std::string_view path ) noexcept
{
    const char *p = path.data();
    const std::size_t n = path.size();

    // Find last '/', '\', or ':'.
    std::size_t sep = 0;
    bool found = false;
    for( std::size_t i = 0; i < n; ++i )
    {
        if( p[i] == '/' || p[i] == '\\' || p[i] == ':' )
        {
            sep   = i;
            found = true;
        }
    }

    if( found )
        return path.substr( sep + 1 );
    return path;
}

void strip_extension( char *path ) noexcept
{
    if( !path ) return;

    const char *ext = find_extension( path );
    if( ext > path && *ext != '\0' )
        path[ext - path - 1] = '\0'; // null out the dot
}

void fix_slashes( char *path ) noexcept
{
    if( !path ) return;
    for( char *p = path; *p; ++p )
    {
        if( *p == '\\' ) *p = '/';
    }
}

void remove_line_feed( char *str, std::size_t size ) noexcept
{
    if( !str || !size ) return;
    for( std::size_t i = 0; i < size && *str != '\0'; ++i, ++str )
    {
        if( *str == '\r' || *str == '\n' )
        {
            *str = '\0';
            return;
        }
    }
}

void trim_space( char *dst, const char *src, std::size_t size ) noexcept
{
    if( !dst || !src || !size ) return;

    // Skip leading whitespace.
    while( *src && std::isspace( static_cast<unsigned char>( *src ) ) )
        ++src;

    // Find length of remaining string.
    std::size_t len = std::strlen( src );

    // Trim trailing whitespace.
    while( len > 0 && std::isspace( static_cast<unsigned char>( src[len - 1] ) ) )
        --len;

    if( len > 0 )
        strncpy( dst, src, std::min( size, len + 1 ) );
    else
        dst[0] = '\0';
}

// ---------------------------------------------------------------------------
// std::string-returning overloads
// ---------------------------------------------------------------------------

std::string file_base( std::string_view path )
{
    // Strip extension, then strip the directory prefix.
    const std::string_view ext  = file_extension( path );
    const std::string_view noext( path.data(), path.size() - ext.size() );
    return std::string( filename( noext ) );
}

std::string strip_extension( std::string_view path )
{
    const std::string_view ext = file_extension( path );
    return std::string( path.data(), path.size() - ext.size() );
}

std::string fix_slashes( std::string_view path )
{
    std::string result( path );
    for( char &c : result )
        if( c == '\\' ) c = '/';
    return result;
}

std::string extract_dir( std::string_view path )
{
    const std::string_view fn = filename( path );
    // fn.data() points inside path; the separator is just before it.
    const std::size_t dir_len = static_cast<std::size_t>( fn.data() - path.data() );
    // Drop the trailing separator (if any).
    const std::size_t keep = ( dir_len > 0 ) ? dir_len - 1 : 0;
    return std::string( path.data(), keep );
}

std::string default_extension( std::string_view path, std::string_view ext )
{
    if( !file_extension( path ).empty() )
        return std::string( path );
    std::string result;
    result.reserve( path.size() + ext.size() );
    result.append( path );
    result.append( ext );
    return result;
}

std::string replace_extension( std::string_view path, std::string_view ext )
{
    return default_extension( strip_extension( path ), ext );
}

std::string trim_space( std::string_view src )
{
    std::size_t start = 0;
    while( start < src.size() && static_cast<unsigned char>( src[start] ) <= ' ' )
        ++start;
    std::size_t end = src.size();
    while( end > start && static_cast<unsigned char>( src[end - 1] ) <= ' ' )
        --end;
    return std::string( src.data() + start, end - start );
}

std::string remove_line_feed( std::string_view s )
{
    std::string result( s );
    const auto pos = result.find_first_of( "\r\n" );
    if( pos != std::string::npos )
        result.resize( pos );
    return result;
}

std::string path_join( std::string_view dir, std::string_view rel )
{
    if( dir.empty() ) return std::string( rel );
    if( rel.empty()  ) return std::string( dir );
    std::string result;
    result.reserve( dir.size() + 1 + rel.size() );
    result.assign( dir );
    const char last = result.back();
    if( last != '/' && last != '\\' )
        result += '/';
    result.append( rel );
    return result;
}

std::string path_join( std::string_view a, std::string_view b, std::string_view c )
{
    return path_join( path_join( a, b ), c );
}

} // namespace xash::utilities
