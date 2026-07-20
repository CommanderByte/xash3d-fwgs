// xash3dpp — entity-string parse: SV_SpawnEntities / SV_LoadFromFile /
// SV_ParseEdict (Chunk 6 S7b).
// Legacy reference: engine/server/sv_game.c — SV_ParseEdict (:4885-5065),
// SV_LoadFromFile (:5079-5126), SV_SpawnEntities (:5136-5169).
// Deep dive: docs/legacy-survey/deep-dive-server-lifecycle.md.
//
// The quirk catalogue this file reproduces (all verified against source):
//   • classname must be first; the game MUST handle it (else Host_Error) —
//     stricter than GoldSrc; the game may override the stored classname;
//   • "wad" key and empty key/value are skipped; keys leading with '_' are
//     dropped for FWORLD_SKYSPHERE worlds only;
//   • trailing spaces are stripped from key names AFTER classname handling
//     (classname itself is never stripped — GoldSrc parity);
//   • pkvd is capped at 256 entries (legacy writes a 257th out of bounds
//     before breaking — we bound-check and drop the overflow, observably
//     identical for every real map);
//   • "angle" is rewritten to "angles" reading the entity's CURRENT angles
//     (post-LINK) with the ±1/±2 up/down specials;
//   • the custom-entity KeyValue ("customclass") is dispatched with no
//     fHandled check (GoldSrc behaviour);
//   • the world edict's origin/angles are cleared after the whole load.
// The ce08_02 HLMODS origin hack (sv_game.c:5002-5009) is OQ-7 — routed to
// the server ICompatPolicy when it lands; left as a marker here.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/lifecycle.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xash::server {

using namespace ::xash::world; // consume the world trace/link kernel (Wave 1)

namespace {

namespace ut = ::xash::utilities;
namespace ml = ::xash::map_loader;

void parse_host_error( ServerRuntime &rt, const char *msg ) noexcept
{
    if ( rt.cfg.host_error != nullptr )
        rt.cfg.host_error( rt.cfg.host_error_ctx, msg );
    else
        ::xash::core::log_error( "server", msg );
}

// SV_FreeEdict (sv_game.c:1004): unlink from the world first, then arena
// scrub (the arena does not track area links by design).
void free_edict( ServerRuntime &rt, ::xash::abi::edict_t *ent ) noexcept
{
    WorldLinks::unlink_edict( ent );
    rt.arena.free_edict( ent, rt.level.time );
}

} // namespace

