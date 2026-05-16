// xash3dpp — platform (Win32) — socket I/O
// Legacy reference: engine/common/net_ws.c (WSA init, socket create, bind,
//                   send/recv Win32 paths), engine/common/net_ws_private.h
//
// Existing platform helpers used:
//   xash3dpp_core — XASH_ASSERT (debug-only invariant checks)

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

#include <xash3dpp/core/assert.hpp>

// Winsock2 — confined to this translation unit.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstring>    // std::memcpy
#include <optional>
#include <string>

namespace xash::platform {

// ---------------------------------------------------------------------------
// OsSocket::close() — Win32 (uses closesocket, not _close)
// ---------------------------------------------------------------------------

void OsSocket::close() noexcept
{
    if( handle_ != k_invalid_socket )
    {
        ::closesocket( static_cast<SOCKET>( handle_ ) );
        handle_ = k_invalid_socket;
    }
}

// ---------------------------------------------------------------------------
// Internal: error mapping
// ---------------------------------------------------------------------------

static NetError map_wsa_error( int err ) noexcept
{
    switch( err )
    {
    case WSAEWOULDBLOCK:
    case WSAEINPROGRESS:  return NetError::WouldBlock;
    case WSAENOTSOCK:
    case WSAEBADF:        return NetError::SocketInvalid;
    case WSAEADDRINUSE:   return NetError::BindFailed;
    case WSAEMSGSIZE:     return NetError::Overflow;
    case WSAEINVAL:       return NetError::BadAddress;
    case WSANOTINITIALISED: return NetError::NotInitialised;
    default:              return NetError::SocketInvalid;
    }
}

// ---------------------------------------------------------------------------
// Internal: NetAddress <-> sockaddr conversions
// ---------------------------------------------------------------------------

static sockaddr_in to_sockaddr_v4( const NetAddress &a ) noexcept
{
    // Invariant: ip6_0[0..1] must be zero for V4 addresses.
    XASH_ASSERT( a.ip6_0[0] == 0 && a.ip6_0[1] == 0 );
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port   = ::htons( a.port );
    std::memcpy( &sa.sin_addr, a.addr.v4, 4 );
    return sa;
}

static sockaddr_in6 to_sockaddr_v6( const NetAddress &a ) noexcept
{
    sockaddr_in6 sa{};
    sa.sin6_family = AF_INET6;
    sa.sin6_port   = ::htons( a.port );
    std::memcpy( &sa.sin6_addr, a.addr.v6, 16 );
    return sa;
}

static NetAddress from_sockaddr( const sockaddr_storage &ss ) noexcept
{
    NetAddress a{};
    if( ss.ss_family == AF_INET )
    {
        const auto *sa = reinterpret_cast<const sockaddr_in *>( &ss );
        a.family = IpFamily::V4;
        a.port   = ::ntohs( sa->sin_port );
        std::memcpy( a.addr.v4, &sa->sin_addr, 4 );
    }
    else
    {
        const auto *sa = reinterpret_cast<const sockaddr_in6 *>( &ss );
        a.family = IpFamily::V6;
        a.port   = ::ntohs( sa->sin6_port );
        std::memcpy( a.addr.v6, &sa->sin6_addr, 16 );
    }
    return a;
}

// ---------------------------------------------------------------------------
// Internal: UTF-8 -> UTF-16 conversion (for inet_pton on older MSVC runtimes
// that do not expose an A-suffixed version accepting char*)
// ---------------------------------------------------------------------------

static std::optional<std::wstring> to_wide( std::string_view s ) noexcept
{
    if( s.empty() )
        return std::wstring{};
    const int n = ::MultiByteToWideChar( CP_UTF8, 0,
        s.data(), static_cast<int>( s.size() ), nullptr, 0 );
    if( n <= 0 )
        return std::nullopt;
    std::wstring w( static_cast<std::size_t>( n ), L'\0' );
    ::MultiByteToWideChar( CP_UTF8, 0,
        s.data(), static_cast<int>( s.size() ), w.data(), n );
    return w;
}

// ---------------------------------------------------------------------------
// WSAStartup / WSACleanup — ref-counted
// ---------------------------------------------------------------------------

static std::atomic<int> s_wsa_refcount{ 0 };

void socket_init() noexcept
{
    if( s_wsa_refcount.fetch_add( 1, std::memory_order_relaxed ) == 0 )
    {
        WSADATA wsad{};
        if( ::WSAStartup( MAKEWORD( 2, 2 ), &wsad ) != 0 )
        {
            // Decrement so a subsequent retry is possible.
            s_wsa_refcount.fetch_sub( 1, std::memory_order_relaxed );
        }
    }
}

void socket_shutdown() noexcept
{
    if( s_wsa_refcount.fetch_sub( 1, std::memory_order_relaxed ) == 1 )
        ::WSACleanup();
}

// ---------------------------------------------------------------------------
// open_udp_socket
// ---------------------------------------------------------------------------

Result<OsSocket> open_udp_socket( IpFamily family, std::uint16_t port,
                                   std::string_view bind_iface ) noexcept
{
    const int af   = ( family == IpFamily::V6 ) ? AF_INET6 : AF_INET;
    SOCKET    sock = ::socket( af, SOCK_DGRAM, IPPROTO_UDP );
    if( sock == INVALID_SOCKET )
        return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );

