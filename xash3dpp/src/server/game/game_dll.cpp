// xash3dpp — game DLL loader implementation (Chunk 6 S6)
// Legacy reference: engine/server/sv_game.c :5214-5366 (SV_LoadProgs),
// :5171-5212 (SV_UnloadProgs library part).
//
// Existing subsystems used:
//   xash3dpp_platform — open_library / get_symbol / close_library
//   xash3dpp_core     — logging, thread-role assertion (OQ-9)

#include <xash3dpp/private/server/game_dll.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <cstdint>

namespace xash::server {

namespace pf = ::xash::platform;

bool GameDll::load( const char *path, ::xash::abi::enginefuncs_t *table,
                    ::xash::abi::globalvars_t *globals )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // The DLL persists across map changes (SV_LoadProgs early-return).
    if ( lib_ )
        return true;

    error_    = LoadError::None;
    funcs_    = {};
    new_funcs_ = {};
    extended_ = has_new_ = false;

    lib_ = pf::open_library( path );
    if ( !lib_ )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                            "game DLL load failed: %s", path );
        error_ = LoadError::LibraryNotFound;
        return false;
    }

    auto get_entity_api = reinterpret_cast<::xash::abi::APIFUNCTION>(         // SAFETY: object-pointer->function-pointer cast — the game DLL exports "GetEntityAPI" with the frozen APIFUNCTION signature (eiface.h); sanctioned loader pun
        pf::get_symbol( lib_, "GetEntityAPI" ));
    auto get_entity_api2 = reinterpret_cast<::xash::abi::APIFUNCTION2>(       // SAFETY: object->function-pointer cast — "GetEntityAPI2" has the frozen APIFUNCTION2 signature (eiface.h)
        pf::get_symbol( lib_, "GetEntityAPI2" ));
    auto give_new_dll_funcs = reinterpret_cast<::xash::abi::NEW_DLL_FUNCTIONS_FN>( // SAFETY: object->function-pointer cast — "GetNewDLLFunctions" has the frozen NEW_DLL_FUNCTIONS_FN signature (eiface.h)
        pf::get_symbol( lib_, "GetNewDLLFunctions" ));

    if ( !get_entity_api && !get_entity_api2 )
    {
        ::xash::core::log_error( "server",
            "game DLL missing GetEntityAPI and GetEntityAPI2 exports" );
        pf::close_library( lib_ );
        error_ = LoadError::MissingEntityApi;
        return false;
    }

    auto give_fnptrs = reinterpret_cast<::xash::abi::GIVEFNPTRSTODLL>(       // SAFETY: object->function-pointer cast — "GiveFnptrsToDll" has the frozen GIVEFNPTRSTODLL signature (eiface.h)
        pf::get_symbol( lib_, "GiveFnptrsToDll" ));

    if ( !give_fnptrs )
    {
        ::xash::core::log_error( "server",
            "game DLL missing GiveFnptrsToDll export" );
        pf::close_library( lib_ );
        error_ = LoadError::MissingGiveFnptrs;
        return false;
    }

    // The fnptr handoff happens FIRST, before any Get*API negotiation.
    give_fnptrs( table, globals );

    if ( give_new_dll_funcs )
    {
        int version = ::xash::abi::k_new_dll_functions_version;

        if ( give_new_dll_funcs( &new_funcs_, &version ))
        {
            has_new_ = true;
        }
        else
        {
            if ( version != ::xash::abi::k_new_dll_functions_version )
                ::xash::core::logf( ::xash::core::LogLevel::Warning, "server",
                                    "new interface version %i should be %i",
                                    ::xash::abi::k_new_dll_functions_version,
                                    version );
            new_funcs_ = {};
        }
    }

    int  version         = ::xash::abi::k_interface_version;
    bool init_entity_api = false;

    if ( get_entity_api2 && get_entity_api2( &funcs_, &version ))
    {
        if ( version == ::xash::abi::k_interface_version )
        {
            init_entity_api = true;
            extended_       = true;
        }
        else
        {
            ::xash::core::logf( ::xash::core::LogLevel::Warning, "server",
                                "interface version %i should be %i",
                                ::xash::abi::k_interface_version, version );
        }
    }

    // Legacy quirk preserved: the fallback passes `version` by value as
    // it stands NOW — a failed API2 negotiation may have overwritten it.
    if ( !init_entity_api && get_entity_api &&
         get_entity_api( &funcs_, version ))
    {
        init_entity_api = true;
    }

    if ( !init_entity_api )
    {
        ::xash::core::log_error( "server", "couldn't get entity API" );
        funcs_     = {};
        new_funcs_ = {};
        has_new_   = false;
        pf::close_library( lib_ );
        error_ = LoadError::EntityApiInitFailed;
        return false;
    }

    return true;
}

