// xash3dpp — platform (POSIX: Linux, macOS, FreeBSD, Android) — socket I/O
// Legacy reference: engine/common/net_ws.c (POSIX socket create, bind,
//                   send/recv paths), engine/common/net_ws_private.h
//
// Existing platform helpers used:
//   xash3dpp_core — XASH_ASSERT (debug-only invariant checks)

#if defined(_WIN32)
#  error "This file is POSIX-only"
#endif

#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>

#include <xash3dpp/core/assert.hpp>

// POSIX socket headers — confined to this translation unit.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>    // std::memcpy
#include <optional>
#include <string>

namespace xash::platform {

// ---------------------------------------------------------------------------
// OsSocket::close() — POSIX (socket fds use ::close, same as regular fds)
// ---------------------------------------------------------------------------

void OsSocket::close() noexcept
{
    if( handle_ >= 0 )
    {
        ::close( handle_ );
        handle_ = k_invalid_socket;
    }
}

// ---------------------------------------------------------------------------
// Internal: error mapping
// ---------------------------------------------------------------------------

static NetError map_errno( int err ) noexcept
{
    switch( err )
    {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return NetError::WouldBlock;
    case EBADF:
    case ENOTSOCK:      return NetError::SocketInvalid;
    case EADDRINUSE:    return NetError::BindFailed;
    case EMSGSIZE:      return NetError::Overflow;
    case EINVAL:        return NetError::BadAddress;
    default:            return NetError::SocketInvalid;
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
        const auto *sa = reinterpret_cast<const sockaddr_in *>( &ss ); // SAFETY: sockaddr family pun — BSD sockets API contract; ss_family == AF_INET checked above
        a.family = IpFamily::V4;
        a.port   = ::ntohs( sa->sin_port );
        std::memcpy( a.addr.v4, &sa->sin_addr, 4 );
    }
    else
    {
        const auto *sa = reinterpret_cast<const sockaddr_in6 *>( &ss ); // SAFETY: sockaddr family pun — BSD sockets API contract; non-AF_INET storage is AF_INET6 here
        a.family = IpFamily::V6;
        a.port   = ::ntohs( sa->sin6_port );
        std::memcpy( a.addr.v6, &sa->sin6_addr, 16 );
    }
    return a;
}

// ---------------------------------------------------------------------------
// Winsock lifecycle — POSIX no-ops
// ---------------------------------------------------------------------------

void socket_init()     noexcept {}
void socket_shutdown() noexcept {}

// ---------------------------------------------------------------------------
// open_udp_socket
// ---------------------------------------------------------------------------

Result<OsSocket> open_udp_socket( IpFamily family, std::uint16_t port,
                                   std::string_view bind_iface ) noexcept
{
    const int af = ( family == IpFamily::V6 ) ? AF_INET6 : AF_INET;
    int       fd;
    do { fd = ::socket( af, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        IPPROTO_UDP ); }
    while( fd < 0 && errno == EINTR );

    if( fd < 0 )
        return std::unexpected( map_errno( errno ) );

    OsSocket result{ fd };

    if( family == IpFamily::V4 )
    {
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = ::htons( port );
        if( !bind_iface.empty() )
        {
            const std::string s( bind_iface );
            ::inet_pton( AF_INET, s.c_str(), &sa.sin_addr );
        }
        if( ::bind( fd, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) < 0 ) // SAFETY: sockaddr_in → sockaddr upcast — BSD bind() takes the generic header
            return std::unexpected( map_errno( errno ) );
    }
    else
    {
        sockaddr_in6 sa{};
        sa.sin6_family = AF_INET6;
        sa.sin6_port   = ::htons( port );
        if( !bind_iface.empty() )
        {
            const std::string s( bind_iface );
            ::inet_pton( AF_INET6, s.c_str(), &sa.sin6_addr );
        }
        if( ::bind( fd, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) < 0 ) // SAFETY: sockaddr_in6 → sockaddr upcast — BSD bind() takes the generic header
            return std::unexpected( map_errno( errno ) );
    }

    return result;
}

// ---------------------------------------------------------------------------
// open_tcp_socket
// ---------------------------------------------------------------------------

Result<OsSocket> open_tcp_socket( IpFamily family ) noexcept
{
    const int af = ( family == IpFamily::V6 ) ? AF_INET6 : AF_INET;
    int       fd;
    do { fd = ::socket( af, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        IPPROTO_TCP ); }
    while( fd < 0 && errno == EINTR );

    if( fd < 0 )
        return std::unexpected( map_errno( errno ) );
    return OsSocket{ fd };
}

// ---------------------------------------------------------------------------
// Socket options
// ---------------------------------------------------------------------------

bool set_non_blocking( const OsSocket &sock, bool on ) noexcept // compliance-allow(thread-assert): stateless OS-handle wrapper — thread affinity belongs to the handle owner
{
    const int flags = ::fcntl( sock.get(), F_GETFL, 0 );
    if( flags < 0 )
        return false;
    const int new_flags = on ? ( flags | O_NONBLOCK ) : ( flags & ~O_NONBLOCK );
    return ::fcntl( sock.get(), F_SETFL, new_flags ) == 0;
}

bool set_broadcast( const OsSocket &sock, bool on ) noexcept // compliance-allow(thread-assert): stateless OS-handle wrapper — thread affinity belongs to the handle owner
{
    const int val = on ? 1 : 0;
    return ::setsockopt( sock.get(), SOL_SOCKET, SO_BROADCAST,
                         &val, sizeof( val ) ) == 0;
}

bool set_reuse_addr( const OsSocket &sock, bool on ) noexcept // compliance-allow(thread-assert): stateless OS-handle wrapper — thread affinity belongs to the handle owner
{
    const int val = on ? 1 : 0;
    return ::setsockopt( sock.get(), SOL_SOCKET, SO_REUSEADDR,
                         &val, sizeof( val ) ) == 0;
}

bool set_recv_buffer( const OsSocket &sock, int bytes ) noexcept // compliance-allow(thread-assert): stateless OS-handle wrapper — thread affinity belongs to the handle owner
{
    return ::setsockopt( sock.get(), SOL_SOCKET, SO_RCVBUF,
                         &bytes, sizeof( bytes ) ) == 0;
}

bool set_send_buffer( const OsSocket &sock, int bytes ) noexcept // compliance-allow(thread-assert): stateless OS-handle wrapper — thread affinity belongs to the handle owner
{
    return ::setsockopt( sock.get(), SOL_SOCKET, SO_SNDBUF,
                         &bytes, sizeof( bytes ) ) == 0;
}

// ---------------------------------------------------------------------------
// bind_socket
// ---------------------------------------------------------------------------

bool bind_socket( const OsSocket &sock, const NetAddress &address ) noexcept
{
    if( address.family == IpFamily::V4 )
    {
        sockaddr_in sa = to_sockaddr_v4( address );
        return ::bind( sock.get(),
                       reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) == 0; // SAFETY: sockaddr_in → sockaddr upcast — BSD bind() takes the generic header
    }
    sockaddr_in6 sa = to_sockaddr_v6( address );
    return ::bind( sock.get(),
                   reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) == 0; // SAFETY: sockaddr_in6 → sockaddr upcast — BSD bind() takes the generic header
}

// ---------------------------------------------------------------------------
// UDP datagram I/O
// ---------------------------------------------------------------------------

Result<std::size_t> sendto( const OsSocket &sock,
                              std::span<const std::byte> data,
                              const NetAddress &to ) noexcept
{
    ::ssize_t n;
    if( to.family == IpFamily::V4 )
    {
        sockaddr_in sa = to_sockaddr_v4( to );
        do { n = ::sendto( sock.get(),
                           data.data(), data.size(),
                           MSG_NOSIGNAL,
                           reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ); } // SAFETY: sockaddr_in → sockaddr upcast — BSD sendto() takes the generic header
        while( n < 0 && errno == EINTR );
    }
    else
    {
        sockaddr_in6 sa = to_sockaddr_v6( to );
        do { n = ::sendto( sock.get(),
                           data.data(), data.size(),
                           MSG_NOSIGNAL,
                           reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ); } // SAFETY: sockaddr_in6 → sockaddr upcast — BSD sendto() takes the generic header
        while( n < 0 && errno == EINTR );
    }
    if( n < 0 )
        return std::unexpected( map_errno( errno ) );
    return static_cast<std::size_t>( n );
}

Result<std::size_t> recvfrom( const OsSocket &sock,
                               std::span<std::byte> buffer,
                               NetAddress &from_out ) noexcept
{
    sockaddr_storage ss{};
    socklen_t        sslen = sizeof( ss );
    ::ssize_t        n;
    do { n = ::recvfrom( sock.get(),
                         buffer.data(), buffer.size(),
                         0,
                         reinterpret_cast<sockaddr *>( &ss ), &sslen ); } // SAFETY: sockaddr_storage out-param pun — BSD recvfrom() contract; kernel writes ≤ sslen
    while( n < 0 && errno == EINTR );

    if( n < 0 )
        return std::unexpected( map_errno( errno ) );
    from_out = from_sockaddr( ss );
    return static_cast<std::size_t>( n );
}

// ---------------------------------------------------------------------------
// TCP stream I/O
// ---------------------------------------------------------------------------

// compliance-allow(thread-assert): stateless ::send() wrapper over a
// caller-owned OsSocket handle — header-contracted T_NetIO-ready, thread
// affinity belongs to the handle owner
Result<std::size_t> send_stream( const OsSocket &sock,
                                  std::span<const std::byte> data ) noexcept
{
    ::ssize_t n;
    do { n = ::send( sock.get(), data.data(), data.size(), MSG_NOSIGNAL ); }
    while( n < 0 && errno == EINTR );

    if( n < 0 )
        return std::unexpected( map_errno( errno ) );
    return static_cast<std::size_t>( n );
}

Result<std::size_t> recv_stream( const OsSocket &sock,
                                  std::span<std::byte> buffer ) noexcept
{
    ::ssize_t n;
    do { n = ::recv( sock.get(), buffer.data(), buffer.size(), 0 ); }
    while( n < 0 && errno == EINTR );

    if( n < 0 )
        return std::unexpected( map_errno( errno ) );
    return static_cast<std::size_t>( n );
}

// compliance-allow(thread-assert): stateless non-blocking ::connect() wrapper
// over a caller-owned OsSocket handle — header-contracted T_NetIO-ready, not
// a Main-pinned lifecycle step
Result<void> connect_stream( const OsSocket &sock,
                               const NetAddress &to ) noexcept
{
    int rc;
    if( to.family == IpFamily::V4 )
    {
        sockaddr_in sa = to_sockaddr_v4( to );
        do { rc = ::connect( sock.get(),
                             reinterpret_cast<sockaddr *>( &sa ), // SAFETY: sockaddr_in → sockaddr upcast — BSD connect() takes the generic header
                             sizeof( sa ) ); }
        while( rc < 0 && errno == EINTR );
    }
    else
    {
        sockaddr_in6 sa = to_sockaddr_v6( to );
        do { rc = ::connect( sock.get(),
                             reinterpret_cast<sockaddr *>( &sa ), // SAFETY: sockaddr_in6 → sockaddr upcast — BSD connect() takes the generic header
                             sizeof( sa ) ); }
        while( rc < 0 && errno == EINTR );
    }
    if( rc == 0 )
        return {};
    if( errno == EINPROGRESS || errno == EWOULDBLOCK )
        return std::unexpected( NetError::WouldBlock );
    return std::unexpected( map_errno( errno ) );
}

// ---------------------------------------------------------------------------
// get_local_address
// ---------------------------------------------------------------------------

std::optional<NetAddress> get_local_address( const OsSocket &sock ) noexcept
{
    sockaddr_storage ss{};
    socklen_t        len = sizeof( ss );
    if( ::getsockname( sock.get(),
                       reinterpret_cast<sockaddr *>( &ss ), &len ) < 0 ) // SAFETY: sockaddr_storage out-param pun — BSD getsockname() contract; kernel writes ≤ len
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
            &reinterpret_cast<const sockaddr_in *>( res->ai_addr )->sin_addr, // SAFETY: sockaddr family pun — ai_family == AF_INET checked; getaddrinfo owns the storage
            4 );
    }
    else
    {
        a.family = IpFamily::V6;
        std::memcpy( a.addr.v6,
            &reinterpret_cast<const sockaddr_in6 *>( res->ai_addr )->sin6_addr, // SAFETY: sockaddr family pun — non-AF_INET result is AF_INET6 (hints filtered); getaddrinfo owns the storage
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
