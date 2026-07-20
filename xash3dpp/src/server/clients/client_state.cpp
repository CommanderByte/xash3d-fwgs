// xash3dpp — client connection state machine (Chunk 6 S9)
// Legacy reference: engine/server/sv_client.c — SV_GetChallenge (:73),
// SV_CheckChallenge (:214), SV_ConnectClient (:295), SV_FakeConnect (:474),
// SV_ExecuteClientCommand (:3103) + the new/spawn/begin ucmds, SV_UserinfoChanged
// (:1805), SV_DropClient (:577); SV_CheckTimeouts (sv_main.c:489).
// Deep dive: docs/legacy-survey/deep-dive-server-clients.md §2.
//
// Scope note (S9): this owns the connection STATE MACHINE + the game-DLL
// client callbacks (ClientConnect / PutInServer / Command / UserInfoChanged /
// Disconnect).  The signon/serverdata payload build ("new" sends serverdata +
// deltadesc + movevars + user-msg regs + lightstyles; "spawn" sends the
// verbatim signon buffer + setview + signonnum) and the netchan transmit are
// S8/send seams — marked inline — because they are what S8's frame loop drives.
//
// Q-20: edict fields go through EntityView; no raw ->v. here.
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/private/server/info_string.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/pmove.hpp> // sv_run_cmd (SV_ParseClientMove)
#include <xash3dpp/private/server/string_pool.hpp>
#include <xash3dpp/utilities/hash.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

namespace xash::server {

namespace {

namespace ut  = ::xash::utilities;
namespace net = ::xash::networking;

// timeout constants (sv_main.c:44-47).
constexpr double k_sv_timeout         = 65.0; // spawned clients
constexpr double k_sv_connect_timeout = 60.0; // connecting clients

// A V4 loopback (127.0.0.1) stands in for the legacy NA_LOOPBACK type; the
// challenge for it is a constant 0 (SV_GetChallenge NA_LOOPBACK branch).
[[nodiscard]] bool is_loopback( const net::NetAddress &a ) noexcept
{
    return a.family == net::IpFamily::V4 && a.addr.v4[0] == 127 &&
           a.addr.v4[1] == 0 && a.addr.v4[2] == 0 && a.addr.v4[3] == 1;
}

void addr_string( const net::NetAddress &a, char *out, std::size_t n ) noexcept
{
    const auto r = net::to_string( a, { out, n } );
    if ( !r )
        ut::strncpy( out, "loopback", n );
}

void reject_connection( IOobSink &sink, const net::NetAddress &from,
                        const char *reason ) noexcept
{
    // SV_RejectConnection sends three OOB packets: errormsg, print, disconnect.
    char text[1024];
    ut::snprintf( text, sizeof( text ),
                  "errormsg\n^1Server rejected the connection:^7 %s", reason );
    sink.send_oob( from, text );
    ut::snprintf( text, sizeof( text ),
                  "print\n^1Server rejected the connection:^7 %s", reason );
    sink.send_oob( from, text );
    sink.send_oob( from, "disconnect\n" );
}

[[nodiscard]] ServerClient *find_empty_slot( ClientMachinery &cm ) noexcept
{
    for ( int i = 0; i < cm.maxclients; ++i )
        if ( cm.clients[i].state == ClientState::Free )
            return &cm.clients[i];
    return nullptr;
}

// name is spawned elsewhere; dedupe helper checks the spawned set.
[[nodiscard]] bool name_in_use( ClientMachinery &cm, const ServerClient &self,
                                const char *name ) noexcept
{
    for ( int i = 0; i < cm.maxclients; ++i )
    {
        const ServerClient &cl = cm.clients[i];
        if ( &cl == &self || cl.state != ClientState::Spawned )
            continue;
        if ( ut::strcmp( cl.name, name ) == 0 )
            return true;
    }
    return false;
}

} // namespace

// --- challenge --------------------------------------------------------------

std::int32_t compute_challenge( const std::uint32_t salt[16],
                                net::NetAddress from,
                                std::uint32_t time_window ) noexcept
{
    if ( is_loopback( from ) )
        return 0;

    ut::Md5State ctx;
    ut::md5_init( ctx );

    if ( from.family == net::IpFamily::V6 )
        ut::md5_update( ctx, from.addr.v6, 16 );
    else
        ut::md5_update( ctx, from.addr.v4, 4 );

    ut::md5_update( ctx, salt, sizeof( std::uint32_t ) * 16 );
    ut::md5_update( ctx, &time_window, sizeof( time_window ) );

    const std::array<std::uint8_t, 16> d = ut::md5_final( ctx );
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>( d[0] ) |
        ( static_cast<std::uint32_t>( d[1] ) << 8 ) |
        ( static_cast<std::uint32_t>( d[2] ) << 16 ) |
        ( static_cast<std::uint32_t>( d[3] ) << 24 ) );
}

