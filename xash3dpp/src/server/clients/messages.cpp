// xash3dpp — user-message registry + multicast message pipeline (S9)
// Legacy reference: engine/server/sv_game.c — SV_RegUserMsg (:3480),
// pfnMessageBegin (:2534), pfnMessageEnd (:2620), the pfnWrite* family
// (:2734+), SV_Multicast (:354).
// Deep dive: docs/legacy-survey/deep-dive-server-clients.md §3 / §11.
//
// The message state machine writes into the sv.multicast scratch buffer; on
// pfnMessageEnd the size word is back-patched (variable messages) or the
// fixed-size mismatch is reported (legacy S_ERROR, no Host_Error), then
// SV_Multicast fans the payload out to the recipient set and clears the
// scratch.  Per-client delivery stages into ServerClient::reliable/datagram;
// S8's frame loop drains those into the netchan.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstring>

namespace xash::server {

namespace {

namespace net = ::xash::networking;

void bridge_error( EngineBridge &bridge, const char *msg ) noexcept
{
    if ( bridge.host_error != nullptr )
        bridge.host_error( bridge.host_error_ctx, msg );
    else
        ::xash::core::log_error( "server", msg );
}

// Append `bits` from `src` to a client's staging buffer at its cursor.
void stage_append( std::byte *buf, std::size_t cap_bytes, std::size_t &cursor_bits,
                   std::span<const std::byte> src, std::size_t bits ) noexcept
{
    net::MessageBuf w( { buf, cap_bytes } );
    if ( !w.seek_to_bit( static_cast<std::ptrdiff_t>( cursor_bits ),
                         net::SeekOrigin::Begin ) )
        return;
    if ( !w.write_bits( src, bits ) )
        return; // overflow: drop (legacy would overflow the netchan buffer)
    cursor_bits = w.tell_bit();
}

} // namespace

void clients_init( ClientMachinery &cm ) noexcept
{
    cm.multicast.rebind( { cm.multicast_buf, k_max_multicast }, "multicast" );
    cm.multicast.reset();
    for ( ServerClient &cl : cm.clients )
    {
        cl.reliable_bits = 0;
        cl.datagram_bits = 0;
    }
    cm.challenge_salt_seeded = 1;
}

// --- registry ---------------------------------------------------------------

int UserMessageRegistry::register_message( const char *name, int size ) noexcept
{
    if ( name == nullptr || name[0] == '\0' )
        return k_svc_bad;
    if ( ::xash::utilities::strlen( name ) >= sizeof( msgs[0].name ) )
        return k_svc_bad; // too long name
    if ( size > k_max_usermsg_length )
        return k_svc_bad;
    if ( size < -1 )
        size = -1; // bound(-1, size, MAX)

    // message 0 reserved for svc_bad; scan for an existing registration.
    int i = 1;
    for ( ; i < k_max_user_messages && msgs[i].name[0] != '\0'; ++i )
    {
        if ( ::xash::utilities::strcmp( msgs[i].name, name ) == 0 )
            return msgs[i].number; // already registered
    }

    if ( i == k_max_user_messages )
        return k_svc_bad; // limit exceeded

    ::xash::utilities::strncpy( msgs[i].name, name, sizeof( msgs[i].name ) );
    msgs[i].number = k_svc_lastmsg + i;
    msgs[i].size   = size;
    return msgs[i].number;
}

int UserMessageRegistry::slot_for_number( int number ) const noexcept
{
    for ( int i = 1; i < k_max_user_messages && msgs[i].name[0] != '\0'; ++i )
    {
        if ( msgs[i].number == number )
            return i;
    }
    return 0;
}

int reg_user_msg( EngineBridge &bridge, const char *name, int size ) noexcept
{
    if ( bridge.clients == nullptr )
        return k_svc_bad;

    const int number = bridge.clients->user_messages.register_message( name, size );

    // XASH3DPP-STUB(S8-seam): when sv.state == ss_active legacy immediately
    // broadcasts the new registration (SV_SendUserReg + SV_Multicast MSG_ALL,
    // sv_game.c:3529-3534).  Mid-game user-message registration is rare; the
    // signon path re-sends every registration at "new".  Wire the live
    // broadcast when the frame loop drives active-server sends.
    return number;
}

// --- message write pipeline -------------------------------------------------

void message_begin( EngineBridge &bridge, int dest, int num,
                    const float *origin, ::xash::abi::edict_t *ent ) noexcept
{
    if ( bridge.clients == nullptr )
        return;
    ClientMachinery &cm = *bridge.clients;

    if ( cm.message.started )
    {
        bridge_error( bridge, "MessageBegin: new message started when the "
                              "previous has not been sent yet\n" );
        return;
    }
    cm.message.started = true;

    // check range: bound( svc_bad, num, 255 )
    if ( num < k_svc_bad )
        num = k_svc_bad;
    if ( num > 255 )
        num = 255;

    int isize = 0;
    if ( num <= k_svc_lastmsg )
    {
        cm.message.index = -num;   // system message
        cm.message.name  = "system";
        isize            = ( num == k_svc_temp_entity ) ? -1 : 0;
    }
    else
    {
        const int slot = cm.user_messages.slot_for_number( num );
        if ( slot == 0 )
        {
            bridge_error( bridge,
                          "MessageBegin: tried to send unregistered message\n" );
            cm.message.started = false;
            return;
        }
        cm.message.name  = cm.user_messages.msgs[slot].name;
        isize            = cm.user_messages.msgs[slot].size;
        cm.message.index = slot;
    }

    cm.multicast.write_byte( static_cast<std::uint8_t>( num ) ); // MSG_WriteCmdExt

    if ( origin != nullptr )
    {
        cm.message.org[0] = origin[0];
        cm.message.org[1] = origin[1];
        cm.message.org[2] = origin[2];
    }
    else
    {
        cm.message.org[0] = cm.message.org[1] = cm.message.org[2] = 0.0f;
    }

    if ( isize == -1 )
    {
        // variable sized: reserve a word for the size, patched at MessageEnd.
        cm.message.size_index =
            static_cast<int>( cm.multicast.num_bytes_written() );
        cm.multicast.write_word( 0 );
    }
    else
    {
        cm.message.size_index = -1;
    }

    cm.message.realsize = 0;
    cm.message.dest     = dest;
    cm.message.ent      = ent;
}

void message_end( EngineBridge &bridge ) noexcept
{
    if ( bridge.clients == nullptr )
        return;
    ClientMachinery &cm = *bridge.clients;

    const char *name = cm.message.name != nullptr ? cm.message.name : "Unknown";

    if ( !cm.message.started )
    {
        bridge_error( bridge, "MessageEnd: called with no active message\n" );
        return;
    }
    cm.message.started = false;

    if ( cm.multicast.overflowed() )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                            "MessageEnd: %s overflowed the multicast buffer",
                            name );
        cm.multicast.reset();
        return;
    }

    const int realsize = cm.message.realsize;

    if ( cm.message.index < 0 )
    {
        if ( cm.message.size_index != -1 )
        {
            if ( realsize > k_max_usermsg_length || realsize < 0 )
            {
                ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                                    "MessageEnd: %s bad size %i", name, realsize );
                cm.multicast.reset();
                return;
            }
            cm.multicast_buf[cm.message.size_index] =
                static_cast<std::byte>( realsize & 0xFF );
            cm.multicast_buf[cm.message.size_index + 1] =
                static_cast<std::byte>( ( realsize >> 8 ) & 0xFF );
        }
    }
    else if ( cm.user_messages.msgs[cm.message.index].size != -1 )
    {
        const int expsize = cm.user_messages.msgs[cm.message.index].size;
        if ( expsize != realsize )
        {
            // legacy: S_ERROR + drop the message (NOT Host_Error).
            ::xash::core::logf(
                ::xash::core::LogLevel::Error, "server",
                "MessageEnd: %s expected %i bytes, it written %i. Ignored.",
                name, expsize, realsize );
            cm.multicast.reset();
            return;
        }
    }
    else if ( cm.message.size_index != -1 )
    {
        if ( realsize > k_max_usermsg_length || realsize < 0 )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                                "MessageEnd: %s bad size %i", name, realsize );
            cm.multicast.reset();
            return;
        }
        cm.multicast_buf[cm.message.size_index] =
            static_cast<std::byte>( realsize & 0xFF );
        cm.multicast_buf[cm.message.size_index + 1] =
            static_cast<std::byte>( ( realsize >> 8 ) & 0xFF );
    }
    else
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                            "MessageEnd: %s encountered an error", name );
        cm.multicast.reset();
        return;
    }

    // clamp dest into [MSG_BROADCAST, MSG_SPEC]
    int dest = cm.message.dest;
    if ( dest < k_msg_broadcast )
        dest = k_msg_broadcast;
    if ( dest > k_msg_spec )
        dest = k_msg_spec;

    const bool have_org = cm.message.org[0] != 0.0f ||
                          cm.message.org[1] != 0.0f || cm.message.org[2] != 0.0f;

    sv_multicast( bridge, dest, have_org ? cm.message.org : nullptr,
                  cm.message.ent, true, false );
}

