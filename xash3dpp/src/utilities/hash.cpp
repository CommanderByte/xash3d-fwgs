// xash3dpp — CRC32 and MD5 implementation
// Legacy reference: public/crclib.c
//
// CRC32 table generated from IEEE 802.3 polynomial 0xEDB88320.

#include <xash3dpp/utilities/hash.hpp>
#include <cstring>
#include <cstdlib>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// CRC32 table  (IEEE 802.3, reflected polynomial 0xEDB88320)
// ---------------------------------------------------------------------------

static constexpr std::uint32_t k_crc32table[256] = {
    0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau,
    0x076dc419u, 0x706af48fu, 0xe963a535u, 0x9e6495a3u,
    0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u,
    0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u,
    0x1db71064u, 0x6ab020f2u, 0xf3b97148u, 0x84be41deu,
    0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
    0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu,
    0x14015c4fu, 0x63066cd9u, 0xfa0f3d63u, 0x8d080df5u,
    0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u,
    0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu,
    0x35b5a8fau, 0x42b2986cu, 0xdbbbc9d6u, 0xacbcf940u,
    0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
    0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u,
    0x21b4f4b5u, 0x56b3c423u, 0xcfba9599u, 0xb8bda50fu,
    0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u,
    0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du,
    0x76dc4190u, 0x01db7106u, 0x98d220bcu, 0xefd5102au,
    0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
    0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u,
    0x7f6a0dbbu, 0x086d3d2du, 0x91646c97u, 0xe6635c01u,
    0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu,
    0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u,
    0x65b0d9c6u, 0x12b7e950u, 0x8bbeb8eau, 0xfcb9887cu,
    0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
    0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u,
    0x4adfa541u, 0x3dd895d7u, 0xa4d1c46du, 0xd3d6f4fbu,
    0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u,
    0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u,
    0x5005713cu, 0x270241aau, 0xbe0b1010u, 0xc90c2086u,
    0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
    0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u,
    0x59b33d17u, 0x2eb40d81u, 0xb7bd5c3bu, 0xc0ba6cadu,
    0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au,
    0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u,
    0xe3630b12u, 0x94643b84u, 0x0d6d6a3eu, 0x7a6a5aa8u,
    0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
    0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu,
    0xf762575du, 0x806567cbu, 0x196c3671u, 0x6e6b06e7u,
    0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu,
    0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u,
    0xd6d6a3e8u, 0xa1d1937eu, 0x38d8c2c4u, 0x4fdff252u,
    0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
    0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u,
    0xdf60efc3u, 0xa867df55u, 0x316e8eefu, 0x4669be79u,
    0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u,
    0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu,
    0xc5ba3bbeu, 0xb2bd0b28u, 0x2bb45a92u, 0x5cb36a04u,
    0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
    0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au,
    0x9c0906a9u, 0xeb0e363fu, 0x72076785u, 0x05005713u,
    0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u,
    0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u,
    0x86d3d2d4u, 0xf1d4e242u, 0x68ddb3f8u, 0x1fda836eu,
    0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
    0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu,
    0x8f659effu, 0xf862ae69u, 0x616bffd3u, 0x166ccf45u,
    0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u,
    0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu,
    0xaed16a4au, 0xd9d65adcu, 0x40df0b66u, 0x37d83bf0u,
    0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
    0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u,
    0xbad03605u, 0xcdd70693u, 0x54de5729u, 0x23d967bfu,
    0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u,
    0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du,
};

// ---------------------------------------------------------------------------
// CRC32
// ---------------------------------------------------------------------------

void crc32_update( Crc32 &state, std::uint8_t byte ) noexcept
{
    state = k_crc32table[static_cast<std::uint8_t>( state ) ^ byte] ^ ( state >> 8 );
}

void crc32_update( Crc32 &state, const void *data, std::size_t len ) noexcept
{
    const auto *p = static_cast<const std::uint8_t *>( data );
    while( len-- )
        crc32_update( state, *p++ );
}

Crc32 crc32( const void *data, std::size_t len ) noexcept
{
    Crc32 s;
    crc32_init( s );
    crc32_update( s, data, len );
    return crc32_final( s );
}

