#pragma once
// xash3dpp — game DLL loader: the SV_LoadProgs export handshake
// Legacy reference: engine/server/sv_game.c — SV_LoadProgs (:5214, the
// library/negotiation part; edict-array + string-pool + GameInit
// orchestration belongs to lifecycle), SV_UnloadProgs (:5171, ditto),
// LINK_ENTITY dispatch by raw classname (SV_AllocPrivateData :1092),
// export typedefs (:35-40); sv_pmove.c :442-462 (SV_InitClientMove hull
// enumeration → map_loader WorldLoadOptions::hull_bounds).
// Deep dive: docs/legacy-survey/deep-dive-server-game-dll-bridge.md §3.
//
// Handshake ORDER is ABI-visible (shipped DLLs latch state per call):
//   1. resolve GetEntityAPI / GetEntityAPI2 / GetNewDLLFunctions — missing
//      both EntityAPI exports is fatal;
//   2. resolve GiveFnptrsToDll — missing is fatal;
//   3. GiveFnptrsToDll( engfuncs, globals ) FIRST, before any Get*API;
//   4. GetNewDLLFunctions (optional; version-reject zeroes the table);
//   5. GetEntityAPI2 with version = 140 by pointer — accepted only when
//      the echoed version still equals 140 ("extended EntityAPI");
//   6. else fall back to GetEntityAPI (version by value, possibly the
//      value the failed API2 negotiation wrote back — legacy quirk).
// The DLL persists across map changes: load() on a loaded instance
// early-returns true; unload() runs only on shutdown/game switch.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/platform/platform.hpp>

#include <string>
#include <unordered_map>

namespace xash::server {

class GameDll
{
public:
    enum class LoadError
    {
        None,
        LibraryNotFound,    // open_library failed
        MissingEntityApi,   // neither GetEntityAPI nor GetEntityAPI2
        MissingGiveFnptrs,  // no GiveFnptrsToDll export
        EntityApiInitFailed // both negotiation paths returned false
    };

    GameDll() = default;

    GameDll( const GameDll & )            = delete;
    GameDll &operator=( const GameDll & ) = delete;

    // The caller owns `table` and `globals` for the DLL's whole lifetime
    // (legacy: function-local statics in SV_LoadProgs — the DLL keeps the
    // raw pointers).  On failure the library is freed and last_error()
    // says why; on success funcs() is populated.
    [[nodiscard]] bool load( const char *path,
                             ::xash::abi::enginefuncs_t *table,
                             ::xash::abi::globalvars_t  *globals );

    // Frees the library and clears the tables.  Contract: the lifecycle
    // orchestrator (S7 SV_UnloadProgs port) calls
    // new_funcs().pfnGameShutdown and unwinds game state BEFORE this.
    void unload();

    [[nodiscard]] bool loaded() const noexcept
    {
        return static_cast<bool>( lib_ );
    }
    [[nodiscard]] LoadError last_error() const noexcept { return error_; }

    // true → GetEntityAPI2 path ("extended EntityAPI"), false → legacy.
    [[nodiscard]] bool extended_api() const noexcept { return extended_; }
    // true → GetNewDLLFunctions returned success (table is populated).
    [[nodiscard]] bool has_new_api() const noexcept { return has_new_; }

    [[nodiscard]] const ::xash::abi::DLL_FUNCTIONS &funcs() const noexcept
    {
        return funcs_;
    }
    [[nodiscard]] const ::xash::abi::NEW_DLL_FUNCTIONS &new_funcs() const noexcept
    {
        return new_funcs_;
    }

    // LINK_ENTITY dispatch: per-classname spawn export resolved by raw
    // name (legacy COM_GetProcAddress( hInstance, classname )).
    [[nodiscard]] ::xash::abi::LINK_ENTITY_FUNC
    entity_link( const char *classname ) const noexcept;

    // Raw export lookup (pfnFunctionFromName support — the FIELD_FUNCTION
    // forward half: COM_FunctionFromName, sv_game.c:3569).
    [[nodiscard]] void *symbol( const char *name ) const noexcept;

    // FIELD_FUNCTION reverse half (pfnNameForFunction / COM_NameForFunction,
    // sv_game.c:3581): resolve an exported function HANDLE back to its name so
    // the save file stores the NAME, not the process-local address.  The handle
    // is the frozen-ABI `unsigned long` pfnFunctionFromName returned — on Win64
    // (LLP64) that is the LOW 32 bits of the real pointer, so the cache is keyed
    // by the SAME `static_cast<unsigned long>` truncation the forward applies,
    // keeping both directions self-consistent regardless of arch (a real module
    // is < 4 GiB, so the low 32 bits are unique within it).  SAV-OQ-3: the
    // reverse-walk primitive is platform-owned (platform::enumerate_exports /
    // name_for_symbol); this is save's per-DLL glue — a lazily-built
    // handle->name ordinal cache over THIS module's exports, valid for the DLL's
    // lifetime (cleared on unload).  Returns nullptr for a handle that matches
    // no named export.  @lifetime: the returned pointer aliases the cache and is
    // valid until unload().
    [[nodiscard]] const char *name_for_function( unsigned long handle ) noexcept;

private:
    ::xash::platform::LibHandle    lib_;
    ::xash::abi::DLL_FUNCTIONS     funcs_{};
    ::xash::abi::NEW_DLL_FUNCTIONS new_funcs_{};
    bool                           extended_ = false;
    bool                           has_new_  = false;
    LoadError                      error_    = LoadError::None;

    // FIELD_FUNCTION reverse-lookup ordinal cache (SAV-OQ-3 glue), built once
    // per DLL load via platform::enumerate_exports.  @lifetime: GameDll.
    std::unordered_map<unsigned long, std::string> reverse_cache_;
    bool                                           reverse_cache_built_ = false;
};

// SV_InitClientMove hull enumeration: pfnGetHullBounds for hulls 0..3.
// Legacy parity: the destination table starts ZEROED (host.player_mins /
// host.player_maxs are zero-initialised globals) and a slot is written
// only when the game returns nonzero — a game without hull `i` leaves
// that entry zero, exactly as legacy does.  Feed the result to
// map_loader WorldLoadOptions::hull_bounds.
[[nodiscard]] ::xash::map_loader::HullBoundsTable
query_hull_bounds( const ::xash::abi::DLL_FUNCTIONS &funcs ) noexcept;

} // namespace xash::server
