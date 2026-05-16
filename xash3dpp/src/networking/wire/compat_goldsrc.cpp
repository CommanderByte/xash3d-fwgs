// xash3dpp — GoldSrc SPLITPACKETGS encode/decode helpers
// Legacy reference: engine/common/net_ws.c NET_SendLong / NET_GetLong (GoldSrc branch).
//
// GoldSrc packs (packet_number, packet_count) into a single byte as nibbles,
// so both values are capped at 15.  The legacy engine restricted this further
// to NET_MAX_GOLDSRC_FRAGMENTS = 5.

#include <xash3dpp/private/networking/wire/split_packet.hpp>

#include <algorithm>
#include <cstring>

namespace xash::networking {

namespace {

constexpr std::size_t k_header_size = sizeof( SplitHeaderGoldSrc );

inline void write_le32( std::byte *p, std::uint32_t v ) noexcept
{
    p[0] = std::byte{ static_cast<std::uint8_t>( v        ) };
    p[1] = std::byte{ static_cast<std::uint8_t>( v >>  8  ) };
    p[2] = std::byte{ static_cast<std::uint8_t>( v >> 16  ) };
    p[3] = std::byte{ static_cast<std::uint8_t>( v >> 24  ) };
}

inline std::uint32_t read_le32( const std::byte *p ) noexcept
{
    return  ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[0] ) )       )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8  )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[2] ) ) << 16 )
          | ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[3] ) ) << 24 );
}

} // namespace

// ---------------------------------------------------------------------------
// SplitProducerGoldSrc
// ---------------------------------------------------------------------------

SplitProducerGoldSrc::SplitProducerGoldSrc( std::span<const std::byte> payload,
                                            std::int32_t sequence_number,
                                            std::size_t splitsize ) noexcept
    : payload_( payload )
    , sequence_number_( sequence_number )
{
    if( splitsize <= k_header_size )
        return;
    body_size_ = splitsize - k_header_size;
    const std::size_t total = ( payload.size() + body_size_ - 1 ) / body_size_;
    if( total > static_cast<std::size_t>( goldsrc_nibble_max ) ) // nibble cap
        return;
    total_ = static_cast<std::uint8_t>( total );
}

Result<std::size_t> SplitProducerGoldSrc::next( std::span<std::byte> out ) noexcept
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
    out[8] = std::byte{ static_cast<std::uint8_t>(
        ( next_index_ << 4 ) | ( total_ & 0x0F ) ) };

    std::memcpy( &out[ k_header_size ], &payload_[ offset ], take );
    ++next_index_;
    return k_header_size + take;
}

// ---------------------------------------------------------------------------
// decode_split_goldsrc
// ---------------------------------------------------------------------------

Result<SplitFragmentInfo> decode_split_goldsrc( std::span<const std::byte> datagram ) noexcept
{
    if( datagram.size() < k_header_size )
        return std::unexpected( NetError::BufferTooSmall );

    const std::uint32_t magic = read_le32( &datagram[0] );
    if( magic != net_header_split_packet )
        return std::unexpected( NetError::BadAddress );

    SplitFragmentInfo info{};
    info.sequence_number = static_cast<std::int32_t>( read_le32( &datagram[4] ) );
    const std::uint8_t packed = std::to_integer<std::uint8_t>( datagram[8] );
    info.packet_number = static_cast<std::uint8_t>( ( packed >> 4 ) & 0x0F );
    info.packet_count  = static_cast<std::uint8_t>(   packed        & 0x0F );
    if( info.packet_count == 0 || info.packet_number >= info.packet_count )
        return std::unexpected( NetError::BadAddress );
    info.payload = datagram.subspan( k_header_size );
    return info;
}

} // namespace xash::networking
