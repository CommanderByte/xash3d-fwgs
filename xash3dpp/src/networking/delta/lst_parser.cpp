// xash3dpp — delta.lst script parser
// Legacy reference: engine/common/net_encode.c Delta_InitFields /
// Delta_ParseTable / Delta_ParseField (tokenised by COM_ParseFile, which
// splits { } ( ) , ' : as single-character tokens).
//
// Structural errors (legacy Sys_Error) log at Error and fail the parse;
// per-field syntax errors log and skip that field (legacy Con_DPrintf).

#include <xash3dpp/private/networking/delta/lst_parser.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/private/networking/delta/delta_tables_impl.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <string>

namespace xash::networking::delta {


namespace {

constexpr std::size_t k_token_max = 256; // legacy `string` token buffer

struct Cursor
{
    const char *pos { nullptr };
    char        token[ k_token_max ] {};

    // Advance one token.  Returns false at end of input (pos == nullptr).
    [[nodiscard]] bool next() noexcept
    {
        pos = ::xash::utilities::parse_token( pos, token, sizeof( token ));
        return pos != nullptr;
    }

    [[nodiscard]] bool token_is( const char *s ) const noexcept
    {
        return ::xash::utilities::strcmp( token, s ) == 0;
    }
};

[[nodiscard]] std::uint32_t flag_for_name( const char *name ) noexcept
{
    if( ::xash::utilities::strcmp( name, "DT_BYTE" ) == 0 )           return k_dt_byte;
    if( ::xash::utilities::strcmp( name, "DT_SHORT" ) == 0 )          return k_dt_short;
    if( ::xash::utilities::strcmp( name, "DT_FLOAT" ) == 0 )          return k_dt_float;
    if( ::xash::utilities::strcmp( name, "DT_INTEGER" ) == 0 )        return k_dt_integer;
    if( ::xash::utilities::strcmp( name, "DT_ANGLE" ) == 0 )          return k_dt_angle;
    if( ::xash::utilities::strcmp( name, "DT_TIMEWINDOW_8" ) == 0 )   return k_dt_timewindow_8;
    if( ::xash::utilities::strcmp( name, "DT_TIMEWINDOW_BIG" ) == 0 ) return k_dt_timewindow_big;
    if( ::xash::utilities::strcmp( name, "DT_STRING" ) == 0 )         return k_dt_string;
    if( ::xash::utilities::strcmp( name, "DT_SIGNED" ) == 0 )         return k_dt_signed;
    return 0; // unknown flags are ignored, like legacy
}

// Legacy Delta_ParseField.  On success `out` is fully populated.
[[nodiscard]] bool parse_field( Cursor &c, const DeltaTable &dt,
                                DeltaField &out, bool post ) noexcept
{
    (void)c.next();
    if( !c.token_is( "(" ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: expected '(', found '%s' instead", c.token );
        return false;
    }

    // field name
    if( !c.next())
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "delta", "parse_field: missing field name" );
        return false;
    }

    const DeltaFieldInfo *info = nullptr;
    for( const auto &fi : dt.info )
    {
        if( ::xash::utilities::strcmp( fi.name, c.token ) == 0 )
        {
            info = &fi;
            break;
        }
    }
    if( !info )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: unable to find field %s", c.token );
        return false;
    }

    (void)c.next();
    if( !c.token_is( "," ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: expected ',', found '%s' instead", c.token );
        return false;
    }

    out.name   = info->name;
    out.offset = info->offset;
    out.size   = info->size;
    out.flags  = 0;

    // delta-flags, '|'-separated, terminated by ','
    while( c.next())
    {
        if( c.token_is( "," ))
            break;
        if( c.token_is( "|" ))
            continue;
        out.flags |= flag_for_name( c.token );
    }

    if( !c.token_is( "," ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: expected ',', found '%s' instead", c.token );
        return false;
    }

    // bits
    if( !c.next())
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: %s field bits argument is missing", out.name );
        return false;
    }
    out.bits = ::xash::utilities::atoi( c.token );

    (void)c.next();
    if( !c.token_is( "," ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: expected ',', found '%s' instead", c.token );
        return false;
    }

    // multiplier
    if( !c.next())
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: %s missing 'multiplier' argument", out.name );
        return false;
    }
    out.multiplier = ::xash::utilities::atof( c.token );

    if( post )
    {
        (void)c.next();
        if( !c.token_is( "," ))
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                        "parse_field: expected ',', found '%s' instead", c.token );
            return false;
        }

