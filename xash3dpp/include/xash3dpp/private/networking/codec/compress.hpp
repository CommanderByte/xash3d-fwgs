#pragma once
// xash3dpp — compression codecs for the networking layer
// Legacy reference: engine/common/common.c (LZSS_Compress/Decompress/
//                   LZSS_IsCompressed/LZSS_GetActualSize); engine/common/zlib_strm.c
//                   and the bzip2 path used by net_chan compressed packets.
//
// Wire-frozen: the LZSS header "LZSS" magic (id) and uncompressed-size field
// must remain identical on the wire.  The algorithm itself (window size,
// look-ahead, command-byte bit ordering) is part of the protocol contract.
//
// All entry points are value-semantic.  Compression returns an owned vector;
// decompression writes into a caller-owned span.  No globals.
//
// `Result` failures use NetError::BufferTooSmall when the destination cannot
// hold the output and NetError::BadAddress when the input is malformed.  We
// avoid introducing a new error category here to keep NetError focused on
// transport semantics.

#include <xash3dpp/networking/errors.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xash::networking::lzss {

inline constexpr std::uint32_t magic_id        = 0x53535A4Cu; // 'LZSS' little-endian
inline constexpr std::size_t   header_size     = 8;
inline constexpr std::size_t   window_size     = 4096;
inline constexpr std::size_t   lookshift       = 4;
inline constexpr std::size_t   lookahead       = 1u << lookshift; // 16

// True if `src` begins with the LZSS magic word.
[[nodiscard]] bool is_compressed( std::span<const std::byte> src ) noexcept;

// Returns the uncompressed size declared in the LZSS header, or 0 if `src`
// is too short or has the wrong magic.
[[nodiscard]] std::uint32_t actual_size( std::span<const std::byte> src ) noexcept;

// Compress `src` into a fresh buffer.  Returns BadAddress if the input is
// too short for an LZSS header or if compression does not save space
// (legacy behaviour: the caller falls back to sending uncompressed).
[[nodiscard]] Result<std::vector<std::byte>> compress( std::span<const std::byte> src );

// Decompress `src` into `dst`.  Returns the number of bytes written on
// success, BufferTooSmall if `dst` is too small, BadAddress on malformed
// input or magic mismatch.
[[nodiscard]] Result<std::size_t>
decompress( std::span<const std::byte> src, std::span<std::byte> dst ) noexcept;

} // namespace xash::networking::lzss

namespace xash::networking::bz2 {

// Link-time selection (OQ-7): compress_bz2.cpp provides the real bzip2-backed
// implementations when XASH_NET_COMPRESSION=ON; compress_null.cpp provides
// stub bodies that return NetError::NotInitialised when OFF.  Exactly one TU
// is linked into xash3dpp_networking.  bzip2 has no on-wire magic of its own
// in netchan's framing — the netchan flag NETCHAN_USE_BZIP2 decides whether
// to feed the payload through this codec, so we expose only compress and
// decompress.

[[nodiscard]] Result<std::vector<std::byte>> compress( std::span<const std::byte> src );

[[nodiscard]] Result<std::size_t>
decompress( std::span<const std::byte> src, std::span<std::byte> dst ) noexcept;

// True if this build links a working bzip2 backend (compress_bz2.cpp).
// False when compress_null.cpp is linked instead.  Lets callers fall back
// to uncompressed transmission without round-tripping through a failing
// compress() call.
[[nodiscard]] bool available() noexcept;

} // namespace xash::networking::bz2
