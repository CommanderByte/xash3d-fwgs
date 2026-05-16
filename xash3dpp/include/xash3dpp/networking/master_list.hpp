#pragma once
// xash3dpp — IMasterListConfig / IMasterListClient: master-server injectable interfaces
// Legacy reference: engine/common/masterlist.c
//
// These interfaces are part of NetworkInitParams (public API).  Callers that
// provide a master-list configuration must implement IMasterListConfig.
// Networking owns the UDP I/O; the server layer configures it.
//
// Boundary spec: docs/boundaries/networking-boundary.md §OQ-6

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace xash::networking {

// ---------------------------------------------------------------------------
// IMasterListConfig — values the server sets at activation time
// ---------------------------------------------------------------------------

struct IMasterListConfig
{
    virtual ~IMasterListConfig() = default;

    [[nodiscard]] virtual bool   lan_only() const noexcept = 0;
    [[nodiscard]] virtual bool   nat_bypass() const noexcept = 0;
    [[nodiscard]] virtual double heartbeat_interval_seconds() const noexcept = 0;

    // Configured master-server addresses.  The satellite iterates this span
    // on every heartbeat()/send_shutdown() pass.  An empty span causes the
    // satellite to skip the send entirely (LAN-only servers).
    [[nodiscard]] virtual std::span<const NetAddress> master_addresses() const noexcept = 0;
};

// ---------------------------------------------------------------------------
// IMasterListClient — actions the master-list satellite exposes
// ---------------------------------------------------------------------------

struct IMasterListClient
{
    virtual ~IMasterListClient() = default;

    // Send a heartbeat to every configured master server.  Called from the
    // server's frame tick.
    virtual void heartbeat() noexcept = 0;

    // Inform the master servers that this server is shutting down.
    virtual void send_shutdown() noexcept = 0;
};

class NetworkContext; // fwd

// Factory — constructs the built-in master-list satellite that ships
// heartbeats / shutdowns through the supplied NetworkContext using the
// configuration values exposed by IMasterListConfig.  The returned client
// is owned by the caller; both `ctx` and `cfg` must outlive it.
[[nodiscard]] std::unique_ptr<IMasterListClient> create_master_list_client(
    NetworkContext    &ctx,
    IMasterListConfig &cfg ) noexcept;

} // namespace xash::networking
