// xash3dpp — Out-of-band packet encode/decode implementation
// Boundary: docs/boundaries/networking-boundary.md (OOB pathway).

#include <xash3dpp/private/networking/wire/oob_packet.hpp>

#include <cstring>

namespace xash::networking::oob {

namespace
{

constexpr std::uint32_t magic_le = net_header_out_of_band_packet;

void write_magic( std::span<std::byte> dst ) noexcept
{
    // Little-endian; matches legacy SDK byte order.
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

bool is_oob( std::span<const std::byte> packet ) noexcept
{
    if( packet.size() < header_size )
        return false;
    return read_magic( packet ) == magic_le;
}

Result<std::size_t> encode(
    std::span<const std::byte> payload,
    std::span<std::byte>       dst ) noexcept
{
    const std::size_t total = header_size + payload.size();
    if( dst.size() < total )
        return std::unexpected( NetError::BufferTooSmall );

    write_magic( dst );
    if( !payload.empty() )
        std::memcpy( dst.data() + header_size, payload.data(), payload.size() );
    return total;
}

Result<std::size_t> encode(
    std::string_view     payload,
    std::span<std::byte> dst ) noexcept
{
    return encode(
        std::span<const std::byte>{
            reinterpret_cast<const std::byte *>( payload.data() ),
            payload.size() },
        dst );
}

Result<std::span<const std::byte>> decode(
    std::span<const std::byte> packet ) noexcept
{
    if( packet.size() < header_size )
        return std::unexpected( NetError::BadAddress );
    if( read_magic( packet ) != magic_le )
        return std::unexpected( NetError::BadAddress );
    return packet.subspan( header_size );
}

} // namespace xash::networking::oob
