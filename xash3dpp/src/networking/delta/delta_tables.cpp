// xash3dpp — DeltaTables lifecycle, table management, and game-DLL hooks
// Legacy reference: engine/common/net_encode.c — dt_info[] management,
// Delta_Init/InitClient/Shutdown, Delta_AddField, Delta_CustomEncode,
// Delta_AddEncoder, Delta_FindField, Delta_Set/UnsetField[ByIndex].

#include <xash3dpp/networking/delta.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/private/networking/delta/delta_tables_impl.hpp>
#include <xash3dpp/private/networking/delta/field_defs.hpp>
#include <xash3dpp/private/networking/delta/lst_parser.hpp>
#include <xash3dpp/utilities/string.hpp>


namespace xash::networking {

using delta::DeltaTable;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

DeltaTables::Impl::Impl() noexcept
{
    for( std::size_t i = 0; i < tables.size(); ++i )
    {
        const auto id  = static_cast<DeltaStructId>( i );
        tables[ i ].name = delta::table_name_for( id );
        tables[ i ].info = delta::field_info_for( id );
    }
}

DeltaTable *DeltaTables::Impl::find_struct( const char *name ) noexcept
{
    if( !name || !*name )
        return nullptr;

    for( auto &dt : tables )
    {
        if( ::xash::utilities::stricmp( dt.name, name ) == 0 )
            return &dt;
    }
    return nullptr;
}

DeltaTable *DeltaTables::Impl::find_struct_by_fields( const DeltaField *fields ) noexcept
{
    if( !fields )
        return nullptr;

    for( auto &dt : tables )
    {
        if( !dt.fields.empty() && dt.fields.data() == fields )
            return &dt;
    }
    return nullptr;
}

DeltaTable *DeltaTables::Impl::find_struct_by_encoder( const char *encoder_name ) noexcept
{
    if( !encoder_name || !*encoder_name )
        return nullptr;

    for( auto &dt : tables )
    {
        if( ::xash::utilities::stricmp( dt.func_name, encoder_name ) == 0 )
            return &dt;
    }
    return nullptr;
}

void DeltaTables::Impl::custom_encode( DeltaTable &dt,
                                       const void *from, const void *to ) noexcept
{
    // set all fields active by default
    for( auto &field : dt.fields )
        field.inactive = false;

    if( dt.user_callback )
        dt.user_callback( dt.fields.data(),
                          static_cast<const std::uint8_t *>( from ),
                          static_cast<const std::uint8_t *>( to ));
}

bool DeltaTables::Impl::add_field( DeltaTable &dt, const char *name,
                                   std::uint32_t flags, int bits,
                                   float multiplier, float post_multiplier ) noexcept
{
    // check for coexisting field — update in place
    for( auto &field : dt.fields )
    {
        if( ::xash::utilities::strcmp( field.name, name ) == 0 )
        {
            field.flags           = flags;
            field.bits            = bits;
            field.multiplier      = multiplier;
            field.post_multiplier = post_multiplier;
            return true;
        }
    }

    // find compile-time field description
    const delta::DeltaFieldInfo *info = nullptr;
    for( const auto &fi : dt.info )
    {
        if( ::xash::utilities::strcmp( fi.name, name ) == 0 )
        {
            info = &fi;
            break;
        }
    }

    if( !info )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "add_field: couldn't find description for %s->%s",
                    dt.name, name );
        return false;
    }

    if( dt.fields.size() + 1 > dt.info.size())
    {
        ::xash::core::logf( ::xash::core::LogLevel::Warning, "delta",
                    "add_field: can't add %s->%s encoder list is full",
                    dt.name, name );
        return false;
    }

    DeltaField field;
    field.name            = info->name;
    field.offset          = info->offset;
    field.size            = info->size;
    field.flags           = flags;
    field.bits            = bits;
    field.multiplier      = multiplier;
    field.post_multiplier = post_multiplier;
    dt.fields.push_back( field ); // @pre-reserved: info.size() (reset_tables)

    return true;
}

void DeltaTables::Impl::reset_tables() noexcept
{
    for( auto &dt : tables )
    {
        dt.fields.clear();
        dt.fields.reserve( dt.info.size()); // one table's worst case
        dt.custom_encode = delta::CustomEncodeKind::None;
        dt.user_callback = nullptr;
        dt.func_name[0]  = '\0';
        dt.initialized   = false;
    }
    initialized = false;
}