bool check_challenge( const ServerRuntime &rt, net::NetAddress from,
                      std::int32_t challenge ) noexcept
{
    const std::uint32_t window =
        static_cast<std::uint32_t>( rt.clients.realtime / k_challenge_window_s );

    if ( compute_challenge( rt.persistent.challenge_salt, from, window ) ==
         challenge )
        return true;
    if ( compute_challenge( rt.persistent.challenge_salt, from, window - 1 ) ==
         challenge )
        return true;
    return false;
}

// --- connectionless dispatch ------------------------------------------------

bool handle_connectionless( ServerRuntime &rt, net::NetAddress from,
                            const char *text, IOobSink &sink ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( text == nullptr )
        return false;

    ut::Tokenizer tok( text );
    const auto    cmd = tok.next();
    if ( !cmd )
        return false;

    const std::string_view c = cmd->text;

    if ( c == "getchallenge" )
    {
        const std::uint32_t window = static_cast<std::uint32_t>(
            rt.clients.realtime / k_challenge_window_s );
        const std::int32_t chal =
            compute_challenge( rt.persistent.challenge_salt, from, window );
        char reply[64];
        // second field is 1 iff a bandwidth testpacket is offered — the
        // testpacket is an OQ-8 trim, so always 0.
        ut::snprintf( reply, sizeof( reply ), "challenge %d 0", chal );
        sink.send_oob( from, reply );
        return true;
    }

    if ( c == "connect" )
    {
        // connect <ver> <challenge> <protinfo> <userinfo>.  Each Token text is
        // only valid until the next tok.next(), so extract as we go.
        const auto a1 = tok.next();
        if ( !a1 )
        {
            reject_connection( sink, from, "insufficient connection info\n" );
            return true;
        }
        const int protocol = ut::atoi( a1->text );

        const auto a2 = tok.next();
        if ( !a2 )
        {
            reject_connection( sink, from, "insufficient connection info\n" );
            return true;
        }
        const std::int32_t challenge = ut::atoi( a2->text );

        const auto a3 = tok.next();
        if ( !a3 )
        {
            reject_connection( sink, from, "insufficient connection info\n" );
            return true;
        }
        char protinfo[k_max_info_string];
        ut::strncpy( protinfo, std::string( a3->text ).c_str(),
                     sizeof( protinfo ) );

        const auto a4 = tok.next();
        if ( !a4 )
        {
            reject_connection( sink, from, "insufficient connection info\n" );
            return true;
        }
        char userinfo[k_max_info_string];
        ut::strncpy( userinfo, std::string( a4->text ).c_str(),
                     sizeof( userinfo ) );

        ( void )connect_client( rt, from, protocol, challenge, protinfo,
                                userinfo, sink );
        return true;
    }

    if ( c == "ping" )
    {
        sink.send_oob( from, "ack" );
        return true;
    }

    return false;
}

// --- connect ----------------------------------------------------------------

