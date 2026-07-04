#pragma once
// xash3dpp — map checksum for the network protocol.
// Legacy reference: engine/common/mod_bmodel.c — CRC32_MapFile (:4093-4165);
// public/crclib.c — CRC32_Init/ProcessBuffer (reflected zlib polynomial).
//
// Wire-frozen algorithm quirks (deep-dive-bsp-loader.md §8):
//  - singleplayer: a fixed constant, no hashing at all;
//  - multiplayer: CRC32 (init 0xFFFFFFFF) over the RAW bytes of lumps
//    LUMP_PLANES..LUMP_MODELS in index order — LUMP_ENTITIES is EXCLUDED —
//    and the accumulator is stored WITHOUT the final xor-invert;
//  - the on-disk lump directory is used as stored (a Blue-Shift map's
//    swapped entities/planes entries affect what gets hashed).

#include <xash3dpp/private/map_loader/bsp/disk_format.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace xash::map_loader::bsp {

// CRC32_MapFile constant for singleplayer: ('H'<<24)|('S'<<16)|('A'<<8)|'X'.
inline constexpr std::uint32_t k_map_crc_singleplayer = 0x58415348u;

// Multiplayer map CRC over a whole-file image.  Lump ranges are clamped to
// the file (legacy streams via FS_Read and stops at EOF).
[[nodiscard]] std::uint32_t map_checksum_multiplayer(
    std::span<const std::byte> file, const dheader_t &header ) noexcept;

} // namespace xash::map_loader::bsp
