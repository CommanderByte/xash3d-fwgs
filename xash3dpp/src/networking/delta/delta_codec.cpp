// xash3dpp — delta struct codecs and baseline testing
// Legacy reference: engine/common/net_encode.c — MSG_Write/ReadDeltaUsercmd,
// MSG_Write/ReadDeltaEvent, MSG_Write/ReadDeltaMovevars, MSG_Write/
// ReadClientData, MSG_Write/ReadWeaponData, MSG_Write/ReadDeltaEntity,
// Delta_TestBaseline, Delta_Write/ReadGSFields.
//
// The mark-bit framing loops go through the Xash IDeltaWireFormat sibling;
// the GS batch codec goes through the GoldSrc sibling.  Rollback behaviours
// (movevars, clientdata, weapon data, entity no-change) mirror the legacy
// MSG_SeekToBit patterns exactly.

#include <xash3dpp/networking/delta.hpp>

#include <xash3dpp/abi/entity_state.hpp>
#include <xash3dpp/abi/event_args.hpp>
#include <xash3dpp/abi/pm_movevars.hpp>
#include <xash3dpp/abi/usercmd.hpp>
#include <xash3dpp/abi/weaponinfo.hpp>
#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_tables_impl.hpp>
#include <xash3dpp/private/networking/delta/field_codec.hpp>
#include <xash3dpp/private/networking/delta/wire_format.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstring>

namespace xash::networking {

using delta::DeltaTable;

namespace {

// Wire-frozen weapon-index width (legacy MAX_WEAPON_BITS: 64 weapons).
inline constexpr int k_weapon_index_bits = 6;

// Legacy COM_NormalizeAngles (common.h): wrap each component to [-180, 180].
void normalize_angles( ::xash::abi::vec3_t angles ) noexcept
{
    for( int i = 0; i < 3; ++i )
    {
        if( angles[ i ] > 180.0f )
            angles[ i ] -= 360.0f;
        else if( angles[ i ] < -180.0f )
            angles[ i ] += 360.0f;
    }
}

[[nodiscard]] std::span<const DeltaField> fields_of( const DeltaTable &dt ) noexcept
{
    return { dt.fields.data(), dt.fields.size() };
}

// Tier-2 stats: measure always (the count is a wire-format return value),
// gate only the counter's existence on XASH_STATS.
void count_fields_written( DeltaStats &stats, std::size_t n ) noexcept
{
#if XASH_STATS
    stats.fields_changed_written.fetch_add( n, std::memory_order_relaxed );
#else
    (void)stats;
    (void)n;
#endif
}

void count_fields_read( DeltaStats &stats, std::size_t n ) noexcept
{
#if XASH_STATS
    stats.fields_changed_read.fetch_add( n, std::memory_order_relaxed );
#else
    (void)stats;
    (void)n;
#endif
}

} // namespace

// ---------------------------------------------------------------------------
// usercmd_t — client writes, server reads
// ---------------------------------------------------------------------------

void DeltaTables::write_delta_usercmd( MessageBuf &msg, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                       const ::xash::abi::usercmd_t *from,
                                       const ::xash::abi::usercmd_t *to ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::Usercmd );
    XASH_ASSERT( dt.initialized );

    impl_->custom_encode( dt, from, to );
    count_fields_written( impl_->stats,
        delta::xash_delta_wire_format().write_fields( msg, fields_of( dt ),
                                                      from, to, 0.0 ));
    impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
}

void DeltaTables::read_delta_usercmd( MessageBuf &msg,
                                      const ::xash::abi::usercmd_t *from,
                                      ::xash::abi::usercmd_t *to ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::Usercmd );
    XASH_ASSERT( dt.initialized );

    *to = *from;
    count_fields_read( impl_->stats,
        delta::xash_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                     from, to, 0.0 ));
    normalize_angles( to->viewangles );
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );
}

// ---------------------------------------------------------------------------
// event_args_t
// ---------------------------------------------------------------------------

void DeltaTables::write_delta_event( MessageBuf &msg, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                     const ::xash::abi::event_args_t *from,
                                     const ::xash::abi::event_args_t *to ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::Event );
    XASH_ASSERT( dt.initialized );

    impl_->custom_encode( dt, from, to );
    count_fields_written( impl_->stats,
        delta::xash_delta_wire_format().write_fields( msg, fields_of( dt ),
                                                      from, to, 0.0 ));
    impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
}

