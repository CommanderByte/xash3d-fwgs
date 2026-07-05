// xash3dpp — server ↔ networking I/O bridge (Chunk 6 S9 seam)
// Legacy reference: engine/server/sv_main.c SV_ReadPackets (:375) +
// engine/common/net_chan.c Netchan_OutOfBand / Netchan_OutOfBandPrint.
//
// Ownership (decided — decisions-architecture.md §ENGINE_CONTEXT, Q-2/Q-4):
// the host owns the single NetworkContext and its UDP sockets (EngineContext
// declares `networking` ahead of `server`).  The server holds a NON-OWNING
// handle (rt.net) and, each frame, drains its server socket here.  This is
// the "host-routes" model — the server never opens a socket.
//
// Connectionless (OOB, leading -1) datagrams route into the connection state
// machine (handle_connectionless); its OOB replies go back out through a
// NetworkContext-backed IOobSink (Netchan_OutOfBandPrint).  In-session netchan
// traffic (clc_move / clc_stringcmd / …) is the Slice-C seam — it needs the
// per-client Netchan (Slice B) that this bridge will feed next.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/networking/networking.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

namespace xash::server {

namespace {

namespace net = ::xash::networking;

// NET_MAX_MESSAGE-class inbound buffer (net_ws.h): one datagram per get_packet.
inline constexpr std::size_t k_net_max_message = 65536;

// OOB reply staging (Netchan_OutOfBand): 4-byte -1 magic + ASCII body.  Every
// connectionless reply the server emits (challenge / client_connect / the
// three-packet reject) is well under this; a longer body is clamped.
inline constexpr std::size_t k_oob_reply_max = 2048;

// Leading -1 dword ⇒ connectionless packet (the Netchan OOB magic word).
[[nodiscard]] bool is_oob( std::span<const std::byte> pkt ) noexcept
{
    return pkt.size() >= 4 && pkt[0] == std::byte{ 0xFF } &&
           pkt[1] == std::byte{ 0xFF } && pkt[2] == std::byte{ 0xFF } &&
           pkt[3] == std::byte{ 0xFF };
}

// Netchan_OutOfBandPrint over the shared NetworkContext: prepend the -1 magic
// word to the reply text and send it on the server socket.  A send failure
// (closed socket / would-block) is swallowed, matching NET_SendPacket.
class ContextOobSink final : public IOobSink
{
public:
    explicit ContextOobSink( net::NetworkContext &ctx ) noexcept : ctx_{ ctx } {}

    void send_oob( const net::NetAddress &to, const char *text ) noexcept override
    {
        ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

        if ( text == nullptr )
            text = "";

        std::array<std::byte, k_oob_reply_max> buf{};
        buf[0] = std::byte{ 0xFF };
        buf[1] = std::byte{ 0xFF };
        buf[2] = std::byte{ 0xFF };
        buf[3] = std::byte{ 0xFF };

        std::size_t len = std::strlen( text );
        if ( len > k_oob_reply_max - 4 )
            len = k_oob_reply_max - 4;
        std::memcpy( buf.data() + 4, text, len );

        ( void )ctx_.send_packet(
            net::SocketKind::Server,
            std::span<const std::byte>{ buf.data(), 4 + len }, to );
    }

private:
    net::NetworkContext &ctx_;
};

} // namespace

void read_packets( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.net == nullptr )
        return; // offline server (scaffold + unit fixtures) — no packet I/O.

    ContextOobSink sink{ *rt.net };

    // Drain the server socket for this frame (SV_ReadPackets loop): get_packet
    // returns an error once the loopback ring + OS socket are empty.
    std::array<std::byte, k_net_max_message> buf;
    net::MessageBuf in_msg; // process() rebinds this to view each datagram
    for ( ;; )
    {
        net::NetAddress from{};
        const auto r = rt.net->get_packet( net::SocketKind::Server, from,
                                           std::span<std::byte>{ buf } );
        if ( !r )
            break;

        const std::span<const std::byte> pkt{ buf.data(), *r };

        if ( is_oob( pkt ) )
        {
            // NUL-terminate the ASCII body past the -1 magic and hand it to the
            // connection state machine (which tokenises from the first token).
            char              text[k_oob_reply_max];
            const std::size_t body = pkt.size() - 4;
            const std::size_t n =
                body < sizeof( text ) - 1 ? body : sizeof( text ) - 1;
            std::memcpy( text, pkt.data() + 4, n );
            text[n] = '\0';
            ( void )handle_connectionless( rt, from, text, sink );
            continue;
        }

        // In-session (sequenced) datagram: match a connected client by source
        // address, demux through its netchan, then run the client-message
        // opcode stream (SV_ReadPackets, sv_main.c:375).
        ClientMachinery &cm   = rt.clients;
        ServerClient    *cl   = nullptr;
        int              slot = -1;
        for ( int i = 0; i < cm.maxclients; ++i )
        {
            ServerClient &c = cm.clients[i];
            if ( c.state == ClientState::Free || c.fakeclient )
                continue;
            if ( !net::compare_base( from, c.adr ) )
                continue;
            cl   = &c;
            slot = i;
            break;
        }
        if ( cl == nullptr )
            continue; // unknown peer — drop

        // NAT routers rewrite the source port between datagrams; adopt the new
        // one so the send path targets the live endpoint.
        if ( cl->adr.port != from.port )
            cl->adr.port = from.port;

        // Netchan_Process demuxes the payload into in_msg (rebound to view the
        // datagram, positioned after the header) and advances ack/sequence
        // state; a stale / duplicate / malformed packet returns false and is
        // silently dropped.
        if ( cm.netchans[slot].process( pkt, in_msg ) )
        {
            // Mirror the netchan receive state onto the client for the snapshot
            // + timeout paths (incoming_acknowledged drives SV_CalcPing;
            // last_received drives SV_CheckTimeouts).
            cl->incoming_acknowledged =
                static_cast<int>( cm.netchans[slot].incoming_acknowledged() );
            cl->last_received = cm.netchans[slot].last_received();

            // FCL_SEND_NET_MESSAGE (SV_ReadPackets): reply at end of frame for
            // the single-player/local client or any non-spawned client; spawned
            // MP clients ride the next_messagetime send-rate gate instead.
            if ( cm.maxclients == 1 || cl->state != ClientState::Spawned )
                cl->send_net_message = true;

            if ( cl->frames != nullptr && cl->state != ClientState::Zombie )
                execute_client_message( rt, *cl, in_msg, sink );
        }

        // XASH3DPP-STUB(chunk6-S9): Netchan_CopyNormalFragments / CopyFileFragments
        // reassembly (large reliable messages + file-upload downloads = OQ-8 trims).
    }
}

} // namespace xash::server
