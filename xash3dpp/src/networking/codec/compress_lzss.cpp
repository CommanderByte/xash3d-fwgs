// xash3dpp — LZSS codec
// Ported from engine/common/common.c (LZSS_Compress / LZSS_Decompress /
// LZSS_IsCompressed / LZSS_GetActualSize).
//
// The on-wire format is frozen — the header magic, look-shift, window size,
// and command-byte bit ordering are part of the network protocol contract.
// This port preserves the exact byte sequence the legacy implementation
// produces for any given input.

#include <xash3dpp/private/networking/codec/compress.hpp>

#include <algorithm>
#include <cstring>

namespace xash::networking::lzss {

namespace {

inline std::uint32_t read_le32( const std::byte *p ) noexcept
{
    return  ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[0] ) )       )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8  )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[2] ) ) << 16 )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[3] ) ) << 24 );
}

inline void write_le32( std::byte *p, std::uint32_t v ) noexcept
{
    p[0] = std::byte{ static_cast<std::uint8_t>( v        ) };
    p[1] = std::byte{ static_cast<std::uint8_t>( v >>  8  ) };
    p[2] = std::byte{ static_cast<std::uint8_t>( v >> 16  ) };
    p[3] = std::byte{ static_cast<std::uint8_t>( v >> 24  ) };
}

// ---- Hash list (one bucket per leading byte) -----------------------------

struct Node
{
    const std::byte *data = nullptr;
    Node            *prev = nullptr;
    Node            *next = nullptr;
};

struct List
{
    Node *start = nullptr;
    Node *end   = nullptr;
};

struct State
{
    List buckets[ 256 ] {};
    std::vector<Node> nodes{ window_size };
};

void build_hash( State &state, const std::byte *source )
{
    const std::size_t index = reinterpret_cast<std::uintptr_t>( source ) & ( window_size - 1 );
    Node &node              = state.nodes[ index ];

    if( node.data )
    {
        List &old = state.buckets[ std::to_integer<std::uint8_t>( *node.data ) ];
        if( node.prev )
        {
            old.end           = node.prev;
            node.prev->next   = nullptr;
        }
        else
        {
            old.start = nullptr;
            old.end   = nullptr;
        }
    }

    List &list = state.buckets[ std::to_integer<std::uint8_t>( *source ) ];
    node.data = source;
    node.prev = nullptr;
    node.next = list.start;
    if( list.start )
        list.start->prev = &node;
    else
        list.end = &node;
    list.start = &node;
}

} // namespace

// ---------------------------------------------------------------------------
// Header queries
// ---------------------------------------------------------------------------

bool is_compressed( std::span<const std::byte> src ) noexcept
{
    if( src.size() <= header_size )
        return false;
    return read_le32( src.data() ) == magic_id;
}

std::uint32_t actual_size( std::span<const std::byte> src ) noexcept
{
    if( src.size() <= header_size )
        return 0;
    if( read_le32( src.data() ) != magic_id )
        return 0;
    return read_le32( src.data() + 4 );
}

// ---------------------------------------------------------------------------
// Compression
// ---------------------------------------------------------------------------

