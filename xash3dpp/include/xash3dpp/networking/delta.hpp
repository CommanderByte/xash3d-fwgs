#pragma once
// xash3dpp — delta encoder public types (Layer 4)
// Legacy reference: engine/common/net_encode.h
//
// The delta encoder serialises differences between two instances of a frozen
// SDK struct (entity_state_t, usercmd_t, ...) into a bit stream.  Which wire
// framing is used (Xash per-field mark bits vs. GoldSrc byte-group masks) is
// selected via IProtocolDriver::delta_tables() — see
// private/networking/delta/wire_format.hpp.
//
// This header carries the value types shared across the delta TUs and the
// game-DLL-facing token types.  The DeltaTables lifecycle class is added by
// the table-management layer (see docs/boundaries/networking-boundary.md,
// Delta encoder section).

#include <cstdint>

namespace xash::networking {

// ---------------------------------------------------------------------------
// DeltaStructId — identity of one delta description table.
// Wire-frozen: the enumerator order IS the legacy DT_* table index sent in
// svc_deltatable's 4-bit tableIndex field.  Never reorder.
// ---------------------------------------------------------------------------

enum class DeltaStructId : std::uint8_t
{
    Event = 0,         // DT_EVENT_T                "event_t"
    Movevars,          // DT_MOVEVARS_T             "movevars_t"
    Usercmd,           // DT_USERCMD_T              "usercmd_t"
    ClientData,        // DT_CLIENTDATA_T           "clientdata_t"
    WeaponData,        // DT_WEAPONDATA_T           "weapon_data_t"
    EntityState,       // DT_ENTITY_STATE_T         "entity_state_t"
    EntityStatePlayer, // DT_ENTITY_STATE_PLAYER_T  "entity_state_player_t"
    CustomEntityState, // DT_CUSTOM_ENTITY_STATE_T  "custom_entity_state_t"
    Count,
};

// ---------------------------------------------------------------------------
// DeltaEntityKind — which entity table an entity delta uses.
// Matches legacy DELTA_ENTITY / DELTA_PLAYER / DELTA_STATIC ("don't change
// order!").  Static entities skip the custom-encode callback.
// ---------------------------------------------------------------------------

enum class DeltaEntityKind : std::uint8_t
{
    Entity = 0,
    Player,
    Static,
};

// ---------------------------------------------------------------------------
// DeltaField — one runtime field descriptor (legacy `delta_t`).
//
// Game DLLs receive a DeltaField* array as the opaque token in their
// registered encode callback and mutate it ONLY through the engine's
// find/set/unset helpers.  A DLL that dereferences legacy `delta_s*`
// directly would see a different layout — documented quirk, revisited by
// the Chunk 6 ABI shim audit (see networking-boundary.md).
// ---------------------------------------------------------------------------

struct DeltaField
{
    const char   *name            { nullptr }; // points at static field-info data
    int           offset          { 0 };       // byte offset inside the SDK struct
    int           size            { 0 };       // bytes; bounds check for DT_STRING
    std::uint32_t flags           { 0 };       // DT_* bits (private delta_types.hpp)
    float         multiplier      { 1.0f };
    float         post_multiplier { 1.0f };    // DEFINE_DELTA_POST decode scale
    int           bits            { 0 };       // wire width
    bool          inactive        { false };   // unset by custom-encode request
};

// ---------------------------------------------------------------------------
// DeltaEncodeFn — game-DLL conditional-encode callback (legacy pfnDeltaEncode).
// Registered via the table manager; called before every write with all fields
// re-activated, may deactivate fields via set/unset helpers.  Raw function
// pointer so the future enginefuncs_t shim forwards without adaptation.
// `noexcept` deliberately absent: DLL function pointers are not noexcept-typed.
// ---------------------------------------------------------------------------

using DeltaEncodeFn = void ( * )( DeltaField *fields,
                                  const std::uint8_t *from,
                                  const std::uint8_t *to );

} // namespace xash::networking
