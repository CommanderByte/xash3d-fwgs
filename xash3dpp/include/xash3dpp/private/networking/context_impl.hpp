#pragma once
// xash3dpp — NetworkContext private implementation header
// Visible only to TUs inside xash3dpp_networking.

#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/networking/lag_queue.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/platform/os_socket.hpp>
#include <xash3dpp/platform/platform_sockets.hpp>
#include <xash3dpp/private/networking/loopback_transport.hpp>
#include <xash3dpp/private/networking/master_list.hpp>
#include <xash3dpp/private/networking/packet_pool.hpp>
#include <xash3dpp/private/networking/protocol_driver.hpp>
#include <xash3dpp/private/networking/split_reassembler.hpp>

#include <array>
#include <cstdint>

namespace xash::networking {

struct NetworkContext::Impl
{
    // Injected dependencies (non-owning).
    xash::platform::IPlatformSockets *sockets             = nullptr;
    IProtocolDriverRegistry          *protocol_registry   = nullptr;
    IMasterListConfig                *master_list_config  = nullptr;

    // Pool for all fragment-buffer allocations.  Created in init().
    xash::memory::PoolHandle pool;

    bool dedicated   = false;
    bool initialised = false;
    bool configured  = false;

    NetworkingStats stats;

    // ---- Layer 1 transport state ----

    // One dual-ring LoopbackTransport (handles Client<->Server via sock^1
    // semantics; see docs/architecture/networking/transport-layer.md).
    LoopbackTransport loopback {};

    // Shared datagram-buffer slab.
    PacketPool packet_pool {};

    // Per-direction (rx-side) fake-lag queue, indexed by SocketKind.
    std::array<LagQueue, 2> lag_queues {};

    // Per-socket split-packet reassembler.
    std::array<SplitReassembler, 2> reassemblers {};

    // Owned OS UDP sockets, one per SocketKind.  Opened by config(true),
    // closed by config(false) or shutdown().  Created lazily; .valid()
    // reflects whether the socket is currently open.
    std::array<xash::platform::OsSocket, 2> os_sockets {};

    // Last-requested bind port per SocketKind.  Tracked for change_port
    // semantics; 0 means kernel-chosen ephemeral.
    std::array<std::uint16_t, 2> bound_ports { 0, 0 };
};

} // namespace xash::networking
