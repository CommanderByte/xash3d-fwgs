#pragma once
// xash3dpp — IPlatformSockets: injectable seam for the networking subsystem
// Legacy reference: engine/common/net_ws.c (loopback / unit-test paths)
//
// The production implementation (DefaultPlatformSockets) wraps the free
// functions in os_socket.hpp.  Tests pass a FakePlatformSockets that records
// sends and replays canned receives without touching the OS.
//
// The networking subsystem stores an IPlatformSockets* in its
// NetworkInitParams.  Production code passes &default_platform_sockets();
// test code passes a local FakePlatformSockets instance.
//
// Per Q-7 (decisions-architecture.md): IPlatformSockets is an intra-process
// seam (same binary, same compiler); it does NOT cross a DLL boundary.
//
// @thread-safety: T_NetIO-ready — the seam mirrors the os_socket.hpp free
// functions (see that header's per-function contracts);
// default_platform_sockets() is a thread-safe C++11 magic-static.

#include <xash3dpp/platform/os_socket.hpp>

#include <cstddef>
#include <span>

namespace xash::platform {

// ---------------------------------------------------------------------------
// IPlatformSockets — injectable interface for hot-path socket operations
// ---------------------------------------------------------------------------

struct IPlatformSockets
{
    virtual ~IPlatformSockets() = default;

    // UDP socket creation + binding.
    virtual Result<OsSocket>
    open_udp( IpFamily family, std::uint16_t port,
              std::string_view bind_iface ) noexcept = 0;

    // TCP socket creation (no bind).
    virtual Result<OsSocket>
    open_tcp( IpFamily family ) noexcept = 0;

    // UDP send.  Returns bytes written or a NetError.
    virtual Result<std::size_t>
    sendto( const OsSocket &sock,
            std::span<const std::byte> data,
            const NetAddress &to ) noexcept = 0;

    // UDP receive.  Returns NetError::WouldBlock when no data is queued.
    virtual Result<std::size_t>
    recvfrom( const OsSocket &sock,
              std::span<std::byte> buffer,
              NetAddress &from_out ) noexcept = 0;

    // TCP write.  Partial writes are indicated by the returned byte count.
    virtual Result<std::size_t>
    send_stream( const OsSocket &sock,
                 std::span<const std::byte> data ) noexcept = 0;

    // TCP read.  Returns 0 bytes on orderly shutdown.
    virtual Result<std::size_t>
    recv_stream( const OsSocket &sock,
                 std::span<std::byte> buffer ) noexcept = 0;

    // Non-blocking connect.  NetError::WouldBlock == "in progress".
    virtual Result<void>
    connect_stream( const OsSocket &sock,
                    const NetAddress &to ) noexcept = 0;
};

// Returns the process-singleton production IPlatformSockets that delegates
// to the free functions in os_socket.hpp.
// Thread-safe: uses C++11 magic-static initialisation.
[[nodiscard]] IPlatformSockets &default_platform_sockets() noexcept;

} // namespace xash::platform
