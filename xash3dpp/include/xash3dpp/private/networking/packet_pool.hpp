#pragma once
// xash3dpp — PacketPool: fixed-capacity slab of datagram buffers
// Boundary: docs/boundaries/networking-boundary.md
//
// Avoids per-packet heap traffic in the T_NetIO tick loop.  Each slot holds
// a `net_max_datagram`-sized byte array.  Acquire returns an RAII handle
// that releases the slot on destruction.
//
// Threading: caller-synchronised; intended for single-thread NetIO use.
// Cross-thread sharing requires external synchronisation.

#include <xash3dpp/limits.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace xash::networking {

class PacketPool;

// RAII handle returned by PacketPool::acquire.  Move-only.
class PacketSlot
{
public:
    PacketSlot() noexcept = default;
    PacketSlot( PacketPool *pool, std::uint32_t index, std::span<std::byte> bytes ) noexcept
        : pool_( pool ), index_( index ), bytes_( bytes ) {}

    PacketSlot( const PacketSlot & )            = delete;
    PacketSlot &operator=( const PacketSlot & ) = delete;

    PacketSlot( PacketSlot &&other ) noexcept
        : pool_( other.pool_ ), index_( other.index_ ), bytes_( other.bytes_ )
    {
        other.pool_  = nullptr;
        other.bytes_ = {};
    }
    PacketSlot &operator=( PacketSlot &&other ) noexcept
    {
        if( this != &other )
        {
            release();
            pool_        = other.pool_;
            index_       = other.index_;
            bytes_       = other.bytes_;
            other.pool_  = nullptr;
            other.bytes_ = {};
        }
        return *this;
    }

    ~PacketSlot() { release(); }

    [[nodiscard]] bool                 valid() const noexcept { return pool_ != nullptr; }
    [[nodiscard]] std::span<std::byte> bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::uint32_t        index() const noexcept { return index_; }

    // Resize the *active* payload (without exceeding the underlying capacity).
    void set_length( std::size_t length ) noexcept
    {
        if( pool_ && length <= bytes_.size() )
            bytes_ = bytes_.subspan( 0, length );
    }

    void release() noexcept;

private:
    PacketPool          *pool_  { nullptr };
    std::uint32_t        index_ { 0 };
    std::span<std::byte> bytes_ {};
};

class PacketPool
{
public:
    static constexpr std::size_t slot_capacity = ::xash::limits::net_max_datagram;
    static constexpr std::size_t slot_count    = ::xash::limits::net_packet_pool_slots;

    PacketPool() noexcept;

    PacketPool( const PacketPool & )            = delete;
    PacketPool &operator=( const PacketPool & ) = delete;

    // Acquire one slot.  Returns std::nullopt when the pool is exhausted.
    [[nodiscard]] std::optional<PacketSlot> acquire() noexcept;

    // Diagnostics.
    [[nodiscard]] std::size_t in_use() const noexcept { return in_use_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return slot_count; }

    // Called by PacketSlot::release; do not call directly.
    void release_slot( std::uint32_t index ) noexcept;

private:
    struct Slot
    {
        std::array<std::byte, slot_capacity> bytes{};
    };

    // Heap-backed to keep multi-MB pool storage off the call stack.
    std::vector<Slot>                       slots_;
    std::array<std::uint32_t, slot_count>   free_stack_{};
    std::uint32_t                           free_top_ { 0 };
    std::size_t                             in_use_   { 0 };
};

inline void PacketSlot::release() noexcept
{
    if( pool_ )
    {
        pool_->release_slot( index_ );
        pool_  = nullptr;
        bytes_ = {};
    }
}

} // namespace xash::networking
