// xash3dpp — delta table-descriptor wire codec (table sync at connect)
// Legacy reference: engine/common/net_encode.c — Delta_WriteTableField,
// Delta_WriteDescriptionToClient, Delta_ParseTableField,
// Delta_ParseTableField_GS.
//
// Xash descriptor wire, per field (after the caller-supplied command byte):
//   tableIndex UBit4, nameIndex UBit8, flags UBit10, (bits-1) UBit5,
//   multiplier: 1 bit + optional float (omitted at 1.0), post_multiplier same.
//
// GoldSrc description: struct-name string, field count short, then per field
// one goldsrc_delta_t record framed by the GS group-mask format through the
// immutable meta-table; DT_SIGNED_GS (bit 31) remaps to DT_SIGNED (bit 8);
// premultiply/postmultiply floats arrive pre-scaled by 4000 through the
// meta-table multipliers.  Legacy brackets the parse in MSG_StartBitWriting /
// MSG_EndBitWriting — the end bracket byte-aligns the cursor, replicated here.

#include <xash3dpp/networking/delta.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_tables_impl.hpp>
#include <xash3dpp/private/networking/delta/field_defs.hpp>
#include <xash3dpp/private/networking/delta/wire_format.hpp>
#include <xash3dpp/utilities/string.hpp>