int connect_client( ServerRuntime &rt, net::NetAddress from, int protocol,
                    std::int32_t challenge, const char *protinfo,
                    const char *userinfo, IOobSink &sink ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    ClientMachinery &cm = rt.clients;

    if ( protocol != k_protocol_version )
    {
        reject_connection( sink, from, "unsupported protocol\n" );
        return -1;
    }

    // local clients still validate — only loopback hashes to 0 consistently.
    if ( !check_challenge( rt, from, challenge ) )
    {
        reject_connection( sink, from, "no challenge for your address\n" );
        return -1;
    }

    if ( protinfo == nullptr || ut::strlen( protinfo ) >= k_max_info_string ||
         !info_is_valid( protinfo ) )
    {
        reject_connection( sink, from, "invalid protinfo in connect command\n" );
        return -1;
    }
    if ( userinfo == nullptr || ut::strlen( userinfo ) >= k_max_info_string ||
         !info_is_valid( userinfo ) )
    {
        reject_connection( sink, from, "invalid userinfo in connect command\n" );
        return -1;
    }

    // XASH3DPP-STUB(chunk6): SV_ProcessUserAgent uuid validation + SV_CheckID
    // ban check + input-device policy join with the operator-command surface.

    char qport_s[32];
    char ext_s[32];
    const std::uint16_t qport = static_cast<std::uint16_t>(
        ut::atoi( info_value_for_key( protinfo, "qport", qport_s,
                                      sizeof( qport_s ) ) ) );
    const int extensions =
        ut::atoi( info_value_for_key( protinfo, "ext", ext_s, sizeof( ext_s ) ) );

    // reconnect slot reuse: base addr AND (qport OR port).
    ServerClient *newcl = nullptr;
    for ( int i = 0; i < cm.maxclients; ++i )
    {
        ServerClient &cl = cm.clients[i];
        if ( cl.state == ClientState::Free || cl.state == ClientState::Zombie )
            continue;
        if ( net::compare_base( from, cl.adr ) &&
             ( cl.qport == qport || from.port == cl.adr.port ) )
        {
            newcl = &cl;
            break;
        }
    }
    if ( newcl == nullptr )
    {
        newcl = find_empty_slot( cm );
        if ( newcl == nullptr )
        {
            reject_connection( sink, from, "server is full\n" );
            return -1;
        }
    }

    const int slot = static_cast<int>( newcl - cm.clients );

    // a1ba quirk: preserve physinfo across the wipe.  Also carry the
    // pre-allocated snapshot frames ring (setup_clients/snapshot_alloc_ring owns
    // one per slot); the wipe would otherwise null the pointer and leak the ring
    // — and the snapshot send path needs cl.frames live for a connected client.
    char saved_physinfo[k_max_info_string];
    std::memcpy( saved_physinfo, newcl->physinfo, sizeof( saved_physinfo ) );
    ClientFrame *saved_frames = newcl->frames;
    *newcl = ServerClient{};
    std::memcpy( newcl->physinfo, saved_physinfo, sizeof( newcl->physinfo ) );
    newcl->frames = saved_frames;

    newcl->adr        = from;
    newcl->qport      = qport;
    newcl->edict      = rt.arena.edict_num( static_cast<std::size_t>( slot + 1 ) );
    newcl->userid     = cm.g_userid++;
    newcl->state      = ClientState::Connected;
    newcl->extensions = extensions & 1; // NET_EXT_SPLITSIZE
    ut::strncpy( newcl->useragent, protinfo, sizeof( newcl->useragent ) );

    // hashedcdkey = first 32 chars of protinfo uuid (not re-hashed, quirk 8).
    char uuid[64];
    info_value_for_key( protinfo, "uuid", uuid, sizeof( uuid ) );
    ut::strncpy( newcl->hashedcdkey, uuid, 33 ); // 32 chars + NUL
    newcl->hashedcdkey[32] = '\0';

    // Netchan_Setup (sv_client.c:404): arm this slot's channel against the peer
    // through the host-owned NetworkContext (its GoldSrc driver + fragment
    // pool).  The netchan is reused per slot; a fresh setup() resets all
    // sequence state, which is exactly the reconnect contract.  NETCHAN_USE_LZSS
    // unless the peer is local.  When the server is offline (rt.net == nullptr —
    // scaffold / connection-state unit tests) the channel is left inactive; the
    // state machine still runs, only transmit/process are unavailable.
    if ( rt.net != nullptr )
    {
        net::NetchanConfig ncfg;
        ncfg.sock                = net::SocketKind::Server;
        ncfg.remote_address      = from;
        ncfg.qport               = qport;
        ncfg.driver              = rt.net->protocol_driver( k_protocol_version );
        ncfg.block_size_provider = &cm.fragment_sizer;
        ncfg.pool                = rt.net->fragment_pool();
        ncfg.flags.use_lzss      = !is_loopback( from );
        ( void )cm.netchans[slot].setup( ncfg );
    }

    newcl->connection_started  = cm.realtime;
    newcl->last_received       = cm.realtime;
    newcl->next_messageinterval = 0.05; // 20 fps default

    ut::strncpy( newcl->userinfo, userinfo, sizeof( newcl->userinfo ) );
    userinfo_changed( rt, *newcl );

    newcl->next_messagetime =
        cm.realtime + static_cast<double>( rt.level.frametime ) +
        newcl->next_messageinterval;

    // reply client_connect with the granted ext + cheats.
    char reply_info[k_max_info_string] = {};
    char extbuf[16];
    ut::snprintf( extbuf, sizeof( extbuf ), "%d", newcl->extensions );
    info_set_value_for_key( reply_info, "ext", extbuf, sizeof( reply_info ) );
    info_set_value_for_key( reply_info, "cheats", "0", sizeof( reply_info ) );

    char reply[k_max_info_string + 32];
    ut::snprintf( reply, sizeof( reply ), "client_connect %s", reply_info );
    sink.send_oob( from, reply );

    ::xash::core::logf( ::xash::core::LogLevel::Info, "server",
                        "client connected (slot %d, userid %d)", slot,
                        newcl->userid );
    return slot;
}

