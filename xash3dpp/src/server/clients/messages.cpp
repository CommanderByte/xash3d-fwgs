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

#include <xash3dpp/abi/server_consts.hpp>       // FEV_*, k_max_events, k_fl_ducking
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/networking/delta.hpp>         // DeltaTables::write_delta_event
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/private/server/entity_view.hpp> // invoker entvars (origin/angles/…)
#include <xash3dpp/private/server/snapshot.hpp>  // k_max_event_bits/queue (shared w/ emit_events)
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
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    cm.multicast.rebind( { cm.multicast_buf, k_max_multicast }, "multicast" );
    cm.multicast.reset();
    cm.reliable_datagram.rebind( { cm.reliable_datagram_buf, k_max_multicast },
                                 "reliable_datagram" );
    cm.reliable_datagram.reset();
    cm.datagram.rebind( { cm.datagram_buf, k_max_multicast }, "datagram" );
    cm.datagram.reset();
    cm.spec_datagram.rebind( { cm.spec_datagram_buf, k_max_multicast },
                             "spec_datagram" );
    cm.spec_datagram.reset();
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
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

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
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

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
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

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
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

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

// --- SV_PlaybackEventFull ----------------------------------------------------

namespace {

// SV_PlaybackReliableEvent (sv_game.c:1185): svc_event_reliable + the event
// index + an optional delay word + null-compressed args, staged straight into
// the client's reliable buffer (bypassing the unreliable event queue).  The
// frame loop drains cl.reliable into the netchan (the S8-seam staging model).
void playback_reliable_event( EngineBridge &bridge, ServerClient &cl,
                              std::uint16_t eventindex, float delay,
                              const ::xash::abi::event_args_t &args ) noexcept
{
    if ( bridge.delta == nullptr )
        return; // delta tables not wired yet (pre-load_progs fixtures)

    std::byte       scratch[64] = {};
    net::MessageBuf w( { scratch, sizeof( scratch ) } );

    w.write_byte( static_cast<std::uint8_t>( k_svc_event_reliable ) );
    w.write_ubit_long( eventindex, k_max_event_bits );

    if ( delay != 0.0f )
    {
        w.write_one_bit( 1 );
        w.write_word(
            static_cast<std::uint16_t>( static_cast<int>( delay * 100.0f ) ) );
    }
    else
    {
        w.write_one_bit( 0 );
    }

    // reliable events use plain null-compression (delta vs a zeroed args), not
    // the per-frame delta baseline the unreliable emit path uses.
    const ::xash::abi::event_args_t nullargs{};
    bridge.delta->write_delta_event( w, &nullargs, &args );

    stage_append( cl.reliable, k_client_stage_bytes, cl.reliable_bits, w.data(),
                  w.num_bits_written() );
}

} // namespace

