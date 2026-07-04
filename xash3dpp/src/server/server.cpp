// xash3dpp — server subsystem implementation (Chunk 6 scaffold)
// Legacy reference: engine/server/sv_main.c (SV_Init/SV_Shutdown shell)
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (server pool)
//   xash3dpp_utilities  — (from S4 on: MD5/CRC32, Info strings, Matrix4x4)
//   xash3dpp_core       — logging, assertions, thread roles (from S6 on)

#include <xash3dpp/server/server.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

namespace xash::server {

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct Server::Impl
{
    xash::memory::PoolHandle pool_;
    ServerStats              stats_;

    bool active_      = false;
    bool initialized_ = false;

    // TODO(chunk6-S4): EdictArena (free-list, serialnumbers, freetime grace,
    //                  stale-field reuse) + string pool (heap arena +
    //                  SV_MakeString INT-range fallback, OQ-6 baseline) +
    //                  vendored ABI structs behind the Q-20 accessor seam.
    // TODO(chunk6-S5): world-interaction state — sv_areanodes[32], box-hull
    //                  scratch, touch-links semaphore, lightstyles.
    // TODO(chunk6-S6): svgame binding — DLL handle, the three function
    //                  tables, globalvars_t (pStringBase), LINK_ENTITY
    //                  dispatch, pfnGetHullBounds ×4 → hull_bounds flow.
    // TODO(chunk6-S7): sv (per-level) / svs (persistent) state split per
    //                  the boundary Owned-state section; lifecycle FSM.
    // TODO(chunk6-S8): frame timing state — sv.time (epoch 1.0), frametime,
    //                  time_residual, pushed[256] stack, pmove bridge.
    // TODO(chunk6-S9): client array, snapshot ring, challenges, filters,
    //                  server log, query responders.
};

Server::Server() : impl_{ std::make_unique<Impl>() } {}
Server::~Server() = default;

Server::Server( Server && ) noexcept            = default;
Server &Server::operator=( Server && ) noexcept = default;

bool Server::init( const ServerInitParams & /*params*/ )
{
    // OQ-9 threading posture: every server entry point is main-thread.
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    impl_->pool_ = xash::memory::create_pool( "server" );
    return static_cast<bool>( impl_->pool_ );
}

void Server::shutdown()
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );
    // TODO(chunk6-S7): SV_Shutdown semantics — final message ×2, master
    // shutdown, deactivate, free clients/ring/testpacket, close log.
    if( impl_->pool_ ) {
        xash::memory::destroy_pool( impl_->pool_ );
        impl_->pool_ = {};
    }
    impl_->active_      = false;
    impl_->initialized_ = false;
}

bool Server::active() const noexcept
{
    return impl_->active_;
}

bool Server::initialized() const noexcept
{
    return impl_->initialized_;
}

const ServerStats &Server::stats() const noexcept
{
    return impl_->stats_;
}

} // namespace xash::server
