// xash3dpp — NetAddress parsing, formatting, comparison
// Replaces legacy: NET_StringToAdr, NET_AdrToString, NET_CompareBaseAdr,
// NET_CompareAdrByMask.
//
// Numeric IPv4 only at this layer.  Hostname resolution (NET_StringToAdrEx
// with DNS) is the DnsResolver's job; IPv6 literal parsing will land with
// the IPlatformSockets work (it can lean on inet_pton).

#include <xash3dpp/networking/address.hpp>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>

namespace xash::networking {

namespace {

// Parse a single decimal octet from `[first, last)`, returning the value and
// the number of characters consumed.  Returns {0, 0} on failure.
struct ParsedNum
{
    std::uint32_t value;
    std::size_t   chars_used;
};

[[nodiscard]] ParsedNum parse_uint_decimal( std::string_view s ) noexcept
{
    std::uint32_t v        = 0;
    std::size_t   consumed = 0;
    for( char c : s )
    {
        if( c < '0' || c > '9' )
            break;
        v          = v * 10 + static_cast<std::uint32_t>( c - '0' );
        ++consumed;
        if( v > 65535 )
            return { 0, 0 };           // out of range for both octet and port
    }
    return { v, consumed };
}

} // namespace

// ---------------------------------------------------------------------------
// from_string
// ---------------------------------------------------------------------------

Result<NetAddress> from_string( std::string_view text ) noexcept
{
    if( text.empty() || text.front() == '[' )
        return std::unexpected( NetError::BadAddress );   // IPv6 literal unsupported here

    NetAddress out{};
    out.family = IpFamily::V4;

    // Parse 4 dot-separated octets.
    std::size_t pos = 0;
    for( int i = 0; i < 4; ++i )
    {
        const auto p = parse_uint_decimal( text.substr( pos ) );
        if( p.chars_used == 0 || p.value > 255 )
            return std::unexpected( NetError::BadAddress );
        out.addr.v4[ i ] = static_cast<std::uint8_t>( p.value );
        pos             += p.chars_used;

        if( i < 3 )
        {
            if( pos >= text.size() || text[ pos ] != '.' )
                return std::unexpected( NetError::BadAddress );
            ++pos;
        }
    }

    // Optional ":port".
    if( pos < text.size() )
    {
        if( text[ pos ] != ':' )
            return std::unexpected( NetError::BadAddress );
        ++pos;
        const auto p = parse_uint_decimal( text.substr( pos ) );
        if( p.chars_used == 0 || pos + p.chars_used != text.size() || p.value > 65535 )
            return std::unexpected( NetError::BadAddress );
        out.port = static_cast<std::uint16_t>( p.value );
    }
    return out;
}

// ---------------------------------------------------------------------------
// to_string
// ---------------------------------------------------------------------------

Result<std::size_t> to_string( const NetAddress &a, std::span<char> out ) noexcept
{
    if( a.family != IpFamily::V4 )
        return std::unexpected( NetError::BadAddress );   // V6 not yet supported

    // Maximum: "255.255.255.255:65535\0" = 22 bytes.
    if( out.size() < 22 )
        return std::unexpected( NetError::BufferTooSmall );

    const int n = std::snprintf( out.data(), out.size(),
                                 "%u.%u.%u.%u:%u",
                                 a.addr.v4[0], a.addr.v4[1],
                                 a.addr.v4[2], a.addr.v4[3],
                                 static_cast<unsigned>( a.port ) );
    if( n <= 0 || static_cast<std::size_t>( n ) >= out.size() )
        return std::unexpected( NetError::BufferTooSmall );
    return static_cast<std::size_t>( n );
}

// ---------------------------------------------------------------------------
// compare_base
// ---------------------------------------------------------------------------

bool compare_base( const NetAddress &a, const NetAddress &b ) noexcept
{
    if( a.family != b.family )
        return false;
    if( a.family == IpFamily::V4 )
        return std::memcmp( a.addr.v4, b.addr.v4, 4 ) == 0;
    return std::memcmp( a.addr.v6, b.addr.v6, 16 ) == 0;
}

// ---------------------------------------------------------------------------
// mask_compare
// ---------------------------------------------------------------------------

bool mask_compare( const NetAddress &a, const NetAddress &b,
                   std::uint8_t mask_bits ) noexcept
{
    if( a.family != b.family )
        return false;
    const std::uint8_t  max_bits = ( a.family == IpFamily::V4 ) ? 32 : 128;
    const std::uint8_t  bits     = std::min( mask_bits, max_bits );
    if( bits == 0 )
        return true;
    const std::uint8_t *pa       = ( a.family == IpFamily::V4 ) ? a.addr.v4 : a.addr.v6;
    const std::uint8_t *pb       = ( a.family == IpFamily::V4 ) ? b.addr.v4 : b.addr.v6;

    const std::uint8_t full_bytes = bits >> 3;
    const std::uint8_t tail_bits  = bits & 7;
    if( full_bytes > 0 && std::memcmp( pa, pb, full_bytes ) != 0 )
        return false;
    if( tail_bits > 0 )
    {
        const std::uint8_t mask = static_cast<std::uint8_t>(
            0xFFu << ( 8 - tail_bits ) );
        if( ( pa[ full_bytes ] & mask ) != ( pb[ full_bytes ] & mask ) )
            return false;
    }
    return true;
}

} // namespace xash::networking
