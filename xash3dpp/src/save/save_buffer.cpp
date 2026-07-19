// xash3dpp — SAVERESTOREDATA working buffer implementation (Chunk 8, S8.1).
// SaveInit/SaveClear/SaveFinish promoted to a pool-owned RAII class
// (save_buffer.hpp).  Integer/float writes are explicit little-endian to match
// the LE-only on-disk format.

#include <xash3dpp/private/save/save_buffer.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <bit>
#include <cstring>

namespace xash::save {

namespace
{
constexpr char k_tag[] = "save";
} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

SaveBuffer::SaveBuffer( ::xash::memory::PoolHandle pool, std::size_t buffer_size,
                        std::size_t token_count, float time ) noexcept
    : pool_( pool )
    , buffer_size_( buffer_size )
    , time_( time )
    , tokens_( token_count )
{
    // Legacy SaveInit uses Mem_Calloc — the working buffer is zero-initialised.
    base_ = static_cast<std::byte *>( ::xash::memory::mem_calloc( pool_, buffer_size ) );
    if ( !base_ )
        buffer_size_ = 0; // valid() == false
}

SaveBuffer::~SaveBuffer()
{
    if ( base_ )
        ::xash::memory::mem_free( base_ ); // SaveFinish (sv_save.c:763-783)
}

void SaveBuffer::operator delete( void *p ) noexcept
{
    ::xash::memory::mem_free( p );
}

void SaveBuffer::operator delete( void *p, std::size_t ) noexcept
{
    ::xash::memory::mem_free( p );
}

// ---------------------------------------------------------------------------
// Writes
// ---------------------------------------------------------------------------

Result<void> SaveBuffer::write_bytes( std::span<const std::byte> data ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cursor_ + data.size() > buffer_size_ )
        return std::unexpected( SaveError::BufferExhausted );

    if ( !data.empty() )
        std::memcpy( base_ + cursor_, data.data(), data.size() );
    cursor_ += data.size();
    if ( cursor_ > data_size_ )
        data_size_ = cursor_;
    return {};
}

Result<void> SaveBuffer::write_i16( std::int16_t v ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    const auto      u   = static_cast<std::uint16_t>( v );
    const std::byte b[2] = {
        static_cast<std::byte>( u & 0xFFu ),
        static_cast<std::byte>( ( u >> 8 ) & 0xFFu ),
    };
    return write_bytes( b );
}

Result<void> SaveBuffer::write_i32( std::int32_t v ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    const auto      u   = static_cast<std::uint32_t>( v );
    const std::byte b[4] = {
        static_cast<std::byte>( u & 0xFFu ),
        static_cast<std::byte>( ( u >> 8 ) & 0xFFu ),
        static_cast<std::byte>( ( u >> 16 ) & 0xFFu ),
        static_cast<std::byte>( ( u >> 24 ) & 0xFFu ),
    };
    return write_bytes( b );
}

Result<void> SaveBuffer::write_f32( float v ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return write_i32( static_cast<std::int32_t>( std::bit_cast<std::uint32_t>( v ) ) );
}

// ---------------------------------------------------------------------------
// Reads
// ---------------------------------------------------------------------------

Result<std::span<const std::byte>> SaveBuffer::read_bytes( std::size_t n ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cursor_ + n > data_size_ )
        return std::unexpected( SaveError::TruncatedBlock );

    std::span<const std::byte> out( base_ + cursor_, n );
    cursor_ += n;
    return out;
}

Result<std::int16_t> SaveBuffer::read_i16() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    const auto r = read_bytes( 2 );
    if ( !r )
        return std::unexpected( r.error() );
    const auto &b = *r;
    const auto  u = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
        ( static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) );
    return static_cast<std::int16_t>( u );
}

Result<std::int32_t> SaveBuffer::read_i32() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    const auto r = read_bytes( 4 );
    if ( !r )
        return std::unexpected( r.error() );
    const auto &b = *r;
    const auto  u =
        static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
        ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) |
        ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[2] ) ) << 16 ) |
        ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[3] ) ) << 24 );
    return static_cast<std::int32_t>( u );
}

Result<float> SaveBuffer::read_f32() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    const auto r = read_i32();
    if ( !r )
        return std::unexpected( r.error() );
    return std::bit_cast<float>( static_cast<std::uint32_t>( *r ) );
}

// ---------------------------------------------------------------------------
// Buffer management
// ---------------------------------------------------------------------------

Result<void> SaveBuffer::load_from( std::span<const std::byte> image ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( image.size() > buffer_size_ )
        return std::unexpected( SaveError::BufferExhausted );

    if ( !image.empty() )
        std::memcpy( base_, image.data(), image.size() );
    data_size_ = image.size();
    cursor_    = 0;
    return {};
}

void SaveBuffer::reset() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    cursor_    = 0;
    data_size_ = 0;
    tokens_.clear();
}

Result<void> SaveBuffer::seek( std::size_t pos ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    if ( pos > buffer_size_ )
        return std::unexpected( SaveError::BufferExhausted );
    cursor_ = pos;
    return {};
}

Result<std::span<const std::byte>>
SaveBuffer::view_at( std::size_t pos, std::size_t n ) const noexcept
{
    // Bounds-check without overflow: test `pos` first, then the remaining span.
    if ( pos > data_size_ || n > data_size_ - pos )
        return std::unexpected( SaveError::TruncatedBlock );
    return std::span<const std::byte>( base_ + pos, n );
}

void SaveBuffer::to_abi( ::xash::abi::SAVERESTOREDATA &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    out = ::xash::abi::SAVERESTOREDATA{}; // zero landmark / level-list fields

    // SAFETY: the ABI window hands the game DLL raw char* into our owned buffer;
    // std::byte* -> char* is a permitted byte-type reinterpretation.
    // @lifetime: SaveBuffer — valid while *this is alive and unmutated.
    out.pBaseData    = reinterpret_cast<char *>( base_ );
    out.pCurrentData = reinterpret_cast<char *>( base_ + cursor_ );
    out.size         = static_cast<int>( data_size_ );
    out.bufferSize   = static_cast<int>( buffer_size_ );
    // SaveInit projection: tokenSize is 0 until StoreHashTable flattens the
    // token blob into the buffer (a later save step); the DLL fills pTokens.
    out.tokenSize    = 0;
    out.tokenCount   = static_cast<int>( tokens_.token_count() );
    out.pTokens      = tokens_.abi_pointers();
    out.time         = time_;
}

std::span<const std::byte> SaveBuffer::data() const noexcept
{
    return std::span<const std::byte>( base_, data_size_ );
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<SaveBuffer> create_save_buffer( ::xash::memory::PoolHandle pool,
                                                std::size_t buffer_size,
                                                std::size_t token_count,
                                                float       time ) noexcept
{
    auto *sb = ::xash::memory::pool_new<SaveBuffer>( pool, pool, buffer_size,
                                                     token_count, time );
    if ( !sb )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag,
                           "create_save_buffer: object allocation failed" );
        return nullptr;
    }
    if ( !sb->valid() )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag,
                           "create_save_buffer: working buffer allocation failed" );
        ::xash::memory::pool_delete( sb );
        return nullptr;
    }
    return std::unique_ptr<SaveBuffer>( sb );
}

} // namespace xash::save