// --- pfnWrite* --------------------------------------------------------------

void message_write_byte( ClientMachinery &cm, int v ) noexcept
{
    cm.multicast.write_byte( static_cast<std::uint8_t>( v & 0xFF ) );
    cm.message.realsize += 1;
}
void message_write_char( ClientMachinery &cm, int v ) noexcept
{
    cm.multicast.write_char( static_cast<std::int8_t>( v ) );
    cm.message.realsize += 1;
}
void message_write_short( ClientMachinery &cm, int v ) noexcept
{
    cm.multicast.write_short( static_cast<std::int16_t>( v ) );
    cm.message.realsize += 2;
}
void message_write_long( ClientMachinery &cm, int v ) noexcept
{
    cm.multicast.write_long( static_cast<std::int32_t>( v ) );
    cm.message.realsize += 4;
}
void message_write_angle( ClientMachinery &cm, float v ) noexcept
{
    cm.multicast.write_bit_angle( v, 8 ); // 8-bit angle, byte-aligned
    cm.message.realsize += 1;
}
void message_write_coord( ClientMachinery &cm, float v ) noexcept
{
    cm.multicast.write_coord( v ); // 16-bit fixed
    cm.message.realsize += 2;
}
void message_write_string( ClientMachinery &cm, const char *s ) noexcept
{
    const char *p = s != nullptr ? s : "";
    ( void )cm.multicast.write_string( p );
    cm.message.realsize +=
        static_cast<int>( ::xash::utilities::strlen( p ) ) + 1;
}
void message_write_entity( ClientMachinery &cm, int v ) noexcept
{
    cm.multicast.write_short( static_cast<std::int16_t>( v ) );
    cm.message.realsize += 2;
}