void DeltaTables::Impl::apply_movevars_fallback() noexcept
{
    using namespace delta;

    DeltaTable &dt = table( DeltaStructId::Movevars );

    if( dt.initialized )
        return; // "movevars_t" already specified by user script

    (void)add_field( dt, "gravity", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "stopspeed", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "maxspeed", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "spectatormaxspeed", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "accelerate", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "airaccelerate", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "wateraccelerate", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "friction", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "edgefriction", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "waterfriction", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "bounce", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "stepsize", k_dt_float | k_dt_signed, 16, 16.0f, 1.0f );
    (void)add_field( dt, "maxvelocity", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );

    // zmax: unsigned 24-bit so 3D-skybox maps aren't clamped at the 16-bit
    // signed max (legacy a1ba comment; see SV_UpdateMovevars).
    (void)add_field( dt, "zmax", k_dt_float, 24, 1.0f, 1.0f );

    (void)add_field( dt, "waveHeight", k_dt_float | k_dt_signed, 16, 16.0f, 1.0f );
    (void)add_field( dt, "skyName", k_dt_string, 1, 1.0f, 1.0f );
    (void)add_field( dt, "footsteps", k_dt_integer, 1, 1.0f, 1.0f );
    (void)add_field( dt, "rollangle", k_dt_float | k_dt_signed, 16, 32.0f, 1.0f );
    (void)add_field( dt, "rollspeed", k_dt_float | k_dt_signed, 16, 8.0f, 1.0f );
    (void)add_field( dt, "skycolor_r", k_dt_float | k_dt_signed, 16, 1.0f, 1.0f ); // 0 - 264
    (void)add_field( dt, "skycolor_g", k_dt_float | k_dt_signed, 16, 1.0f, 1.0f );
    (void)add_field( dt, "skycolor_b", k_dt_float | k_dt_signed, 16, 1.0f, 1.0f );
    (void)add_field( dt, "skyvec_x", k_dt_float | k_dt_signed, 16, 32.0f, 1.0f ); // 0 - 1
    (void)add_field( dt, "skyvec_y", k_dt_float | k_dt_signed, 16, 32.0f, 1.0f );
    (void)add_field( dt, "skyvec_z", k_dt_float | k_dt_signed, 16, 32.0f, 1.0f );
    (void)add_field( dt, "wateralpha", k_dt_float | k_dt_signed, 16, 32.0f, 1.0f );
    (void)add_field( dt, "fog_settings", k_dt_integer, 32, 1.0f, 1.0f );

    // Legacy re-asserts the count as ARRAYSIZE(pm_fields) - 4 == 27 (skydir
    // xyz + skyangle are never networked).  add_field appended exactly 27,
    // so this is a consistency check rather than a truncation.
    XASH_ASSERT( dt.fields.size() == delta::field_info_for(
        DeltaStructId::Movevars ).size() - 4 );

    dt.initialized = true;
}

// ---------------------------------------------------------------------------
// DeltaTables — lifecycle
// ---------------------------------------------------------------------------

DeltaTables::DeltaTables() noexcept : impl_{ std::make_unique<Impl>() }
{
    impl_->reset_tables();
}

DeltaTables::~DeltaTables() = default;
DeltaTables::DeltaTables( DeltaTables && ) noexcept            = default;
DeltaTables &DeltaTables::operator=( DeltaTables && ) noexcept = default;

bool DeltaTables::init( ::xash::filesystem::Filesystem &fs ) noexcept // compliance-allow(thread-assert): sim-thread single-thread caller contract (NOT NetIO — networking-threading.md flip nuance) — delta-table state has no internal sync; owning subsystem serialises init/parse/encode (networking-threading.md)
{
    const std::vector<std::byte> file = fs.load_file( "delta.lst" );
    if( file.empty())
    {
        // Legacy: Sys_Error — fatal.  Q-5: log at the public API and fail.
        ::xash::core::log( ::xash::core::LogLevel::Error, "delta",
                   "init: couldn't load file delta.lst" );
        return false;
    }

    return init_from_script( std::string_view{
        reinterpret_cast<const char *>( file.data()), file.size() }); // SAFETY: re-views the delta.lst byte buffer as char for text parsing; same object, byte<->char aliasing is well-defined
}

bool DeltaTables::init_from_script( std::string_view script ) noexcept
{
    // Legacy Delta_Init shuts down first when already initialised; a fresh
    // reset also seeds the per-table field reserves.
    impl_->reset_tables();

    if( !delta::parse_delta_lst( script, *impl_ ))
        return false;

    impl_->initialized = true;

    impl_->apply_movevars_fallback();

    impl_->stats.tables_parsed.fetch_add( 1, std::memory_order_relaxed );
    return true;
}