bool parse_edict( ServerRuntime &rt, const ml::WorldData &world,
                  const char *&cursor, ::xash::abi::edict_t *ent ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    constexpr std::size_t k_max_pairs = 256; // legacy KeyValueData pkvd[256]

    const auto &funcs = rt.game.funcs();

    // Non-classname pairs accumulated for the second (apply) pass; each owns
    // its key/value storage (legacy copystring) so a game overwrite of the
    // KeyValueData pointers cannot leak.
    std::vector<std::pair<std::string, std::string>> pairs;

    const char *classname = nullptr; // SV_GetString(ent->v.classname)
    const bool  skysphere = ( world.flags() & ml::k_fworld_skysphere ) != 0;

    while ( true )
    {
        char keyname[256];
        char value[2048];

        cursor = ut::parse_token( cursor, keyname, sizeof( keyname ));
        if ( cursor == nullptr )
        {
            parse_host_error( rt, "SV_ParseEdict: EOF without closing brace" );
            return false;
        }
        if ( keyname[0] == '}' )
            break; // end of this entity

        cursor = ut::parse_token( cursor, value, sizeof( value ));
        if ( cursor == nullptr )
        {
            parse_host_error( rt, "SV_ParseEdict: EOF without closing brace" );
            return false;
        }
        if ( value[0] == '}' )
        {
            parse_host_error( rt,
                              "SV_ParseEdict: closing brace without data" );
            return false;
        }

        // Skip empty key/value and the already-handled "wad" field.
        const std::string_view key_view( keyname );
        if ( keyname[0] == '\0' || value[0] == '\0' || key_view == "wad" )
            continue;

        // Utility comments (leading '_') are dropped on sky-sphere worlds.
        if ( skysphere && keyname[0] == '_' )
            continue;

        // classname must be first and MUST be handled by the game.
        if ( key_view == "classname" )
        {
            if ( classname != nullptr )
                continue; // no double classnames

            ::xash::abi::KeyValueData kvd{};
            kvd.szClassName = nullptr;
            kvd.szKeyName   = keyname;
            kvd.szValue     = value;
            kvd.fHandled    = 0;
            funcs.pfnKeyValue( ent, &kvd );

            if ( !kvd.fHandled )
            {
                // Legacy includes the offending value (sv_game.c:4943).
                char msg[128];
                std::snprintf(
                    msg, sizeof( msg ),
                    "SV_ParseEdict: game didn't handle \"%s\" classname",
                    value );
                parse_host_error( rt, msg );
                return false;
            }

            // The game may override the classname to something bogus that is
            // exported in the DLL — re-read it from the entity.
            classname = rt.strings.get_string( EntityView( ent ).classname());
            continue;
        }

        // GoldSrc strips trailing spaces from key names (but only after the
        // classname is sucked out — classname itself is not stripped).
        for ( std::size_t len = key_view.size();
              len > 0 && keyname[len - 1] == ' '; --len )
            keyname[len - 1] = '\0';

        // pkvd[256] cap: legacy checks AFTER writing (a 257th write is out of
        // bounds); we bound-check first and drop the overflow — observably
        // identical for every real entity.
        if ( pairs.size() >= k_max_pairs )
        {
            if ( classname != nullptr )
                ::xash::core::logf(
                    ::xash::core::LogLevel::Error, "server",
                    "SV_ParseEdict: too many keyvalue pairs for %s!",
                    classname );
            else
                ::xash::core::log_error(
                    "server", "SV_ParseEdict: too many keyvalue pairs!" );
            break;
        }

        pairs.emplace_back( keyname, value );
    }

    if ( classname == nullptr )
        return false; // no classname → inhibited (strings free with the vec)

    // SV_AllocPrivateData: LINK dispatch (with the "custom" fallback).
    bool customentity = false;
    ent = alloc_private_data( ent, EntityView( ent ).classname(),
                              &customentity );

    EntityView view( ent );
    if ( !view.valid() ||
         ( view.flags() & ::xash::abi::k_fl_killme ) != 0 )
        return false;

    if ( customentity )
    {
        // The custom KeyValue is dispatched with NO fHandled check.
        ::xash::abi::KeyValueData kvd{};
        kvd.szClassName = const_cast<char *>( "custom" );          // SAFETY: Q-16 const_cast — KeyValueData's ABI fields are char* but pfnKeyValue only READS them; "custom" is a string literal (never written)
        kvd.szKeyName   = const_cast<char *>( "customclass" );     // SAFETY: Q-16 const_cast — read-only KeyValueData field; string literal
        kvd.szValue     = const_cast<char *>( classname );         // SAFETY: Q-16 const_cast — read-only KeyValueData field; classname is caller-owned and not written by pfnKeyValue
        kvd.fHandled    = 0;
        funcs.pfnKeyValue( ent, &kvd );
    }

    // TODO(OQ-7): the ce08_02 HACKS_RELATED_HLMODS origin nudge
    // (sv_game.c:5002-5009) routes through the server ICompatPolicy; until
    // then adjust_origin is always false and the "origin" rewrite is inert.

    for ( auto &pair : pairs )
    {
        std::string &key   = pair.first;
        std::string &value = pair.second;

        // "angle" → "angles": read the entity's CURRENT angles (post-LINK).
        if ( key == "angle" )
        {
            const float yaw = ut::atof( value.c_str() );
            const Vec3  ang = view.angles();
            char        temp[64];
            if ( yaw >= 0.0f )
                std::snprintf( temp, sizeof( temp ), "%g %g %g",
                               static_cast<double>( ang.x ),
                               static_cast<double>( yaw ),
                               static_cast<double>( ang.z ));
            else if ( yaw == -1.0f )
                std::snprintf( temp, sizeof( temp ), "-90 0 0" );
            else if ( yaw == -2.0f )
                std::snprintf( temp, sizeof( temp ), "90 0 0" );
            else
                std::snprintf( temp, sizeof( temp ), "0 0 0" );

            key   = "angles";
            value = temp;
        }

        ::xash::abi::KeyValueData kvd{};
        kvd.szClassName = const_cast<char *>( classname );         // SAFETY: Q-16 const_cast — read-only KeyValueData ABI field; pfnKeyValue never writes szClassName; classname is caller-owned
        kvd.szKeyName   = key.data();
        kvd.szValue     = value.data();
        kvd.fHandled    = 0;
        funcs.pfnKeyValue( ent, &kvd );
    }

    return true;
}