// --- userinfo ---------------------------------------------------------------

void userinfo_changed( ServerRuntime &rt, ServerClient &cl ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    ClientMachinery &cm = rt.clients;

    if ( !info_is_valid( cl.userinfo ) )
        return;

    // name: trim, "console" → "unnamed", empty → "unnamed", dedupe.
    char raw[32];
    info_value_for_key( cl.userinfo, "name", raw, sizeof( raw ) );

    std::string_view trimmed = ut::trim_sv( raw );
    char             name[32];
    ut::strncpy( name, std::string( trimmed ).c_str(), sizeof( name ) );

    if ( name[0] == '\0' || ut::stricmp( name, "console" ) == 0 )
        ut::strncpy( name, "unnamed", sizeof( name ) );

    if ( name_in_use( cm, cl, name ) )
    {
        char deduped[32];
        for ( std::uint32_t n = 1; n < 1000; ++n )
        {
            ut::snprintf( deduped, sizeof( deduped ), "%s (%u)", name, n );
            if ( !name_in_use( cm, cl, deduped ) )
                break;
        }
        ut::strncpy( name, deduped, sizeof( name ) );
    }

    ut::strncpy( cl.name, name, sizeof( cl.name ) );
    info_set_value_for_key( cl.userinfo, "name", name, sizeof( cl.userinfo ) );

    // cl_updaterate ≤ 0 ⇒ 20 fps default (quirk 21).
    char ur[32];
    info_value_for_key( cl.userinfo, "cl_updaterate", ur, sizeof( ur ) );
    const int updaterate = ut::atoi( ur );
    if ( updaterate > 0 )
        cl.next_messageinterval = 1.0 / static_cast<double>( updaterate );
    else
        cl.next_messageinterval = 1.0 / 20.0;

    // XASH3DPP-STUB(S8-seam): rate → netchan.rate (SV_CheckRate is a known
    // no-op; the real clamp lives in the netchan the host wires per slot).

    // let the game override / read the userinfo, then re-sync netname.
    if ( rt.game.funcs().pfnClientUserInfoChanged != nullptr &&
         cl.edict != nullptr )
        rt.game.funcs().pfnClientUserInfoChanged( cl.edict, cl.userinfo );

    if ( cl.edict != nullptr )
    {
        char after[32];
        info_value_for_key( cl.userinfo, "name", after, sizeof( after ) );
        ut::strncpy( cl.name, after, sizeof( cl.name ) );
        EntityView( cl.edict ).set_netname( rt.strings.alloc_string( after ) );
    }
}

// --- stringcmd dispatch -----------------------------------------------------