void DeltaTables::init_client() noexcept
{
    // already initialised (local game: server tables live in-process)
    if( impl_->initialized )
        return;

    int num_active = 0;
    for( auto &dt : impl_->tables )
    {
        if( !dt.fields.empty())
        {
            dt.initialized = true;
            ++num_active;
        }
    }

    if( num_active )
        impl_->initialized = true;
}

void DeltaTables::clear() noexcept // compliance-allow(thread-assert): sim-thread single-thread caller contract (NOT NetIO — networking-threading.md flip nuance) — delta-table state has no internal sync; owning subsystem serialises init/parse/encode (networking-threading.md)
{
    if( !impl_->initialized )
        return;

    impl_->reset_tables();
}

bool DeltaTables::is_initialized() const noexcept
{
    return impl_->initialized;
}

// ---------------------------------------------------------------------------
// Game-DLL hook surface
// ---------------------------------------------------------------------------

bool DeltaTables::register_encoder( const char *name, DeltaEncodeFn fn ) noexcept
{
    DeltaTable *dt = impl_->find_struct_by_encoder( name );

    if( !dt || !dt->initialized )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "register_encoder: couldn't find delta with specified custom encode %s",
                    name ? name : "(null)" );
        return false;
    }

    if( dt->custom_encode == delta::CustomEncodeKind::None )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "register_encoder: %s not supposed for custom encoding", dt->name );
        return false;
    }

    dt->user_callback = fn;
    return true;
}

int DeltaTables::find_field( const DeltaField *fields, const char *fieldname ) const noexcept
{
    const DeltaTable *dt = impl_->find_struct_by_fields( fields );
    if( !dt || !fieldname || !fieldname[0] )
        return -1;

    for( std::size_t i = 0; i < dt->fields.size(); ++i )
    {
        if( ::xash::utilities::strcmp( dt->fields[ i ].name, fieldname ) == 0 )
            return static_cast<int>( i );
    }
    return -1;
}

void DeltaTables::set_field( DeltaField *fields, const char *fieldname ) noexcept // compliance-allow(thread-assert): sim-thread single-thread caller contract (NOT NetIO — networking-threading.md flip nuance) — delta-table state has no internal sync; owning subsystem serialises init/parse/encode (networking-threading.md)
{
    DeltaTable *dt = impl_->find_struct_by_fields( fields );
    if( !dt || !fieldname || !fieldname[0] )
        return;

    for( auto &field : dt->fields )
    {
        if( ::xash::utilities::strcmp( field.name, fieldname ) == 0 )
        {
            field.inactive = false;
            return;
        }
    }
}

void DeltaTables::unset_field( DeltaField *fields, const char *fieldname ) noexcept
{
    DeltaTable *dt = impl_->find_struct_by_fields( fields );
    if( !dt || !fieldname || !fieldname[0] )
        return;

    for( auto &field : dt->fields )
    {
        if( ::xash::utilities::strcmp( field.name, fieldname ) == 0 )
        {
            field.inactive = true;
            return;
        }
    }
}

void DeltaTables::set_field_by_index( DeltaField *fields, int field_number ) noexcept // compliance-allow(thread-assert): sim-thread single-thread caller contract (NOT NetIO — networking-threading.md flip nuance) — delta-table state has no internal sync; owning subsystem serialises init/parse/encode (networking-threading.md)
{
    DeltaTable *dt = impl_->find_struct_by_fields( fields );
    if( !dt || field_number < 0
        || field_number >= static_cast<int>( dt->fields.size()))
        return;

    dt->fields[ static_cast<std::size_t>( field_number ) ].inactive = false;
}

void DeltaTables::unset_field_by_index( DeltaField *fields, int field_number ) noexcept
{
    DeltaTable *dt = impl_->find_struct_by_fields( fields );
    if( !dt || field_number < 0
        || field_number >= static_cast<int>( dt->fields.size()))
        return;

    dt->fields[ static_cast<std::size_t>( field_number ) ].inactive = true;
}

// ---------------------------------------------------------------------------
// Introspection + stats
// ---------------------------------------------------------------------------

bool DeltaTables::table_initialized( DeltaStructId id ) const noexcept
{
    return impl_->table( id ).initialized;
}

int DeltaTables::table_field_count( DeltaStructId id ) const noexcept
{
    return static_cast<int>( impl_->table( id ).fields.size());
}

DeltaField *DeltaTables::table_fields( DeltaStructId id ) noexcept
{
    auto &fields = impl_->table( id ).fields;
    return fields.empty() ? nullptr : fields.data();
}

const DeltaStats &DeltaTables::stats() const noexcept
{
    return impl_->stats;
}

} // namespace xash::networking