void DeltaTables::read_delta_event( MessageBuf &msg,
                                    const ::xash::abi::event_args_t *from,
                                    ::xash::abi::event_args_t *to ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::Event );
    XASH_ASSERT( dt.initialized );

    *to = *from;
    count_fields_read( impl_->stats,
        delta::xash_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                     from, to, 0.0 ));
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );
}

// ---------------------------------------------------------------------------
// movevars_t — command byte + rollback on zero changes
// ---------------------------------------------------------------------------

bool DeltaTables::write_delta_movevars( MessageBuf &msg, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                        const ::xash::abi::movevars_t *from,
                                        const ::xash::abi::movevars_t *to,
                                        std::uint32_t svc_deltamovevars_cmd ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::Movevars );
    XASH_ASSERT( dt.initialized );

    const std::size_t start_bit = msg.tell_bit();

    impl_->custom_encode( dt, from, to );

    msg.write_byte( static_cast<std::uint8_t>( svc_deltamovevars_cmd ));

    const std::size_t num_changes = delta::xash_delta_wire_format().write_fields(
        msg, fields_of( dt ), from, to, 0.0 );

    // if we have no changes — kill the message
    if( num_changes == 0 )
    {
        (void)msg.seek_to_bit( static_cast<std::ptrdiff_t>( start_bit ),
                               SeekOrigin::Begin );
#if XASH_STATS
        impl_->stats.rollbacks.fetch_add( 1, std::memory_order_relaxed );
#endif
        return false;
    }

    count_fields_written( impl_->stats, num_changes );
    impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
    return true;
}

void DeltaTables::read_delta_movevars( MessageBuf &msg,
                                       const ::xash::abi::movevars_t *from,
                                       ::xash::abi::movevars_t *to ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::Movevars );
    XASH_ASSERT( dt.initialized );

    *to = *from;
    count_fields_read( impl_->stats,
        delta::xash_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                     from, to, 0.0 ));
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );
}

// ---------------------------------------------------------------------------
// clientdata_t — "have clientdata" bit, rewritten to 0 on zero changes
// ---------------------------------------------------------------------------

void DeltaTables::write_clientdata( MessageBuf &msg, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                    const ::xash::abi::clientdata_t *from,
                                    const ::xash::abi::clientdata_t *to,
                                    double timebase ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::ClientData );
    XASH_ASSERT( dt.initialized );

    const std::size_t start_bit = msg.tell_bit();

    msg.write_one_bit( 1 ); // have clientdata

    impl_->custom_encode( dt, from, to );

    const std::size_t num_changes = delta::xash_delta_wire_format().write_fields(
        msg, fields_of( dt ), from, to, timebase );

    if( num_changes != 0 )
    {
        count_fields_written( impl_->stats, num_changes );
        impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
        return; // we have updates
    }

    (void)msg.seek_to_bit( static_cast<std::ptrdiff_t>( start_bit ),
                           SeekOrigin::Begin );
    msg.write_one_bit( 0 ); // no changes
#if XASH_STATS
    impl_->stats.rollbacks.fetch_add( 1, std::memory_order_relaxed );
#endif
}

void DeltaTables::read_clientdata( MessageBuf &msg,
                                   const ::xash::abi::clientdata_t *from,
                                   ::xash::abi::clientdata_t *to,
                                   double timebase ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::ClientData );
    XASH_ASSERT( dt.initialized );

    const bool no_changes = msg.read_one_bit() == 0;

    if( no_changes )
    {
        // legacy copies every field individually (not a struct assign)
        for( const DeltaField &field : fields_of( dt ))
            delta::copy_field( field, from, to );
        return;
    }

    count_fields_read( impl_->stats,
        delta::xash_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                     from, to, timebase ));
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );
}

// ---------------------------------------------------------------------------
// weapon_data_t — 1 bit + weapon index, fully rolled back on zero changes
// ---------------------------------------------------------------------------

void DeltaTables::write_weapon_data( MessageBuf &msg, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                     const ::xash::abi::weapon_data_t *from,
                                     const ::xash::abi::weapon_data_t *to,
                                     double timebase, int index ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::WeaponData );
    XASH_ASSERT( dt.initialized );

    impl_->custom_encode( dt, from, to );

    const std::size_t start_bit = msg.tell_bit();

    msg.write_one_bit( 1 );
    msg.write_ubit_long( static_cast<std::uint32_t>( index ), k_weapon_index_bits );

    const std::size_t num_changes = delta::xash_delta_wire_format().write_fields(
        msg, fields_of( dt ), from, to, timebase );

    // if we have no changes — kill the message
    if( num_changes == 0 )
    {
        (void)msg.seek_to_bit( static_cast<std::ptrdiff_t>( start_bit ),
                               SeekOrigin::Begin );
#if XASH_STATS
        impl_->stats.rollbacks.fetch_add( 1, std::memory_order_relaxed );
#endif
        return;
    }

    count_fields_written( impl_->stats, num_changes );
    impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
}

