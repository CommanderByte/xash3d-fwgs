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

} // namespace xash::networking
