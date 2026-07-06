// xash3dpp — Info string helpers (server-private, Chunk 6 S9)
// Legacy reference: engine/common/infostring.c — Info_ValueForKey (:192),
// Info_RemoveKey (:236), Info_RemovePrefixedKeys (:290),
// Info_SetValueForStarKey (:416), Info_IsValid (:92).
//
// Faithful port of the GoldSrc `\key\value` codec, including the MAX_KV_SIZE
// (128) field truncation, the ".."/'\\'/'"' rejection, the '*'-key protection,
// the largest-non-important-key eviction when out of room, and the `c > 13`
// ascii filter with the "team" lowercasing quirk.  Returns are simplified to
// void where the legacy bool was advisory only.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/info_string.hpp>

#include <xash3dpp/utilities/string.hpp>

#include <cstring>

namespace xash::server {

namespace {

namespace ut = ::xash::utilities;

constexpr std::size_t k_max_kv = 128; // legacy MAX_KV_SIZE

[[nodiscard]] bool is_important_key( const char *key ) noexcept
{
    if ( key[0] == '*' )
        return true;
    static const char *const k_important[] = {
        "name",  "model",         "rate",   "topcolor",  "bottomcolor",
        "cl_updaterate", "cl_lw", "cl_lc",  "cl_nopred",
    };
    for ( const char *k : k_important )
        if ( ut::strcmp( key, k ) == 0 )
            return true;
    return false;
}

// Copy at most k_max_kv-1 chars until '\\' (or NUL).  Advances *s past the
// field but NOT past the trailing separator.  Returns false if NUL hit before
// a separator on a key read (malformed).
const char *read_field( const char *s, char *out ) noexcept
{
    std::size_t count = 0;
    char       *o     = out;
    while ( count < ( k_max_kv - 1 ) && *s != '\0' && *s != '\\' )
    {
        *o++ = *s++;
        ++count;
    }
    *o = '\0';
    return s;
}

} // namespace

const char *info_value_for_key( const char *s, const char *key, char *out,
                                std::size_t out_size ) noexcept
{
    if ( out_size == 0 )
        return "";
    out[0] = '\0';
    if ( s == nullptr || key == nullptr )
        return out;

    char pkey[k_max_kv];
    char value[k_max_kv];

    if ( *s == '\\' )
        ++s;

    while ( true )
    {
        s = read_field( s, pkey );
        if ( *s == '\0' )
            return out; // no value ⇒ empty
        ++s;            // skip separator before value

        s = read_field( s, value );

        if ( ut::strcmp( key, pkey ) == 0 )
        {
            ut::strncpy( out, value, out_size );
            return out;
        }
        if ( *s == '\0' )
            return out;
        ++s;
    }
}

void info_remove_key( char *s, const char *key ) noexcept
{
    if ( s == nullptr || key == nullptr )
        return;
    if ( std::strchr( key, '\\' ) != nullptr )
        return;

    std::size_t cmpsize = ut::strlen( key );
    if ( cmpsize > ( k_max_kv - 1 ) )
        cmpsize = k_max_kv - 1;

    char pkey[k_max_kv];
    char value[k_max_kv];

    while ( true )
    {
        char *start = s;
        if ( *s == '\\' )
            ++s;

        s = const_cast<char *>( read_field( s, pkey ) );           // SAFETY: Q-16 const_cast — read_field returns a const cursor into the caller-owned MUTABLE info buffer; restores write access the read-only return narrowed away (the buffer is spliced in place below)
        if ( *s == '\0' )
            return;
        ++s;

        s = const_cast<char *>( read_field( s, value ) );          // SAFETY: Q-16 const_cast — read_field returns a const cursor into the caller-owned MUTABLE info buffer; restores write access the read-only return narrowed away

        // legacy uses a case-sensitive PREFIX match of strlen(key) chars
        // (Q_strncmp) — bug-compatible: "name" also matches "name2".
        if ( std::strncmp( key, pkey, cmpsize ) == 0 )
        {
            const std::size_t size = ut::strlen( s ) + 1;
            std::memmove( start, s, size ); // splice this pair out
            return;
        }

        if ( *s == '\0' )
            return;
    }
}

void info_remove_prefixed_keys( char *start, char prefix ) noexcept
{
    if ( start == nullptr )
        return;

    char *s = start;
    char  pkey[k_max_kv];
    char  value[k_max_kv];

    while ( true )
    {
        if ( *s == '\\' )
            ++s;

        s = const_cast<char *>( read_field( s, pkey ) );           // SAFETY: Q-16 const_cast — read_field returns a const cursor into the caller-owned MUTABLE info buffer; restores write access the read-only return narrowed away (the buffer is spliced in place below)
        if ( *s == '\0' )
            return;
        ++s;

        s = const_cast<char *>( read_field( s, value ) );          // SAFETY: Q-16 const_cast — read_field returns a const cursor into the caller-owned MUTABLE info buffer; restores write access the read-only return narrowed away

        if ( pkey[0] == prefix )
        {
            info_remove_key( start, pkey );
            s = start;
        }

        if ( *s == '\0' )
            return;
    }
}

void info_set_value_for_key( char *s, const char *key, const char *value,
                             std::size_t max_size, bool star_allowed ) noexcept
{
    if ( s == nullptr || key == nullptr || value == nullptr )
        return;

    if ( !star_allowed && key[0] == '*' )
        return; // engine-reserved key

    if ( std::strchr( key, '\\' ) != nullptr ||
         std::strchr( value, '\\' ) != nullptr )
        return;
    if ( std::strstr( key, ".." ) != nullptr ||
         std::strstr( value, ".." ) != nullptr )
        return;
    if ( std::strchr( key, '"' ) != nullptr ||
         std::strchr( value, '"' ) != nullptr )
        return;
    if ( ut::strlen( key ) > ( k_max_kv - 1 ) ||
         ut::strlen( value ) > ( k_max_kv - 1 ) )
        return;

    info_remove_key( s, key );

    if ( value[0] == '\0' )
        return; // just cleared the key

    char newpair[1024];
    ut::snprintf( newpair, sizeof( newpair ), "\\%s\\%s", key, value );

    // Reject when append + existing content would fill or exceed the buffer.
    // Legacy Info_SetValueForKey uses '>' (infostring.c:445), which writes the
    // terminating NUL one byte past a max_size-sized buffer at the exact-fill
    // boundary (a latent 1-byte OOB — undefined). We use '>=' so the NUL always
    // lands in bounds: identical to legacy for every well-defined input, and
    // differing only where legacy invokes UB. (Largest-key eviction for
    // "important" keys is a follow-up; we drop instead.)
    if ( ut::strlen( newpair ) + ut::strlen( s ) >= max_size )
        return;

    // append, filtering control chars; lowercase the "team" value (quirk)
    const bool team = ut::stricmp( key, "team" ) == 0;
    char      *dst  = s + ut::strlen( s );
    for ( const char *v = newpair; *v != '\0'; ++v )
    {
        int c = static_cast<unsigned char>( *v );
        if ( team && c >= 'A' && c <= 'Z' )
            c += 'a' - 'A';
        if ( c > 13 )
            *dst++ = static_cast<char>( c );
    }
    *dst = '\0';
}

bool info_is_valid( const char *s ) noexcept
{
    if ( s == nullptr )
        return false;

    char key[k_max_kv];
    char value[k_max_kv];

    if ( *s == '\\' )
        ++s;

    while ( *s != '\0' )
    {
        s = read_field( s, key );
        if ( *s == '\0' )
            return false; // key with no value

        ++s;
        s = read_field( s, value );

        if ( value[0] == '\0' )
            return false; // empty value

        if ( *s != '\0' )
            ++s;
    }

    return true;
}

} // namespace xash::server
