// xash3dpp — networking subsystem: NetworkContext lifecycle (scaffold stub)
// Boundary spec: docs/boundaries/networking-boundary.md
//
// This file currently holds only the pimpl skeleton and lifecycle stubs.
// Transport, netchan, codec, and delta work lands in Layer 1+ chunks.

#include <xash3dpp/private/networking/context_impl.hpp>
#include <xash3dpp/private/networking/wire/protocol_driver_default.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <utility>

namespace xash::networking {

namespace {

// SocketKind → 0/1 index used into Impl::os_sockets and Impl::bound_ports.
constexpr std::size_t socket_index( SocketKind kind ) noexcept
{
    return kind == SocketKind::Client ? 0u : 1u;
}

// Is `a` a 127.0.0.0/8 IPv4 address?  Used by send_packet to short-circuit
// to the in-process loopback ring (legacy ref: NA_LOOPBACK routing in
// NET_SendPacket / NET_SendLoopPacket).
constexpr bool is_loopback_v4( const NetAddress &a ) noexcept
{
    return a.family == xash::platform::IpFamily::V4 && a.addr.v4[ 0 ] == 127;
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

bool NetworkContext::init( const NetworkInitParams &params ) noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
{
    if( !impl_ )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "networking",
                   "init() called on moved-from NetworkContext" );
        return false;
    }
    if( impl_->initialised )
        return true;

    // IPlatformSockets is mandatory; without it no real I/O can happen.
    // See docs/architecture/platform/sockets.md for the contract.
    if( params.sockets == nullptr )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "networking",
                   "init() requires a non-null IPlatformSockets" );
        return false;
    }

    impl_->sockets            = params.sockets;
    impl_->protocol_registry  = params.protocol_registry;
    impl_->master_list_config = params.master_list_config;
    impl_->dedicated          = params.dedicated;

    impl_->pool = xash::memory::create_pool( "networking" );
    if( impl_->pool == xash::memory::k_null_pool )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "networking",
                   "failed to create memory pool" );
        return false;
    }

    impl_->initialised = true;
    return true;
}

void NetworkContext::shutdown() noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
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
    SocketKind           sock,
    NetAddress          &from,
    std::span<std::byte> data ) noexcept
{
    if( !is_active() )
        return std::unexpected( NetError::NotInitialised );

    // 1) Consult the in-process loopback ring first.  For listen-server
    //    builds this is the fast path; legacy ref: NET_GetLoopPacket.
    if( auto loop = impl_->loopback.receive( sock, data ); loop.has_value() )
    {
        from = NetAddress::loopback_v4();
        impl_->stats.packets_received.fetch_add( 1, std::memory_order_relaxed );
        impl_->stats.bytes_received.fetch_add( *loop, std::memory_order_relaxed );
        return *loop;
    }
    else if( loop.error() != NetError::WouldBlock )
    {
        // Loopback returned a hard error (BufferTooSmall, etc.).  Surface it.
        return std::unexpected( loop.error() );
    }

    // 2) No loopback packet — drain one datagram from the real socket if it
    //    is open.  When the socket is closed (e.g. config(false)) this
    //    short-circuits to WouldBlock so callers can keep polling without
    //    branching on socket state.
    const auto idx = socket_index( sock );
    if( !impl_->os_sockets[ idx ].valid() )
        return std::unexpected( NetError::WouldBlock );

    auto rx = impl_->sockets->recvfrom( impl_->os_sockets[ idx ], data, from );
    if( !rx )
        return std::unexpected( rx.error() );

    impl_->stats.packets_received.fetch_add( 1, std::memory_order_relaxed );
    impl_->stats.bytes_received.fetch_add( *rx, std::memory_order_relaxed );
    return *rx;
}

// compliance-allow(thread-assert): T_NetIO single-thread caller contract —
// transport stack has no internal sync; role unasserted until the NetIO
// thread is split out (G-2)
Result<void> NetworkContext::send_packet(
    SocketKind                  sock,
    std::span<const std::byte>  data,
    const NetAddress           &to ) noexcept
{
    if( !is_active() )
        return std::unexpected( NetError::NotInitialised );

    // 1) Loopback routing — addresses in 127.0.0.0/8 bypass the OS and
    //    enqueue onto the opposite-side ring (matches legacy NA_LOOPBACK).
    //    Dedicated builds skip the loopback ring entirely (no local client).
    if( is_loopback_v4( to ) && !impl_->dedicated )
    {
        if( auto r = impl_->loopback.send( sock, data ); !r )
            return std::unexpected( r.error() );

        impl_->stats.packets_sent.fetch_add( 1, std::memory_order_relaxed );
        impl_->stats.bytes_sent.fetch_add( data.size(), std::memory_order_relaxed );
        return {};
    }

    // 2) Real socket path — refuse if no socket is bound for this kind.
    const auto idx = socket_index( sock );
    if( !impl_->os_sockets[ idx ].valid() )
        return std::unexpected( NetError::NotInitialised );

    auto tx = impl_->sockets->sendto( impl_->os_sockets[ idx ], data, to );
    if( !tx )
        return std::unexpected( tx.error() );

    impl_->stats.packets_sent.fetch_add( 1, std::memory_order_relaxed );
    impl_->stats.bytes_sent.fetch_add( *tx, std::memory_order_relaxed );
    return {};
}

IProtocolDriver *NetworkContext::protocol_driver( std::uint16_t protocol ) noexcept
{
    if( !impl_ )
        return nullptr;

    // Same resolution the internal netchan path uses: the injected registry,
    // or the process-wide default (GoldSrc 48/49) when none was supplied.
    IProtocolDriverRegistry &reg = impl_->protocol_registry != nullptr
                                       ? *impl_->protocol_registry
                                       : default_protocol_driver_registry();
    return reg.resolve( protocol );
}

xash::memory::PoolHandle NetworkContext::fragment_pool() const noexcept
{
    return impl_ ? impl_->pool : xash::memory::PoolHandle{};
}

const NetworkingStats &NetworkContext::stats() const noexcept
{
    return impl_->stats;
}

} // namespace xash::networking