namespace {

void client_new( ServerRuntime &rt, ServerClient &cl, IOobSink &sink ) noexcept
{
    if ( cl.state != ClientState::Connected )
        return;

    // XASH3DPP-STUB(S8-seam): SV_SendServerdata builds the signon (serverdata
    // + delta descriptions + movevars + user-message regs + lightstyles) and
    // fragments it to the client — the "new" send path the frame loop drives.

    char name[32];
    info_value_for_key( cl.userinfo, "name", name, sizeof( name ) );
    char addr[64];
    addr_string( cl.adr, addr, sizeof( addr ) );

    char reject[128] = {};
    int  ok          = 1;
    if ( rt.game.funcs().pfnClientConnect != nullptr )
        ok = rt.game.funcs().pfnClientConnect( cl.edict, name, addr, reject );

    if ( !ok )
    {
        reject_connection( sink, cl.adr,
                           reject[0] != '\0' ? reject : "connection rejected\n" );
        drop_client( rt, cl, false );
    }
}

void client_spawn( ServerRuntime &rt, ServerClient &cl ) noexcept
{
    if ( cl.state != ClientState::Connected )
        return;

    // SV_PutClientInServer fresh branch: flags/netname/colormap, then the game
    // hook.  (The verbatim signon append + svc_setview + svc_signonnum are the
    // S9 send seams.)
    if ( cl.edict != nullptr )
    {
        char name[32];
        info_value_for_key( cl.userinfo, "name", name, sizeof( name ) );

        EntityView ev( cl.edict );
        ev.set_flags( 0 ); // HLTV ⇒ FL_PROXY (deferred with the HLTV trim)
        ev.set_netname( rt.strings.alloc_string( name ) );
        ev.set_colormap( rt.arena.index_of( cl.edict ) );
    }

    if ( rt.game.funcs().pfnClientPutInServer != nullptr && cl.edict != nullptr )
        rt.game.funcs().pfnClientPutInServer( cl.edict );

    // loadgame restore: stage the svc_restore message (.HL2 name + connection
    // list) into the client's reliable buffer, then lift the freeze/pause
    // (sv_client.c:1352-1390 SV_PutClientInServer: the whole block — including
    // the `sv.loadgame = sv.paused = false` clear — is gated on sv.loadgame,
    // so a NORMAL (non-restore) spawn never touches sv.paused).
    // emit_svc_restore itself no-ops when !rt.level.loadgame, mirroring the
    // legacy `if (sv.loadgame)` guard; `was_loadgame` gates the clear the same
    // way so this function stays a no-op for sv.paused outside a restore.
    const bool was_loadgame = rt.level.loadgame;
    emit_svc_restore( rt, cl );
    if ( was_loadgame )
    {
        rt.level.loadgame = false;
        rt.level.paused   = false;
    }

    cl.state = ClientState::Spawning;
}

void client_begin( ServerRuntime &rt, ServerClient &cl ) noexcept
{
    if ( cl.state != ClientState::Spawning )
        return;
    cl.state       = ClientState::Spawned;
    cl.connecttime = rt.clients.realtime;
}

} // namespace

void execute_client_command( ServerRuntime &rt, ServerClient &cl,
                             const char *cmd, IOobSink &sink ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cmd == nullptr )
        return;

    ut::Tokenizer tok( cmd );
    const auto    first = tok.next();
    if ( !first )
        return;

    const std::string_view c = first->text;

    if ( c == "new" )
        client_new( rt, cl, sink );
    else if ( c == "spawn" )
        client_spawn( rt, cl );
    else if ( c == "begin" )
        client_begin( rt, cl );
    else if ( c == "disconnect" )
        drop_client( rt, cl, false );
    else if ( c == "setinfo" )
    {
        const auto k = tok.next();
        if ( !k )
            return;
        char key[64];
        ut::strncpy( key, std::string( k->text ).c_str(), sizeof( key ) );
        const auto v = tok.next();
        if ( v && cl.state >= ClientState::Connected )
        {
            char val[128];
            ut::strncpy( val, std::string( v->text ).c_str(), sizeof( val ) );
            info_set_value_for_key( cl.userinfo, key, val,
                                    sizeof( cl.userinfo ) );
            userinfo_changed( rt, cl );
        }
    }
    else
    {
        // forward to the game (enttools / fullupdate / custom cmds are S9/OQ-8
        // trims that hang off this path).
        if ( rt.game.funcs().pfnClientCommand != nullptr && cl.edict != nullptr )
            rt.game.funcs().pfnClientCommand( cl.edict );
    }
}

// --- client message (clc_*) parse -------------------------------------------

