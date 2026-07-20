// xash3dpp — EdictArena implementation (Chunk 6 S4, Q-20)
// Legacy reference: engine/server/sv_game.c :961-1066
//
// Existing subsystems used:
//   xash3dpp_memory — the edict array + DLL private data blocks
//   xash3dpp_core   — thread-role assertion (OQ-9: main-thread only)

#include <xash3dpp/private/server/edict_arena.hpp>

#include <xash3dpp/core/thread_role.hpp>

#include <cstring>

namespace xash::server {

using ::xash::abi::edict_t;
using ::xash::abi::entvars_t;

namespace {

// sv_game.c:1049-1051 — "the first couple seconds of server time can
// involve a lot of freeing and allocating, so relax the replacement
// policy": slots freed before k_reuse_relax_window are reusable at once;
// later frees must age past k_reuse_grace.
inline constexpr float k_reuse_relax_window = 2.0f;
inline constexpr float k_reuse_grace       = 0.5f;

// SV_InitEdict: bone controllers rest at midpoint (0x7F), not zero.
inline constexpr ::xash::abi::byte k_controller_rest = 0x7F;

} // namespace

bool EdictArena::init( ::xash::memory::PoolHandle pool,
                       std::size_t max_edicts, std::size_t reserved )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    pool_   = pool;
    edicts_ = static_cast<edict_t *>(
        ::xash::memory::mem_calloc( pool, sizeof( edict_t ) * max_edicts ));
    if ( edicts_ == nullptr )
        return false;

    // "mark all edicts as freed" (sv_game.c:5345-5346): a never-allocated
    // slot must fail SV_IsValidEdict until init_edict claims it.
    for ( std::size_t i = 0; i < max_edicts; ++i )
        edicts_[i].free = 1;

    max_edicts_   = max_edicts;
    reserved_     = reserved;
    num_entities_ = reserved;
    return true;
}

void EdictArena::shutdown()
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( edicts_ != nullptr )
    {
        for ( std::size_t i = 0; i < num_entities_; ++i )
            free_private( &edicts_[i] );
        ::xash::memory::mem_free( edicts_ );
        edicts_ = nullptr;
    }
    max_edicts_ = num_entities_ = reserved_ = 0;
}

edict_t *EdictArena::alloc_edict( double sv_time )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    std::size_t i;
    for ( i = reserved_; i < num_entities_; ++i )
    {
        edict_t *e = &edicts_[i];
        if ( e->free && ( e->freetime < k_reuse_relax_window ||
                          ( sv_time - e->freetime ) > k_reuse_grace ))
        {
            init_edict( e );
            return e;
        }
    }

    if ( i >= max_edicts_ )
        return nullptr; // legacy: Host_Error "no free edicts" — bridge maps it

    ++num_entities_;
    edict_t *e = &edicts_[i];
    init_edict( e );
    return e;
}

void EdictArena::free_edict( edict_t *ed, double sv_time )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( ed->free )
        return;

    // Legacy unlinks from the world first (SV_UnlinkEdict) — S5's caller
    // contract; the arena owns only the slot state.
    free_private( ed );

    ed->freetime = static_cast<float>( sv_time );
    ed->serialnumber++; // invalidate EHANDLE's

    // The scrubbed subset (sv_game.c:1017-1030); everything else stays
    // STALE deliberately — games read freed edicts.
    ed->v.solid      = 0; // SOLID_NOT
    ed->v.flags      = 0;
    ed->v.model      = 0;
    ed->v.takedamage = 0;
    ed->v.modelindex = 0;
    ed->v.nextthink  = -1.0f;
    ed->v.colormap   = 0;
    ed->v.frame      = 0.0f;
    ed->v.scale      = 0.0f;
    ed->v.gravity    = 0.0f;
    ed->v.skin       = 0;

    ed->v.angles[0] = ed->v.angles[1] = ed->v.angles[2] = 0.0f;
    ed->v.origin[0] = ed->v.origin[1] = ed->v.origin[2] = 0.0f;

    ed->free = 1;
}

void EdictArena::init_edict( edict_t *ed )
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    free_private( ed );
    std::memset( &ed->v, 0, sizeof( entvars_t ));
    ed->v.pContainingEntity = ed;
    ed->v.controller[0] = k_controller_rest;
    ed->v.controller[1] = k_controller_rest;
    ed->v.controller[2] = k_controller_rest;
    ed->v.controller[3] = k_controller_rest;
    ed->free = 0;
}

void *EdictArena::alloc_private( edict_t *ed, std::size_t size )
{
    free_private( ed );
    // 16-byte round-up per sv_game.c:2955 — shipped binary mods scribble
    // into the last sixteen bytes past their requested size.
    ed->pvPrivateData =
        ::xash::memory::mem_calloc( pool_, private_data_size( size ));
    return ed->pvPrivateData;
}

void EdictArena::free_private( edict_t *ed )
{
    if ( ed == nullptr || ed->pvPrivateData == nullptr )
        return;

    // NOTE: new interface can be missing (legacy dllFuncs2 nullable).
    if ( releaser_ != nullptr )
        releaser_( releaser_ctx_, ed );

    ::xash::memory::mem_free( ed->pvPrivateData );
    ed->pvPrivateData = nullptr;
}

edict_t *EdictArena::edict_num( std::size_t i ) const noexcept
{
    return i < max_edicts_ ? &edicts_[i] : nullptr;
}

int EdictArena::index_of( const edict_t *ed ) const noexcept
{
    return static_cast<int>( ed - edicts_ );
}

std::ptrdiff_t EdictArena::offset_of( const edict_t *ed ) const noexcept
{
    return reinterpret_cast<const char *>( ed ) -                 // SAFETY: edict->byte-address pun — ed and edicts_ both index the one contiguous arena (edicts_[max_edicts_]); the char* difference is the intra-arena byte offset
           reinterpret_cast<const char *>( edicts_ );             // SAFETY: arena base viewed as a byte address — same contiguous array as ed above
}

edict_t *EdictArena::ent_of_offset( std::ptrdiff_t off ) const noexcept
{
    return reinterpret_cast<edict_t *>(                           // SAFETY: byte-offset->edict pun — off is an offset_of() result taken within this same arena, so base+off lands on an edict boundary in edicts_
        reinterpret_cast<char *>( edicts_ ) + off );              // SAFETY: arena base viewed as a byte address for the offset add
}

} // namespace xash::server
