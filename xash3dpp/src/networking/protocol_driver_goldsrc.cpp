// xash3dpp — default GoldSrc / Xash protocol driver
// Boundary spec: docs/boundaries/networking-boundary.md
//                §"Pluggable game protocol per client"
//
// The GoldSrc driver is always linked.  Alternative drivers are added via
// IProtocolDriverRegistry per the Q-12 per-subsystem compat paradigm.
//
// This TU also exposes the built-in default registry accessor
// (default_protocol_driver_registry()) that NetworkContext falls back to
// when NetworkInitParams::protocol_registry is nullptr.

#include <xash3dpp/private/networking/protocol_driver.hpp>
#include <xash3dpp/private/networking/protocol_driver_default.hpp>

#include <xash3dpp/networking/message_buf.hpp>

namespace xash::networking {

namespace {

// Bit-31 sentinel in w1 / w2.  Used by both the writer and the reader so the
// two sides stay in sync with the legacy on-wire layout.
inline constexpr std::uint32_t k_reliable_bit = 0x80000000u;
// Bit-30 sentinel in w1, set only when at least one reliable fragment is
// being shipped alongside the reliable payload.
inline constexpr std::uint32_t k_reliable_fragment_bit = 0x40000000u;

class GoldSrcProtocolDriver : public IProtocolDriver
{
public:
    [[nodiscard]] const char *name() const noexcept override { return "goldsrc"; }

    [[nodiscard]] SplitFormat   split_format() const noexcept override { return SplitFormat::GoldSrc; }
    [[nodiscard]] DeltaTableSet delta_tables() const noexcept override { return DeltaTableSet::GoldSrc; }

    // Legacy `chan->gs_netchan == true` path: no qport word in the client
    // header.  XashProtocolDriver (below) overrides this to return true.
    [[nodiscard]] bool sends_qport() const noexcept override { return false; }

    [[nodiscard]] Result<void> write_packet_header(
        MessageBuf &out,
        const PacketHeaderInput &in ) noexcept override
    {
        std::uint32_t w1 = in.outgoing_sequence;
        std::uint32_t w2 = in.incoming_sequence;
        if( in.send_reliable )                                w1 |= k_reliable_bit;
        if( in.send_reliable && in.send_reliable_fragment )   w1 |= k_reliable_fragment_bit;
        if( in.incoming_reliable_sequence & 1u )              w2 |= k_reliable_bit;

        out.write_dword( w1 );
        out.write_dword( w2 );

        if( sends_qport() && in.is_client )
            out.write_word( in.qport );

        // Reliable-fragment block descriptors (per-stream fragid / start /
        // length / terminator) are written by Netchan::transmit() directly
        // after this call when in.send_reliable_fragment is true.  Keeping
        // the descriptor pass in the channel keeps the driver header-only
        // and lets the channel walk its outgoing_fragments deques without
        // exposing them through the IProtocolDriver seam.

        if( out.overflowed() )
            return std::unexpected( NetError::Overflow );
        return {};
    }

    [[nodiscard]] Result<FrameMeta> read_packet_header(
        MessageBuf &in, bool is_server_socket ) noexcept override
    {
        if( in.num_bytes_left() < 8u )
            return std::unexpected( NetError::BufferTooSmall );

        const std::uint32_t w1 = in.read_dword();
        const std::uint32_t w2 = in.read_dword();
        if( in.overflowed() )
            return std::unexpected( NetError::BufferTooSmall );

        FrameMeta meta;
        meta.sequence       = w1 & ~( k_reliable_bit | k_reliable_fragment_bit );
        meta.sequence_ack   = w2 & ~k_reliable_bit;
        meta.is_reliable    = ( w1 & k_reliable_bit ) != 0u;
        meta.reliable_ack   = ( w2 & k_reliable_bit ) != 0u;
        meta.is_fragment    = ( w1 & k_reliable_fragment_bit ) != 0u;
        meta.is_split       = false; // split discriminator lives one layer up
        meta.is_oob         = false;

        // Qport is present only in client→server datagrams.  Consume it
        // when we are the server reading a client packet; skip on the
        // client reading a server packet (no qport was written).
        if( sends_qport() && is_server_socket )
        {
            if( in.num_bytes_left() < 2u )
                return std::unexpected( NetError::BufferTooSmall );
            (void) in.read_word();
        }
        return meta;
    }
};

// XashProtocolDriver — standard Xash netchan (protocol 49, gs_netchan=false).
// The wire codec (w1/w2 LE words, reliable bits) is identical to GoldSrc;
// only the metadata and the client-side qport word differ.
// See legacy net_chan.c L1691-1694 for the qport conditional.
class XashProtocolDriver final : public GoldSrcProtocolDriver
{
public:
    [[nodiscard]] const char   *name()         const noexcept override { return "xash"; }
    [[nodiscard]] SplitFormat   split_format() const noexcept override { return SplitFormat::Xash; }
    [[nodiscard]] DeltaTableSet delta_tables() const noexcept override { return DeltaTableSet::Xash; }

    // Standard Xash clients append a 2-byte qport word after w2 so the
    // server can demux multiple clients sharing an IP address behind a NAT.
    [[nodiscard]] bool sends_qport() const noexcept override { return true; }
};

// Wire-protocol identifiers recognised by the built-in registry.
inline constexpr std::uint16_t k_protocol_goldsrc = 48;
inline constexpr std::uint16_t k_protocol_xash    = 49;

class DefaultProtocolDriverRegistry final : public IProtocolDriverRegistry
{
public:
    [[nodiscard]] IProtocolDriver *resolve( std::uint16_t protocol ) noexcept override
    {
        if( protocol == k_protocol_goldsrc ) return &goldsrc_driver_;
        if( protocol == k_protocol_xash    ) return &xash_driver_;
        return nullptr;
    }

private:
    GoldSrcProtocolDriver goldsrc_driver_ {};
    XashProtocolDriver    xash_driver_    {};
};

} // namespace

IProtocolDriverRegistry &default_protocol_driver_registry() noexcept
{
    // Meyers singleton: thread-safe init under C++23, lazily constructed on
    // first use, never destroyed during normal teardown so callers holding
    // a reference past shutdown() do not dangle.
    static DefaultProtocolDriverRegistry instance;
    return instance;
}

} // namespace xash::networking