std::uint8_t crc32_block_sequence( const std::uint8_t *base, int length, int sequence ) noexcept
{
    if( !base ) return 0;
    if( sequence < 0 ) sequence = std::abs( sequence );
    if( length > 60 ) length = 60;

    std::array<std::uint8_t, 64> buffer{};
    std::memcpy( buffer.data(), base, static_cast<std::size_t>( length ) );

    // Append 4 bytes from the CRC table, little-endian, at the sequence offset.
    const int off = sequence % 0x3FC;
    const std::uint32_t t0 = k_crc32table[ off / 4     ];
    const std::uint32_t t1 = k_crc32table[ off / 4 + 1 ];
    std::array<std::uint8_t, 8> le8{};
    le8[0] = static_cast<std::uint8_t>( t0       );  le8[1] = static_cast<std::uint8_t>( t0 >>  8 );
    le8[2] = static_cast<std::uint8_t>( t0 >> 16 );  le8[3] = static_cast<std::uint8_t>( t0 >> 24 );
    le8[4] = static_cast<std::uint8_t>( t1       );  le8[5] = static_cast<std::uint8_t>( t1 >>  8 );
    le8[6] = static_cast<std::uint8_t>( t1 >> 16 );  le8[7] = static_cast<std::uint8_t>( t1 >> 24 );
    std::memcpy( buffer.data() + length, le8.data() + ( off & 3 ), 4 );

    Crc32 state;
    crc32_init( state );
    crc32_update( state, buffer.data(), static_cast<std::size_t>( length + 4 ) );
    return static_cast<std::uint8_t>( crc32_final( state ) );
}

// ---------------------------------------------------------------------------
// MD5 internals
// ---------------------------------------------------------------------------

static constexpr std::uint32_t md5_rotl( std::uint32_t v, int s ) noexcept
{
    return ( v << s ) | ( v >> ( 32 - s ) );
}

// The four MD5 round functions.
static constexpr std::uint32_t md5_f1( std::uint32_t x, std::uint32_t y, std::uint32_t z ) noexcept { return z ^ ( x & ( y ^ z ) ); }
static constexpr std::uint32_t md5_f2( std::uint32_t x, std::uint32_t y, std::uint32_t z ) noexcept { return y ^ ( z & ( x ^ y ) ); }
static constexpr std::uint32_t md5_f3( std::uint32_t x, std::uint32_t y, std::uint32_t z ) noexcept { return x ^ y ^ z; }
static constexpr std::uint32_t md5_f4( std::uint32_t x, std::uint32_t y, std::uint32_t z ) noexcept { return y ^ ( x | ~z ); }

// One MD5 step: w += f(x,y,z) + data; w = rotl(w,s); w += x.
// Load 16 little-endian uint32 words from a 64-byte block.
static void md5_load_block( std::span<std::uint32_t, 16>      w,
                             std::span<const std::uint8_t, 64> in ) noexcept
{
    for( int i = 0; i < 16; ++i )
        w[i] = static_cast<std::uint32_t>( in[i*4+0] )
             | static_cast<std::uint32_t>( in[i*4+1] ) <<  8
             | static_cast<std::uint32_t>( in[i*4+2] ) << 16
             | static_cast<std::uint32_t>( in[i*4+3] ) << 24;
}

// Single MD5 round step — applies F, adds data, rotates left by s, then adds x.
template<auto F>
static constexpr void md5_step(
    std::uint32_t &w, std::uint32_t x, std::uint32_t y, std::uint32_t z,
    std::uint32_t data, int s ) noexcept
{
    w += F( x, y, z ) + data;
    w  = md5_rotl( w, s );
    w += x;
}

