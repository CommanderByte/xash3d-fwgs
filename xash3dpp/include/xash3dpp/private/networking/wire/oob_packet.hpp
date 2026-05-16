#pragma once
// xash3dpp — Out-of-band (connectionless) packet encode/decode
// Legacy reference: engine/common/net_chan.c — Netchan_OutOfBandPrint() and
//                   the leading 0xFFFFFFFF magic-word check before the
//                   netchan sequence parser.
//
// OOB packets carry connection-setup traffic (getchallenge, connect, info,
// rcon, master-server queries, etc.) outside the netchan reliable stream.
// On the wire: 4-byte little-endian magic `net_header_out_of_band_packet`
// followed by an ASCII command string.  Body framing past the header is
// command-specific; this header treats it as an opaque payload span.

#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/private/networking/wire/wire_format.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xash::networking::oob {

inline constexpr std::size_t header_size = sizeof( std::uint32_t ); // 4 bytes

// True iff `packet` begins with the OOB magic word.
[[nodiscard]] bool is_oob( std::span<const std::byte> packet ) noexcept;

// Write `net_header_out_of_band_packet` followed by `payload` into `dst`.
// Returns the total bytes written (header_size + payload.size()), or
// NetError::BufferTooSmall when `dst` cannot hold the encoded packet.
[[nodiscard]] Result<std::size_t> encode(
    std::span<const std::byte> payload,
    std::span<std::byte>       dst ) noexcept;

// Convenience overload — encode a text command (no trailing NUL written).
[[nodiscard]] Result<std::size_t> encode(
    std::string_view     payload,
    std::span<std::byte> dst ) noexcept;

// Strip the OOB header and return the inner payload view aliasing `packet`.
// Returns BadAddress when `packet` is shorter than header_size or lacks the
// magic word.
[[nodiscard]] Result<std::span<const std::byte>> decode(
    std::span<const std::byte> packet ) noexcept;

} // namespace xash::networking::oob
