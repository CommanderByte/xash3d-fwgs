// xash3dpp — MessageBuf implementation
// Legacy reference: engine/common/net_buffer.c
//
// All multi-byte primitives are emitted little-endian regardless of host
// endianness.  The legacy code special-cased big-endian with byteswap macros;
// we use explicit per-byte shifts so the on-wire layout is independent of
// platform without any preprocessor conditionals at the call site.

#include <xash3dpp/networking/message_buf.hpp>

#include <algorithm>
#include <bit>
#include <cstring>

namespace xash::networking {

namespace {

constexpr std::uint8_t bit_mask( int n ) noexcept
{
    return static_cast<std::uint8_t>( ( 1u << n ) - 1u );
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

MessageBuf::MessageBuf( std::span<std::byte> data, const char *name ) noexcept
{
    rebind( data, name );
}

void MessageBuf::reset() noexcept
{
    cur_bit_  = 0;
    overflow_ = false;
}

void MessageBuf::rebind( std::span<std::byte> data, const char *name ) noexcept
{
    data_     = data.data();
    num_bits_ = data.size() * 8;
    name_     = name ? name : "unnamed";
    reset();
}

void MessageBuf::rebind_read( std::span<const std::byte> data, const char *name ) noexcept
{
    // SAFETY: all read_* methods advance cur_bit_ but never write through
    // data_.  Callers must not invoke write_* on a buffer bound this way.
    rebind( std::span<std::byte>{ const_cast<std::byte *>( data.data() ), data.size() }, name );
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

std::size_t MessageBuf::num_bytes_written() const noexcept
{
    // Round up to the next byte boundary.
    return ( cur_bit_ + 7 ) >> 3;
}

std::size_t MessageBuf::num_bits_left() const noexcept
{
    return cur_bit_ >= num_bits_ ? 0 : ( num_bits_ - cur_bit_ );
}

// ---------------------------------------------------------------------------
// Seek
// ---------------------------------------------------------------------------

bool MessageBuf::seek_to_bit( std::ptrdiff_t bit, SeekOrigin origin ) noexcept
{
    std::ptrdiff_t target = bit;
    switch( origin )
    {
        case SeekOrigin::Begin:                                              break;
        case SeekOrigin::Current: target += static_cast<std::ptrdiff_t>( cur_bit_  ); break;
        case SeekOrigin::End:     target += static_cast<std::ptrdiff_t>( num_bits_ ); break;
    }
    if( target < 0 || static_cast<std::size_t>( target ) > num_bits_ )
        return false;
    cur_bit_ = static_cast<std::size_t>( target );
    return true;
}

// ---------------------------------------------------------------------------
// Overflow gate
// ---------------------------------------------------------------------------

bool MessageBuf::check_overflow( std::size_t additional_bits ) noexcept
{
    if( cur_bit_ + additional_bits > num_bits_ )
    {
        overflow_ = true;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Bit writes
// ---------------------------------------------------------------------------

void MessageBuf::write_one_bit( int value ) noexcept
{
    if( check_overflow( 1 ) )
        return;
    const std::size_t byte_idx = cur_bit_ >> 3;
    const std::uint8_t mask    = static_cast<std::uint8_t>( 1u << ( cur_bit_ & 7 ) );
    auto &b = reinterpret_cast<std::uint8_t &>( data_[ byte_idx ] );
    if( value ) b |=  mask;
    else        b &= ~mask;
    ++cur_bit_;
}

void MessageBuf::write_ubit_long( std::uint32_t value, int num_bits ) noexcept
{
    if( num_bits <= 0 || num_bits > 32 )
    {
        overflow_ = true;
        return;
    }
    if( check_overflow( static_cast<std::size_t>( num_bits ) ) )
        return;

    // Mask the input to num_bits to avoid leaking high bits.
    const std::uint64_t mask = ( num_bits == 32 )
        ? 0xFFFFFFFFull
        : ( ( 1ull << num_bits ) - 1ull );
    std::uint64_t v = value & mask;

    int bits_remaining = num_bits;
    while( bits_remaining > 0 )
    {
        const std::size_t  byte_idx   = cur_bit_ >> 3;
        const int          bit_in_byte = static_cast<int>( cur_bit_ & 7 );
        const int          take        = std::min( bits_remaining, 8 - bit_in_byte );
        const std::uint8_t shifted     = static_cast<std::uint8_t>(
            ( v & bit_mask( take ) ) << bit_in_byte );
        const std::uint8_t clear_mask  = static_cast<std::uint8_t>(
            bit_mask( take ) << bit_in_byte );

        auto &b = reinterpret_cast<std::uint8_t &>( data_[ byte_idx ] );
        b = static_cast<std::uint8_t>( ( b & ~clear_mask ) | shifted );

        v             >>= take;
        cur_bit_       += static_cast<std::size_t>( take );
        bits_remaining -= take;
    }
}

void MessageBuf::write_sbit_long( std::int32_t value, int num_bits ) noexcept
{
    // Sign-extension-aware: store low (num_bits - 1) bits then a sign bit.
    if( num_bits <= 1 || num_bits > 32 )
    {
        overflow_ = true;
        return;
    }
    const std::uint32_t mask = ( num_bits == 32 )
        ? 0xFFFFFFFFu
        : ( ( 1u << num_bits ) - 1u );
    write_ubit_long( static_cast<std::uint32_t>( value ) & mask, num_bits );
}

bool MessageBuf::write_bits( std::span<const std::byte> src, std::size_t num_bits ) noexcept
{
    if( num_bits > src.size() * 8 )
    {
        overflow_ = true;
        return false;
    }
    if( check_overflow( num_bits ) )
        return false;

    std::size_t bits_remaining = num_bits;
    std::size_t src_bit        = 0;
    while( bits_remaining >= 8 )
    {
        write_ubit_long(
            static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( src[ src_bit >> 3 ] ) ),
            8 );
        src_bit         += 8;
        bits_remaining  -= 8;
    }
    if( bits_remaining > 0 )
    {
        const std::uint8_t tail = std::to_integer<std::uint8_t>( src[ src_bit >> 3 ] )
                                  & bit_mask( static_cast<int>( bits_remaining ) );
        write_ubit_long( tail, static_cast<int>( bits_remaining ) );
    }
    return !overflow_;
}

// ---------------------------------------------------------------------------
// Byte writes — all little-endian on the wire
// ---------------------------------------------------------------------------

void MessageBuf::write_byte( std::uint8_t  v ) noexcept { write_ubit_long( v, 8  ); }
void MessageBuf::write_char( std::int8_t   v ) noexcept { write_ubit_long( static_cast<std::uint8_t> ( v ), 8  ); }
void MessageBuf::write_word( std::uint16_t v ) noexcept { write_ubit_long( v, 16 ); }
void MessageBuf::write_short( std::int16_t v ) noexcept { write_ubit_long( static_cast<std::uint16_t>( v ), 16 ); }
void MessageBuf::write_dword( std::uint32_t v ) noexcept { write_ubit_long( v, 32 ); }
void MessageBuf::write_long ( std::int32_t  v ) noexcept { write_ubit_long( static_cast<std::uint32_t>( v ), 32 ); }

void MessageBuf::write_float( float v ) noexcept
{
    std::uint32_t bits = std::bit_cast<std::uint32_t>( v );
    write_ubit_long( bits, 32 );
}

bool MessageBuf::write_string( std::string_view s ) noexcept
{
    for( char c : s )
    {
        write_byte( static_cast<std::uint8_t>( c ) );
        if( overflow_ )
            return false;
    }
    write_byte( 0 );
    return !overflow_;
}

bool MessageBuf::write_bytes( std::span<const std::byte> src ) noexcept
{
    return write_bits( src, src.size() * 8 );
}

// ---------------------------------------------------------------------------
// Bit reads
// ---------------------------------------------------------------------------

int MessageBuf::read_one_bit() noexcept
{
    if( check_overflow( 1 ) )
        return 0;
    const std::size_t  byte_idx = cur_bit_ >> 3;
    const std::uint8_t mask     = static_cast<std::uint8_t>( 1u << ( cur_bit_ & 7 ) );
    const int          result   = ( std::to_integer<std::uint8_t>( data_[ byte_idx ] ) & mask ) ? 1 : 0;
    ++cur_bit_;
    return result;
}

std::uint32_t MessageBuf::read_ubit_long( int num_bits ) noexcept
{
    if( num_bits <= 0 || num_bits > 32 )
    {
        overflow_ = true;
        return 0;
    }
    if( check_overflow( static_cast<std::size_t>( num_bits ) ) )
        return 0;

    std::uint32_t out         = 0;
    int           bits_read   = 0;
    int           bits_left   = num_bits;
    while( bits_left > 0 )
    {
        const std::size_t  byte_idx    = cur_bit_ >> 3;
        const int          bit_in_byte = static_cast<int>( cur_bit_ & 7 );
        const int          take        = std::min( bits_left, 8 - bit_in_byte );
        const std::uint8_t byte_val    = std::to_integer<std::uint8_t>( data_[ byte_idx ] );
        const std::uint32_t chunk      = ( byte_val >> bit_in_byte ) & bit_mask( take );
        out          |= chunk << bits_read;
        cur_bit_     += static_cast<std::size_t>( take );
        bits_read    += take;
        bits_left    -= take;
    }
    return out;
}

std::int32_t MessageBuf::read_sbit_long( int num_bits ) noexcept
{
    if( num_bits <= 1 || num_bits > 32 )
    {
        overflow_ = true;
        return 0;
    }
    const std::uint32_t raw = read_ubit_long( num_bits );
    if( num_bits == 32 )
        return static_cast<std::int32_t>( raw );
    const std::uint32_t sign_bit = 1u << ( num_bits - 1 );
    if( raw & sign_bit )
        return static_cast<std::int32_t>( raw | ~( ( 1u << num_bits ) - 1u ) );
    return static_cast<std::int32_t>( raw );
}

bool MessageBuf::read_bits( std::span<std::byte> dst, std::size_t num_bits ) noexcept
{
    if( num_bits > dst.size() * 8 )
    {
        overflow_ = true;
        return false;
    }
    if( check_overflow( num_bits ) )
        return false;

    std::size_t bits_remaining = num_bits;
    std::size_t dst_bit        = 0;
    while( bits_remaining >= 8 )
    {
        dst[ dst_bit >> 3 ] = std::byte{ static_cast<std::uint8_t>( read_ubit_long( 8 ) ) };
        dst_bit         += 8;
        bits_remaining  -= 8;
    }
    if( bits_remaining > 0 )
    {
        dst[ dst_bit >> 3 ] = std::byte{
            static_cast<std::uint8_t>( read_ubit_long( static_cast<int>( bits_remaining ) ) )
        };
    }
    return !overflow_;
}

// ---------------------------------------------------------------------------
// Byte reads
// ---------------------------------------------------------------------------

std::uint8_t  MessageBuf::read_byte () noexcept { return static_cast<std::uint8_t>( read_ubit_long( 8  ) ); }
std::int8_t   MessageBuf::read_char () noexcept { return static_cast<std::int8_t> ( read_ubit_long( 8  ) ); }
std::uint16_t MessageBuf::read_word () noexcept { return static_cast<std::uint16_t>( read_ubit_long( 16 ) ); }
std::int16_t  MessageBuf::read_short() noexcept { return static_cast<std::int16_t> ( read_ubit_long( 16 ) ); }
std::uint32_t MessageBuf::read_dword() noexcept { return read_ubit_long( 32 ); }
std::int32_t  MessageBuf::read_long () noexcept { return static_cast<std::int32_t>( read_ubit_long( 32 ) ); }

float MessageBuf::read_float() noexcept
{
    const std::uint32_t bits = read_ubit_long( 32 );
    return std::bit_cast<float>( bits );
}

std::size_t MessageBuf::read_string( std::span<char> dst ) noexcept
{
    if( dst.empty() )
        return 0;
    std::size_t written = 0;
    while( true )
    {
        const std::uint8_t c = read_byte();
        if( overflow_ || c == 0 )
            break;
        if( written + 1 < dst.size() )
            dst[ written++ ] = static_cast<char>( c );
        else
        {
            // truncate; keep draining bytes until we hit NUL or overflow
            // so the stream stays aligned past the string.
        }
    }
    dst[ written ] = '\0';
    return written;
}

bool MessageBuf::read_bytes( std::span<std::byte> dst ) noexcept
{
    return read_bits( dst, dst.size() * 8 );
}

// ---------------------------------------------------------------------------
// Quantised reals.  Legacy reference: engine/common/net_buffer.c
//   MSG_WriteCoord / MSG_ReadCoord (1/8-unit fixed-point int16)
//   MSG_WriteBitAngle / MSG_ReadBitAngle (numbits-quantised angle in [0,360))
// ---------------------------------------------------------------------------

namespace
{

constexpr float coord_scale = 8.0f;

float wrap_angle_0_360( float angle ) noexcept
{
    // Match legacy fmod-then-shift behaviour.
    float wrapped = angle - 360.0f * static_cast<int>( angle / 360.0f );
    if( wrapped < 0.0f )
        wrapped += 360.0f;
    return wrapped;
}

} // namespace

void MessageBuf::write_coord( float v ) noexcept
{
    // Round-toward-zero matches the legacy `(int)( val * 8 )` truncation.
    write_short( static_cast<std::int16_t>( v * coord_scale ) );
}

void MessageBuf::write_coord_large( float v ) noexcept
{
    // Equivalent of Q_rint: round half-away-from-zero.
    const float r = v >= 0.0f ? v + 0.5f : v - 0.5f;
    write_short( static_cast<std::int16_t>( r ) );
}

void MessageBuf::write_bit_angle( float angle, int num_bits ) noexcept
{
    if( num_bits <= 0 || num_bits > 32 )
    {
        overflow_ = true;
        return;
    }
    const std::uint32_t shift   = ( num_bits == 32 )
        ? 0u
        : ( 1u << num_bits );
    const std::uint32_t mask    = ( num_bits == 32 )
        ? 0xFFFFFFFFu
        : ( shift - 1u );
    const float         wrapped = wrap_angle_0_360( angle );
    const std::uint32_t scale   = ( num_bits == 32 )
        ? 0xFFFFFFFFu
        : shift;
    const std::int64_t  d       = static_cast<std::int64_t>(
        ( static_cast<double>( wrapped ) * scale ) / 360.0 );
    write_ubit_long( static_cast<std::uint32_t>( d ) & mask, num_bits );
}

void MessageBuf::write_vec3_coord( float x, float y, float z ) noexcept
{
    write_coord( x );
    write_coord( y );
    write_coord( z );
}

void MessageBuf::write_vec3_angles( float x, float y, float z ) noexcept
{
    write_bit_angle( x, 16 );
    write_bit_angle( y, 16 );
    write_bit_angle( z, 16 );
}

float MessageBuf::read_coord() noexcept
{
    return static_cast<float>( read_short() ) * ( 1.0f / coord_scale );
}

float MessageBuf::read_coord_large() noexcept
{
    return static_cast<float>( read_short() );
}

float MessageBuf::read_bit_angle( int num_bits ) noexcept
{
    if( num_bits <= 0 || num_bits > 32 )
    {
        overflow_ = true;
        return 0.0f;
    }
    const std::uint32_t shift = ( num_bits == 32 )
        ? 0xFFFFFFFFu
        : ( 1u << num_bits );
    const std::uint32_t i     = read_ubit_long( num_bits );
    float               r     = static_cast<float>(
        ( static_cast<double>( i ) * 360.0 ) / shift );
    if( r > 180.0f )
        r -= 360.0f;
    return r;
}

void MessageBuf::read_vec3_coord( float &x, float &y, float &z ) noexcept
{
    x = read_coord();
    y = read_coord();
    z = read_coord();
}

void MessageBuf::read_vec3_angles( float &x, float &y, float &z ) noexcept
{
    x = read_bit_angle( 16 );
    y = read_bit_angle( 16 );
    z = read_bit_angle( 16 );
}

} // namespace xash::networking
