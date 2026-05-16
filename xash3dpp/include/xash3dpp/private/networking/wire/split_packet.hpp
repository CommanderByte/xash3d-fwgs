#pragma once
// xash3dpp — Xash & GoldSrc split-packet encode/decode helpers
// Legacy reference: engine/common/net_ws.c (NET_SendLong producer side and
//                   NET_GetLong receiver fragment parser).
//
// These are stateless wire-level helpers.  Reassembly (LongPacket state)
// belongs to Layer 3 (#21 Xash, #22 GoldSrc).
//
// Both protocols frame fragments identically except for the header struct:
//   Xash    — 10-byte SplitHeaderXash, packet_id = (number << 8) | count
//   GoldSrc — 9-byte  SplitHeaderGoldSrc, packet_id = (number << 4) | count
//
// Note: GoldSrc's nibble-packed packet_id implies a hard cap of 15 fragments
// per packet, with 5 used in practice (NET_MAX_GOLDSRC_FRAGMENTS).

#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/private/networking/wire/wire_format.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace xash::networking {

// ---------------------------------------------------------------------------
// SplitFragmentInfo — decoded view of a single received split fragment.
// `payload` aliases the input buffer past the header; do not free it.
// ---------------------------------------------------------------------------

struct SplitFragmentInfo
{
    std::int32_t              sequence_number = 0;
    std::uint8_t              packet_number   = 0;
    std::uint8_t              packet_count    = 0;
    std::span<const std::byte> payload {};
};

// ---------------------------------------------------------------------------
// SplitProducer — pull-style fragment iterator.
//
// Construct over the source buffer plus a `splitsize` (the maximum on-wire
// packet size including the header).  Each call to `next` writes one
// fragment into `out` and returns the number of bytes written, or zero when
// the producer is exhausted.  `out` must be at least `splitsize` bytes.
//
// The first 4 bytes of `out` are always the magic; the SplitHeader follows.
//
// Use a single SplitProducer per outgoing logical packet; do not share.
// ---------------------------------------------------------------------------

class SplitProducerXash
{
public:
    SplitProducerXash( std::span<const std::byte> payload,
                       std::int32_t sequence_number,
                       std::size_t splitsize ) noexcept;

    [[nodiscard]] bool exhausted() const noexcept { return next_index_ >= total_; }
    [[nodiscard]] std::uint8_t total_fragments() const noexcept { return total_; }

    // Returns bytes written into `out` (0 when exhausted; BadAddress when the
    // configuration is invalid).
    [[nodiscard]] Result<std::size_t> next( std::span<std::byte> out ) noexcept;

private:
    std::span<const std::byte> payload_;
    std::int32_t  sequence_number_ = 0;
    std::size_t   body_size_       = 0;   // splitsize - sizeof(header)
    std::uint8_t  total_           = 0;
    std::uint8_t  next_index_      = 0;
};

class SplitProducerGoldSrc
{
public:
    SplitProducerGoldSrc( std::span<const std::byte> payload,
                          std::int32_t sequence_number,
                          std::size_t splitsize ) noexcept;

    [[nodiscard]] bool exhausted() const noexcept { return next_index_ >= total_; }
    [[nodiscard]] std::uint8_t total_fragments() const noexcept { return total_; }

    [[nodiscard]] Result<std::size_t> next( std::span<std::byte> out ) noexcept;

private:
    std::span<const std::byte> payload_;
    std::int32_t  sequence_number_ = 0;
    std::size_t   body_size_       = 0;
    std::uint8_t  total_           = 0;
    std::uint8_t  next_index_      = 0;
};

// ---------------------------------------------------------------------------
// Single-packet decoders.
//
// Parse a received datagram that starts with `net_header_split_packet`.  The
// caller has already confirmed the magic; these helpers re-validate and
// extract the SplitFragmentInfo.
// ---------------------------------------------------------------------------

[[nodiscard]] Result<SplitFragmentInfo>
decode_split_xash( std::span<const std::byte> datagram ) noexcept;

[[nodiscard]] Result<SplitFragmentInfo>
decode_split_goldsrc( std::span<const std::byte> datagram ) noexcept;

} // namespace xash::networking