// --- SV_Multicast -----------------------------------------------------------

int sv_multicast( EngineBridge &bridge, int dest, const float *origin,
                  ::xash::abi::edict_t *ent, bool usermessage,
                  bool /*filter*/ ) noexcept
{
    if ( bridge.clients == nullptr )
        return 0;
    ClientMachinery &cm = *bridge.clients;

    // some mods try to send after the server dies (ss_dead == 0).
    if ( bridge.server_state == 0 )
    {
        cm.multicast.reset();
        return 0;
    }

    bool reliable  = false;
    bool specproxy = false;
    int  first     = 0;
    int  count     = cm.maxclients;

    switch ( dest )
    {
    case k_msg_init:
        // XASH3DPP-STUB(S8-seam): during ss_loading MSG_INIT copies into the
        // signon buffer (sv_game.c:372-379); the signon assembly rides with
        // the spawn/"new" send path.  In-game it falls through to MSG_ALL.
        reliable = true;
        break;
    case k_msg_all:
        reliable = true;
        break;
    case k_msg_broadcast:
        break;
    case k_msg_pas_r:
        reliable = true;
        [[fallthrough]];
    case k_msg_pas:
        if ( origin == nullptr )
        {
            cm.multicast.reset();
            return 0;
        }
        break;
    case k_msg_pvs_r:
        reliable = true;
        [[fallthrough]];
    case k_msg_pvs:
        if ( origin == nullptr )
        {
            cm.multicast.reset();
            return 0;
        }
        break;
    case k_msg_one:
        reliable = true;
        [[fallthrough]];
    case k_msg_one_unreliable:
    {
        if ( ent == nullptr || bridge.arena == nullptr )
        {
            cm.multicast.reset();
            return 0;
        }
        const int j = bridge.arena->index_of( ent );
        if ( j < 1 || j > cm.maxclients )
        {
            cm.multicast.reset();
            return 0;
        }
        first = j - 1;
        count = 1;
        break;
    }
    case k_msg_spec:
        specproxy = reliable = true;
        break;
    default:
        bridge_error( bridge, "SV_Multicast: bad dest\n" );
        cm.multicast.reset();
        return 0;
    }

    // XASH3DPP-STUB(S8-seam): PVS/PAS mask visibility (SV_CheckClientVisiblity
    // against a fat-PVS/PHS mask, and the sv.current_client predict filter)
    // needs the per-client view leaf the snapshot pipeline caches each frame.
    // Until the frame loop caches it, mask dests fan out to every eligible
    // client (the legacy "NULL mask → visible" rule) — a parity approximation
    // flagged for the orchestrator's snapshot/PVS gate.

    const std::span<const std::byte> src = cm.multicast.data();
    const std::size_t                bits = cm.multicast.num_bits_written();

    int numsends = 0;
    for ( int j = 0; j < count; ++j )
    {
        const int    slot = first + j;
        ServerClient &cl   = cm.clients[slot];

        if ( cl.state == ClientState::Free || cl.state == ClientState::Zombie )
            continue;
        if ( cl.state != ClientState::Spawned && ( !reliable || usermessage ) )
            continue;
        if ( specproxy && !cl.hltv )
            continue;
        if ( cl.edict == nullptr || cl.fakeclient )
            continue;

        if ( reliable )
            stage_append( cl.reliable, k_client_stage_bytes, cl.reliable_bits,
                          src, bits );
        else
            stage_append( cl.datagram, k_client_stage_bytes, cl.datagram_bits,
                          src, bits );
        ++numsends;
    }

    cm.multicast.reset();
    return numsends;
}

} // namespace xash::server