    OsSocket result{ static_cast<SocketHandle>( sock ) };

    // Non-blocking mode.
    u_long nb = 1;
    ::ioctlsocket( sock, FIONBIO, &nb );

    if( family == IpFamily::V4 )
    {
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = ::htons( port );
        if( !bind_iface.empty() )
        {
            const auto wface = to_wide( bind_iface );
            if( wface )
                ::InetPtonW( AF_INET, wface->c_str(), &sa.sin_addr );
        }
        if( ::bind( sock,
                    reinterpret_cast<sockaddr *>( &sa ),
                    sizeof( sa ) ) != 0 )
            return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );
    }
    else
    {
        sockaddr_in6 sa{};
        sa.sin6_family = AF_INET6;
        sa.sin6_port   = ::htons( port );
        if( !bind_iface.empty() )
        {
            const auto wface = to_wide( bind_iface );
            if( wface )
                ::InetPtonW( AF_INET6, wface->c_str(), &sa.sin6_addr );
        }
        if( ::bind( sock,
                    reinterpret_cast<sockaddr *>( &sa ),
                    sizeof( sa ) ) != 0 )
            return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );
    }

    return result;
}

// ---------------------------------------------------------------------------
// open_tcp_socket
// ---------------------------------------------------------------------------

Result<OsSocket> open_tcp_socket( IpFamily family ) noexcept
{
    const int af   = ( family == IpFamily::V6 ) ? AF_INET6 : AF_INET;
    SOCKET    sock = ::socket( af, SOCK_STREAM, IPPROTO_TCP );
    if( sock == INVALID_SOCKET )
        return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );

    u_long nb = 1;
    ::ioctlsocket( sock, FIONBIO, &nb );

    return OsSocket{ static_cast<SocketHandle>( sock ) };
}

// ---------------------------------------------------------------------------
// Socket options
// ---------------------------------------------------------------------------

bool set_non_blocking( const OsSocket &sock, bool on ) noexcept
{
    u_long nb = on ? 1 : 0;
    return ::ioctlsocket( static_cast<SOCKET>( sock.get() ), FIONBIO, &nb ) == 0;
}

bool set_broadcast( const OsSocket &sock, bool on ) noexcept
{
    const int val = on ? 1 : 0;
    return ::setsockopt( static_cast<SOCKET>( sock.get() ),
        SOL_SOCKET, SO_BROADCAST,
        reinterpret_cast<const char *>( &val ), sizeof( val ) ) == 0;
}

bool set_reuse_addr( const OsSocket &sock, bool on ) noexcept
{
    const int val = on ? 1 : 0;
    return ::setsockopt( static_cast<SOCKET>( sock.get() ),
        SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char *>( &val ), sizeof( val ) ) == 0;
}