Result<std::vector<std::byte>> compress( std::span<const std::byte> src )
{
    if( src.size() <= header_size + 8 )
        return std::unexpected( NetError::BadAddress );

    std::vector<std::byte> out( src.size() );
    // The output cursor stops `header_size + 8` bytes before the end so that
    // the worst-case "no compression possible" path can still emit the
    // trailing command and two zero bytes without overflowing.
    std::byte *const p_start = out.data();
    std::byte *const p_end   = p_start + src.size() - header_size - 8;
    std::byte *p_out         = p_start + header_size;

    write_le32( p_start,     magic_id );
    write_le32( p_start + 4, static_cast<std::uint32_t>( src.size() ) );

    State state;

    const std::byte *p_input    = src.data();
    const std::byte *p_lookahead = p_input;
    std::size_t      remaining   = src.size();
    int              put_cmd_bit = 0;
    std::byte       *p_cmd_byte  = nullptr;
    const std::byte *p_encoded   = nullptr;

    while( remaining > 0 )
    {
        const std::size_t look_len = std::min<std::size_t>( remaining, lookahead );
        const std::byte  *p_window = ( p_lookahead - p_input >= static_cast<std::ptrdiff_t>( window_size ) )
                                     ? ( p_lookahead - window_size ) : p_input;
        (void) p_window; // window is enforced by the hash table; explicit pointer kept for parity

        Node *hash = state.buckets[ std::to_integer<std::uint8_t>( *p_lookahead ) ].start;
        std::size_t encoded_length = 0;

        if( put_cmd_bit == 0 )
        {
            p_cmd_byte  = p_out++;
            *p_cmd_byte = std::byte{ 0 };
        }
        put_cmd_bit = ( put_cmd_bit + 1 ) & 0x07;

        while( hash != nullptr )
        {
            std::size_t length       = look_len;
            std::size_t match_length = 0;
            while( length-- && hash->data[ match_length ] == p_lookahead[ match_length ] )
                ++match_length;

            if( match_length > encoded_length )
            {
                encoded_length = match_length;
                p_encoded      = hash->data;
            }
            if( match_length == look_len )
                break;
            hash = hash->next;
        }

        if( encoded_length >= 3 )
        {
            const std::uint8_t cmd =
                ( std::to_integer<std::uint8_t>( *p_cmd_byte ) >> 1 ) | 0x80;
            *p_cmd_byte = std::byte{ cmd };
            const std::ptrdiff_t back = p_lookahead - p_encoded - 1;
            *p_out++ = std::byte{ static_cast<std::uint8_t>( back >> lookshift ) };
            *p_out++ = std::byte{ static_cast<std::uint8_t>(
                ( back << lookshift ) | ( encoded_length - 1 ) ) };
        }
        else
        {
            const std::uint8_t cmd =
                std::to_integer<std::uint8_t>( *p_cmd_byte ) >> 1;
            *p_cmd_byte = std::byte{ cmd };
            *p_out++ = *p_lookahead;
            encoded_length = 1;
        }

        for( std::size_t i = 0; i < encoded_length; ++i )
            build_hash( state, p_lookahead++ );

        remaining -= encoded_length;

        if( p_out >= p_end )
            return std::unexpected( NetError::BadAddress ); // compression worse than original
    }

    if( put_cmd_bit == 0 )
    {
        p_cmd_byte  = p_out++;
        *p_cmd_byte = std::byte{ 0x01 };
    }
    else
    {
        const std::uint8_t cmd =
            ( ( std::to_integer<std::uint8_t>( *p_cmd_byte ) >> 1 ) | 0x80 )
            >> ( 7 - put_cmd_bit );
        *p_cmd_byte = std::byte{ cmd };
    }
    *p_out++ = std::byte{ 0 };
    *p_out++ = std::byte{ 0 };

    out.resize( static_cast<std::size_t>( p_out - p_start ) );
    return out;
}

// ---------------------------------------------------------------------------
// Decompression
// ---------------------------------------------------------------------------

Result<std::size_t> decompress( std::span<const std::byte> src,
                                std::span<std::byte> dst ) noexcept
{
    if( src.size() <= header_size )
        return std::unexpected( NetError::BadAddress );

    const std::uint32_t size = actual_size( src );
    if( size == 0 )
        return std::unexpected( NetError::BadAddress );
    if( size > dst.size() )
        return std::unexpected( NetError::BufferTooSmall );

    const std::byte *p_in     = src.data() + header_size;
    const std::byte *p_in_end = src.data() + src.size() - 1;
    std::byte       *p_out    = dst.data();
    std::byte       *p_orig   = dst.data();
    std::size_t      total    = 0;
    int              get_bit  = 0;
    std::uint8_t     cmd      = 0;

    while( true )
    {
        if( get_bit == 0 )
        {
            if( p_in > p_in_end )
                return std::unexpected( NetError::BadAddress );
            cmd = std::to_integer<std::uint8_t>( *p_in++ );
        }
        get_bit = ( get_bit + 1 ) & 0x07;

        if( cmd & 0x01 )
        {
            if( p_in > p_in_end )
                return std::unexpected( NetError::BadAddress );
            int position = std::to_integer<std::uint8_t>( *p_in++ ) << lookshift;
            position    |= std::to_integer<std::uint8_t>( *p_in   ) >> lookshift;
            const int count = ( std::to_integer<std::uint8_t>( *p_in++ ) & 0x0F ) + 1;
            if( count == 1 )
                break;

            std::byte *src_ptr = p_out - position - 1;
            if( total + count > dst.size() || src_ptr < p_orig )
                return std::unexpected( NetError::BadAddress );
            for( int i = 0; i < count; ++i )
                *p_out++ = *src_ptr++;
            total += static_cast<std::size_t>( count );
        }
        else
        {
            if( total + 1 > dst.size() || p_in > p_in_end )
                return std::unexpected( NetError::BadAddress );
            *p_out++ = *p_in++;
            ++total;
        }
        cmd >>= 1;
    }

    if( total != size )
        return std::unexpected( NetError::BadAddress );
    return total;
}

} // namespace xash::networking::lzss