void load_from_file( ServerRuntime &rt, const ml::WorldData &world,
                     const char *entities ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // TODO(chunk6-S8): physFuncs.SV_LoadEntities full-override hook.
    if ( entities == nullptr )
        return;

    const auto &funcs        = rt.game.funcs();
    const char *cursor       = entities;
    int         inhibited    = 0;
    bool        create_world = true;
    char        token[2048];

    while (( cursor = ut::parse_token( cursor, token, sizeof( token ))) !=
           nullptr )
    {
        if ( token[0] != '{' )
        {
            char msg[128];
            std::snprintf( msg, sizeof( msg ),
                           "SV_LoadFromFile: found %s when expecting {",
                           token );
            parse_host_error( rt, msg );
            return;
        }

        ::xash::abi::edict_t *ent;
        if ( create_world )
        {
            create_world = false;
            ent          = rt.arena.edict_num( 0 ); // already initialised
        }
        else
        {
            ent = rt.arena.alloc_edict( rt.level.time );
        }

        if ( ent == nullptr )
        {
            // Legacy SV_AllocEdict Host_Errors on exhaustion and aborts the
            // whole load (sv_game.c:1058) — a `continue` here would leave the
            // cursor mid-entity and mis-parse the next token, so abort too.
            parse_host_error( rt, "SV_AllocEdict: no free edicts" );
            return;
        }

        if ( !parse_edict( rt, world, cursor, ent ))
            continue;

        if ( funcs.pfnSpawn( ent ) == -1 )
        {
            // Game rejected the spawn; free unless it self-marked FL_KILLME.
            if ( ( EntityView( ent ).flags() & ::xash::abi::k_fl_killme ) == 0 )
            {
                free_edict( rt, ent );
                ++inhibited;
            }
        }
    }

    ::xash::core::logf( ::xash::core::LogLevel::Verbose, "server",
                        "%i entities inhibited", inhibited ); // Con_DPrintf

    // Reset world origin and angles "for some reason" (legacy comment).
    EntityView world_view( rt.arena.edict_num( 0 ));
    world_view.set_origin( { 0.0f, 0.0f, 0.0f } );
    world_view.set_angles( { 0.0f, 0.0f, 0.0f } );
}

void spawn_entities( ServerRuntime &rt, const ml::WorldData &world ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // XASH3DPP-STUB(chunk6): Cvar_Reset of sv_zmax/sv_wateramp/sv_wateralpha
    // + the sky parms (sv_game.c:5141-5152) — those cvars are registered by
    // SV_Init (S8/S9); resetting them is a no-op until then.

    ::xash::abi::edict_t *world_ed = rt.arena.edict_num( 0 );
    if ( world_ed == nullptr )
        return;

    EntityView w( world_ed );
    if ( w.freed() ) // SV_InitEdict if the world slot is free
        rt.arena.init_edict( world_ed );
    w.set_model( rt.strings.make_string(
        rt.precache.model_name( ::xash::abi::k_world_index )));
    w.set_modelindex( ::xash::abi::k_world_index );
    w.set_solid( ::xash::abi::k_solid_bsp );
    w.set_movetype( ::xash::abi::k_movetype_push );
    // svgame.movevars.fog_settings = 0 — movevars land in S8.

    rt.globals.maxEntities = static_cast<int>( rt.cfg.max_edicts );
    rt.globals.mapname     = rt.strings.make_string( rt.level.name );
    rt.globals.startspot   = rt.strings.make_string( rt.level.startspot );
    rt.globals.time        = static_cast<float>( rt.level.time );

    load_from_file( rt, world, world.entities().data() );
}

} // namespace xash::server
