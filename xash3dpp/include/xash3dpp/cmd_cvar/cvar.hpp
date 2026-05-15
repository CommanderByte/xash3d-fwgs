#pragma once
// xash3dpp — cvar public types
// Legacy reference: common/cvardef.h, engine/common/cvar.h
//
// The Cvar struct starts with fields layout-identical to the legacy cvar_t
// (see common/cvardef.h).  The ABI shim in the host layer reinterpret_casts
// Cvar* to cvar_t* only there; cmd_cvar itself always uses Cvar*.
//
// NEVER reorder or insert fields before 'owner_flags' in the Cvar struct.

#include <atomic>
#include <cstdint>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// CvarFlags — mirrors the legacy FCVAR_* enum in common/cvardef.h exactly.
// Values that appear here must never change; game DLLs compare against them.
// ---------------------------------------------------------------------------

enum CvarFlags : std::uint32_t {
    FCVAR_ARCHIVE           = 1u << 0,  // saved to vars.rc on exit
    FCVAR_USERINFO          = 1u << 1,  // sent in userinfo to server
    FCVAR_SERVER            = 1u << 2,  // sent in serverinfo to clients
    FCVAR_EXTDLL            = 1u << 3,  // registered by server DLL
    FCVAR_CLIENTDLL         = 1u << 4,  // registered by client DLL
    FCVAR_PROTECTED         = 1u << 5,  // private server cvar (not sent)
    FCVAR_SPONLY            = 1u << 6,  // can be set in singleplayer only
    FCVAR_PRINTABLEONLY     = 1u << 7,  // only printable characters allowed
    FCVAR_UNLOGGED          = 1u << 8,  // do not notify clients of server-side change
    FCVAR_NOEXTRAWHITESPACE = 1u << 9,  // strip leading/trailing whitespace on set
    FCVAR_PRIVILEGED        = 1u << 10, // only executable in trusted (local) context
    FCVAR_FILTERABLE        = 1u << 11, // treated as privileged when cl_filterstuffcmd > 0
    FCVAR_GLCONFIG          = 1u << 12, // saved to <renderer>.cfg
    FCVAR_CHANGED           = 1u << 13, // set on every write; polled by legacy DLLs
    FCVAR_GAMEUIDLL         = 1u << 14, // registered by menu DLL
    FCVAR_CHEAT             = 1u << 15, // blocked when sv_cheats == 0
    FCVAR_RENDERINFO        = 1u << 16, // saved to video.cfg
    FCVAR_READ_ONLY         = 1u << 17, // display only; cannot be set by user
    FCVAR_EXTENDED          = 1u << 18, // reserved: future cvar_v2_t layout signal
    FCVAR_ALLOCATED         = 1u << 19, // string memory owned by the engine
    FCVAR_VIDRESTART        = 1u << 20, // triggers video-subsystem recreate
    FCVAR_TEMPORARY         = 1u << 21, // may be unlinked between map loads
    FCVAR_MOVEVARS          = 1u << 22, // mirrored in movevars_t
    FCVAR_USER_CREATED      = 1u << 23, // created by a "set" command (no DLL owner)
    FCVAR_DLL_WRAPPER       = 1u << 24, // pool-alloc'd engine wrapper around a DLL CvarAbi
    FCVAR_REFDLL            = 1u << 29, // registered by the renderer DLL
    FCVAR_LATCH             = 1u << 30, // change deferred until server restart
};

// ---------------------------------------------------------------------------
// CvarType — type hint for UI, autocomplete, and scripting layers.
// Not enforced by the core registry; used only for display/validation hints.
// ---------------------------------------------------------------------------

enum class CvarType : std::uint8_t {
    Unknown = 0,
    Bool,
    Int,
    Float,
    String,
    Flags,  // bit-field; format via help text
};

// ---------------------------------------------------------------------------
// CvarWriteSource — tracks who triggered the most recent write.
// Present in all builds; only stored conditionally (see Cvar, below).
// ---------------------------------------------------------------------------

