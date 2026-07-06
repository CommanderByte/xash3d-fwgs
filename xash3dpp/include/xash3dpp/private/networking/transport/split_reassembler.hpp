#pragma once
// xash3dpp — split-packet reassembler
// Legacy reference: engine/common/net_ws.c NET_GetLong() fragment merger.
//
// The reassembler accepts one decoded fragment at a time (already validated
// by decode_split_xash / decode_split_goldsrc).  Fragments that share a
// sequence_number are accumulated.  When the last expected fragment arrives,
// the assembled payload is exposed as a span aliasing internal storage that
// remains valid until the next ingest() or reset() call.
//
// Single-slot semantics, matching legacy LongPacket: receiving a fragment
// with a different sequence_number drops the in-progress assembly.  Drop
// rates from packet loss are reported through the returned Outcome.
//
// Threading: caller-synchronised.

#include <xash3dpp/private/networking/wire/split_packet.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xash::networking {

class SplitReassembler
{
public:
    // Caps to uint8_t packet_number/packet_count.  Goldsrc fragments still
    // fit (≤ 15) under the same limit.
    static constexpr std::size_t max_fragments = ::xash::limits::net_splitpacket_max_fragments;

    enum class Outcome : std::uint8_t
    {
        Discarded,  // Fragment rejected (bad count, mismatched body size, etc.)
        Pending,    // Accepted; further fragments still needed
        Complete,   // Fragment completed the packet — see assembled()
        Duplicate,  // Already had this fragment; state unchanged
    };

    struct Ingested
    {
        Outcome                    outcome   { Outcome::Discarded };
        std::span<const std::byte> assembled {};
    };

    SplitReassembler() noexcept = default;

    // Ingest one decoded fragment.  When Outcome::Complete is returned, the
    // accompanying span aliases the reassembler's internal buffer and is
    // valid until the next call to ingest() or reset().
    [[nodiscard]] Ingested ingest( const SplitFragmentInfo &frag ) noexcept;

    void reset() noexcept;

    [[nodiscard]] std::int32_t current_sequence() const noexcept { return sequence_; }
    [[nodiscard]] std::uint8_t fragments_received() const noexcept { return received_; }
    [[nodiscard]] std::uint8_t fragments_expected() const noexcept { return expected_; }

private:
    void                begin_new_( const SplitFragmentInfo &frag ) noexcept;
    [[nodiscard]] Outcome store_fragment_( const SplitFragmentInfo &frag ) noexcept;
    [[nodiscard]] bool  is_complete_() const noexcept;
    void                assemble_into_buffer_() noexcept;

    std::int32_t                                              sequence_ { -1 };
    std::uint8_t                                              expected_ { 0 };
    std::uint8_t                                              received_ { 0 };
    std::array<bool, max_fragments>                           got_      {};
    std::array<std::vector<std::byte>, max_fragments>         fragments_{}; // @pre-reserved: each slot assign()ed once to its fragment body in store_fragment_(); array bounded by max_fragments (per-slot reserve N/A)
    std::vector<std::byte>                                    assembled_{}; // @pre-reserved: assemble_into_buffer_() reserves to the summed fragment length before insert
};

} // namespace xash::networking