namespace {

// SV_PlayerIsFrozen (sv_client.c:3279): a frozen player's movement commands are
// zeroed but viewangles still read.
[[nodiscard]] bool player_is_frozen( ServerRuntime &rt,
                                     ::xash::abi::edict_t *player ) noexcept
{
    // sv_background_freeze on a background (menu) map — a listen-server/menu
    // concern (OQ-4); rt.level.background is tracked, the cvar read best-effort.
    if ( rt.level.background && rt.cvars != nullptr &&
         rt.cvars->cvar_variable_value( "sv_background_freeze" ) != 0.0f )
        return true;
    // ENGINE_QUAKE_COMPATIBLE (host.features) short-circuits to not-frozen; the
    // Q-12 ICompatPolicy seam is not wired into the server, so it never fires.
    return ( EntityView( player ).flags() & ::xash::abi::k_fl_frozen ) != 0;
}

// SV_EstablishTimeBase (sv_client.c:1143): back-date cl.timebase so that after
// replaying `dropped` catch-up commands plus the `numcmds` fresh ones the sim
// lands at sv.time + sv.frametime.
void establish_timebase( ServerRuntime &rt, ServerClient &cl,
                         const ::xash::abi::usercmd_t *cmds, int dropped,
                         int numbackup, int numcmds ) noexcept
{
    double runcmd_time = 0.0;
    if ( dropped < 24 )
    {
        while ( dropped > numbackup )
        {
            runcmd_time = static_cast<double>( cl.lastcmd.msec ) / 1000.0;
            dropped--;
        }
        while ( dropped > 0 )
        {
            const int cmdnum = dropped + numcmds - 1;
            runcmd_time += static_cast<double>( cmds[cmdnum].msec ) / 1000.0;
            dropped--;
        }
    }
    for ( int i = numcmds - 1; i >= 0; --i )
        runcmd_time += static_cast<double>( cmds[i].msec ) / 1000.0;

    cl.timebase = rt.level.time + rt.level.frametime - runcmd_time;
}

// SV_ParseClientMove (sv_client.c:3305): decode the delta-compressed usercmd
// chain (newest-first, each delta'd from the previous, starting from a null
// cmd), then establish the timebase and run each command through the pmove
// chain (SV_RunCmd), updating lastcmd / packet_loss / ping.
void parse_client_move( ServerRuntime &rt, ServerClient &cl,
                        net::MessageBuf &msg ) noexcept
{
    ClientFrame *frame =
        cl.frames != nullptr
            ? &cl.frames[cl.incoming_acknowledged & rt.snapshot.update_mask]
            : nullptr;

    ( void )msg.read_byte();                    // checksum1
    const int packet_loss = static_cast<int>( msg.read_byte() );
    const int numbackup   = static_cast<int>( msg.read_byte() );
    const int numcmds     = static_cast<int>( msg.read_byte() );
    const int totalcmds   = numcmds + numbackup;

    if ( totalcmds < 0 || totalcmds >= k_cmd_mask )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                            "SV_ParseClientMove: %s sending too many commands %d",
                            cl.name, totalcmds );
        drop_client( rt, cl, false );
        return;
    }

    ::xash::abi::usercmd_t        cmds[k_cmd_backup] = {};
    const ::xash::abi::usercmd_t  nullcmd            = {};
    const ::xash::abi::usercmd_t *from               = &nullcmd;
    for ( int i = totalcmds - 1; i >= 0; --i )
    {
        rt.delta.read_delta_usercmd( msg, from, &cmds[i] );
        from = &cmds[i];
    }

    if ( cl.state != ClientState::Spawned )
        return;

    // XASH3DPP-STUB(chunk6-S9): command checksum (CRC32_BlockSequence over the
    // post-checksum bytes keyed by netchan.incoming_sequence; skipped for local
    // clients, mismatch ⇒ ignore-not-drop) — an anti-tamper guard, not core to
    // the milestone.
    cl.packet_loss = packet_loss;

    // freeze the player while a savegame is being restored (Chunk 8 seam).
    if ( rt.level.loadgame )
        return;

    // Pause / frozen: hold movement but keep reading viewangles
    // (sv_client.c:3360-3376).  CL_IsInGame() is true on a dedicated server;
    // the listen-server "local client not yet active" case is the OQ-4
    // client-state hook (assumed in-game here).
    EntityView pv( cl.edict );
    const bool frozen = player_is_frozen( rt, cl.edict );
    if ( rt.level.paused || frozen )
    {
        for ( int i = 0; i < numcmds; ++i )
        {
            cmds[i].msec        = 0;
            cmds[i].forwardmove = 0.0f;
            cmds[i].sidemove    = 0.0f;
            cmds[i].upmove      = 0.0f;
            cmds[i].buttons     = 0;
            if ( frozen )
                cmds[i].impulse = 0;
            pv.set_v_angle( ut::Vec3{ cmds[i].viewangles[0],
                                      cmds[i].viewangles[1],
                                      cmds[i].viewangles[2] } );
        }
    }
    else if ( pv.fixangle() == 0 )
    {
        pv.set_v_angle( ut::Vec3{ cmds[0].viewangles[0], cmds[0].viewangles[1],
                                  cmds[0].viewangles[2] } );
    }

    // SV_EstablishTimeBase.  net_drop drives the dropped-packet catch-up terms.
    // XASH3DPP-STUB(S8-seam): net_drop = netchan.dropped - (numcmds-1) and the
    // replay loops (re-run cl.lastcmd / the backup cmds when net_drop>0,
    // sv_client.c:3385-3399) need the netchan.dropped mirror; net_drop is pinned
    // to 0 here (the no-packet-loss path), so only the fresh-cmd loop runs.
    int net_drop = 0;
    establish_timebase( rt, cl, cmds, net_drop, numbackup, numcmds );

    // Run each fresh command through the move chain, newest last.  The random
    // seed is the netchan incoming sequence (minus the command's age).
    const int slot = static_cast<int>( &cl - rt.clients.clients );
    const int seq  = ( slot >= 0 && slot < k_max_clients )
                         ? static_cast<int>(
                               rt.clients.netchans[slot].incoming_sequence() )
                         : 0;
    for ( int i = numcmds - 1; i >= 0; --i )
        sv_run_cmd( rt, cl, cmds[i], seq - i );

    // Was the player kicked mid-run?  Then lastcmd / ping would be stale.
    if ( cl.state == ClientState::Zombie )
        return;

    // XASH3DPP-STUB(chunk7): SV_ModelHandle studio animtime clamp
    // (sv_client.c:3416-3423) needs the model cache.

    cl.lastcmd = cmds[0];

    // Latency arrives ~½ a client frame late (sv_client.c:3414).
    if ( frame != nullptr )
    {
        frame->ping_time -= static_cast<float>(
            static_cast<double>( cl.lastcmd.msec ) * 0.5 / 1000.0 );
        if ( frame->ping_time < 0.0f )
            frame->ping_time = 0.0f;
    }
}

} // namespace