void playback_event_full( EngineBridge &bridge, int flags,
                          const ::xash::abi::edict_t *invoker_raw,
                          std::uint16_t eventindex, float delay,
                          const float *origin, const float *angles,
                          float fparam1, float fparam2, int iparam1, int iparam2,
                          int bparam1, int bparam2 ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( ( flags & ::xash::abi::k_fev_client ) != 0 )
        return; // "someone stupid joke" — a client-only event fired on the server

    // No active server / precache table (pre-lifecycle fixtures): drop silently.
    if ( bridge.clients == nullptr || bridge.precache == nullptr )
        return;

    // out-of-bounds event index
    if ( eventindex < 1 || eventindex >= ::xash::abi::k_max_events )
    {
        bridge_error( bridge, "SV_PlaybackEvent: invalid eventindex\n" );
        return;
    }

    // event must be precached
    if ( bridge.precache->event_name( eventindex )[0] == '\0' )
    {
        bridge_error( bridge, "SV_PlaybackEvent: event was not precached\n" );
        return;
    }

    ::xash::abi::event_args_t args{};

    if ( origin != nullptr &&
         !( origin[0] == 0.0f && origin[1] == 0.0f && origin[2] == 0.0f ) )
    {
        args.origin[0] = origin[0];
        args.origin[1] = origin[1];
        args.origin[2] = origin[2];
        args.flags |= ::xash::abi::k_fevent_origin;
    }

    if ( angles != nullptr &&
         !( angles[0] == 0.0f && angles[1] == 0.0f && angles[2] == 0.0f ) )
    {
        args.angles[0] = angles[0];
        args.angles[1] = angles[1];
        args.angles[2] = angles[2];
        args.flags |= ::xash::abi::k_fevent_angles;
    }

    args.fparam1 = fparam1;
    args.fparam2 = fparam2;
    args.iparam1 = iparam1;
    args.iparam2 = iparam2;
    args.bparam1 = bparam1;
    args.bparam2 = bparam2;

    // The PVS point is the invoker eye (origin + view_ofs); with no invoker it
    // falls back to the supplied origin.  Only the FEV_GLOBAL null-origin guard
    // reads it here — the per-client PHS cull is stubbed (see below).
    float pvspoint[3] = { 0.0f, 0.0f, 0.0f };
    int   invoker_index;

    // Reading the invoker's entvars must go through the EntityView facade; the
    // ABI hands us a const edict but every access below is read-only.
    EntityView invoker( const_cast<::xash::abi::edict_t *>( invoker_raw ) );

    if ( invoker.valid() ) // SV_IsValidEdict
    {
        const auto io = invoker.origin();
        const auto vo = invoker.view_ofs();
        pvspoint[0] = io.x + vo.x;
        pvspoint[1] = io.y + vo.y;
        pvspoint[2] = io.z + vo.z;

        invoker_index =
            bridge.arena != nullptr ? bridge.arena->index_of( invoker_raw ) : 0;
        args.entindex = invoker_index;
        args.ducking  = ( invoker.flags() & ::xash::abi::k_fl_ducking ) ? 1 : 0;

        // origin/angles are transmitted only for reliable events; fill them from
        // the invoker when the caller did not state them.
        if ( ( args.flags & ::xash::abi::k_fevent_origin ) == 0 )
        {
            args.origin[0] = io.x;
            args.origin[1] = io.y;
            args.origin[2] = io.z;
        }
        if ( ( args.flags & ::xash::abi::k_fevent_angles ) == 0 )
        {
            const auto ia  = invoker.angles();
            args.angles[0] = ia.x;
            args.angles[1] = ia.y;
            args.angles[2] = ia.z;
        }
    }
    else
    {
        pvspoint[0]   = args.origin[0];
        pvspoint[1]   = args.origin[1];
        pvspoint[2]   = args.origin[2];
        args.entindex = 0;
        invoker_index = -1;
    }

    if ( ( flags & ::xash::abi::k_fev_global ) == 0 && pvspoint[0] == 0.0f &&
         pvspoint[1] == 0.0f && pvspoint[2] == 0.0f )
        return; // a non-global event with no origin — ignored

    // FEV_NOTHOST/FEV_HOSTONLY only make sense when the invoker is a spawned
    // client; legacy clears them (with a warning) otherwise.
    if ( ( flags & ( ::xash::abi::k_fev_nothost | ::xash::abi::k_fev_hostonly ) ) !=
         0 )
    {
        const ServerClient *icl = client_for_edict( *bridge.clients, invoker_raw );
        if ( icl == nullptr || icl->state != ClientState::Spawned )
        {
            flags &= ~::xash::abi::k_fev_nothost;
            flags &= ~::xash::abi::k_fev_hostonly;
        }
    }

    flags |= ::xash::abi::k_fev_server; // it's a server event
    if ( delay < 0.0f )
        delay = 0.0f; // fixup negative delays

    // XASH3DPP-STUB(S8-seam): the recipient PHS cull — Mod_FatPVS(FATPHS) +
    // SV_CheckClientVisiblity, plus the groupinfo/groupop group-mask filter —
    // needs the per-client view leaf the frame loop will cache each tick.  Until
    // then, exactly like SV_Multicast (messages.cpp), events fan to every
    // eligible (spawned, non-fake) client (legacy "NULL mask → visible").  Both
    // paths ride the same future snapshot/PVS gate.

    ClientMachinery &cm = *bridge.clients;
    for ( int slot = 0; slot < cm.maxclients; ++slot )
    {
        ServerClient &cl = cm.clients[slot];

        if ( cl.state != ClientState::Spawned || cl.edict == nullptr ||
             cl.fakeclient )
            continue;

        // FEV_NOTHOST: skip the invoker's own client when it predicts weapons
        // locally.  Legacy also matches sv.current_client, which is not tracked
        // yet (engine_table.cpp:1873) — the edict==invoker half is the case that
        // matters for a listen host and is well-defined here.
        if ( ( flags & ::xash::abi::k_fev_nothost ) != 0 &&
             cl.edict == invoker_raw && cl.local_weapons )
            continue; // will be played on the client side

        if ( ( flags & ::xash::abi::k_fev_hostonly ) != 0 && cl.edict != invoker_raw )
            continue; // send only to the invoker

        // reliable event: skip the queue, stage it directly
        if ( ( flags & ::xash::abi::k_fev_reliable ) != 0 )
        {
            playback_reliable_event( bridge, cl, eventindex, delay, args );
            continue;
        }

        // unreliable event: store in the per-client queue (drained by emit_events)
        ::xash::abi::event_state_t &es       = cl.events;
        int                         bestslot = -1;

        if ( ( flags & ::xash::abi::k_fev_update ) != 0 )
        {
            for ( int j = 0; j < k_max_event_queue; ++j )
            {
                if ( es.ei[j].index == eventindex && invoker_index != -1 &&
                     invoker_index == es.ei[j].entity_index )
                {
                    bestslot = j;
                    break;
                }
            }
        }

        if ( bestslot == -1 )
        {
            for ( int j = 0; j < k_max_event_queue; ++j )
            {
                if ( es.ei[j].index == 0 )
                {
                    bestslot = j;
                    break;
                }
            }
        }

        if ( bestslot == -1 )
            continue; // queue full for this client

        ::xash::abi::event_info_t &ei = es.ei[bestslot];
        ei.index        = eventindex;
        ei.fire_time    = delay;
        ei.entity_index = static_cast<std::int16_t>( invoker_index );
        ei.packet_index = -1;
        ei.flags        = flags;
        ei.args         = args;
    }
}

} // namespace xash::server
