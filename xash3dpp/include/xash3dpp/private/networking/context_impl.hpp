#pragma once
// xash3dpp — NetworkContext private implementation header
// Visible only to TUs inside xash3dpp_networking.

#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/private/networking/master_list.hpp>
#include <xash3dpp/private/networking/protocol_driver.hpp>

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

    // TODO(Layer 1+): transport state (loopback rings, lag queue, split
    // reassembly), netchan registry, codec sizebuf, delta tables.
};

} // namespace xash::networking