void execute_client_message( ServerRuntime &rt, ServerClient &cl,
                             net::MessageBuf &msg, IOobSink &sink ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cl.frames == nullptr )
        return; // ASSERT(cl->frames) — no snapshot ring yet

    ClientMachinery &cm = rt.clients;

    // Frame-ping bookkeeping (sv_client.c:3643): the acked frame's round-trip
    // minus the send interval; zeroed on the first frame and through the 2s
    // signon settle.
    ClientFrame &frame =
        cl.frames[cl.incoming_acknowledged & rt.snapshot.update_mask];
    frame.ping_time = static_cast<float>( cm.realtime - frame.senttime -
                                          cl.next_messageinterval );
    if ( frame.senttime == 0.0 )
        frame.ping_time = 0.0f;
    if ( ( cm.realtime - cl.connection_started ) < 2.0 && frame.ping_time > 0.0f )
        frame.ping_time = 0.0f;

    // XASH3DPP-STUB(chunk6-S9): SV_CalcClientTime unlag latency sample.
    cl.delta_sequence = -1; // no delta unless a clc_delta arrives

    bool move_issued = false;
    while ( cl.state != ClientState::Zombie )
    {
        if ( msg.overflowed() )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                                "incoming overflow for %s", cl.name );
            drop_client( rt, cl, false );
            return;
        }
        if ( msg.num_bits_left() < 8 )
            break; // end of message

        const int c = static_cast<int>( msg.read_byte() );
        switch ( c )
        {
        case k_clc_nop:
            break;
        case k_clc_delta:
            cl.delta_sequence = static_cast<int>( msg.read_byte() );
            break;
        case k_clc_move:
            if ( move_issued )
                return; // second clc_move in one packet ⇒ cheat (quirk 28)
            move_issued = true;
            parse_client_move( rt, cl, msg );
            break;
        case k_clc_stringcmd:
        {
            char cmd[k_max_usermsg_length];
            msg.read_string( std::span<char>{ cmd } );
            execute_client_command( rt, cl, cmd, sink );
            if ( cl.state == ClientState::Zombie )
                return; // disconnect command
            break;
        }
        default:
            // clc_resourcelist / fileconsistency / voicedata / requestcvarvalue
            // (2) are OQ-8 trims (SV_SendResources / voice / cvar-query are
            // stubbed, so the server never elicits them at this milestone).  Any
            // unsupported opcode drops the client, matching legacy clc_bad.
            ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                                "%s: clc_bad (%d)", cl.name, c );
            drop_client( rt, cl, false );
            return;
        }
    }
}

// --- drop / timeout ---------------------------------------------------------