bool set_recv_buffer( const OsSocket &sock, int bytes ) noexcept
{
    return ::setsockopt( static_cast<SOCKET>( sock.get() ),
        SOL_SOCKET, SO_RCVBUF,
        reinterpret_cast<const char *>( &bytes ), sizeof( bytes ) ) == 0;
}

bool set_send_buffer( const OsSocket &sock, int bytes ) noexcept
{
    return ::setsockopt( static_cast<SOCKET>( sock.get() ),
        SOL_SOCKET, SO_SNDBUF,
        reinterpret_cast<const char *>( &bytes ), sizeof( bytes ) ) == 0;
}

// ---------------------------------------------------------------------------
// bind_socket
// ---------------------------------------------------------------------------

bool bind_socket( const OsSocket &sock, const NetAddress &address ) noexcept
{
    if( address.family == IpFamily::V4 )
    {
        sockaddr_in sa = to_sockaddr_v4( address );
        return ::bind( static_cast<SOCKET>( sock.get() ),
                       reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) == 0;
    }
    sockaddr_in6 sa = to_sockaddr_v6( address );
    return ::bind( static_cast<SOCKET>( sock.get() ),
                   reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) == 0;
}

// ---------------------------------------------------------------------------
// UDP datagram I/O
// ---------------------------------------------------------------------------

Result<std::size_t> sendto( const OsSocket &sock,
                              std::span<const std::byte> data,
                              const NetAddress &to ) noexcept
{
    int n;
    if( to.family == IpFamily::V4 )
    {
        sockaddr_in sa = to_sockaddr_v4( to );
        n = ::sendto( static_cast<SOCKET>( sock.get() ),
            reinterpret_cast<const char *>( data.data() ),
            static_cast<int>( data.size() ),
            0, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) );
    }
    else
    {
        sockaddr_in6 sa = to_sockaddr_v6( to );
        n = ::sendto( static_cast<SOCKET>( sock.get() ),
            reinterpret_cast<const char *>( data.data() ),
            static_cast<int>( data.size() ),
            0, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) );
    }
    if( n < 0 )
        return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );
    return static_cast<std::size_t>( n );
}

Result<std::size_t> recvfrom( const OsSocket &sock,
                               std::span<std::byte> buffer,
                               NetAddress &from_out ) noexcept
{
    sockaddr_storage ss{};
    int              sslen = sizeof( ss );
    const int n = ::recvfrom( static_cast<SOCKET>( sock.get() ),
        reinterpret_cast<char *>( buffer.data() ),
        static_cast<int>( buffer.size() ),
        0, reinterpret_cast<sockaddr *>( &ss ), &sslen );
    if( n < 0 )
        return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );
    from_out = from_sockaddr( ss );
    return static_cast<std::size_t>( n );
}

// ---------------------------------------------------------------------------
// TCP stream I/O
// ---------------------------------------------------------------------------

Result<std::size_t> send_stream( const OsSocket &sock,
                                  std::span<const std::byte> data ) noexcept
{
    const int n = ::send( static_cast<SOCKET>( sock.get() ),
        reinterpret_cast<const char *>( data.data() ),
        static_cast<int>( data.size() ), 0 );
    if( n < 0 )
        return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );
    return static_cast<std::size_t>( n );
}

Result<std::size_t> recv_stream( const OsSocket &sock,
                                  std::span<std::byte> buffer ) noexcept
{
    const int n = ::recv( static_cast<SOCKET>( sock.get() ),
        reinterpret_cast<char *>( buffer.data() ),
        static_cast<int>( buffer.size() ), 0 );
    if( n < 0 )
        return std::unexpected( map_wsa_error( ::WSAGetLastError() ) );
    return static_cast<std::size_t>( n );
}