static void md5_transform( std::span<std::uint32_t, 4>       buf,
                            std::span<const std::uint8_t, 64> in ) noexcept
{
    std::array<std::uint32_t, 16> w{};
    md5_load_block( w, in );

    std::uint32_t a = buf[0], b = buf[1], c = buf[2], d = buf[3];

    md5_step<md5_f1>( a,b,c,d, w[ 0]+0xd76aa478u,  7 ); md5_step<md5_f1>( d,a,b,c, w[ 1]+0xe8c7b756u, 12 );
    md5_step<md5_f1>( c,d,a,b, w[ 2]+0x242070dbu, 17 ); md5_step<md5_f1>( b,c,d,a, w[ 3]+0xc1bdceeeu, 22 );
    md5_step<md5_f1>( a,b,c,d, w[ 4]+0xf57c0fafu,  7 ); md5_step<md5_f1>( d,a,b,c, w[ 5]+0x4787c62au, 12 );
    md5_step<md5_f1>( c,d,a,b, w[ 6]+0xa8304613u, 17 ); md5_step<md5_f1>( b,c,d,a, w[ 7]+0xfd469501u, 22 );
    md5_step<md5_f1>( a,b,c,d, w[ 8]+0x698098d8u,  7 ); md5_step<md5_f1>( d,a,b,c, w[ 9]+0x8b44f7afu, 12 );
    md5_step<md5_f1>( c,d,a,b, w[10]+0xffff5bb1u, 17 ); md5_step<md5_f1>( b,c,d,a, w[11]+0x895cd7beu, 22 );
    md5_step<md5_f1>( a,b,c,d, w[12]+0x6b901122u,  7 ); md5_step<md5_f1>( d,a,b,c, w[13]+0xfd987193u, 12 );
    md5_step<md5_f1>( c,d,a,b, w[14]+0xa679438eu, 17 ); md5_step<md5_f1>( b,c,d,a, w[15]+0x49b40821u, 22 );

    md5_step<md5_f2>( a,b,c,d, w[ 1]+0xf61e2562u,  5 ); md5_step<md5_f2>( d,a,b,c, w[ 6]+0xc040b340u,  9 );
    md5_step<md5_f2>( c,d,a,b, w[11]+0x265e5a51u, 14 ); md5_step<md5_f2>( b,c,d,a, w[ 0]+0xe9b6c7aau, 20 );
    md5_step<md5_f2>( a,b,c,d, w[ 5]+0xd62f105du,  5 ); md5_step<md5_f2>( d,a,b,c, w[10]+0x02441453u,  9 );
    md5_step<md5_f2>( c,d,a,b, w[15]+0xd8a1e681u, 14 ); md5_step<md5_f2>( b,c,d,a, w[ 4]+0xe7d3fbc8u, 20 );
    md5_step<md5_f2>( a,b,c,d, w[ 9]+0x21e1cde6u,  5 ); md5_step<md5_f2>( d,a,b,c, w[14]+0xc33707d6u,  9 );
    md5_step<md5_f2>( c,d,a,b, w[ 3]+0xf4d50d87u, 14 ); md5_step<md5_f2>( b,c,d,a, w[ 8]+0x455a14edu, 20 );
    md5_step<md5_f2>( a,b,c,d, w[13]+0xa9e3e905u,  5 ); md5_step<md5_f2>( d,a,b,c, w[ 2]+0xfcefa3f8u,  9 );
    md5_step<md5_f2>( c,d,a,b, w[ 7]+0x676f02d9u, 14 ); md5_step<md5_f2>( b,c,d,a, w[12]+0x8d2a4c8au, 20 );

    md5_step<md5_f3>( a,b,c,d, w[ 5]+0xfffa3942u,  4 ); md5_step<md5_f3>( d,a,b,c, w[ 8]+0x8771f681u, 11 );
    md5_step<md5_f3>( c,d,a,b, w[11]+0x6d9d6122u, 16 ); md5_step<md5_f3>( b,c,d,a, w[14]+0xfde5380cu, 23 );
    md5_step<md5_f3>( a,b,c,d, w[ 1]+0xa4beea44u,  4 ); md5_step<md5_f3>( d,a,b,c, w[ 4]+0x4bdecfa9u, 11 );
    md5_step<md5_f3>( c,d,a,b, w[ 7]+0xf6bb4b60u, 16 ); md5_step<md5_f3>( b,c,d,a, w[10]+0xbebfbc70u, 23 );
    md5_step<md5_f3>( a,b,c,d, w[13]+0x289b7ec6u,  4 ); md5_step<md5_f3>( d,a,b,c, w[ 0]+0xeaa127fau, 11 );
    md5_step<md5_f3>( c,d,a,b, w[ 3]+0xd4ef3085u, 16 ); md5_step<md5_f3>( b,c,d,a, w[ 6]+0x04881d05u, 23 );
    md5_step<md5_f3>( a,b,c,d, w[ 9]+0xd9d4d039u,  4 ); md5_step<md5_f3>( d,a,b,c, w[12]+0xe6db99e5u, 11 );
    md5_step<md5_f3>( c,d,a,b, w[15]+0x1fa27cf8u, 16 ); md5_step<md5_f3>( b,c,d,a, w[ 2]+0xc4ac5665u, 23 );

    md5_step<md5_f4>( a,b,c,d, w[ 0]+0xf4292244u,  6 ); md5_step<md5_f4>( d,a,b,c, w[ 7]+0x432aff97u, 10 );
    md5_step<md5_f4>( c,d,a,b, w[14]+0xab9423a7u, 15 ); md5_step<md5_f4>( b,c,d,a, w[ 5]+0xfc93a039u, 21 );
    md5_step<md5_f4>( a,b,c,d, w[12]+0x655b59c3u,  6 ); md5_step<md5_f4>( d,a,b,c, w[ 3]+0x8f0ccc92u, 10 );
    md5_step<md5_f4>( c,d,a,b, w[10]+0xffeff47du, 15 ); md5_step<md5_f4>( b,c,d,a, w[ 1]+0x85845dd1u, 21 );
    md5_step<md5_f4>( a,b,c,d, w[ 8]+0x6fa87e4fu,  6 ); md5_step<md5_f4>( d,a,b,c, w[15]+0xfe2ce6e0u, 10 );
    md5_step<md5_f4>( c,d,a,b, w[ 6]+0xa3014314u, 15 ); md5_step<md5_f4>( b,c,d,a, w[13]+0x4e0811a1u, 21 );
    md5_step<md5_f4>( a,b,c,d, w[ 4]+0xf7537e82u,  6 ); md5_step<md5_f4>( d,a,b,c, w[11]+0xbd3af235u, 10 );
    md5_step<md5_f4>( c,d,a,b, w[ 2]+0x2ad7d2bbu, 15 ); md5_step<md5_f4>( b,c,d,a, w[ 9]+0xeb86d391u, 21 );

    buf[0] += a;  buf[1] += b;  buf[2] += c;  buf[3] += d;
}

