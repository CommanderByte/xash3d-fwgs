// xash3dpp — networking subsystem: NetworkContext lifecycle (scaffold stub)
// Boundary spec: docs/boundaries/networking-boundary.md
//
// This file currently holds only the pimpl skeleton and lifecycle stubs.
// Transport, netchan, codec, and delta work lands in Layer 1+ chunks.

#include <xash3dpp/private/networking/context_impl.hpp>

#include <xash3dpp/memory/memory.hpp>

#include <utility>

namespace xash::networking {

NetworkContext::NetworkContext() noexcept
    : impl_( std::make_unique<Impl>() )
{
}

NetworkContext::~NetworkContext() = default;

NetworkContext::NetworkContext( NetworkContext && ) noexcept            = default;
NetworkContext &NetworkContext::operator=( NetworkContext && ) noexcept = default;

bool NetworkContext::init( const NetworkInitParams &params ) noexcept
{
    if( !impl_ )
        return false;
    if( impl_->initialised )
        return true;

    // IPlatformSockets is mandatory; without it no real I/O can happen.
    // See docs/architecture/platform/sockets.md for the contract.
    if( params.sockets == nullptr )
        return false;

    impl_->sockets            = params.sockets;
    impl_->protocol_registry  = params.protocol_registry;
    impl_->master_list_config = params.master_list_config;
    impl_->dedicated          = params.dedicated;

    impl_->pool = xash::memory::create_pool( "networking" );
    if( impl_->pool == xash::memory::k_null_pool )
        return false;

    impl_->initialised = true;
    return true;
}

void NetworkContext::shutdown() noexcept
{
    if( !impl_ || !impl_->initialised )
        return;

    if( impl_->pool != xash::memory::k_null_pool )
    {
        xash::memory::destroy_pool( impl_->pool );
        impl_->pool = xash::memory::k_null_pool;
    }

    impl_->sockets            = nullptr;
    impl_->protocol_registry  = nullptr;
    impl_->master_list_config = nullptr;
    impl_->configured         = false;
    impl_->initialised        = false;
}

bool NetworkContext::is_active() const noexcept
{
    return impl_ && impl_->initialised;
}

Result<void> NetworkContext::config( bool /*multiplayer*/, bool /*change_port*/ ) noexcept
{
    if( !is_active() )
        return std::unexpected( NetError::NotInitialised );
    // TODO(Chunk 4): open/close real UDP sockets via IPlatformSockets.
    return {};
}

Result<std::size_t> NetworkContext::get_packet(
    SocketKind /*sock*/,
    NetAddress & /*from*/,
    std::span<std::byte> /*data*/ ) noexcept
{
    if( !is_active() )
        return std::unexpected( NetError::NotInitialised );
    // TODO(Chunk 4+): consult loopback ring, then IPlatformSockets::recvfrom.
    return std::unexpected( NetError::WouldBlock );
}

Result<void> NetworkContext::send_packet(
    SocketKind /*sock*/,
    std::span<const std::byte> /*data*/,
    const NetAddress & /*to*/ ) noexcept
{
    if( !is_active() )
        return std::unexpected( NetError::NotInitialised );
    // TODO(Chunk 4+): loopback short-circuit, else IPlatformSockets::sendto.
    return {};
}

const NetworkingStats &NetworkContext::stats() const noexcept
{
    return impl_->stats;
}

} // namespace xash::networking
