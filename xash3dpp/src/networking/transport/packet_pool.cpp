// xash3dpp — PacketPool implementation
// Boundary: docs/boundaries/networking-boundary.md.

#include <xash3dpp/private/networking/transport/packet_pool.hpp>

namespace xash::networking {

PacketPool::PacketPool() noexcept
    : slots_( slot_count )
{
    // Push slots onto the free stack in reverse so acquire() yields index 0 first.
    for( std::uint32_t i = 0; i < slot_count; ++i )
        free_stack_[i] = static_cast<std::uint32_t>( slot_count - 1 - i );
    free_top_ = static_cast<std::uint32_t>( slot_count );
}

std::optional<PacketSlot> PacketPool::acquire() noexcept
{
    if( free_top_ == 0 )
        return std::nullopt;

    --free_top_;
    const std::uint32_t index = free_stack_[free_top_];
    ++in_use_;

    return PacketSlot{ this, index, std::span<std::byte>{ slots_[index].bytes } };
}

void PacketPool::release_slot( std::uint32_t index ) noexcept
{
    if( index >= slot_count || free_top_ >= slot_count )
        return; // defensive — double release or out-of-range
    free_stack_[free_top_++] = index;
    if( in_use_ > 0 )
        --in_use_;
}

} // namespace xash::networking
