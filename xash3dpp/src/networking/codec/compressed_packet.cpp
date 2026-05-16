// xash3dpp — Compressed-packet wrapper implementation
// Boundary: docs/boundaries/networking-boundary.md.

#include <xash3dpp/private/networking/codec/compress.hpp>
#include <xash3dpp/private/networking/codec/compressed_packet.hpp>

#include <cstring>

namespace xash::networking::compressed_packet {

namespace
{

constexpr std::uint32_t magic_le = net_header_compressed_packet;

void write_magic( std::span<std::byte> dst ) noexcept
{
    dst[0] = static_cast<std::byte>( magic_le & 0xFFu );
    dst[1] = static_cast<std::byte>( ( magic_le >> 8 ) & 0xFFu );
    dst[2] = static_cast<std::byte>( ( magic_le >> 16 ) & 0xFFu );
    dst[3] = static_cast<std::byte>( ( magic_le >> 24 ) & 0xFFu );
}

std::uint32_t read_magic( std::span<const std::byte> src ) noexcept
{
    return static_cast<std::uint32_t>( src[0] )
         | ( static_cast<std::uint32_t>( src[1] ) << 8 )
         | ( static_cast<std::uint32_t>( src[2] ) << 16 )
         | ( static_cast<std::uint32_t>( src[3] ) << 24 );
}

} // namespace

bool is_compressed_packet( std::span<const std::byte> packet ) noexcept
{
    if( packet.size() < header_size )
        return false;
    return read_magic( packet ) == magic_le;
}

Result<std::vector<std::byte>> encode(
    std::span<const std::byte> payload ) noexcept
{
    auto inner = lzss::compress( payload );
    if( !inner.has_value() )
        return std::unexpected( inner.error() );

    std::vector<std::byte> out;
    out.resize( header_size + inner->size() );
    write_magic( out );
    std::memcpy( out.data() + header_size, inner->data(), inner->size() );
    return out;
}

Result<std::size_t> decode(
    std::span<const std::byte> packet,
    std::span<std::byte>       out ) noexcept
{
    if( packet.size() < header_size || read_magic( packet ) != magic_le )
        return std::unexpected( NetError::BadAddress );
    return lzss::decompress( packet.subspan( header_size ), out );
}

std::uint32_t inflated_size( std::span<const std::byte> packet ) noexcept
{
    if( packet.size() < header_size || read_magic( packet ) != magic_le )
        return 0;
    return lzss::actual_size( packet.subspan( header_size ) );
}

} // namespace xash::networking::compressed_packet
