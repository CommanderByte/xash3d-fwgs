#pragma once
// xash3dpp — Compressed packet wire wrapper (net_header_compressed_packet)
// Legacy reference: engine/common/net_ws.c, where -3 (0xFFFFFFFD) prefixes a
//                   compressed datagram.  The legacy engine uses bz2 for the
//                   client connect handshake and LZSS for runtime traffic
//                   when XASH_NET_COMPRESSION is on.
//
// This Layer 2 wrapper only handles the LZSS codec (item #11).  bz2 encode
// lands later (deferred #10).  Decode auto-detects via the inner LZSS magic
// once the outer wrapper magic has been stripped.

#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/private/networking/wire/wire_format.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xash::networking::compressed_packet {

inline constexpr std::size_t header_size = sizeof( std::uint32_t ); // 4 bytes

// True iff `packet` begins with the compressed-packet magic.
[[nodiscard]] bool is_compressed_packet( std::span<const std::byte> packet ) noexcept;

// Compress `payload` with LZSS, prefixing the result with
// `net_header_compressed_packet`.  Returns a heap-allocated buffer (callers
// usually transfer it into a PacketPool slot before send).  Fails with
// NetError::BadAddress when LZSS cannot shrink the payload below the header
// overhead — caller should fall back to the uncompressed wire path.
[[nodiscard]] Result<std::vector<std::byte>> encode(
    std::span<const std::byte> payload ) noexcept;

// Strip the compressed-packet header and inflate the body into `out`.
// Returns bytes written.  NetError::BadAddress when the wrapper magic is
// missing; NetError::BufferTooSmall when `out` cannot hold the inflated
// payload.
[[nodiscard]] Result<std::size_t> decode(
    std::span<const std::byte> packet,
    std::span<std::byte>       out ) noexcept;

// Inspect the inner LZSS header to report the inflated size of a
// compressed-wrapper packet.  Returns 0 when `packet` is malformed.
[[nodiscard]] std::uint32_t inflated_size(
    std::span<const std::byte> packet ) noexcept;

} // namespace xash::networking::compressed_packet