void DeltaTables::read_weapon_data( MessageBuf &msg,
                                    const ::xash::abi::weapon_data_t *from,
                                    ::xash::abi::weapon_data_t *to,
                                    double timebase ) noexcept
{
    DeltaTable &dt = impl_->table( DeltaStructId::WeaponData );
    XASH_ASSERT( dt.initialized );

    count_fields_read( impl_->stats,
        delta::xash_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                     from, to, timebase ));
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );
}

// ---------------------------------------------------------------------------
// entity_state_t
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] DeltaStructId entity_table_for( int entity_type,
                                              DeltaEntityKind kind ) noexcept
{
    if( entity_type & ::xash::abi::k_entity_beam )
        return DeltaStructId::CustomEntityState;
    if( kind == DeltaEntityKind::Player )
        return DeltaStructId::EntityStatePlayer;
    return DeltaStructId::EntityState;
}

} // namespace

bool DeltaTables::write_delta_entity( MessageBuf &msg, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                      const ::xash::abi::entity_state_t *from,
                                      const ::xash::abi::entity_state_t *to,
                                      const WriteDeltaEntityParams &params ) noexcept
{
    if( to == nullptr )
    {
        if( from == nullptr )
            return true; // nothing to do, like legacy

        // a NULL `to` is a delta remove message.
        // removeType: 1 — remove from delta message (keep states),
        //             2 — completely remove from server.
        msg.write_ubit_long( static_cast<std::uint32_t>( from->number ),
                             delta::k_entity_number_bits );
        msg.write_ubit_long( params.force ? 2u : 1u, delta::k_entity_remove_bits );
        return true;
    }

    if( to->number < 0 || ( params.max_edicts > 0 && to->number >= params.max_edicts ))
    {
        // legacy Host_Error — recoverable per Q-5: log and refuse to write.
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "write_delta_entity: bad entity number: %i", to->number );
        return false;
    }

    const std::size_t start_bit = msg.tell_bit();
    std::size_t num_changes = 0;

    msg.write_ubit_long( static_cast<std::uint32_t>( to->number ),
                         delta::k_entity_number_bits );
    msg.write_ubit_long( 0, delta::k_entity_remove_bits ); // alive

    if( params.baseline != 0 )
    {
        msg.write_one_bit( 1 );
        msg.write_sbit_long( params.baseline, delta::k_entity_baseline_bits );
    }
    else
    {
        msg.write_one_bit( 0 );
    }

    if( params.force || ( to->entityType != from->entityType ))
    {
        msg.write_one_bit( 1 );
        msg.write_ubit_long( static_cast<std::uint32_t>( to->entityType ),
                             delta::k_entity_type_bits );
        ++num_changes;
    }
    else
    {
        msg.write_one_bit( 0 );
    }

    DeltaTable &dt = impl_->table( entity_table_for( to->entityType, params.kind ));
    XASH_ASSERT( dt.initialized );

    if( params.kind == DeltaEntityKind::Static )
    {
        // static entities won't be custom encoded
        for( auto &field : dt.fields )
            field.inactive = false;
    }
    else
    {
        impl_->custom_encode( dt, from, to );
    }

    const std::size_t field_changes = delta::xash_delta_wire_format().write_fields(
        msg, fields_of( dt ), from, to, params.timebase );
    num_changes += field_changes;

    // if we have no changes — kill the message
    if( num_changes == 0 && !params.force )
    {
        (void)msg.seek_to_bit( static_cast<std::ptrdiff_t>( start_bit ),
                               SeekOrigin::Begin );
#if XASH_STATS
        impl_->stats.rollbacks.fetch_add( 1, std::memory_order_relaxed );
#endif
        return true;
    }

    count_fields_written( impl_->stats, field_changes );
    impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
    return true;
}

