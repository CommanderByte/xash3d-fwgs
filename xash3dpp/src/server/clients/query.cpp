// xash3dpp — server query responder (Chunk 6 S9 satellite)
// Legacy reference: engine/server/sv_client.c — SV_Info (:864); the A2S/
// netinfo responders (sv_query.c) share this live-state read path.
// Deep dive: docs/legacy-survey/deep-dive-server-clients.md §4.
//
// SV_Info builds the Xash `info` infostring answer from live server state.
// The exact key order is wire-observable: p / map / dm / team / coop / numcl /
// maxcl / gamedir / password / host.  A2S ('I'/'U'/'V') and netinfo responders
// are further OQ-8-adjacent surfaces layered on this same state read; the
// dedicated single-map milestone ships the `info` answer and leaves the byte-
// framed A2S packet build as a follow-up.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/private/server/info_string.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/utilities/string.hpp>

namespace xash::server {

namespace {

namespace ut = ::xash::utilities;

const char *cvar_string_or( const ServerRuntime &rt, const char *name,
                            const char *fallback ) noexcept
{
    if ( rt.cvars == nullptr )
        return fallback;
    const char *v = rt.cvars->cvar_variable_string( name );
    return ( v != nullptr && v[0] != '\0' ) ? v : fallback;
}

[[nodiscard]] bool cvar_true( const ServerRuntime &rt, const char *name ) noexcept
{
    return rt.cvars != nullptr && rt.cvars->cvar_variable_value( name ) != 0.0f;
}

} // namespace

std::size_t query_info( ServerRuntime &rt, int protocol, char *out,
                        std::size_t out_size ) noexcept
{
    if ( out == nullptr || out_size == 0 )
        return 0;

    const char *hostname = cvar_string_or( rt, "hostname", "unnamed" );

    if ( protocol != k_protocol_version )
    {
        ut::snprintf( out, out_size, "%s: wrong version\n", hostname );
        return ut::strlen( out );
    }

    // count real (non-fake) clients that are past cs_free.
    int numcl = 0;
    for ( int i = 0; i < rt.clients.maxclients; ++i )
    {
        const ServerClient &cl = rt.clients.clients[i];
        if ( cl.state != ClientState::Free && cl.state != ClientState::Zombie &&
             !cl.fakeclient )
            ++numcl;
    }

    char info[k_max_serverinfo] = {};
    char num[16];

    info_set_value_for_key( info, "p", "49", sizeof( info ) );
    info_set_value_for_key( info, "map",
                            rt.level.name[0] != '\0' ? rt.level.name : "",
                            sizeof( info ) );
    info_set_value_for_key( info, "dm", cvar_true( rt, "deathmatch" ) ? "1" : "0",
                            sizeof( info ) );
    info_set_value_for_key( info, "team", cvar_true( rt, "teamplay" ) ? "1" : "0",
                            sizeof( info ) );
    info_set_value_for_key( info, "coop", cvar_true( rt, "coop" ) ? "1" : "0",
                            sizeof( info ) );

    ut::snprintf( num, sizeof( num ), "%d", numcl );
    info_set_value_for_key( info, "numcl", num, sizeof( info ) );
    ut::snprintf( num, sizeof( num ), "%d", rt.clients.maxclients );
    info_set_value_for_key( info, "maxcl", num, sizeof( info ) );

    info_set_value_for_key( info, "gamedir",
                            rt.cfg.game_dir != nullptr ? rt.cfg.game_dir : "",
                            sizeof( info ) );

    const char *pw = cvar_string_or( rt, "sv_password", "" );
    const bool have_pw = pw[0] != '\0' && ut::stricmp( pw, "none" ) != 0;
    info_set_value_for_key( info, "password", have_pw ? "1" : "0",
                            sizeof( info ) );
    info_set_value_for_key( info, "host", hostname, sizeof( info ) );

    ut::strncpy( out, info, out_size );
    return ut::strlen( out );
}

} // namespace xash::server