        if( !c.next())
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                        "parse_field: %s missing 'post_multiply' argument", out.name );
            return false;
        }
        out.post_multiplier = ::xash::utilities::atof( c.token );
    }
    else
    {
        // to avoid division by zero
        out.post_multiplier = 1.0f;
    }

    // closing brace...
    (void)c.next();
    if( !c.token_is( ")" ))
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                    "parse_field: expected ')', found '%s' instead", c.token );
        return false;
    }

    // ...and optional trailing ',' — backtrack when it is something else
    const char *oldpos = c.pos;
    (void)c.next();
    if( c.token[0] != ',' )
        c.pos = oldpos;

    return true;
}

// Legacy Delta_ParseTable — assumes '{' was already consumed.
void parse_table( Cursor &c, DeltaTable &dt,
                  const char *encode_dll, const char *encode_func ) noexcept
{
    dt.fields.clear();
    dt.fields.reserve( dt.info.size()); // @pre-reserved: table maxFields

    while( c.next())
    {
        const bool is_post = c.token_is( "DEFINE_DELTA_POST" );

        if( c.token_is( "DEFINE_DELTA" ) || is_post )
        {
            // Legacy Assert( numFields <= maxFields ); the guard below is
            // the memory-safe equivalent of legacy's fixed allocation.
            if( dt.fields.size() >= dt.info.size())
            {
                ::xash::core::logf( ::xash::core::LogLevel::Warning, "delta",
                            "parse_table: %s field list is full, skipping rest",
                            dt.name );
                XASH_ASSERT( false );
                break;
            }

            DeltaField field;
            if( parse_field( c, dt, field, is_post ))
                dt.fields.push_back( field );
        }
        else if( c.token[0] == '}' )
        {
            break; // end of the section
        }
    }

    (void)::xash::utilities::strncpy( dt.func_name, encode_func, sizeof( dt.func_name ));

    if( ::xash::utilities::stricmp( encode_dll, "none" ) == 0 )
        dt.custom_encode = CustomEncodeKind::None;
    else if( ::xash::utilities::stricmp( encode_dll, "gamedll" ) == 0 )
        dt.custom_encode = CustomEncodeKind::Server;
    else if( ::xash::utilities::stricmp( encode_dll, "clientdll" ) == 0 )
        dt.custom_encode = CustomEncodeKind::Client;

    dt.initialized = true; // table is ok
}

} // namespace

bool parse_delta_lst( std::string_view script, DeltaTables::Impl &impl ) noexcept
{
    // parse_token expects NUL-terminated input; load_file buffers are not.
    const std::string text{ script };

    Cursor c;
    c.pos = text.c_str();

    char encode_dll[ k_token_max ]  = {};
    char encode_func[ k_token_max ] = {};

    while( c.next())
    {
        DeltaTable *dt = impl.find_struct( c.token );
        if( !dt )
        {
            // Legacy: Sys_Error — structural failure fails the whole parse.
            ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                        "delta.lst: unknown struct %s", c.token );
            return false;
        }

        if( !c.next())
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                        "delta.lst: missing encoder type in section %s", dt->name );
            return false;
        }
        (void)::xash::utilities::strncpy( encode_dll, c.token, sizeof( encode_dll ));

        if( ::xash::utilities::stricmp( encode_dll, "none" ) == 0 )
        {
            (void)::xash::utilities::strncpy( encode_func, "null", sizeof( encode_func ));
        }
        else
        {
            if( !c.next())
            {
                ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                            "delta.lst: missing encoder name in section %s", dt->name );
                return false;
            }
            (void)::xash::utilities::strncpy( encode_func, c.token, sizeof( encode_func ));
        }

        // jump to '{'
        (void)c.next();
        if( c.token[0] != '{' )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "delta",
                        "delta.lst: missing '{' in section %s", dt->name );
            return false;
        }

        parse_table( c, *dt, encode_dll, encode_func );
    }

    return true;
}

} // namespace xash::networking::delta