enum class CvarWriteSource : std::uint8_t {
    init,           // initial value set at registration
    Console,        // typed at the local console
    ExecConfig,     // sourced from an exec'd .cfg file
    StuffCmd,       // arrived via server stuffcmd
    EngineInternal, // set by engine code (e.g. cheat-state reset)
    GameDLL,        // pfnCvar_DirectSet / pfnCvar_RegisterVariable
    ClientDLL,      // pfnRegisterVariable from the client DLL
    MenuDLL,        // pfnRegisterVariable from the menu DLL
    Script,         // set by an external scripting backend
};

// ---------------------------------------------------------------------------
// CvarAbi — ABI-stable struct, layout-identical to the legacy cvar_t.
//
// sizeof(CvarAbi) == sizeof(cvar_t) on both 32-bit and 64-bit targets.
// Verified by static_assert in cvar.cpp.
//
// The ABI shim reinterpret_casts CvarAbi* to cvar_t* when filling legacy
// DLL function tables.  No other code should perform this cast.
// ---------------------------------------------------------------------------

struct CvarAbi {
    char         *name;   // +0 on both 32/64-bit
    char         *string;
    std::uint32_t flags;
    float         value;
    CvarAbi      *next;   // ABI linked-list; only traversed for legacy Cvar_GetList
};

// ---------------------------------------------------------------------------
// Cvar — full engine-internal cvar record.
//
// 'abi' is the FIRST member and MUST remain at offset 0 so that
//   reinterpret_cast<CvarAbi*>(cvar_ptr)
// is equivalent to
//   &cvar_ptr->abi
// and is layout-compatible with the legacy cvar_t.
//
// The fields after 'abi' are opaque to legacy DLLs.
// ---------------------------------------------------------------------------

struct Cvar {
    // -- ABI-stable prefix -- keep 'abi' first, never insert before it --
    CvarAbi abi;

    // -- convar_t extensions (present in legacy engine-internal convar_s too) --
    const char *desc;        // human-readable description; may be nullptr
    const char *def_string;  // default value string; used to restore on Cvar_Unlink

    // -- Internal extensions (opaque to legacy DLLs) --

    // Incremented atomically on every write; allows lock-free change detection
    // by consumers that cache the last-seen generation value.
    std::atomic<std::uint32_t> generation { 0 };

    CvarType     type_hint  { CvarType::Unknown };
    float        range_min  { 0.0f };   // unconstrained when range_min > range_max
    float        range_max  { -1.0f };  // sentinel: min > max means unconstrained

    // Ownership tag set once at registration; used by cvar_unlink to identify
    // which cvars belong to a DLL being unloaded.
    std::uint32_t owner_flags { 0 };

#if XASH_STATS
    // Times this cvar has been written since registration.
    std::atomic<std::uint32_t> write_count { 0 };
    // Game frame number of the most recent write (game-thread-only write).
    std::uint32_t last_write_frame { 0 };
    // Who triggered the most recent write.
    CvarWriteSource last_write_source { CvarWriteSource::init };
#endif

#if XASH_DEBUG_CVARS
    // Pointer to a string literal of the form "file.cpp:123".
    // Set via XASH_CVAR_WRITE_LOCATION() helper macro in write path.
    const char *last_write_location { nullptr };
#endif
};

// Verify that the ABI struct is at offset 0 (required for the cast to be valid).
static_assert(__builtin_offsetof(Cvar, abi) == 0,
    "CvarAbi must be the first member of Cvar at offset 0");

// ---------------------------------------------------------------------------
// CvarDesc — copyable read-only snapshot for scripting / UI / autocomplete
// ---------------------------------------------------------------------------

struct CvarDesc {
    const char   *name;
    const char   *value;
    const char   *def_string;
    const char   *desc;
    std::uint32_t flags;
    CvarType      type_hint;
    float         range_min;
    float         range_max;
};

// ---------------------------------------------------------------------------
// CvarChangeRecord — one entry in the debug change log (XASH_DEBUG_CVARS)
// ---------------------------------------------------------------------------

#if XASH_DEBUG_CVARS
struct CvarChangeRecord {
    const char     *cvar_name;  // stable pointer into registry; never freed while log exists
    char            old_value[64];
    char            new_value[64];
    std::uint32_t   frame;
    CvarWriteSource source;
};
#endif

} // namespace xash::cmd_cvar
