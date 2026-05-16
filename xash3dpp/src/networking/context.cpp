// xash3dpp — networking subsystem: NetworkContext lifecycle (scaffold stub)
// Boundary spec: docs/boundaries/networking-boundary.md
//
// This file currently holds only the pimpl skeleton and lifecycle stubs.
// Transport, netchan, codec, and delta work lands in Layer 1+ chunks.

#include <xash3dpp/private/networking/context_impl.hpp>

#include <xash3dpp/memory/memory.hpp>

#include <utility>

namespace xash::networking {

namespace {

// SocketKind → 0/1 index used into Impl::os_sockets and Impl::bound_ports.
constexpr std::size_t socket_index( SocketKind kind ) noexcept
{
    return kind == SocketKind::Client ? 0u : 1u;
}

} // namespace

// Open one OS UDP socket for `kind` on `port` (0 = ephemeral).  Closes any
// previously-owned socket on this slot first.  Returns the NetError from the
// platform layer on failure; the slot is left invalid in that case.
//
// Member function so it can access the private Impl type without friendship.
Result<void> NetworkContext::open_socket_for_kind_(
    SocketKind    kind,
    std::uint16_t port ) noexcept
{
    const auto idx = socket_index( kind );

    // Drop any prior socket first; assigning OsSocket onto a valid slot
    // would close it via OsSocket::operator=, but doing it explicitly makes
    // the intent obvious.
    impl_->os_sockets[ idx ] = xash::platform::OsSocket{};

    auto result = impl_->sockets->open_udp(
        xash::platform::IpFamily::V4,
        port,
        /*bind_iface=*/ std::string_view{} );

    if( !result )
        return std::unexpected( result.error() );

    impl_->os_sockets[ idx ]  = std::move( *result );
    impl_->bound_ports[ idx ] = port;
    return {};
}

void NetworkContext::close_socket_for_kind_( SocketKind kind ) noexcept
{
    const auto idx = socket_index( kind );
    impl_->os_sockets[ idx ]  = xash::platform::OsSocket{};
    impl_->bound_ports[ idx ] = 0;
}

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

    // Close OS sockets before clearing the IPlatformSockets pointer so the
    // RAII close still goes through the right platform layer.
    close_socket_for_kind_( SocketKind::Client );
    close_socket_for_kind_( SocketKind::Server );

    // Drain transport state before releasing the pool so any pool-backed
    // buffers (Layer 3+) are emptied while their backing pool is still alive.
    impl_->loopback.clear();
    for( auto &q : impl_->lag_queues )
        q.clear();
    for( auto &r : impl_->reassemblers )
        r.reset();

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

Result<void> NetworkContext::config( bool multiplayer, bool change_port ) noexcept
{
    if( !is_active() )
        return std::unexpected( NetError::NotInitialised );

    if( !multiplayer )
    {
        // Tear down both sockets.  Loopback ring and transport state remain
        // intact for in-process single-player.
        close_socket_for_kind_( SocketKind::Client );
        close_socket_for_kind_( SocketKind::Server );
        impl_->configured = false;
        return {};
    }

    // Open the server socket first if not already open.  Dedicated builds
    // still want a server socket; only the client socket is suppressed.
    if( !impl_->os_sockets[ socket_index( SocketKind::Server ) ].valid() )
    {
        if( auto r = open_socket_for_kind_( SocketKind::Server, 0 ); !r )
            return std::unexpected( r.error() );
    }

    if( !impl_->dedicated )
    {
        const bool need_open  = !impl_->os_sockets[ socket_index( SocketKind::Client ) ].valid();
        const bool need_reopen = change_port && !need_open;

        if( need_open || need_reopen )
        {
            if( auto r = open_socket_for_kind_( SocketKind::Client, 0 ); !r )
                return std::unexpected( r.error() );
        }
    }

    impl_->configured = true;
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
