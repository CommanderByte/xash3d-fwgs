// xash3dpp — platform (Win32) — OS file I/O
// Legacy reference: filesystem/filesystem.c (FS_SysOpen WIN32 branches),
//                   filesystem/dir.c (WIN32 listdirectory branch)

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/os_io.hpp>

// Windows headers — confined to this translation unit.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <chrono>
#include <optional>
#include <string>

namespace xash::platform {

// ---------------------------------------------------------------------------
// OsFd::close() — defined here so <io.h> stays confined to this TU.
// ---------------------------------------------------------------------------

void OsFd::close() noexcept
{
    if( fd_ >= 0 )
    {
        ::_close( fd_ );
        fd_ = -1;
    }
}

// ---------------------------------------------------------------------------
// Internal: UTF-8 → UTF-16 path conversion.
// Two-call MultiByteToWideChar handles paths of any length.
// ---------------------------------------------------------------------------

static std::optional<std::wstring> to_wide( std::string_view s ) noexcept
{
    if( s.empty() ) return std::wstring{};
    const int n = ::MultiByteToWideChar( CP_UTF8, 0,
        s.data(), static_cast<int>( s.size() ), nullptr, 0 );
    if( n <= 0 ) return std::nullopt;
    std::wstring w( static_cast<std::size_t>( n ), L'\0' );
    ::MultiByteToWideChar( CP_UTF8, 0,
        s.data(), static_cast<int>( s.size() ), w.data(), n );
    return w;
}

// ---------------------------------------------------------------------------
// File open
// ---------------------------------------------------------------------------

OsFd open_file( std::string_view path, OpenMode mode ) noexcept
{
    using M = OpenMode;
    int flags = _O_BINARY;

    if( any( mode & M::ReadWrite ) )       flags |= _O_RDWR;
    else if( any( mode & M::WriteOnly ) )  flags |= _O_WRONLY;
    else                                   flags |= _O_RDONLY;

    if( any( mode & M::Create ) )   flags |= _O_CREAT;
    if( any( mode & M::Truncate ) ) flags |= _O_TRUNC;
    if( any( mode & M::Append ) )   flags |= _O_APPEND;

    const auto wpath = to_wide( path );
    if( !wpath ) return OsFd{};

    int fd = ::_wopen( wpath->c_str(), flags, 0666 );
    if( fd < 0 ) return OsFd{};
    return OsFd{ fd };
}

OsFd open_memfd( std::string_view /*name*/ ) noexcept
{
    // Windows has no native memfd; back with a self-deleting temporary file.
    wchar_t temp_dir[MAX_PATH];
    wchar_t temp_file[MAX_PATH];

    if( !::GetTempPathW( static_cast<DWORD>( std::size( temp_dir ) ), temp_dir ) )
        return OsFd{};
    if( !::GetTempFileNameW( temp_dir, L"xash", 0, temp_file ) )
        return OsFd{};

    HANDLE h = ::CreateFileW( temp_file,
        GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr );
    if( h == INVALID_HANDLE_VALUE )
    {
        ::DeleteFileW( temp_file );
        return OsFd{};
    }

    int fd = ::_open_osfhandle( reinterpret_cast<intptr_t>( h ), _O_RDWR | _O_BINARY );
    if( fd < 0 )
    {
        ::CloseHandle( h );
        return OsFd{};
    }
    return OsFd{ fd };
}

// ---------------------------------------------------------------------------
// Read / write / seek
// ---------------------------------------------------------------------------

std::int64_t read( OsFd &fd, void *buf, std::size_t size ) noexcept
{
    return static_cast<std::int64_t>(
        ::_read( fd.get(), buf, static_cast<unsigned>( size ) ) );
}

std::int64_t write( OsFd &fd, const void *buf, std::size_t size ) noexcept
{
    return static_cast<std::int64_t>(
        ::_write( fd.get(), buf, static_cast<unsigned>( size ) ) );
}

std::int64_t seek( OsFd &fd, std::int64_t offset, int whence ) noexcept
{
    return static_cast<std::int64_t>( ::_lseeki64( fd.get(), offset, whence ) );
}

std::int64_t tell( OsFd &fd ) noexcept
{
    return static_cast<std::int64_t>( ::_telli64( fd.get() ) );
}

void flush( OsFd &fd ) noexcept
{
    ::_commit( fd.get() );
}

void close_fd( int raw_fd ) noexcept
{
    if( raw_fd >= 0 ) ::_close( raw_fd );
}

// ---------------------------------------------------------------------------
// File metadata
// ---------------------------------------------------------------------------

std::optional<std::int64_t> file_size( std::string_view path ) noexcept
{
    const auto wpath = to_wide( path );
    if( !wpath ) return std::nullopt;
    struct __stat64 st{};
    if( ::_wstat64( wpath->c_str(), &st ) < 0 ) return std::nullopt;
    return static_cast<std::int64_t>( st.st_size );
}

std::optional<std::filesystem::file_time_type>
file_time( std::string_view path ) noexcept
{
    const auto wpath = to_wide( path );
    if( !wpath ) return std::nullopt;
    struct __stat64 st{};
    if( ::_wstat64( wpath->c_str(), &st ) < 0 ) return std::nullopt;
    auto sys = std::chrono::system_clock::from_time_t( st.st_mtime );
    return std::chrono::clock_cast<std::filesystem::file_time_type::clock>( sys );
}

// ---------------------------------------------------------------------------
// Directory listing
// ---------------------------------------------------------------------------

std::vector<std::string> list_directory( std::string_view path ) noexcept
{
    std::string pattern( path );
    pattern += "/*";

    const auto wpattern = to_wide( pattern );
    if( !wpattern ) return {};

    WIN32_FIND_DATAW data{};
    HANDLE h = ::FindFirstFileW( wpattern->c_str(), &data );
    if( h == INVALID_HANDLE_VALUE ) return {};

    std::vector<std::string> result;
    do
    {
        if( data.cFileName[0] == L'.' &&
            ( data.cFileName[1] == L'\0' ||
              ( data.cFileName[1] == L'.' && data.cFileName[2] == L'\0' ) ) )
            continue;

        const int len = ::WideCharToMultiByte( CP_UTF8, 0,
            data.cFileName, -1, nullptr, 0, nullptr, nullptr );
        if( len > 1 )
        {
            std::string name( static_cast<std::size_t>( len - 1 ), '\0' );
            ::WideCharToMultiByte( CP_UTF8, 0,
                data.cFileName, -1, name.data(), len, nullptr, nullptr );
            result.push_back( std::move( name ) );
        }
    }
    while( ::FindNextFileW( h, &data ) );
    ::FindClose( h );
    return result;
}

bool is_case_insensitive( std::string_view /*path*/ ) noexcept
{
    // NTFS is case-insensitive by default on Windows.
    return true;
}

// ---------------------------------------------------------------------------
// Filesystem mutations
// ---------------------------------------------------------------------------

bool make_directory( std::string_view path ) noexcept
{
    const auto wpath = to_wide( path );
    if( !wpath ) return false;
    return ::CreateDirectoryW( wpath->c_str(), nullptr ) ||
           ::GetLastError() == ERROR_ALREADY_EXISTS;
}

bool rename_file( std::string_view from, std::string_view to ) noexcept
{
    const auto wfrom = to_wide( from );
    if( !wfrom ) return false;
    const auto wto = to_wide( to );
    if( !wto ) return false;
    return ::MoveFileExW( wfrom->c_str(), wto->c_str(), MOVEFILE_REPLACE_EXISTING ) != 0;
}

bool delete_file( std::string_view path ) noexcept
{
    const auto wpath = to_wide( path );
    if( !wpath ) return false;
    return ::DeleteFileW( wpath->c_str() ) != 0;
}

} // namespace xash::platform
