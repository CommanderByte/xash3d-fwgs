// xash3dpp — File abstract base + OsFile concrete implementation  (internal)
// Legacy reference: filesystem/filesystem_internal.h  (file_t),
//                   filesystem/filesystem.c (FS_Read, FS_Write, FS_Seek, …)

#include <xash3dpp/filesystem/file.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/filesystem/os_fd.hpp>
#include <xash3dpp/private/filesystem/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/os_file_factory.hpp>

#include <miniz.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>   // SEEK_SET
#include <cstring>  // memcpy
#include <optional>
#include <string>

namespace xash::filesystem {

// ---------------------------------------------------------------------------
// ZlibState — lazy-init incremental DEFLATE decompressor for archive entries
// ---------------------------------------------------------------------------

struct ZlibState {
    mz_stream z   = {};
    std::array<mz_uint8, 65536> in_buf{};
    bool      done = false;

    ZlibState()  { mz_inflateInit2( &z, -MZ_DEFAULT_WINDOW_BITS ); }
    ~ZlibState() { mz_inflateEnd( &z ); }

    ZlibState( const ZlibState& )            = delete;
    ZlibState& operator=( const ZlibState& ) = delete;
};

// ---------------------------------------------------------------------------
// OsFile — concrete streaming file backed by a native OS descriptor.
//
// position_ tracks bytes consumed from the raw data source (fd or inflate
// stream), including bytes that have been prefetched into buf_ but not yet
// returned to the caller.
//
//   Tell()  = position_ − (buf_len_ − buf_pos_)   [effective cursor]
//   Eof()   = Tell() >= length_
// ---------------------------------------------------------------------------

class OsFile final : public File {
public:
    OsFile( OsFd fd, FsOffset length, FsOffset real_offset, bool deflated );
    ~OsFile() override = default;

    FsOffset Read( std::span<std::byte> buf )        override;
    FsOffset Write( std::span<const std::byte> buf ) override;
    FsOffset Seek( FsOffset offset, SeekOrigin origin ) override;
    FsOffset Tell()   const                          override;
    FsOffset Length() const                          override;
    bool     Eof()    const                          override;
    void     Flush()                                 override;

    std::optional<std::string> Gets()         override;
    int                        Getc()         override;
    void                       UnGetc( int c ) override;

private:
    OsFd     fd_;
    FsOffset length_       = 0;
    FsOffset position_     = 0;   // bytes consumed from raw source
    FsOffset real_offset_  = 0;   // byte offset within an archive file
    bool     deflated_     = false;

    std::optional<ZlibState> zlib_;

    static constexpr std::size_t k_buf_size = 2048;
    std::array<std::byte, k_buf_size> buf_{};
    std::size_t buf_pos_ = 0;
    std::size_t buf_len_ = 0;

    int ungetc_ = EOF;

    // Read n bytes from the inflate stream into 'out'; advances position_.
    FsOffset inflate_read( std::span<std::byte> out );
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

OsFile::OsFile( OsFd fd, FsOffset length, FsOffset real_offset, bool deflated )
    : fd_{ std::move( fd ) },
      length_{ length },
      real_offset_{ real_offset },
      deflated_{ deflated }
{
    if ( deflated_ ) {
        zlib_.emplace();
        platform::seek( fd_, real_offset_, SEEK_SET );
    }
}

// ---------------------------------------------------------------------------
// inflate_read
// ---------------------------------------------------------------------------

FsOffset OsFile::inflate_read( std::span<std::byte> out ) {
    ZlibState& zs = *zlib_;
    if ( zs.done || out.empty() ) return 0;

    zs.z.next_out  = reinterpret_cast<mz_uint8*>( out.data() );
    zs.z.avail_out = static_cast<unsigned int>( out.size() );

    while ( zs.z.avail_out > 0 && !zs.done ) {
        if ( zs.z.avail_in == 0 ) {
            const FsOffset got = platform::read( fd_, zs.in_buf.data(), zs.in_buf.size() );
            if ( got <= 0 ) break;
            zs.z.next_in  = zs.in_buf.data();
            zs.z.avail_in = static_cast<unsigned int>( got );
        }
        const int ret = mz_inflate( &zs.z, MZ_SYNC_FLUSH );
        if ( ret == MZ_STREAM_END ) { zs.done = true; break; }
        if ( ret != MZ_OK )          break;
    }

    const FsOffset produced = static_cast<FsOffset>( out.size() )
                            - static_cast<FsOffset>( zs.z.avail_out );
    position_ += produced;
    return produced;
}

// ---------------------------------------------------------------------------
// Read
// ---------------------------------------------------------------------------

FsOffset OsFile::Read( std::span<std::byte> buf ) {
    if ( buf.empty() ) return 0;

    const FsOffset eff = Tell();
    if ( eff >= length_ ) return 0;

    const FsOffset    remaining = length_ - eff;
    const std::size_t want      = static_cast<std::size_t>(
        std::min<FsOffset>( static_cast<FsOffset>( buf.size() ), remaining ) );

    FsOffset total = 0;

    // 1. Drain any prefetched read-ahead bytes.
    if ( buf_pos_ < buf_len_ ) {
        const std::size_t avail = buf_len_ - buf_pos_;
        const std::size_t take  = std::min( avail, want );
        std::memcpy( buf.data(), buf_.data() + buf_pos_, take );
        buf_pos_ += take;
        total    += static_cast<FsOffset>( take );
    }

    const std::size_t still_need = want - static_cast<std::size_t>( total );
    if ( still_need == 0 ) return total;

    // 2. Read directly from the data source.
    if ( deflated_ ) {
        const FsOffset got = inflate_read( buf.subspan( total, still_need ) );
        if ( got > 0 ) total += got;
    } else {
        if ( platform::seek( fd_, real_offset_ + position_, SEEK_SET ) < 0 )
            return total > 0 ? total : -1;
        const FsOffset got = platform::read( fd_, buf.data() + total, still_need );
        if ( got > 0 ) {
            position_ += got;
            total     += got;
        }
    }

    return total > 0 ? total : -1;
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

FsOffset OsFile::Write( std::span<const std::byte> buf ) {
    if ( deflated_ ) return -1;   // archive entries are read-only
    if ( buf.empty() ) return 0;

    // Discard any read-ahead so the write cursor stays consistent.
    buf_pos_ = 0;
    buf_len_ = 0;

    const FsOffset n = platform::write( fd_, buf.data(), buf.size() );
    if ( n > 0 ) position_ += n;
    return n;
}

// ---------------------------------------------------------------------------
// Seek
// ---------------------------------------------------------------------------

FsOffset OsFile::Seek( FsOffset offset, SeekOrigin origin ) {
    FsOffset target;
    switch ( origin ) {
    case SeekOrigin::Begin:   target = offset;               break;
    case SeekOrigin::Current: target = Tell() + offset;      break;
    case SeekOrigin::End:     target = length_ + offset;     break;
    default:                  return -1;
    }
    if ( target < 0 || target > length_ ) return -1;

    // Invalidate read-ahead buffer and push-back.
    buf_pos_ = 0;
    buf_len_ = 0;
    ungetc_  = EOF;

    if ( !deflated_ ) {
        if ( platform::seek( fd_, real_offset_ + target, SEEK_SET ) < 0 )
            return -1;
        position_ = target;
        return target;
    }

    // Deflated: reinflate from the start for backward seeks.
    if ( target < position_ ) {
        ZlibState& zs = *zlib_;
        mz_inflateEnd( &zs.z );
        zs.z    = {};
        zs.done = false;
        mz_inflateInit2( &zs.z, -MZ_DEFAULT_WINDOW_BITS );
        platform::seek( fd_, real_offset_, SEEK_SET );
        position_ = 0;
    }

    // Discard (inflate and throw away) bytes up to target.
    {
        FsOffset discard = target - position_;
        while ( discard > 0 && !zlib_->done ) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<FsOffset>( discard, static_cast<FsOffset>( k_buf_size ) ) );
            const FsOffset got = inflate_read( { buf_.data(), chunk } );
            if ( got <= 0 ) return -1;
            discard -= got;
        }
    }

