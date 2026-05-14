#pragma once
// xash3dpp — CRC32 and MD5 hash utilities
// Legacy reference: public/crclib.h + public/crclib.c
//
// CRC32 output is IEEE 802.3 compatible.  The four functions below are also
// bound into enginefuncs_t (game DLL ABI) as function pointers; a shim at the
// fill-site is all that is needed — no ABI constraint on this implementation.

#include <array>
#include <cstdint>
#include <cstddef>
#include <span>

namespace xash::utilities {

// ---------------------------------------------------------------------------
// CRC32  (IEEE 802.3 polynomial, init 0xFFFFFFFF, final XOR 0xFFFFFFFF)
// ---------------------------------------------------------------------------

using Crc32 = std::uint32_t;

static constexpr Crc32 CRC32_INIT = 0xFFFFFFFFu;

// Initialise state.
constexpr void crc32_init( Crc32 &state ) noexcept { state = CRC32_INIT; }

// Feed a buffer.
void crc32_update( Crc32 &state, const void *data, std::size_t len ) noexcept;

// Feed a single byte.
void crc32_update( Crc32 &state, std::uint8_t byte ) noexcept;

// Finalise (applies XOR).
constexpr Crc32 crc32_final( Crc32 state ) noexcept { return state ^ 0xFFFFFFFFu; }

// One-shot helper.
Crc32 crc32( const void *data, std::size_t len ) noexcept;

// Sequence-keyed CRC used for demo/resource integrity checks.
// Legacy: CRC32_BlockSequence
std::uint8_t crc32_block_sequence( const std::uint8_t *base, int length, int sequence ) noexcept;

// ---------------------------------------------------------------------------
// MD5  (engine-internal; not exposed to game DLLs)
// ---------------------------------------------------------------------------

struct Md5State
{
    std::array<std::uint32_t, 4>  buf{};
    std::array<std::uint32_t, 2>  bits{};
    std::array<std::uint8_t,  64> in{};
};

void md5_init( Md5State &state ) noexcept;
void md5_update( Md5State &state, const void *data, std::size_t len ) noexcept;
std::array<std::uint8_t, 16> md5_final( Md5State &state ) noexcept;

// ---------------------------------------------------------------------------
// RAII hasher wrappers
// ---------------------------------------------------------------------------

class Crc32Hasher
{
public:
    Crc32Hasher() noexcept { crc32_init( state_ ); }

    Crc32Hasher &update( const void *data, std::size_t len ) noexcept
    {
        crc32_update( state_, data, len );
        return *this;
    }
    Crc32Hasher &update( std::uint8_t byte ) noexcept
    {
        crc32_update( state_, byte );
        return *this;
    }
    Crc32Hasher &update( std::span<const std::byte> data ) noexcept
    {
        crc32_update( state_, data.data(), data.size() );
        return *this;
    }

    Crc32 finalize() noexcept { return crc32_final( state_ ); }

    static Crc32 hash( const void *data, std::size_t len ) noexcept
    {
        return crc32( data, len );
    }

private:
    Crc32 state_{};
};

class Md5Hasher
{
public:
    Md5Hasher() noexcept { md5_init( state_ ); }

    Md5Hasher &update( const void *data, std::size_t len ) noexcept
    {
        md5_update( state_, data, len );
        return *this;
    }
    Md5Hasher &update( std::span<const std::byte> data ) noexcept
    {
        md5_update( state_, data.data(), data.size() );
        return *this;
    }

    std::array<std::uint8_t, 16> finalize() noexcept
    {
        return md5_final( state_ );
    }

    static std::array<std::uint8_t, 16> hash( const void *data, std::size_t len ) noexcept
    {
        Md5Hasher h;
        h.update( data, len );
        return h.finalize();
    }

private:
    Md5State state_{};
};

} // namespace xash::utilities
