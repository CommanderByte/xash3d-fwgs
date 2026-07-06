// xash3dpp — split-packet reassembler implementation

#include <xash3dpp/private/networking/transport/split_reassembler.hpp>

namespace xash::networking {

void SplitReassembler::reset() noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
{
    sequence_ = -1;
    expected_ = 0;
    received_ = 0;
    got_.fill( false );
    for( auto &f : fragments_ )
        f.clear();
    assembled_.clear();
}

void SplitReassembler::begin_new_( const SplitFragmentInfo &frag ) noexcept
{
    sequence_ = frag.sequence_number;
    expected_ = frag.packet_count;
    received_ = 0;
    got_.fill( false );
    for( auto &f : fragments_ )
        f.clear();
    assembled_.clear();
}

SplitReassembler::Outcome
SplitReassembler::store_fragment_( const SplitFragmentInfo &frag ) noexcept
{
    if( got_[frag.packet_number] )
        return Outcome::Duplicate;

    auto &slot = fragments_[frag.packet_number];
    slot.assign( frag.payload.begin(), frag.payload.end() );
    got_[frag.packet_number] = true;
    ++received_;
    return Outcome::Pending;
}

bool SplitReassembler::is_complete_() const noexcept
{
    return received_ == expected_;
}

void SplitReassembler::assemble_into_buffer_() noexcept
{
    std::size_t total = 0;
    for( std::uint8_t i = 0; i < expected_; ++i )
        total += fragments_[i].size();

    assembled_.clear();
    assembled_.reserve( total );
    for( std::uint8_t i = 0; i < expected_; ++i )
        assembled_.insert( assembled_.end(),
                           fragments_[i].begin(),
                           fragments_[i].end() );
}

SplitReassembler::Ingested
SplitReassembler::ingest( const SplitFragmentInfo &frag ) noexcept
{
    // Reject obviously-bad fragments.
    if( frag.packet_count == 0
        || frag.packet_number >= frag.packet_count
        || frag.packet_count > max_fragments )
        return { Outcome::Discarded, {} };

    // New sequence — start over.
    if( frag.sequence_number != sequence_ )
        begin_new_( frag );
    else if( frag.packet_count != expected_ )
        // Same sequence, contradictory count: legacy treats this as an
        // adversarial / lossy packet and drops state.
        return { Outcome::Discarded, {} };

    const Outcome stored = store_fragment_( frag );
    if( stored == Outcome::Duplicate )
        return { Outcome::Duplicate, {} };

    if( is_complete_() )
    {
        assemble_into_buffer_();
        // Reset bookkeeping so a subsequent fragment with the same sequence
        // does not re-trigger completion, while leaving `assembled_` intact
        // for the caller to read this turn.
        sequence_ = -1;
        expected_ = 0;
        received_ = 0;
        got_.fill( false );
        for( auto &f : fragments_ )
            f.clear();
        return { Outcome::Complete, std::span<const std::byte>{ assembled_ } };
    }

    return { Outcome::Pending, {} };
}

} // namespace xash::networking