    return position_;   // == target after successful discard
}

// ---------------------------------------------------------------------------
// Tell / Length / Eof / Flush
// ---------------------------------------------------------------------------

FsOffset OsFile::Tell() const {
    return position_ - static_cast<FsOffset>( buf_len_ - buf_pos_ );
}

FsOffset OsFile::Length() const { return length_; }

bool OsFile::Eof() const { return Tell() >= length_; }

void OsFile::Flush() { platform::flush( fd_ ); }

// ---------------------------------------------------------------------------
// Getc / UnGetc / Gets
// ---------------------------------------------------------------------------

int OsFile::Getc() {
    if ( ungetc_ != EOF ) {
        const int c = ungetc_;
        ungetc_ = EOF;
        return c;
    }
    if ( Eof() ) return EOF;

    // Refill the read-ahead buffer when exhausted.
    if ( buf_pos_ >= buf_len_ ) {
        buf_pos_ = 0;
        buf_len_ = 0;
        const FsOffset remaining = length_ - position_;
        if ( remaining <= 0 ) return EOF;

        const std::size_t to_fill = static_cast<std::size_t>(
            std::min<FsOffset>( remaining, static_cast<FsOffset>( k_buf_size ) ) );

        FsOffset got;
        if ( deflated_ ) {
            got = inflate_read( { buf_.data(), to_fill } );
        } else {
            if ( platform::seek( fd_, real_offset_ + position_, SEEK_SET ) < 0 )
                return EOF;
            got = platform::read( fd_, buf_.data(), to_fill );
            if ( got > 0 ) position_ += got;
        }
        if ( got <= 0 ) return EOF;
        buf_len_ = static_cast<std::size_t>( got );
    }

    return static_cast<int>( static_cast<unsigned char>(
        std::to_integer<unsigned char>( buf_[buf_pos_++] ) ) );
}

void OsFile::UnGetc( int c ) { ungetc_ = c; }

std::optional<std::string> OsFile::Gets() {
    const int first = Getc();
    if ( first == EOF ) return std::nullopt;

    std::string line;
    int c = first;
    while ( c != EOF && c != '\n' ) {
        if ( c != '\r' ) line += static_cast<char>( c );
        c = Getc();
    }
    return line;
}

// ---------------------------------------------------------------------------
// File::operator delete — routes deallocation through the memory subsystem.
// Called by std::unique_ptr<File>'s default deleter when the object was
// created via pool_new (which prepends an 8-byte header with the pool index).
// ---------------------------------------------------------------------------

void File::operator delete( void* p ) noexcept
{
    xash::memory::mem_free( p );
}

void File::operator delete( void* p, std::size_t ) noexcept
{
    xash::memory::mem_free( p );
}

// ---------------------------------------------------------------------------
// make_os_file — factory used by backends
// ---------------------------------------------------------------------------

std::unique_ptr<File> make_os_file( xash::memory::PoolHandle pool,
                                    OsFd fd, FsOffset length,
                                    FsOffset real_offset, bool deflated ) {
    return std::unique_ptr<File>{
        xash::memory::pool_new<OsFile>( pool, std::move( fd ),
                                        length, real_offset, deflated ) };
}

} // namespace xash::filesystem