void drop_client( ServerRuntime &rt, ServerClient &cl, bool crash ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cl.state == ClientState::Zombie )
        return;

    if ( !crash )
    {
        // svc_disconnect into the reliable stream + final transmit are send
        // seams; the game hook fires only for spawned clients.
        if ( cl.state == ClientState::Spawned &&
             rt.game.funcs().pfnClientDisconnect != nullptr &&
             cl.edict != nullptr )
            rt.game.funcs().pfnClientDisconnect( cl.edict );
    }

    cl.state      = ClientState::Zombie;
    cl.name[0]    = '\0';
    cl.fakeclient = false;
    cl.hltv       = false;
    cl.edict      = nullptr;
    cl.reliable_bits = 0;
    cl.datagram_bits = 0;

    // Netchan_Clear (sv_client.c:612): flush this slot's reliable + fragment
    // queues; a later connect re-arms it via Netchan_Setup.
    const int slot = static_cast<int>( &cl - rt.clients.clients );
    if ( slot >= 0 && slot < k_max_clients )
        rt.clients.netchans[slot].clear();

    // XASH3DPP-STUB(S8-seam): broadcast SV_FullClientUpdate (empty-name form)
    // into sv.reliable_datagram + NET_MasterClear on empty server.
}

void check_timeouts( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    ClientMachinery &cm = rt.clients;

    for ( int i = 0; i < cm.maxclients; ++i )
    {
        ServerClient &cl = cm.clients[i];

        // zombie lives exactly one frame (sv_main.c:511-514).
        if ( cl.state == ClientState::Zombie )
        {
            cl.state = ClientState::Free;
            continue;
        }

        if ( cl.fakeclient || cl.state == ClientState::Free )
            continue;

        const bool local = is_loopback( cl.adr );
        if ( local )
            continue;

        if ( ( cl.state == ClientState::Connected ||
               cl.state == ClientState::Spawning ) &&
             cl.connection_started < cm.realtime - k_sv_connect_timeout )
        {
            // timed-out connectors skip zombie (state = free) — quirk 13.
            drop_client( rt, cl, false );
            cl.state = ClientState::Free;
        }
        else if ( cl.state == ClientState::Spawned &&
                  cl.last_received < cm.realtime - k_sv_timeout )
        {
            drop_client( rt, cl, false );
        }
    }
}

// --- fake client ------------------------------------------------------------

::xash::abi::edict_t *fake_connect( ServerRuntime &rt,
                                    const char *netname ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    ClientMachinery &cm = rt.clients;

    ServerClient *cl = find_empty_slot( cm );
    if ( cl == nullptr )
        return nullptr;

    const int slot = static_cast<int>( cl - cm.clients );

    *cl = ServerClient{}; // full wipe — no physinfo preservation for bots

    // default userinfo (name=netname|"Bot", model=gordon, colors 1/1).
    info_set_value_for_key( cl->userinfo, "name",
                            ( netname != nullptr && netname[0] != '\0' )
                                ? netname
                                : "Bot",
                            sizeof( cl->userinfo ) );
    info_set_value_for_key( cl->userinfo, "model", "gordon",
                            sizeof( cl->userinfo ) );
    info_set_value_for_key( cl->userinfo, "topcolor", "1",
                            sizeof( cl->userinfo ) );
    info_set_value_for_key( cl->userinfo, "bottomcolor", "1",
                            sizeof( cl->userinfo ) );

    cl->edict = rt.arena.edict_num( static_cast<std::size_t>( slot + 1 ) );
    cl->userid     = cm.g_userid++;
    cl->fakeclient = true;
    cl->state      = ClientState::Spawned; // bots go straight to spawned

    info_value_for_key( cl->userinfo, "name", cl->name, sizeof( cl->name ) );

    if ( cl->edict != nullptr )
    {
        EntityView ev( cl->edict );
        ev.add_flags( ::xash::abi::k_fl_client | ::xash::abi::k_fl_fakeclient );
        ev.set_netname( rt.strings.alloc_string( cl->name ) );
    }

    return cl->edict;
}

// --- lookup -----------------------------------------------------------------

ServerClient *client_for_edict( ClientMachinery &cm,
                                const ::xash::abi::edict_t *ed ) noexcept
{
    if ( ed == nullptr )
        return nullptr;
    for ( int i = 0; i < cm.maxclients; ++i )
    {
        if ( cm.clients[i].state != ClientState::Free &&
             cm.clients[i].edict == ed )
            return &cm.clients[i];
    }
    return nullptr;
}

} // namespace xash::server
