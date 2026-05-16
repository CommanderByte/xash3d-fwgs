// xash3dpp — LagQueue implementation

#include <xash3dpp/networking/lag_queue.hpp>

namespace xash::networking {

bool LagQueue::enqueue( std::uint64_t now_ms,
                        std::uint32_t delay_ms,
                        const NetAddress &peer,
                        std::span<const std::byte> data )
{
    if( data.empty() )
        return false;

    DelayedPacket pkt;
    pkt.release_time_ms = now_ms + delay_ms;
    pkt.peer            = peer;
    pkt.data.assign( data.begin(), data.end() );
    queue_.push_back( std::move( pkt ) );
    return true;
}

std::optional<DelayedPacket> LagQueue::try_dequeue( std::uint64_t now_ms )
{
    if( queue_.empty() )
        return std::nullopt;
    if( queue_.front().release_time_ms > now_ms )
        return std::nullopt;
    DelayedPacket out = std::move( queue_.front() );
    queue_.pop_front();
    return out;
}

} // namespace xash::networking
