#pragma once
// xash3dpp — LagQueue: in-memory delay queue for simulated lag
// Legacy reference: net_ws.c fakelag / fakeloss handling.
//
// LagQueue holds incoming or outgoing datagrams for a configurable delay
// before releasing them.  Packet loss simulation is the caller's
// responsibility: drop the packet before calling `enqueue` based on whatever
// RNG and rate the caller wants.  Keeping drop policy out of the queue keeps
// the type deterministic and trivially testable.
//
// Time is a monotonic millisecond count supplied by the caller on every
// call; the queue never reads a clock itself.  This lets unit tests step
// time deterministically and avoids coupling to xash::core::Clock.
//
// One LagQueue is private to one direction (rx or tx).

#include <xash3dpp/networking/address.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>

namespace xash::networking {

struct DelayedPacket
{
    std::uint64_t       release_time_ms = 0;  // absolute monotonic ms
    NetAddress          peer {};
    std::vector<std::byte> data;
};

class LagQueue
{
public:
    LagQueue() noexcept = default;

    // Add `data` to the queue, scheduled for release at `now_ms + delay_ms`.
    // Returns false if `data` is empty (still appended as a zero-byte packet
    // is rarely useful and would mask bugs upstream).
    [[nodiscard]] bool enqueue( std::uint64_t now_ms,
                                std::uint32_t delay_ms,
                                const NetAddress &peer,
                                std::span<const std::byte> data );

    // Pop the next packet whose release time is <= `now_ms`.  Returns
    // std::nullopt when none are due.
    [[nodiscard]] std::optional<DelayedPacket> try_dequeue( std::uint64_t now_ms );

    [[nodiscard]] std::size_t size()  const noexcept { return queue_.size(); }
    [[nodiscard]] bool        empty() const noexcept { return queue_.empty(); }

    void clear() noexcept { queue_.clear(); }

private:
    // FIFO order matches enqueue order; the front element's release time is
    // always the earliest because delay is monotone with arrival time only
    // when the delay is constant.  When delay varies across calls,
    // try_dequeue still scans only the head — out-of-order packets simply
    // wait longer, matching legacy `fakelag` behaviour.
    std::deque<DelayedPacket> queue_;
};

} // namespace xash::networking
