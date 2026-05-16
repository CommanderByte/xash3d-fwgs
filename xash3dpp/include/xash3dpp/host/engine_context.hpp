#pragma once
// xash3dpp — EngineContext: flat owner of all stateful subsystems.
//
// Decision ref: docs/design/decisions-architecture.md §ENGINE_CONTEXT (Q-2)
//
// Members are declared in dependency order.  C++ guarantees construction in
// declaration order and destruction in reverse declaration order — there is no
// need for an explicit shutdown() sequence driven by the caller.  The explicit
// init() call IS necessary because each subsystem requires different parameters.
//
// Deliberate non-members:
//   • memory  — global pool registry; must outlive EngineContext (see Q-1).
//   • platform — stateless free functions; no lifecycle.

#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/clock.hpp>
#include <xash3dpp/host/host.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>

#include <cstdint>
#include <string_view>

namespace xash {

// ---------------------------------------------------------------------------
// EngineContextInitParams — all init-time configuration in one flat struct.
// Carries both configuration values and injected dependency pointers.
// Decision ref: decisions-architecture.md §DI_PARAMS (Q-4)
// ---------------------------------------------------------------------------

struct EngineContextInitParams {
    // Filesystem roots (non-owning string_views; caller owns storage for the
    // duration of the init() call — values are copied into the subsystems).
    std::string_view rootdir;    // engine install directory
    std::string_view basedir;    // always-mounted base game folder (e.g. "valve")
    std::string_view gamedir;    // active game folder (e.g. "cstrike"); empty = basedir
    std::string_view rodir;      // read-only content mirror; empty = disabled

    // CmdCvar injected dependencies (non-owning; must outlive EngineContext).
    // trust_oracle:  provided by the host at init; replaced by Server at Chunk 5.
    // compat_policy: provided by get_compat_policy() (link-time selection).
    cmd_cvar::ITrustOracle  *trust_oracle  = nullptr;
    cmd_cvar::ICompatPolicy *compat_policy = nullptr;

    // GoldSrc bug-compatibility bitfield (Resolved-decision OQ-7).
    // Parsed once by the launcher; copied verbatim into EngineContext::bugcomp.
    std::uint32_t bugcomp = 0;
};

// ---------------------------------------------------------------------------
// EngineContext — flat struct owning all stateful subsystems in construction
// (= dependency) order.
//
// Chunk 2 adds:  networking::NetworkContext  networking;
// Chunk 5 adds:  server::Server              server;
// Chunk 9 adds:  client::Client              client;   (non-dedicated only)
// ---------------------------------------------------------------------------

struct EngineContext {
    filesystem::Filesystem    filesystem;
    cmd_cvar::CmdCvarContext  cmd_cvar;
    core::Clock               clock;
    // Chunk 2: networking::NetworkContext  networking;
    MapLoader                 map_loader;
    Host                      host;
    // Chunk 5: server::Server              server;
    // Chunk 9: client::Client              client;

    // Resolved-decision OQ-7: centralised parse, distributed consumption.
    // Subsystems read `engine_ctx.bugcomp & BUGCOMP_X` at the point of
    // behaviour divergence; there is no central dispatcher.
    std::uint32_t bugcomp = 0;

    // Initialise subsystems in declaration order.  Returns false on the first
    // failure, cleaning up any already-initialised subsystems before returning.
    [[nodiscard]] bool init(const EngineContextInitParams &p) noexcept;

    // Shut down subsystems in reverse declaration order.
    // Idempotent — safe to call on a partially-initialised context.
    void shutdown() noexcept;
};

} // namespace xash
