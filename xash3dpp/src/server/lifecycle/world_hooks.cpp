// xash3dpp — lifecycle-owned IWorldLinkHooks (Chunk 6 S7b).
// See world_hooks.hpp for the design note.

#include <xash3dpp/private/server/world_hooks.hpp>

namespace xash::server {

void GameWorldHooks::set_abs_box( ::xash::abi::edict_t *ent ) noexcept
{
    // pfnSetAbsBox (SetObjectCollisionBox) — engine has no fallback.
    if ( game_ != nullptr && game_->funcs().pfnSetAbsBox != nullptr )
        game_->funcs().pfnSetAbsBox( ent );
}

void GameWorldHooks::dispatch_touch( ::xash::abi::edict_t *trigger,
                                     ::xash::abi::edict_t *other ) noexcept
{
    if ( game_ != nullptr && game_->funcs().pfnTouch != nullptr )
        game_->funcs().pfnTouch( trigger, other );
}

bool GameWorldHooks::brush_trigger_intersects(
    ::xash::abi::edict_t *trigger, ::xash::abi::edict_t *ent ) noexcept
{
    if ( env_ == nullptr )
        return true; // no world bound → accept the AABB hit (legacy default)
    return ::xash::server::brush_trigger_intersects( *env_, trigger, ent );
}

} // namespace xash::server
