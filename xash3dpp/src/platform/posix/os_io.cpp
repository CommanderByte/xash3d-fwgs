// xash3dpp — platform (POSIX: Linux, macOS, FreeBSD, Android) — OS file I/O
// Legacy reference: filesystem/filesystem.c (FS_SysOpen, FS_Sys_Read, …),
//                   filesystem/dir.c (listdirectory)

#if defined(_WIN32)
#  error "This file is POSIX-only"
#endif

#include <xash3dpp/platform/os_io.hpp>

// POSIX headers — confined to this translation unit.
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
#  include <sys/ioctl.h>
#  include <sys/syscall.h>
#  include <linux/fs.h>   // FS_IOC_GETFLAGS, FS_CASEFOLD_FL
#  ifndef FS_CASEFOLD_FL
#    define FS_CASEFOLD_FL 0x40000000
#  endif
#endif

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <string>

namespace xash::platform {

// ---------------------------------------------------------------------------
// OsFd::close() — defined here so POSIX headers stay confined to this TU.
// ---------------------------------------------------------------------------

void OsFd::close() noexcept
{
    if( fd_ >= 0 )
    {
        ::close( fd_ );
        fd_ = -1;
    }
}

// ---------------------------------------------------------------------------
// File open
// ---------------------------------------------------------------------------

OsFd open_file( std::string_view path, OpenMode mode ) noexcept
{
    using M = OpenMode;
    int flags = O_CLOEXEC;

    if( any( mode & M::ReadWrite ) )      flags |= O_RDWR;
    else if( any( mode & M::WriteOnly ) ) flags |= O_WRONLY;
    else                                  flags |= O_RDONLY;

    if( any( mode & M::Create ) )   flags |= O_CREAT;
    if( any( mode & M::Truncate ) ) flags |= O_TRUNC;
    if( any( mode & M::Append ) )   flags |= O_APPEND;

    std::string path_str( path );
    int fd;
    do { fd = ::open( path_str.c_str(), flags, 0666 ); }
    while( fd < 0 && errno == EINTR );
    if( fd < 0 ) return OsFd{};
    return OsFd{ fd };
}

OsFd open_memfd( [[maybe_unused]] std::string_view name ) noexcept
{
#if defined(__linux__) && defined(SYS_memfd_create)
    std::string name_str( name );
    int fd = static_cast<int>( ::syscall( SYS_memfd_create,
        name_str.c_str(), 0x1u /* MFD_CLOEXEC */ ) );
    if( fd >= 0 ) return OsFd{ fd };
#endif
    return OsFd{};
}

// ---------------------------------------------------------------------------
// Read / write / seek
// ---------------------------------------------------------------------------

std::int64_t read( OsFd &fd, void *buf, std::size_t size ) noexcept
{
    ssize_t n;
    do { n = ::read( fd.get(), buf, size ); } while( n < 0 && errno == EINTR );
    return static_cast<std::int64_t>( n );
}

std::int64_t write( OsFd &fd, const void *buf, std::size_t size ) noexcept
{
    ssize_t n;
    do { n = ::write( fd.get(), buf, size ); } while( n < 0 && errno == EINTR );
    return static_cast<std::int64_t>( n );
}

std::int64_t seek( OsFd &fd, std::int64_t offset, int whence ) noexcept
{
    return static_cast<std::int64_t>( ::lseek( fd.get(), offset, whence ) );
}

std::int64_t tell( OsFd &fd ) noexcept
{
    return static_cast<std::int64_t>( ::lseek( fd.get(), 0, SEEK_CUR ) );
}

void flush( OsFd &fd ) noexcept
{
    ::fsync( fd.get() );
}

void close_fd( int raw_fd ) noexcept
{
    if( raw_fd >= 0 ) ::close( raw_fd );
}

// ---------------------------------------------------------------------------
// File metadata
// ---------------------------------------------------------------------------

std::optional<std::int64_t> file_size( std::string_view path ) noexcept
{
    std::string path_str( path );
    struct stat st{};
    if( ::stat( path_str.c_str(), &st ) < 0 ) return std::nullopt;
    return static_cast<std::int64_t>( st.st_size );
}

std::optional<std::filesystem::file_time_type>
file_time( std::string_view path ) noexcept
{
    std::string path_str( path );
    struct stat st{};
    if( ::stat( path_str.c_str(), &st ) < 0 ) return std::nullopt;
    auto sys = std::chrono::system_clock::from_time_t( st.st_mtime );
    return std::chrono::clock_cast<std::filesystem::file_time_type::clock>( sys );
}

// ---------------------------------------------------------------------------
// Directory listing
// ---------------------------------------------------------------------------

std::vector<std::string> list_directory( std::string_view path ) noexcept
{
    std::string path_str( path );
    DIR *dir = ::opendir( path_str.c_str() );
    if( !dir ) return {};

    std::vector<std::string> result;
    while( struct dirent *entry = ::readdir( dir ) )
    {
        const char *name = entry->d_name;
        if( name[0] == '.' &&
            ( name[1] == '\0' || ( name[1] == '.' && name[2] == '\0' ) ) )
            continue;
        result.emplace_back( name );
    }
    ::closedir( dir );
    return result;
}

bool is_case_insensitive( std::string_view path ) noexcept
{
#if defined(__APPLE__)
    (void)path;
    return true; // macOS default filesystem (HFS+/APFS) is case-insensitive
#elif defined(__linux__)
    // Check for the per-directory case-folding flag (kernel 5.2+, e2fsprogs).
    std::string path_str( path );
    int fd = ::open( path_str.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC );
    if( fd < 0 ) return false;
    long flags = 0;
    int ret = ::ioctl( fd, FS_IOC_GETFLAGS, &flags );
    ::close( fd );
    return ( ret == 0 ) && ( ( flags & FS_CASEFOLD_FL ) != 0 );
#else
    (void)path;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Filesystem mutations
// ---------------------------------------------------------------------------

bool make_directory( std::string_view path ) noexcept
{
    std::string path_str( path );
    return ::mkdir( path_str.c_str(), 0755 ) == 0 || errno == EEXIST;
}

bool rename_file( std::string_view from, std::string_view to ) noexcept
{
    return ::rename( std::string{ from }.c_str(), std::string{ to }.c_str() ) == 0;
}

bool delete_file( std::string_view path ) noexcept
{
    return ::unlink( std::string{ path }.c_str() ) == 0;
}

} // namespace xash::platform
