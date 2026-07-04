// xash3dpp — server precache index registries (Chunk 6 S7)
// Legacy reference: engine/server/sv_init.c :103-274 (the four index
// registrars), sv_game.c :1315 (pfnModelIndex lookup).
//
// Existing subsystems used:
//   xash3dpp_memory     — one pool block per table
//   xash3dpp_utilities  — strncpy/stricmp (Q_* ports), fix_slashes
//   xash3dpp_core       — logging, thread-role assert

#include <xash3dpp/private/server/precache.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstdio>
#include <cstring>

namespace xash::server {

namespace ut = ::xash::utilities;

namespace {

// Legacy MAX_QPATH slot width (matches limits::map_qpath_max).
inline constexpr std::size_t k_qpath = ::xash::limits::map_qpath_max;

// SV_ModelIndex/SV_SoundIndex name preparation: optionally strip ONE
// leading slash, bounded copy, then COM_FixSlashes.
void prepare_name( char ( &dst )[k_qpath], const char *src, bool strip_lead )
{
    if ( strip_lead && ( *src == '\\' || *src == '/' ))
        ++src;
    ut::strncpy( dst, src, sizeof( dst ));
    ut::fix_slashes( dst );
}

} // namespace

char *PrecacheTables::Table::slot( std::size_t i ) const noexcept
{
    return names + i * k_qpath;
}

bool PrecacheTables::init( ::xash::memory::PoolHandle pool,
                           const PrecacheCaps &caps )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    pool_ = pool;
    caps_ = caps;

    Table *tables[4]         = { &models_, &sounds_, &events_, &generics_ };
    const std::size_t cap[4] = { caps.models, caps.sounds, caps.events,
                                 caps.generics };
    for ( int i = 0; i < 4; ++i )
    {
        tables[i]->cap   = cap[i];
        tables[i]->names = static_cast<char *>(
            ::xash::memory::mem_calloc( pool, cap[i] * k_qpath ));
        if ( tables[i]->names == nullptr )
            return false;
    }

    model_flags_ = static_cast<std::uint32_t *>( ::xash::memory::mem_calloc(
        pool, caps.models * sizeof( std::uint32_t )));
    return model_flags_ != nullptr;
}

void PrecacheTables::shutdown()
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( Table *t : { &models_, &sounds_, &events_, &generics_ } )
    {
        if ( t->names != nullptr )
            ::xash::memory::mem_free( t->names );
        *t = {};
    }
    if ( model_flags_ != nullptr )
    {
        ::xash::memory::mem_free( model_flags_ );
        model_flags_ = nullptr;
    }
    pool_ = {};
}

void PrecacheTables::clear()
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( Table *t : { &models_, &sounds_, &events_, &generics_ } )
    {
        if ( t->names != nullptr )
            std::memset( t->names, 0, t->cap * k_qpath );
    }
    if ( model_flags_ != nullptr )
        std::memset( model_flags_, 0, caps_.models * sizeof( std::uint32_t ));
}

void PrecacheTables::error( const char *fmt, std::size_t limit )
{
    char msg[128];
    std::snprintf( msg, sizeof( msg ), fmt, static_cast<int>( limit ));
    if ( error_hook_ != nullptr )
        error_hook_( error_hook_ctx_, msg );
    else
        ::xash::core::log_error( "server", msg );
}

void PrecacheTables::late_notify( PrecacheKind kind, const char *name,
                                  int index, std::uint32_t flags, bool warn )
{
    // Legacy order: SV_SendSingleResource first, then the console warning
    // (models/sounds only — events/generic broadcast silently).
    if ( late_sink_ != nullptr )
        late_sink_( late_sink_ctx_, kind, name, index, flags );
    if ( warn )
        ::xash::core::logf( ::xash::core::LogLevel::Warning, "server",
                            "late precache of %s", name );
}

// The shared scan/register kernel: dedup stops at the first empty slot,
// matches are case-insensitive, index 0 is never used.  `fresh` reports
// whether a new slot was written — the late-precache path fires only for
// new registrations (legacy returns early on a dedup hit).
int PrecacheTables::index_in( Table &t, const char *prepared,
                              const char *limit_msg, bool &fresh )
{
    fresh = false;

    std::size_t i = 1;
    for ( ; i < t.cap && t.slot( i )[0] != '\0'; ++i )
    {
        if ( ut::stricmp( t.slot( i ), prepared ) == 0 )
            return static_cast<int>( i );
    }

    if ( i == t.cap )
    {
        error( limit_msg, t.cap );
        return 0;
    }

    ut::strncpy( t.slot( i ), prepared, k_qpath );
    fresh = true;
    return static_cast<int>( i );
}

