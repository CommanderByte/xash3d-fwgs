// xash3dpp — master-server list satellite
// Boundary spec: docs/boundaries/networking-boundary.md §OQ-6
// Q-11 satellite placement: same target (lives in xash3dpp_networking).
//
// Legacy reference: engine/common/masterlist.c (NET_MasterHeartbeat /
// NET_MasterShutdown).  The wire format for a heartbeat is the two-byte
// OOB connectionless prefix 0xFF 0xFF 0xFF 0xFF followed by the GoldSrc
// challenge query `q\n`; for shutdown it is the same prefix followed by
// `b\n`.  We don't currently model the multi-step challenge handshake —
// the heartbeat sends the simple `q\n` form which the master server uses
// to ask the server to identify itself.  This matches the legacy minimum
// behaviour and is sufficient to register the server with public masters
// that drive the challenge from their side.

#include <xash3dpp/private/networking/master_list.hpp>
#include <xash3dpp/networking/networking.hpp>
#include <xash3dpp/core/log.hpp>

#include <array>
#include <cstddef>
#include <memory>

namespace xash::networking {

namespace {

// OOB connectionless prefix.  GoldSrc / Xash netchans tag every
// non-channel-framed packet with four leading 0xFF bytes so the receiver
// can distinguish it from the in-channel datagram stream.
inline constexpr std::array<std::byte, 4> k_oob_prefix{
    std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF } };

// Master-server commands (legacy masterlist.c).
inline constexpr std::array<std::byte, 6> k_heartbeat_packet{
    std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF },
    std::byte{ 'q'  }, std::byte{ '\n' } };

inline constexpr std::array<std::byte, 6> k_shutdown_packet{
    std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF },
    std::byte{ 'b'  }, std::byte{ '\n' } };

class MasterListClient final : public IMasterListClient
{
public:
    MasterListClient( NetworkContext &ctx, IMasterListConfig &cfg ) noexcept
        : ctx_( ctx ), cfg_( cfg ) {}

    void heartbeat() noexcept override
    {
        // LAN-only servers never advertise themselves on public masters.
        if( cfg_.lan_only() )
            return;

        const auto addrs = cfg_.master_addresses();
        if( addrs.empty() )
        {
            ::xash::core::log( ::xash::core::LogLevel::Verbose, "master_list",
                       "heartbeat: no master addresses configured, skipping" );
            return;
        }

        for( const auto &addr : addrs )
        {
            const auto r = ctx_.send_packet( SocketKind::Server,
                                             std::span<const std::byte>{ k_heartbeat_packet },
                                             addr );
            if( !r.has_value() )
            {
                ::xash::core::log( ::xash::core::LogLevel::Warning, "master_list",
                           "heartbeat: send_packet failed for one master "
                           "address (continuing)" );
            }
        }
    }

    void send_shutdown() noexcept override
    {
        if( cfg_.lan_only() )
            return;

        const auto addrs = cfg_.master_addresses();
        if( addrs.empty() )
            return;

        for( const auto &addr : addrs )
        {
            const auto r = ctx_.send_packet( SocketKind::Server,
                                             std::span<const std::byte>{ k_shutdown_packet },
                                             addr );
            if( !r.has_value() )
            {
                ::xash::core::log( ::xash::core::LogLevel::Warning, "master_list",
                           "send_shutdown: send_packet failed for one master "
                           "address (continuing)" );
            }
        }
    }

private:
    NetworkContext    &ctx_;
    IMasterListConfig &cfg_;
};

} // namespace

std::unique_ptr<IMasterListClient> create_master_list_client(
    NetworkContext    &ctx,
    IMasterListConfig &cfg ) noexcept
{
    // std::make_unique is unavailable under /EHs-c-; nothrow-new is fine
    // for a tiny POD-like state object.  Q-3 ownership: caller owns.
    return std::unique_ptr<IMasterListClient>( new( std::nothrow )
                                               MasterListClient( ctx, cfg ) );
}

} // namespace xash::networking
