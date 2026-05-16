#pragma once
// xash3dpp — platform socket abstraction: RAII handle + free functions
// Legacy reference: engine/common/net_ws.c (socket create / bind / send / recv),
//                   engine/common/net_ws_private.h
//
// Implementations:
//   src/platform/win32/os_socket.cpp   — Windows (Winsock2)
//   src/platform/posix/os_socket.cpp   — Linux / macOS / FreeBSD / Android
//
// Porting contract:
//   All functions must be implemented for every platform target.
//   OS-specific socket headers (<winsock2.h>, <sys/socket.h>, etc.) are
//   confined to the implementation TUs — never included in this header.
//
// Threading annotations:
//   // @thread-safety: T_NetIO-ready
//   means: callable from ThreadRole::Main today; callable from a future
//   ThreadRole::NetIO thread with zero code change (threading-model.md §7).

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace xash::platform {

// ---------------------------------------------------------------------------
// Re-export networking types used throughout the platform socket API.
// Callers who include this header get IpFamily, NetAddress, NetError, Result
// without needing to include the networking/ headers directly.
// ---------------------------------------------------------------------------

using xash::networking::IpFamily;
using xash::networking::NetAddress;
using xash::networking::NetError;
using xash::networking::Result;

// ---------------------------------------------------------------------------
// SocketHandle — opaque OS socket handle type.
//
// Win32: uintptr_t — matches SOCKET (= UINT_PTR); k_invalid_socket == ~0.
// POSIX: int       — sockets are file descriptors; k_invalid_socket == -1.
// ---------------------------------------------------------------------------

#ifdef _WIN32
using SocketHandle = std::uintptr_t;
inline constexpr SocketHandle k_invalid_socket = static_cast<SocketHandle>( ~0ull );
#else
using SocketHandle = int;
inline constexpr SocketHandle k_invalid_socket = static_cast<SocketHandle>( -1 );
#endif

// ---------------------------------------------------------------------------
// OsSocket — RAII wrapper around a native socket handle.
//
// Rules:
//   • Not copyable — ownership is exclusive.
//   • Movable — O(1), leaves source invalid.
//   • ~OsSocket() calls close(), which is a no-op on an already-invalid handle.
//
// OsSocket::close() is defined in the platform-specific os_socket.cpp so that
// Winsock / POSIX socket headers stay confined to those translation units.
// ---------------------------------------------------------------------------

class OsSocket
{
public:
    OsSocket() noexcept = default;
    explicit OsSocket( SocketHandle h ) noexcept : handle_{ h } {}
    ~OsSocket() noexcept { close(); }

    OsSocket( const OsSocket & )             = delete;
    OsSocket &operator=( const OsSocket & )  = delete;

    OsSocket( OsSocket &&o ) noexcept : handle_{ o.handle_ }
    {
        o.handle_ = k_invalid_socket;
    }

    OsSocket &operator=( OsSocket &&o ) noexcept
    {
        close();
        handle_   = o.handle_;
        o.handle_ = k_invalid_socket;
        return *this;
    }

    [[nodiscard]] constexpr bool         valid()  const noexcept { return handle_ != k_invalid_socket; }
    [[nodiscard]] constexpr SocketHandle get()    const noexcept { return handle_; }

    // Relinquish ownership without closing.  Caller is responsible for the
    // handle's lifetime after this call.
    [[nodiscard]] SocketHandle release() noexcept
    {
        SocketHandle h = handle_;
        handle_ = k_invalid_socket;
        return h;
    }

    // Close the socket.  Safe to call on an invalid handle (no-op).
    // Defined in the platform os_socket translation unit.
    void close() noexcept;

private:
    SocketHandle handle_{ k_invalid_socket };
};

// ---------------------------------------------------------------------------
// Winsock lifecycle (Win32 only; POSIX: no-ops).
// Ref-counted around all networking lifetimes:
//   socket_init()     — first call issues WSAStartup(2,2).
//   socket_shutdown() — last call issues WSACleanup().
// Both are safe to call multiple times in any order.
// ---------------------------------------------------------------------------

// @thread-safety: T_NetIO-ready
void socket_init()     noexcept;

// @thread-safety: T_NetIO-ready
void socket_shutdown() noexcept;

// ---------------------------------------------------------------------------
// Socket creation
// ---------------------------------------------------------------------------

// Create and bind a non-blocking UDP socket.
//   family     — IpFamily::V4 or IpFamily::V6
//   port       — 0 requests a kernel-chosen ephemeral port
//   bind_iface — dotted / colon-hex interface string, or empty for INADDR_ANY
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<OsSocket>
open_udp_socket( IpFamily family, std::uint16_t port,
                 std::string_view bind_iface ) noexcept;

// Create a non-blocking TCP socket (no bind).  Used by the HTTP downloader.
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<OsSocket>
open_tcp_socket( IpFamily family ) noexcept;

// ---------------------------------------------------------------------------
// Socket options
// ---------------------------------------------------------------------------

// @thread-safety: T_NetIO-ready
[[nodiscard]] bool set_non_blocking( const OsSocket &sock, bool on ) noexcept;

// Enable SO_BROADCAST for master-server discovery packets.
// @thread-safety: T_NetIO-ready
[[nodiscard]] bool set_broadcast(   const OsSocket &sock, bool on ) noexcept;

// SO_REUSEADDR for dedicated-server restart scenarios.
// @thread-safety: T_NetIO-ready
[[nodiscard]] bool set_reuse_addr(  const OsSocket &sock, bool on ) noexcept;

// Set SO_RCVBUF.  Networking calls this with the per-cvar value at config time.
// @thread-safety: T_NetIO-ready
[[nodiscard]] bool set_recv_buffer( const OsSocket &sock, int bytes ) noexcept;

// Set SO_SNDBUF.
// @thread-safety: T_NetIO-ready
[[nodiscard]] bool set_send_buffer( const OsSocket &sock, int bytes ) noexcept;

// ---------------------------------------------------------------------------
// Bind (for sockets not bound at creation time)
// ---------------------------------------------------------------------------

// @thread-safety: T_NetIO-ready
[[nodiscard]] bool bind_socket( const OsSocket &sock,
                                const NetAddress &address ) noexcept;

// ---------------------------------------------------------------------------
// UDP datagram I/O
// ---------------------------------------------------------------------------

// Send a UDP datagram.  Returns bytes written or a NetError.
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<std::size_t>
sendto( const OsSocket &sock,
        std::span<const std::byte> data,
        const NetAddress &to ) noexcept;

// Receive a UDP datagram.  Returns NetError::WouldBlock when no data queued.
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<std::size_t>
recvfrom( const OsSocket &sock,
          std::span<std::byte> buffer,
          NetAddress &from_out ) noexcept;

// ---------------------------------------------------------------------------
// TCP stream I/O
// ---------------------------------------------------------------------------

// TCP write.  Partial writes are indicated by the returned byte count.
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<std::size_t>
send_stream( const OsSocket &sock,
             std::span<const std::byte> data ) noexcept;

// TCP read.  Returning 0 bytes indicates an orderly shutdown by the peer.
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<std::size_t>
recv_stream( const OsSocket &sock,
             std::span<std::byte> buffer ) noexcept;

// Issue a non-blocking connect.  NetError::WouldBlock is the normal
// "in progress" return; the caller polls for writability to detect completion.
// @thread-safety: T_NetIO-ready
[[nodiscard]] Result<void>
connect_stream( const OsSocket &sock, const NetAddress &to ) noexcept;

// ---------------------------------------------------------------------------
// Address query
// ---------------------------------------------------------------------------

// Returns the local bound address, or nullopt on error.
// Equivalent of legacy NET_GetLocalAddress.
// @thread-safety: T_NetIO-ready
[[nodiscard]] std::optional<NetAddress>
get_local_address( const OsSocket &sock ) noexcept;

// ---------------------------------------------------------------------------
// DNS resolution
// ---------------------------------------------------------------------------

// Synchronous getaddrinfo.  MUST NOT be called from ThreadRole::Main —
// only from ThreadRole::Worker (today) or ThreadRole::NetIO (when introduced).
// @thread-safety: Worker / NetIO ONLY
[[nodiscard]] Result<NetAddress>
resolve_blocking( std::string_view host, IpFamily family ) noexcept;

} // namespace xash::platform
