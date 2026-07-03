#pragma once
// xash3dpp — DeltaTables::Impl, shared between the delta TUs
// (delta_tables.cpp lifecycle, table_wire.cpp descriptors, delta_codec.cpp
// struct codecs).  Precedent: private/networking/context_impl.hpp.
//
// Legacy equivalent: the static dt_info[] array + delta_init flag +
// Delta_FindStruct* helpers in net_encode.c.

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>

#include <array>
#include <cstddef>

namespace xash::networking {

struct DeltaTables::Impl
{
    std::array<delta::DeltaTable,
               static_cast<std::size_t>( DeltaStructId::Count )> tables {};
    bool       initialized { false }; // legacy `delta_init`
    DeltaStats stats {};

    Impl() noexcept; // binds names + field-info spans (delta_tables.cpp)

    [[nodiscard]] delta::DeltaTable &table( DeltaStructId id ) noexcept
    {
        return tables[ static_cast<std::size_t>( id ) ];
    }

    // Case-insensitive name lookup (legacy Delta_FindStruct; logs nothing —
    // callers decide the diagnostic).  nullptr when not found.
    [[nodiscard]] delta::DeltaTable *find_struct( const char *name ) noexcept;

    // Pointer-identity lookup from a game-DLL field token
    // (legacy Delta_FindStructByDelta).
    [[nodiscard]] delta::DeltaTable *
    find_struct_by_fields( const DeltaField *fields ) noexcept;

    // funcName lookup (legacy Delta_FindStructByEncoder).
    [[nodiscard]] delta::DeltaTable *
    find_struct_by_encoder( const char *encoder_name ) noexcept;

    // Re-activate all fields then run the table's user callback
    // (legacy Delta_CustomEncode).
    void custom_encode( delta::DeltaTable &dt,
                        const void *from, const void *to ) noexcept;

    // Append-or-update one field descriptor (legacy Delta_AddField).
    // Returns false when the name is unknown to the table's field-info or
    // the table is full.
    [[nodiscard]] bool add_field( delta::DeltaTable &dt, const char *name,
                                  std::uint32_t flags, int bits,
                                  float multiplier, float post_multiplier ) noexcept;

    // Reset every table (legacy Delta_Shutdown body).
    void reset_tables() noexcept;

    // Built-in movevars_t table (legacy Delta_Init fallback block).
    void apply_movevars_fallback() noexcept;
};

} // namespace xash::networking
