#pragma once
// xash3dpp — wire-format POD types for the networking transport layer
// Legacy reference: engine/common/net_ws.c (SPLITPACKET, SPLITPACKETGS, LONGPACKET),
//                   engine/common/net_ws.h (NET_HEADER_* magic constants).
//
// Frozen wire ABI — these byte layouts must match GoldSrc/Xash on the wire.
// Do NOT add fields, change types, or alter ordering.  Any new header shape
// belongs in a new POD struct selected via IProtocolDriver.
//
// Visibility: this header lives under include/xash3dpp/private/networking/
// so the rest of the engine cannot accidentally depend on wire framing.

#include <xash3dpp/limits.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::networking {

// ---------------------------------------------------------------------------
// Packet header magic words.
//
// Stored as 32-bit little-endian on the wire.  Each magic value flags a
// non-connection-oriented packet shape.  Anything that does *not* match one
// of these magics is a normal sequenced netchan datagram.
//
// Legacy expressed these as negative ints (-1, -2, -3) reinterpreted via
// signed/unsigned punning; we use their unsigned bit patterns directly.
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t net_header_out_of_band_packet  = 0xFFFFFFFFu; // -1
inline constexpr std::uint32_t net_header_split_packet        = 0xFFFFFFFEu; // -2
inline constexpr std::uint32_t net_header_compressed_packet   = 0xFFFFFFFDu; // -3

// ---------------------------------------------------------------------------
// SplitHeaderXash — Xash protocol SPLITPACKET on-wire header.
// Layout: 10 bytes packed.
//   net_id           = net_header_split_packet
//   sequence_number  = group identifier shared by all fragments of one packet
//   packet_id        = high byte: packet_number, low byte: packet_count
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct SplitHeaderXash
{
    std::uint32_t net_id;
    std::int32_t  sequence_number;
    std::int16_t  packet_id;
};
#pragma pack(pop)

static_assert( sizeof( SplitHeaderXash ) == 10,
               "SplitHeaderXash wire layout must remain 10 bytes" );

// ---------------------------------------------------------------------------
// SplitHeaderGoldSrc — GoldSrc protocol SPLITPACKETGS on-wire header.
// Layout: 9 bytes packed.
//   packet_id high nibble = packet_number (≤ 15)
//   packet_id low  nibble = packet_count  (≤ 15) — max 5 fragments in practice
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct SplitHeaderGoldSrc
{
    std::uint32_t net_id;
    std::int32_t  sequence_number;
    std::uint8_t  packet_id;
};
#pragma pack(pop)

static_assert( sizeof( SplitHeaderGoldSrc ) == 9,
               "SplitHeaderGoldSrc wire layout must remain 9 bytes" );

// Maximum fragment number or count encodable in a 4-bit nibble field of
// SplitHeaderGoldSrc (high nibble = packet_number, low nibble = packet_count).
static constexpr std::uint8_t goldsrc_nibble_max = 15u;

// ---------------------------------------------------------------------------
// LongPacket — split-packet reassembly state (not a wire type).
//
// One in-flight assembly is permitted per direction; the legacy engine had a
// single global `split` slot per netsrc_t.  This struct owns its own
// scratch storage and is reset whenever the sequence_number changes.
//
// Storage size is xash::limits::net_max_fragment bytes; if that becomes too
// large for stack allocation, callers should heap-allocate via the pool.
// ---------------------------------------------------------------------------

struct LongPacket
{
    std::int32_t current_sequence = -1; // -1 → slot is idle
    std::int32_t split_count      = 0;
    std::int32_t total_size       = 0;
    std::byte    buffer[ xash::limits::net_max_fragment ] {};
};

} // namespace xash::networking