// ---------------------------------------------------------------------------
// MD5 public API
// ---------------------------------------------------------------------------

void md5_init( Md5State &state ) noexcept
{
    state.buf[0] = 0x67452301u;
    state.buf[1] = 0xefcdab89u;
    state.buf[2] = 0x98badcfeu;
    state.buf[3] = 0x10325476u;
    state.bits[0] = 0;
    state.bits[1] = 0;
}

void md5_update( Md5State &state, const void *data, std::size_t len ) noexcept
{
    const auto *buf = static_cast<const std::uint8_t *>( data );

    // Update bit count (64-bit, little-endian internally).
    const std::uint32_t t = state.bits[0];
    if( ( state.bits[0] = t + static_cast<std::uint32_t>( len ) * 8u ) < t )
        ++state.bits[1];
    state.bits[1] += static_cast<std::uint32_t>( len >> 29 );

    // Bytes already buffered.
    const std::uint32_t offset = ( t >> 3 ) & 0x3fu;

    if( offset )
    {
        const std::uint32_t space = 64u - offset;
        if( len < space )
        {
            std::memcpy( state.in.data() + offset, buf, len );
            return;
        }
        std::memcpy( state.in.data() + offset, buf, space );
        md5_transform( state.buf, state.in );
        buf += space;
        len -= space;
    }

    while( len >= 64 )
    {
        std::memcpy( state.in.data(), buf, 64 );
        md5_transform( state.buf, state.in );
        buf += 64;
        len -= 64;
    }

    std::memcpy( state.in.data(), buf, len );
}

static void store_le32( std::uint8_t *p, std::uint32_t v ) noexcept
{
    p[0] = static_cast<std::uint8_t>( v       );
    p[1] = static_cast<std::uint8_t>( v >>  8 );
    p[2] = static_cast<std::uint8_t>( v >> 16 );
    p[3] = static_cast<std::uint8_t>( v >> 24 );
}

std::array<std::uint8_t, 16> md5_final( Md5State &state ) noexcept
{
    // Number of bytes already in state.in.
    const std::uint32_t count = ( state.bits[0] >> 3 ) & 0x3fu;

    // Pad with 0x80 followed by zeros to 56 mod 64.
    std::uint8_t *p = state.in.data() + count;
    *p++ = 0x80u;

    const std::uint32_t pad = 64u - 1u - count;
    if( pad < 8u )
    {
        std::memset( p, 0, pad );
        md5_transform( state.buf, state.in );
        std::memset( state.in.data(), 0, 56 );
    }
    else
    {
        std::memset( p, 0, pad - 8u );
    }

    // Append length in bits as little-endian uint64.
    store_le32( state.in.data() + 56, state.bits[0] );
    store_le32( state.in.data() + 60, state.bits[1] );

    md5_transform( state.buf, state.in );

    // Write digest in little-endian byte order.
    std::array<std::uint8_t, 16> digest{};
    for( int i = 0; i < 4; ++i )
        store_le32( digest.data() + i * 4, state.buf[i] );

    std::memset( &state, 0, sizeof( state ) );
    return digest;
}

} // namespace xash::utilities