int PrecacheTables::model_index( const char *name )
{
    if ( name == nullptr || name[0] == '\0' )
        return 0;

    char prepared[k_qpath];
    prepare_name( prepared, name, /*strip_lead=*/true );

    bool fresh = false;
    const int i = index_in( models_, prepared,
                            "MAX_MODELS limit exceeded (%d)\n", fresh );
    if ( fresh && !loading_ )
        late_notify( PrecacheKind::Model, prepared, i, model_flags_[i],
                     /*warn=*/true );
    return i;
}

int PrecacheTables::sound_index( const char *name )
{
    if ( name == nullptr || name[0] == '\0' )
        return 0;

    if ( name[0] == '!' )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Warning, "server",
                            "'%s' do not precache sentence names!", name );
        return 0;
    }

    char prepared[k_qpath];
    prepare_name( prepared, name, /*strip_lead=*/true );

    bool fresh = false;
    const int i = index_in( sounds_, prepared,
                            "MAX_SOUNDS limit exceeded (%d)\n", fresh );
    if ( fresh && !loading_ )
        late_notify( PrecacheKind::Sound, prepared, i, 0, /*warn=*/true );
    return i;
}

int PrecacheTables::event_index( const char *name )
{
    if ( name == nullptr || name[0] == '\0' )
        return 0;

    char prepared[k_qpath];
    prepare_name( prepared, name, /*strip_lead=*/false );

    bool fresh = false;
    const int i = index_in( events_, prepared,
                            "MAX_EVENTS limit exceeded (%d)\n", fresh );
    if ( fresh && !loading_ )
        late_notify( PrecacheKind::Event, prepared, i,
                     ::xash::abi::k_res_fatalifmissing, /*warn=*/false );
    return i;
}

int PrecacheTables::generic_index( const char *name )
{
    if ( name == nullptr || name[0] == '\0' )
        return 0;

    char prepared[k_qpath];
    prepare_name( prepared, name, /*strip_lead=*/false );

    bool fresh = false;
    const int i = index_in( generics_, prepared,
                            "MAX_CUSTOM limit exceeded (%d)\n", fresh );
    if ( fresh && !loading_ )
        late_notify( PrecacheKind::Generic, prepared, i,
                     ::xash::abi::k_res_fatalifmissing, /*warn=*/false );
    return i;
}

int PrecacheTables::find_model( const char *name ) const
{
    if ( name == nullptr || name[0] == '\0' )
        return 0;

    char prepared[k_qpath];
    prepare_name( prepared, name, /*strip_lead=*/true );

    for ( std::size_t i = 1; i < models_.cap && models_.slot( i )[0] != '\0';
          ++i )
    {
        if ( ut::stricmp( models_.slot( i ), prepared ) == 0 )
            return static_cast<int>( i );
    }

    ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                        "Cannot get index for model %s: not precached",
                        prepared );
    return 0;
}

const char *PrecacheTables::model_name( std::size_t i ) const noexcept
{
    return i < models_.cap && models_.names != nullptr ? models_.slot( i ) : "";
}

const char *PrecacheTables::sound_name( std::size_t i ) const noexcept
{
    return i < sounds_.cap && sounds_.names != nullptr ? sounds_.slot( i ) : "";
}

const char *PrecacheTables::event_name( std::size_t i ) const noexcept
{
    return i < events_.cap && events_.names != nullptr ? events_.slot( i ) : "";
}

const char *PrecacheTables::generic_name( std::size_t i ) const noexcept
{
    return i < generics_.cap && generics_.names != nullptr ? generics_.slot( i )
                                                           : "";
}

std::uint32_t PrecacheTables::model_flags( std::size_t i ) const noexcept
{
    return i < caps_.models && model_flags_ != nullptr ? model_flags_[i] : 0u;
}

void PrecacheTables::set_model_flags( std::size_t i,
                                      std::uint32_t bits ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( i < caps_.models && model_flags_ != nullptr )
        model_flags_[i] |= bits;
}

} // namespace xash::server
