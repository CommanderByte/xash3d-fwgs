#pragma once
// xash3dpp — networking address types
// Legacy reference: common/netadr.h (netadr_t / netadr_s binary layout)
//
// NetAddress is a clean internal type; the legacy-ABI netadr_t shim lives in
// xash3dpp_abi and performs the conversion at the engine DLL boundary.
//
// IpFamily is defined here (networking concept) and brought into the platform
// socket API via platform/os_socket.hpp's using-declarations.
//
// Port values are in host byte order.  The platform/os_socket implementations
// convert to network byte order when building sockaddr_in / sockaddr_in6.
//
// Invariant (matches legacy netadr_t): ip6_0[0..1] must be zero for V4
// addresses.  The platform sockets layer asserts this before populating
// kernel structs.

#include <cstdint>
#include <cstring>    // std::memcmp
#include <span>
#include <string_view>

#include <xash3dpp/networking/errors.hpp>

namespace xash::networking {

// ---------------------------------------------------------------------------
// IpFamily — address family discriminator
// ---------------------------------------------------------------------------

enum class IpFamily : std::uint8_t
{
    V4,
    V6,
};

// ---------------------------------------------------------------------------
// NetAddress — unified IPv4 / IPv6 endpoint
// ---------------------------------------------------------------------------

struct NetAddress
{
    IpFamily      family{ IpFamily::V4 };
    std::uint16_t port{ 0 };       // host byte order
    // ip6_0[0..1] must be zero for V4 addresses — enforced by factories and
    // asserted in the platform conversion helpers (see os_socket.cpp).
    std::uint8_t  ip6_0[2]{ 0, 0 };

    union
    {
        std::uint8_t v4[4];   // IPv4 address bytes
        std::uint8_t v6[16];  // Full IPv6 address bytes
    } addr{};

    // -----------------------------------------------------------------------
    // Factories
    // -----------------------------------------------------------------------

    [[nodiscard]] static constexpr NetAddress loopback_v4( std::uint16_t p = 0 ) noexcept
    {
        NetAddress a{};
        a.family     = IpFamily::V4;
        a.port       = p;
        a.addr.v4[0] = 127;
        a.addr.v4[1] = 0;
        a.addr.v4[2] = 0;
        a.addr.v4[3] = 1;
        return a;
    }

    [[nodiscard]] static constexpr NetAddress any_v4( std::uint16_t p = 0 ) noexcept
    {
        NetAddress a{};
        a.family = IpFamily::V4;
        a.port   = p;
        // addr.v4 zero-initialised → INADDR_ANY
        return a;
    }

    // -----------------------------------------------------------------------
    // Comparison
    // -----------------------------------------------------------------------

    [[nodiscard]] constexpr bool operator==( const NetAddress &o ) const noexcept
    {
        if( family != o.family || port != o.port )
            return false;
        if( family == IpFamily::V4 )
            return addr.v4[0] == o.addr.v4[0] && addr.v4[1] == o.addr.v4[1]
                && addr.v4[2] == o.addr.v4[2] && addr.v4[3] == o.addr.v4[3];
        // V6 — compare all 16 bytes
        return std::memcmp( addr.v6, o.addr.v6, 16 ) == 0;
    }

    [[nodiscard]] constexpr bool operator!=( const NetAddress &o ) const noexcept
    {
        return !( *this == o );
    }
};

// ---------------------------------------------------------------------------
// Parsing & formatting
// Replaces legacy NET_StringToAdr / NET_AdrToString.
//
// `from_string` parses numeric IPv4 endpoints only: "a.b.c.d" or
// "a.b.c.d:port".  IPv6 literals ("[::1]:27015") and hostnames are rejected
// with NetError::BadAddress; hostname resolution belongs to DnsResolver
// (platform/async, see Layer 1 item #14).  Octet values must be 0-255 and
// port (if present) must be 0-65535.
//
// `to_string` writes "a.b.c.d:port" into `out` and returns the number of
// chars written (excluding the NUL terminator).  Returns BufferTooSmall if
// `out` cannot hold the longest IPv4 form ("255.255.255.255:65535\0" = 22
// bytes).  IPv6 is not yet supported.
// ---------------------------------------------------------------------------

[[nodiscard]] Result<NetAddress> from_string( std::string_view text ) noexcept;
[[nodiscard]] Result<std::size_t> to_string( const NetAddress &a, std::span<char> out ) noexcept;

// Compare two addresses ignoring port.  Equivalent to legacy
// NET_CompareBaseAdr — useful for ban lists and rate limits where the source
// port varies per packet.
[[nodiscard]] bool compare_base( const NetAddress &a, const NetAddress &b ) noexcept;

// Compare the high `mask_bits` of two addresses, ignoring port.  Replaces
// NET_CompareAdrByMask.  `mask_bits` of 0 matches everything; values
// exceeding the address width (32 for V4, 128 for V6) are clamped.  Returns
// false if the address families differ.
[[nodiscard]] bool mask_compare( const NetAddress &a, const NetAddress &b,
                                 std::uint8_t mask_bits ) noexcept;

} // namespace xash::networking
