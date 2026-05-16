// xash3dpp — LoopbackTransport implementation
// Boundary: docs/boundaries/networking-boundary.md (loopback pathway).

#include <xash3dpp/private/networking/loopback_transport.hpp>

#include <cstring>

namespace xash::networking {

static_assert( ( ::xash::limits::net_max_loopback & ( ::xash::limits::net_max_loopback - 1 ) ) == 0,
               "net_max_loopback must be a power of two for the ring mask" );

namespace
{
constexpr std::uint32_t mask_ =
    static_cast<std::uint32_t>( ::xash::limits::net_max_loopback - 1 );
}

LoopbackTransport::LoopbackTransport() noexcept = default;

Result<void> LoopbackTransport::send(
    SocketKind                 sender,
    std::span<const std::byte> data ) noexcept
{
    if( data.size() > loopback_slot_capacity )
        return std::unexpected( NetError::Overflow );

    // Legacy semantics: NET_SendLoopPacket writes to loopbacks[sock ^ 1].
    Ring &ring = rings_[index_( sender ) ^ 1u];

    // Clamp pending depth to the ring capacity (mirrors the
    // `send - get > MAX_LOOPBACK` clamp in NET_GetLoopPacket).
    if( ring.put - ring.get > ::xash::limits::net_max_loopback )
        ring.get = ring.put - static_cast<std::uint32_t>( ::xash::limits::net_max_loopback );

    const std::uint32_t slot_index = ring.put & mask_;
    Slot               &slot       = ring.slots[slot_index];
    if( !data.empty() )
        std::memcpy( slot.bytes.data(), data.data(), data.size() );
    slot.length = data.size();
    ring.put++;

    return {};
}

Result<std::size_t> LoopbackTransport::receive(
    SocketKind           target,
    std::span<std::byte> out ) noexcept
{
    Ring &ring = rings_[index_( target )];

    if( ring.put - ring.get > ::xash::limits::net_max_loopback )
        ring.get = ring.put - static_cast<std::uint32_t>( ::xash::limits::net_max_loopback );

    if( ring.get >= ring.put )
        return std::unexpected( NetError::WouldBlock );

    const std::uint32_t slot_index = ring.get & mask_;
    const Slot         &slot       = ring.slots[slot_index];

    if( out.size() < slot.length )
        return std::unexpected( NetError::BufferTooSmall );

    if( slot.length > 0 )
        std::memcpy( out.data(), slot.bytes.data(), slot.length );
    ring.get++;
    return slot.length;
}

void LoopbackTransport::clear() noexcept
{
    for( Ring &ring : rings_ )
    {
        ring.get = 0;
        ring.put = 0;
    }
}

std::size_t LoopbackTransport::pending( SocketKind target ) const noexcept
{
    const Ring         &ring = rings_[index_( target )];
    const std::uint32_t raw  = ring.put - ring.get;
    return raw > ::xash::limits::net_max_loopback
        ? ::xash::limits::net_max_loopback
        : static_cast<std::size_t>( raw );
}

} // namespace xash::networking
