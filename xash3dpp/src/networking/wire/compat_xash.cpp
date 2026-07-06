// xash3dpp — Xash SPLITPACKET encode/decode helpers
// Legacy reference: engine/common/net_ws.c NET_SendLong / NET_GetLong (Xash branch)

#include <xash3dpp/private/networking/wire/split_packet.hpp>

#include <algorithm>
#include <cstring>

namespace xash::networking {

namespace {

constexpr std::size_t k_header_size = sizeof( SplitHeaderXash );

inline void write_le32( std::byte *p, std::uint32_t v ) noexcept // compliance-allow(thread-assert): stateless wire transform — no thread affinity
{
    p[0] = std::byte{ static_cast<std::uint8_t>( v        ) };
    p[1] = std::byte{ static_cast<std::uint8_t>( v >>  8  ) };
    p[2] = std::byte{ static_cast<std::uint8_t>( v >> 16  ) };
    p[3] = std::byte{ static_cast<std::uint8_t>( v >> 24  ) };
}

inline void write_le16( std::byte *p, std::uint16_t v ) noexcept // compliance-allow(thread-assert): stateless wire transform — no thread affinity
{
    p[0] = std::byte{ static_cast<std::uint8_t>( v       ) };
    p[1] = std::byte{ static_cast<std::uint8_t>( v >> 8  ) };
}

inline std::uint32_t read_le32( const std::byte *p ) noexcept
{
    return  ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[0] ) )       )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8  )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[2] ) ) << 16 )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[3] ) ) << 24 );
}

inline std::uint16_t read_le16( const std::byte *p ) noexcept
{
    return static_cast<std::uint16_t>(
        ( static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( p[0] ) )       )
      | ( static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8  ) );
}

} // namespace

// ---------------------------------------------------------------------------
// SplitProducerXash
// ---------------------------------------------------------------------------

SplitProducerXash::SplitProducerXash( std::span<const std::byte> payload,
                                      std::int32_t sequence_number,
                                      std::size_t splitsize ) noexcept
    : payload_( payload )
    , sequence_number_( sequence_number )
{
    if( splitsize <= k_header_size )
        return;                                 // total_=0, exhausted
    body_size_ = splitsize - k_header_size;
    const std::size_t total = ( payload.size() + body_size_ - 1 ) / body_size_;
    if( total > 255 )
        return;                                 // can't represent in one byte
    total_ = static_cast<std::uint8_t>( total );
}

Result<std::size_t> SplitProducerXash::next( std::span<std::byte> out ) noexcept
{
    if( total_ == 0 || body_size_ == 0 )
        return std::unexpected( NetError::BadAddress );
    if( next_index_ >= total_ )
        return static_cast<std::size_t>( 0 );

    const std::size_t offset = next_index_ * body_size_;
    const std::size_t take   = std::min( body_size_, payload_.size() - offset );

    if( out.size() < k_header_size + take )
        return std::unexpected( NetError::BufferTooSmall );

    write_le32( &out[0], net_header_split_packet );
    write_le32( &out[4], static_cast<std::uint32_t>( sequence_number_ ) );
    const std::uint16_t packet_id =
        static_cast<std::uint16_t>( ( next_index_ << 8 ) | total_ );
    write_le16( &out[8], packet_id );

    std::memcpy( &out[ k_header_size ], &payload_[ offset ], take );
    ++next_index_;
    return k_header_size + take;
}

// ---------------------------------------------------------------------------
// decode_split_xash
// ---------------------------------------------------------------------------

Result<SplitFragmentInfo> decode_split_xash( std::span<const std::byte> datagram ) noexcept
{
    if( datagram.size() < k_header_size )
        return std::unexpected( NetError::BufferTooSmall );

    const std::uint32_t magic = read_le32( &datagram[0] );
    if( magic != net_header_split_packet )
        return std::unexpected( NetError::BadAddress );

    SplitFragmentInfo info{};
    info.sequence_number = static_cast<std::int32_t>( read_le32( &datagram[4] ) );
    const std::uint16_t packet_id = read_le16( &datagram[8] );
    info.packet_number = static_cast<std::uint8_t>( packet_id >> 8 );
    info.packet_count  = static_cast<std::uint8_t>( packet_id & 0xFF );
    if( info.packet_count == 0 || info.packet_number >= info.packet_count )
        return std::unexpected( NetError::BadAddress );
    info.payload = datagram.subspan( k_header_size );
    return info;
}

} // namespace xash::networking
