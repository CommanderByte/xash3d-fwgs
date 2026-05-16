#pragma once
// xash3dpp — LoopbackTransport: in-process datagram queue
// Legacy reference: engine/common/net_ws.c (net_loopback_t, NET_GetLoopPacket,
//                   NET_SendLoopPacket).
//
// LoopbackTransport mirrors the legacy "send to NA_LOOPBACK" pathway used by
// listen servers when the local client and server share a process.  It owns
// two fixed-capacity rings keyed by destination SocketKind; sending from a
// given side enqueues onto the opposite side's ring, matching the legacy
// `loopbacks[sock^1]` semantics.
//
// Threading: caller-synchronised.  Designed for the T_NetIO single-thread
// tick loop; concurrent use across threads is not supported.

#include <xash3dpp/limits.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/networking.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace xash::networking {

// Per-slot capacity in bytes — large enough for a full unreliable datagram.
inline constexpr std::size_t loopback_slot_capacity = ::xash::limits::net_max_datagram;

class LoopbackTransport
{
public:
    LoopbackTransport() noexcept;

    LoopbackTransport( const LoopbackTransport & )            = delete;
    LoopbackTransport &operator=( const LoopbackTransport & ) = delete;

    // Enqueue `data` for delivery to the side opposite `sender`.  Drops
    // silently and returns Overflow when the payload exceeds the slot
    // capacity; oldest slot is recycled when the ring is full (matches
    // legacy `send - get > MAX_LOOPBACK` clamp).
    [[nodiscard]] Result<void> send(
        SocketKind                 sender,
        std::span<const std::byte> data ) noexcept;

    // Pull one packet destined for `target` into `out`.  Returns the number
    // of bytes written, or BufferTooSmall when `out` cannot hold the slot
    // payload, or WouldBlock when no packet is queued.
    [[nodiscard]] Result<std::size_t> receive(
        SocketKind           target,
        std::span<std::byte> out ) noexcept;

    // Drop every queued packet on both sides.
    void clear() noexcept;

    // Diagnostics.
    [[nodiscard]] std::size_t pending( SocketKind target ) const noexcept;

private:
    struct Slot
    {
        std::array<std::byte, loopback_slot_capacity> bytes{};
        std::size_t length{ 0 };
    };

    struct Ring
    {
        std::array<Slot, ::xash::limits::net_max_loopback> slots{};
        std::uint32_t                              get { 0 };
        std::uint32_t                              put { 0 };
    };

    std::array<Ring, 2> rings_{};

    static constexpr std::size_t index_( SocketKind k ) noexcept
    {
        return k == SocketKind::Client ? 0u : 1u;
    }
};

} // namespace xash::networking