Result<void> connect_stream( const OsSocket &sock,
                               const NetAddress &to ) noexcept
{
    int rc;
    if( to.family == IpFamily::V4 )
    {
        sockaddr_in sa = to_sockaddr_v4( to );
        rc = ::connect( static_cast<SOCKET>( sock.get() ),
                        reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) );
    }
    else
    {
        sockaddr_in6 sa = to_sockaddr_v6( to );
        rc = ::connect( static_cast<SOCKET>( sock.get() ),
                        reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) );
    }
    if( rc == 0 )
        return {};
    const int err = ::WSAGetLastError();
    if( err == WSAEWOULDBLOCK || err == WSAEINPROGRESS )
        return std::unexpected( NetError::WouldBlock );
    return std::unexpected( map_wsa_error( err ) );
}

// ---------------------------------------------------------------------------
// get_local_address
// ---------------------------------------------------------------------------

std::optional<NetAddress> get_local_address( const OsSocket &sock ) noexcept
{
    sockaddr_storage ss{};
    int              len = sizeof( ss );
    if( ::getsockname( static_cast<SOCKET>( sock.get() ),
                       reinterpret_cast<sockaddr *>( &ss ), &len ) != 0 )
        return std::nullopt;
    return from_sockaddr( ss );
}

// ---------------------------------------------------------------------------
// DNS resolution (synchronous — call from Worker / NetIO only)
// ---------------------------------------------------------------------------

Result<NetAddress> resolve_blocking( std::string_view host,
                                      IpFamily family ) noexcept
{
    const std::string host_str( host );
    addrinfo          hints{};
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = ( family == IpFamily::V6 ) ? AF_INET6 : AF_INET;

    addrinfo  *res = nullptr;
    const int  err = ::getaddrinfo( host_str.c_str(), nullptr, &hints, &res );
    if( err != 0 )
    {
        return std::unexpected( err == EAI_AGAIN ? NetError::DnsAgain
                                                  : NetError::DnsFailure );
    }

    NetAddress a{};
    if( res->ai_family == AF_INET )
    {
        a.family = IpFamily::V4;
        std::memcpy( a.addr.v4,
            &reinterpret_cast<const sockaddr_in *>( res->ai_addr )->sin_addr,
            4 );
    }
    else
    {
        a.family = IpFamily::V6;
        std::memcpy( a.addr.v6,
            &reinterpret_cast<const sockaddr_in6 *>( res->ai_addr )->sin6_addr,
            16 );
    }
    ::freeaddrinfo( res );
    return a;
}

// ---------------------------------------------------------------------------
// DefaultPlatformSockets — production IPlatformSockets implementation
// ---------------------------------------------------------------------------

namespace {

struct DefaultPlatformSockets final : IPlatformSockets
{
    Result<OsSocket>
    open_udp( IpFamily family, std::uint16_t port,
              std::string_view bind_iface ) noexcept override
    {
        return xash::platform::open_udp_socket( family, port, bind_iface );
    }

    Result<OsSocket> open_tcp( IpFamily family ) noexcept override
    {
        return xash::platform::open_tcp_socket( family );
    }

    Result<std::size_t>
    sendto( const OsSocket &sock, std::span<const std::byte> data,
            const NetAddress &to ) noexcept override
    {
        return xash::platform::sendto( sock, data, to );
    }

    Result<std::size_t>
    recvfrom( const OsSocket &sock, std::span<std::byte> buffer,
              NetAddress &from_out ) noexcept override
    {
        return xash::platform::recvfrom( sock, buffer, from_out );
    }

    Result<std::size_t>
    send_stream( const OsSocket &sock,
                 std::span<const std::byte> data ) noexcept override
    {
        return xash::platform::send_stream( sock, data );
    }

    Result<std::size_t>
    recv_stream( const OsSocket &sock,
                 std::span<std::byte> buffer ) noexcept override
    {
        return xash::platform::recv_stream( sock, buffer );
    }

    Result<void>
    connect_stream( const OsSocket &sock,
                    const NetAddress &to ) noexcept override
    {
        return xash::platform::connect_stream( sock, to );
    }
};

} // anonymous namespace

IPlatformSockets &default_platform_sockets() noexcept
{
    static DefaultPlatformSockets s_instance;
    return s_instance;
}

} // namespace xash::platform