void GameDll::unload()
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( !lib_ )
        return;

    pf::close_library( lib_ );
    funcs_     = {};
    new_funcs_ = {};
    extended_ = has_new_ = false;
    error_               = LoadError::None;
    reverse_cache_.clear();
    reverse_cache_built_ = false;
}

::xash::abi::LINK_ENTITY_FUNC
GameDll::entity_link( const char *classname ) const noexcept
{
    return reinterpret_cast<::xash::abi::LINK_ENTITY_FUNC>(                  // SAFETY: object->function-pointer cast — the game DLL exports each classname as a LINK_ENTITY_FUNC spawn function (legacy SV_AllocPrivateData path); missing symbol yields nullptr
        pf::get_symbol( lib_, classname ));
}

void *GameDll::symbol( const char *name ) const noexcept
{
    return pf::get_symbol( lib_, name );
}

namespace {

// The frozen-ABI function handle for an export address: the same truncation the
// forward pfnFunctionFromName applies (static_cast<unsigned long>).
[[nodiscard]] unsigned long function_handle( const void *addr ) noexcept
{
    return static_cast<unsigned long>( reinterpret_cast<std::uintptr_t>( addr ) );
}

// enumerate_exports visitor: populate the handle->name reverse cache.  First
// name wins for a given handle (export-table order), mirroring the linear scan
// in legacy COM_NameForFunction which returns the first matching ordinal.
void collect_export( std::string_view name, const void *addr, void *userdata ) noexcept
{
    auto *cache = static_cast<std::unordered_map<unsigned long, std::string> *>( userdata );
    cache->try_emplace( function_handle( addr ), name );
}

} // namespace

const char *GameDll::name_for_function( unsigned long handle ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( handle == 0 || !lib_ )
        return nullptr;

    // Build the per-DLL ordinal cache once (SAV-OQ-3 glue over the platform
    // reverse-walk primitive).  On POSIX enumerate_exports is unsupported and
    // returns false without populating the cache; fall back to the per-address
    // name_for_symbol (dladdr) primitive below so restore still resolves names
    // (POSIX is LP64, so `handle` == the full pointer there — no truncation).
    if ( !reverse_cache_built_ )
    {
        pf::enumerate_exports( lib_, collect_export, &reverse_cache_ );
        reverse_cache_built_ = true;
    }

    if ( auto it = reverse_cache_.find( handle ); it != reverse_cache_.end() )
        return it->second.c_str();

    // Cache miss (or POSIX, where the cache is empty): reconstruct the address
    // and consult the per-address primitive, then memoize.
    const auto *addr = reinterpret_cast<const void *>( static_cast<std::uintptr_t>( handle ) );
    if ( auto name = pf::name_for_symbol( lib_, addr ); name.has_value() )
    {
        auto [it, _] = reverse_cache_.try_emplace( handle, *name );
        return it->second.c_str();
    }

    return nullptr;
}

::xash::map_loader::HullBoundsTable
query_hull_bounds( const ::xash::abi::DLL_FUNCTIONS &funcs ) noexcept
{
    ::xash::map_loader::HullBoundsTable bounds{}; // zeroed (legacy globals)

    if ( !funcs.pfnGetHullBounds ) // hardening: legacy calls unchecked
        return bounds;

    for ( int i = 0; i < 4; ++i )
    {
        float mins[3]{}, maxs[3]{};

        if ( funcs.pfnGetHullBounds( i, mins, maxs ))
        {
            bounds[static_cast<std::size_t>( i )].mins = { mins[0], mins[1],
                                                           mins[2] };
            bounds[static_cast<std::size_t>( i )].maxs = { maxs[0], maxs[1],
                                                           maxs[2] };
        }
    }

    return bounds;
}

} // namespace xash::server