namespace xash::networking {

using delta::DeltaTable;

namespace {

// GS description struct-name read buffer; longest real name is
// "custom_entity_state_t" (21 chars).
constexpr std::size_t k_gs_struct_name_max = 64;

// Legacy Delta_WriteTableField.
void write_table_field( MessageBuf &msg, std::uint32_t svc_deltatable_cmd, // compliance-allow(thread-assert): stateless delta codec — pure wire transform, no thread affinity
                        int table_index, const DeltaTable &dt,
                        const DeltaField &field ) noexcept
{
    if( !field.name || !field.name[0] )
        return; // not initialized?

    XASH_ASSERT( dt.initialized );

    // nameIndex — position of this field inside the compile-time info array
    int name_index = -1;
    for( std::size_t i = 0; i < dt.info.size(); ++i )
    {
        if( ::xash::utilities::strcmp( dt.info[ i ].name, field.name ) == 0 )
        {
            name_index = static_cast<int>( i );
            break;
        }
    }
    XASH_ASSERT( name_index >= 0 );
    if( name_index < 0 )
        return;

    msg.write_byte( static_cast<std::uint8_t>( svc_deltatable_cmd ));
    msg.write_ubit_long( static_cast<std::uint32_t>( table_index ),
                         delta::k_table_index_bits );
    msg.write_ubit_long( static_cast<std::uint32_t>( name_index ),
                         delta::k_name_index_bits );
    msg.write_ubit_long( field.flags, delta::k_field_flags_bits );
    msg.write_ubit_long( static_cast<std::uint32_t>( field.bits - 1 ),
                         delta::k_field_bits_bits );

    // multipliers are null-compressed
    if( !delta::q_equal( field.multiplier, 1.0f ))
    {
        msg.write_one_bit( 1 );
        msg.write_float( field.multiplier );
    }
    else
    {
        msg.write_one_bit( 0 );
    }

    if( !delta::q_equal( field.post_multiplier, 1.0f ))
    {
        msg.write_one_bit( 1 );
        msg.write_float( field.post_multiplier );
    }
    else
    {
        msg.write_one_bit( 0 );
    }
}

} // namespace

// ---------------------------------------------------------------------------
// write_description — legacy Delta_WriteDescriptionToClient
// ---------------------------------------------------------------------------

void DeltaTables::write_description( MessageBuf &msg, // compliance-allow(thread-assert): stateless delta codec — pure wire transform, no thread affinity
                                     std::uint32_t svc_deltatable_cmd ) noexcept
{
    for( std::size_t t = 0; t < impl_->tables.size(); ++t )
    {
        const DeltaTable &dt = impl_->tables[ t ];

        for( const DeltaField &field : dt.fields )
            write_table_field( msg, svc_deltatable_cmd,
                               static_cast<int>( t ), dt, field );
    }
}

// ---------------------------------------------------------------------------
// parse_table_field — legacy Delta_ParseTableField (Xash path)
// ---------------------------------------------------------------------------

bool DeltaTables::parse_table_field( MessageBuf &msg ) noexcept
{
    const std::uint32_t table_index =
        msg.read_ubit_long( delta::k_table_index_bits );

    // Legacy blind-indexes dt_info[tableIndex] (4-bit wire value vs 8 tables
    // — a wire-reachable overread); bounded here and treated like the legacy
    // Host_Error "not initialized" path.
    if( table_index >= static_cast<std::uint32_t>( DeltaStructId::Count ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_table_field: bad table index %u", table_index );
        return false;
    }

    DeltaTable &dt = impl_->tables[ table_index ];

    const std::uint32_t name_index =
        msg.read_ubit_long( delta::k_name_index_bits );
    const char *name   = nullptr;
    bool        ignore = false;

    if( name_index < dt.info.size())
    {
        name = dt.info[ name_index ].name;
    }
    else
    {
        ignore = true;
        ::xash::core::logf( ::xash::core::LogLevel::Warning, "delta",
                    "parse_table_field: wrong nameIndex %u for table %s, ignoring",
                    name_index, dt.name );
    }

    const std::uint32_t flags = msg.read_ubit_long( delta::k_field_flags_bits );
    const int bits = static_cast<int>(
        msg.read_ubit_long( delta::k_field_bits_bits )) + 1;

    float mul = 1.0f, post_mul = 1.0f;
    if( msg.read_one_bit())
        mul = msg.read_float();
    if( msg.read_one_bit())
        post_mul = msg.read_float();

    if( ignore )
        return true; // descriptor consumed, field skipped (legacy behaviour)

    // delta encoders already initialised on this machine (local game):
    // the first wire descriptor wipes ALL local tables (legacy quirk).
    if( impl_->initialized )
        impl_->reset_tables();

    (void)impl_->add_field( dt, name, flags, bits, mul, post_mul );

    impl_->stats.tables_parsed.fetch_add( 1, std::memory_order_relaxed );
    return true;
}

// ---------------------------------------------------------------------------
// parse_table_gs — legacy Delta_ParseTableField_GS
// ---------------------------------------------------------------------------

bool DeltaTables::parse_table_gs( MessageBuf &msg ) noexcept
{
    char name[ k_gs_struct_name_max ] = {};
    (void)msg.read_string({ name, sizeof( name ) });

    DeltaTable *dt = impl_->find_struct( name );

    // Legacy order: the local-game wipe happens BEFORE the null check.
    if( impl_->initialized )
        impl_->reset_tables();

    if( !dt )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_table_gs: unknown struct %s", name );
        return false;
    }

    const int num_fields = msg.read_short();
    if( num_fields > static_cast<int>( dt->info.size()))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_table_gs: numFields %d > maxFields %zu for %s",
                    num_fields, dt->info.size(), dt->name );
        return false;
    }

    const delta::goldsrc_delta_t null_desc {};

    for( int i = 0; i < num_fields; ++i )
    {
        delta::goldsrc_delta_t to {};

        delta::goldsrc_delta_wire_format().read_fields(
            msg, delta::k_goldsrc_meta_runtime, &null_desc, &to, 0.0 );

        // patch our DT_SIGNED flag
        std::uint32_t field_type = static_cast<std::uint32_t>( to.fieldType );
        if( field_type & delta::k_dt_signed_gs )
        {
            field_type &= ~delta::k_dt_signed_gs;
            field_type |= delta::k_dt_signed;
        }

        (void)impl_->add_field( *dt, to.fieldName, field_type,
                                to.significant_bits,
                                to.premultiply, to.postmultiply );
    }

    // Legacy MSG_EndBitWriting pads the cursor to the next byte boundary.
    const std::size_t misalign = msg.tell_bit() & 7u;
    if( misalign != 0 )
        (void)msg.seek_to_bit( static_cast<std::ptrdiff_t>( 8u - misalign ),
                               SeekOrigin::Current );

    impl_->stats.tables_parsed.fetch_add( 1, std::memory_order_relaxed );
    return true;
}

} // namespace xash::networking
