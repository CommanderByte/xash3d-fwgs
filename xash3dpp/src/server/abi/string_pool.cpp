// xash3dpp — server string pool implementation (Chunk 6 S4, OQ-6 baseline)
// Legacy reference: engine/server/sv_game.c :2976-3345
//
// Existing subsystems used:
//   xash3dpp_memory    — the 2× arena block + transient processed copies
//   xash3dpp_utilities — strlen/strcmp/strncpy (legacy Q_* semantics)
//   xash3dpp_core      — thread-role assertion (OQ-9: main-thread only)

#include <xash3dpp/private/server/string_pool.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <climits>

namespace xash::server {

using ::xash::abi::string_t;

bool StringPool::init( ::xash::memory::PoolHandle pool,
                       std::size_t max_edicts, std::size_t arena_size )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( arena_size == 0 )
    {
        // Legacy sizing: 65536 * ceil(max_edicts / 1024)  (sv_game.c:3074).
        const std::size_t blocks =
            ( max_edicts + ::xash::limits::server_string_quantum - 1 ) /
            ::xash::limits::server_string_quantum;
        arena_size = ::xash::limits::server_string_block * blocks;
    }

    pool_  = pool;
    block_ = static_cast<char *>(
        ::xash::memory::mem_calloc( pool, arena_size * 2 ));
    if ( block_ == nullptr )
        return false;

    arena_size_  = arena_size;
    dynamic_     = false;
    // Legacy boot state (SV_AllocStringPool): base at the DYNAMIC half
    // until SV_SetStringArrayMode picks the static phase.
    cursor_base_ = old_base_ = block_;
    last_        = block_ + 1; // offset 0 stays the empty string
    stats_       = {};
    return true;
}

void StringPool::shutdown()
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    ::xash::memory::mem_free( block_ );
    block_ = cursor_base_ = old_base_ = last_ = nullptr;
    arena_size_ = 0;
}

void StringPool::set_dynamic( bool dynamic )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( dynamic == dynamic_ )
        return;
    dynamic_ = dynamic;
    empty_pool( /*clear_stats=*/false );
}

void StringPool::empty_pool( bool clear_stats )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( dynamic_ )
    {
        // Dynamic arena = first half; dedup floor deliberately NOT reset
        // (legacy keeps poldstringbase — static-phase strings stay
        // findable until the next wrap).
        cursor_base_ = block_;
    }
    else
    {
        cursor_base_ = old_base_ = block_ + arena_size_; // static half
        last_        = cursor_base_ + 1;
    }

    if ( clear_stats )
        stats_ = {};
}

std::size_t StringPool::process_string( char *dst, const char *src ) noexcept
{
    std::size_t i = 0;
    const char *p = src;

    while ( *p )
    {
        if ( *p == '\\' )
        {
            char replace = 0;
            switch ( p[1] )
            {
            case 'n': replace = '\n'; break;
            // GoldSrc doesn't replace these symbols but the old
            // pfnWriteString hack did (legacy comment).
            case 'r': replace = '\r'; break;
            case 't': replace = '\t'; break;
            default: break;
            }

            if ( replace )
            {
                if ( dst )
                    dst[i] = replace;
                ++i;
                p += 2;
                continue;
            }
        }

        if ( dst )
            dst[i] = *p;
        ++i;
        ++p;
    }

    if ( dst )
        dst[i] = '\0';
    ++i;

    return i;
}

bool StringPool::offset_in_int_range( const char *base, const char *p ) noexcept
{
    const auto diff = static_cast<std::intptr_t>(
                          reinterpret_cast<std::uintptr_t>( p )) -      // SAFETY: pointer->integer for signed offset math only — no dereference; the value is range-checked below (legacy string_t is an int offset)
                      static_cast<std::intptr_t>(
                          reinterpret_cast<std::uintptr_t>( base ));    // SAFETY: pointer->integer for the arena-base address; offset arithmetic only
    return diff <= INT_MAX && diff >= INT_MIN;
}

string_t StringPool::alloc_string( const char *value )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const std::size_t len = process_string( nullptr, value );
    char *processed = static_cast<char *>(
        ::xash::memory::mem_calloc( pool_, len ));
    process_string( processed, value );

    // (physics-interface pfnAllocString override hooks in at the bridge —
    // S6/S8 seam; absent here.)

    char *placed = nullptr;

    if ( !allow_dup_ )
    {
        for ( char *dupe = old_base_ + 1; dupe < last_;
              dupe += ::xash::utilities::strlen( dupe ) + 1 )
        {
            if ( ::xash::utilities::strcmp( dupe, processed ) == 0 )
            {
                placed = dupe;
                break;
            }
        }
    }

    if ( placed == nullptr )
    {
        if ( static_cast<std::size_t>( last_ - old_base_ ) + len + 1 >
             arena_size_ )
        {
            // Arena full: wrap to the active arena start and overwrite
            // (legacy overflow quirk — old string_t values now read newer
            // text).
            last_     = cursor_base_ + 1;
            old_base_ = cursor_base_;
            ++stats_.num_overflows;
        }

        ::xash::utilities::strncpy( last_, processed, len );
        stats_.total_alloc += len;

        placed = last_;
        last_ += len;
    }
    else
    {
        ++stats_.num_dups;
    }

    if ( static_cast<std::size_t>( placed - block_ ) > stats_.max_alloc )
        stats_.max_alloc = static_cast<std::size_t>( placed - block_ );

    ::xash::memory::mem_free( processed );

    return static_cast<string_t>( placed - block_ );
}

string_t StringPool::make_string( const char *value )
{
    if ( offset_in_int_range( block_, value ))
    {
        const auto diff = static_cast<std::intptr_t>(
                              reinterpret_cast<std::uintptr_t>( value )) - // SAFETY: pointer->integer for offset math — value is inside the pool block_ (offset_in_int_range checked above); no dereference
                          static_cast<std::intptr_t>(
                              reinterpret_cast<std::uintptr_t>( block_ )); // SAFETY: pointer->integer for the pool-base address; yields the legacy string_t offset
        return static_cast<string_t>( diff );
    }
    return alloc_string( value );
}

} // namespace xash::server
