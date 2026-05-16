#pragma once
// xash3dpp — networking subsystem public API
// Legacy reference: engine/common/net_ws.h, netchan.h, net_buffer.h, net_encode.h
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed fragment buffers
//   xash3dpp_utilities  — string/path helpers, hash
//   xash3dpp_core       — log, assert, thread_role
//   xash3dpp_platform   — IPlatformSockets (see platform/platform_sockets.hpp)
//
// All public functions are noexcept and main-thread-only unless documented
// otherwise.  Background DNS thread is internal and uses the resolver mutex.

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/master_list.hpp>
#include <xash3dpp/networking/protocol_driver.hpp>
#include <xash3dpp/networking/stats.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

// IPlatformSockets lives in xash3dpp_platform; forward-declare to keep this
// header from pulling in the full platform socket types.
namespace xash::platform { struct IPlatformSockets; }

namespace xash::networking {

// ---------------------------------------------------------------------------
// SocketKind — which logical socket pair an operation uses
// ---------------------------------------------------------------------------

enum class SocketKind : std::uint8_t
{
    Client,
    Server,
};

// ---------------------------------------------------------------------------
// NetworkInitParams — passed to NetworkContext::init()
// ---------------------------------------------------------------------------

struct NetworkInitParams
{
    // Required: OS socket abstraction.  All real socket I/O routes through
    // this interface.  See docs/architecture/platform/sockets.md.
    xash::platform::IPlatformSockets *sockets = nullptr;

    // Optional: registry of additional IProtocolDriver factories.  When
    // nullptr, only the default GoldSrc protocol driver is available.
    IProtocolDriverRegistry *protocol_registry = nullptr;

    // Optional: master-server list configuration (LAN-only, NAT bypass, etc.).
    // When nullptr, master-list heartbeats are disabled.
    IMasterListConfig *master_list_config = nullptr;

    // True when the engine is a dedicated server.  Disables loopback ring,
    // bzip2/LZSS compression (per XASH_NET_COMPRESSION), and any client-only
    // diagnostics.
    bool dedicated = false;
};

// ---------------------------------------------------------------------------
// NetworkContext — owns transport, netchan, codec, and delta encoder state.
// ---------------------------------------------------------------------------

class NetworkContext
{
public:
    NetworkContext() noexcept;
    ~NetworkContext();

    NetworkContext( const NetworkContext & )            = delete;
    NetworkContext &operator=( const NetworkContext & ) = delete;

    // Move is supported; definitions are in context.cpp where Impl is complete.
    NetworkContext( NetworkContext && ) noexcept;
    NetworkContext &operator=( NetworkContext && ) noexcept;

    // ---- Lifecycle --------------------------------------------------------

    [[nodiscard]] bool init( const NetworkInitParams &params ) noexcept;
    void               shutdown() noexcept;

    [[nodiscard]] bool is_active() const noexcept;

    // ---- Configuration ----------------------------------------------------

    // Open (multiplayer=true) or close (false) the real UDP sockets.  No-op
    // for single-player or when networking is not initialised.  change_port
    // forces a new ephemeral client port.
    [[nodiscard]] Result<void> config( bool multiplayer, bool change_port ) noexcept;

    // ---- Send / Receive (the only packet I/O entry points) ---------------

    // Pull one packet.  Returns the bytes read into `data` and writes the
    // sender into `from`.  Loopback is consulted before real sockets.
    // @thread-safety: T_NetIO-ready
    [[nodiscard]] Result<std::size_t> get_packet(
        SocketKind           sock,
        NetAddress          &from,
        std::span<std::byte> data ) noexcept;

    // Send one datagram.  Routes to loopback for in-process addresses or to
    // the platform sockets layer otherwise.
    // @thread-safety: T_NetIO-ready
    [[nodiscard]] Result<void> send_packet(
        SocketKind                  sock,
        std::span<const std::byte>  data,
        const NetAddress           &to ) noexcept;

    // ---- Stats ------------------------------------------------------------

    [[nodiscard]] const NetworkingStats &stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::networking
