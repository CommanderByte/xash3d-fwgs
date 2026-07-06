#pragma once
// xash3dpp — networking error codes and Result<T> alias
// @thread-safety: pure types (NetError enum + Result<T> alias) — no shared state
// Legacy reference: engine/common/net_ws.c (WinsockError / errno mapping),
//                   engine/common/net_ws_private.h
//
// This header is the minimal networking-error contract required by the
// platform sockets layer (platform/os_socket.hpp).  The full networking
// subsystem implementation lives in xash3dpp_networking (Chunk 2).
//
// std::expected is C++23.  Required toolchain minimum: MSVC 19.34+,
// GCC 12+ (libstdc++, -std=c++23), Clang 16+ (libc++).
// The project CMake standard was bumped to C++23 when this header was
// introduced.

#include <cstdint>
#include <expected>

namespace xash::networking {

// ---------------------------------------------------------------------------
// NetError — typed error codes for the networking and platform-sockets layers.
// ---------------------------------------------------------------------------

enum class NetError : std::uint32_t
{
    // OS-level socket errors
    WouldBlock,     // EAGAIN / EWOULDBLOCK / WSAEWOULDBLOCK
    SocketInvalid,  // EBADF / WSAENOTSOCK / invalid handle returned by syscall
    BindFailed,     // EADDRINUSE / WSAEADDRINUSE
    Overflow,       // EMSGSIZE / WSAEMSGSIZE — caller is expected to fragment
    BadAddress,     // EINVAL from bind with malformed address
    BufferTooSmall, // Receive buffer truncated
    NotInitialised, // Socket layer not initialised (Win32 pre-WSAStartup)
    InvalidArgument, // Caller-supplied parameter is malformed or out of range

    // DNS errors
    DnsAgain,    // EAI_AGAIN — transient failure; retry later
    DnsFailure,  // Any other getaddrinfo failure
};

// ---------------------------------------------------------------------------
// Result<T> — success-or-NetError alias (std::expected<T, NetError>).
// Used by the platform sockets layer and the full networking subsystem.
// ---------------------------------------------------------------------------

template<typename T>
using Result = std::expected<T, NetError>;

} // namespace xash::networking