bool DeltaTables::read_delta_entity( MessageBuf &msg,
                                     const ::xash::abi::entity_state_t *from,
                                     ::xash::abi::entity_state_t *to,
                                     const ReadDeltaEntityParams &params ) noexcept
{
    if( params.number < 0
        || ( params.max_entities > 0 && params.number >= params.max_entities ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "read_delta_entity: bad delta entity number: %i", params.number );
        return false;
    }

    const std::uint32_t remove_type =
        msg.read_ubit_long( delta::k_entity_remove_bits );

    if( remove_type != 0 )
    {
        // check for a remove
        std::memset( to, 0, sizeof( *to ));

        if( remove_type & 1 )
            return false; // removed from delta-message

        if( remove_type & 2 )
        {
            to->number = -1; // entity was removed from server
            return false;
        }

        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "read_delta_entity: unknown update type %u", remove_type );
        return false;
    }

    int baseline_offset = 0;
    if( msg.read_one_bit())
        baseline_offset = msg.read_sbit_long( delta::k_entity_baseline_bits );

    if( baseline_offset != 0 && params.baselines != nullptr )
    {
        const ::xash::abi::entity_state_t *resolved =
            params.baselines->resolve( baseline_offset, params.kind );
        if( resolved != nullptr )
            from = resolved;
    }

    *to = *from;

    if( msg.read_one_bit())
        to->entityType = static_cast<int>(
            msg.read_ubit_long( delta::k_entity_type_bits ));
    to->number = params.number;

    DeltaTable &dt = impl_->table( entity_table_for( to->entityType, params.kind ));

    if( !dt.initialized )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "delta", "read_delta_entity: broken delta" );
        return true; // message parsed, like legacy
    }

    count_fields_read( impl_->stats,
        delta::xash_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                     from, to, params.timebase ));
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );

    return true; // message parsed
}

// ---------------------------------------------------------------------------
// test_baseline — legacy Delta_TestBaseline
// ---------------------------------------------------------------------------

int DeltaTables::test_baseline( const ::xash::abi::entity_state_t *from,
                                const ::xash::abi::entity_state_t *to,
                                bool player, double timebase ) noexcept
{
    int count_bits = delta::k_entity_number_bits + 2;

    if( to == nullptr )
    {
        if( from == nullptr )
            return 0;
        return count_bits;
    }

    DeltaTable &dt = impl_->table( entity_table_for(
        to->entityType, player ? DeltaEntityKind::Player : DeltaEntityKind::Entity ));
    XASH_ASSERT( dt.initialized );

    ++count_bits; // entityType flag

    // activate fields and call custom encode func
    impl_->custom_encode( dt, from, to );

    for( const DeltaField &field : fields_of( dt ))
    {
        ++count_bits; // per-field change flag (always sent)

        if( !delta::compare_field( field, from, to ))
        {
            if( field.flags & delta::k_dt_string )
                count_bits += static_cast<int>( ::xash::utilities::strlen(
                    reinterpret_cast<const char *>( to ) + field.offset )) * 8; // SAFETY: byte-addresses a string field within the caller's struct at its ABI offset (from/to share the game-struct layout)
            else
                count_bits += field.bits;
        }
    }

    return count_bits;
}

// ---------------------------------------------------------------------------
// GoldSrc batch codec — legacy Delta_Write/ReadGSFields
// ---------------------------------------------------------------------------

void DeltaTables::write_gs_fields( MessageBuf &msg, DeltaStructId id, // compliance-allow(thread-assert): sim-thread single-thread caller contract — NOT stateless: writes reach Impl delta state (tables + custom-encode inactive flags); confinement per networking-threading.md, asserts land at the sim/NetIO flip
                                   const void *from, const void *to,
                                   double timebase ) noexcept
{
    DeltaTable &dt = impl_->table( id );

    impl_->custom_encode( dt, from, to );
    count_fields_written( impl_->stats,
        delta::goldsrc_delta_wire_format().write_fields( msg, fields_of( dt ),
                                                         from, to, timebase ));
    impl_->stats.structs_encoded.fetch_add( 1, std::memory_order_relaxed );
}

void DeltaTables::read_gs_fields( MessageBuf &msg, DeltaStructId id,
                                  const void *from, void *to,
                                  double timebase ) noexcept
{
    DeltaTable &dt = impl_->table( id );

    count_fields_read( impl_->stats,
        delta::goldsrc_delta_wire_format().read_fields( msg, fields_of( dt ),
                                                        from, to, timebase ));
    impl_->stats.structs_decoded.fetch_add( 1, std::memory_order_relaxed );
}

} // namespace xash::networking
