#pragma once
// xash3dpp — lifecycle-owned IWorldLinkHooks: the game-DLL callbacks the
// areanode linker fires.  Legacy reference: engine/server/sv_world.c —
// SV_LinkEdict → pfnSetAbsBox (:640, the game's SetObjectCollisionBox),
// SV_TouchLinks → pfnTouch (:506), the brush-trigger refinement
// (:546-567).  Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md.
//
// S5 tests supplied fixture doubles; the running server routes these to the
// real game DLL (set_abs_box, dispatch_touch) and to the engine trace
// kernel (brush_trigger_intersects).  The bound MoveEnv is the one the
// lifecycle owns — the trigger refinement needs the world + areanode root.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/game_dll.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/world/trace.hpp>

namespace xash::server {

class GameWorldHooks final : public IWorldLinkHooks
{
public:
    // Bound at SV_LoadProgs (game) and each SV_SpawnServer (env); env is
    // cleared on deactivate so a stale world is never refined against.
    void bind( const GameDll *game, const MoveEnv *env ) noexcept
    {
        game_ = game;
        env_  = env;
    }

    // pfnSetAbsBox — absmin/absmax expansion is the game DLL's job (HLSDK
    // SetObjectCollisionBox); the engine has no fallback, so a missing slot
    // simply leaves the last box (legacy calls unconditionally).
    void set_abs_box( ::xash::abi::edict_t *ent ) noexcept override;

    // pfnTouch dispatch (the caller applied the playersonly gate).
    void dispatch_touch( ::xash::abi::edict_t *trigger,
                         ::xash::abi::edict_t *other ) noexcept override;

    // Exact BSP-hull trigger refinement via the engine trace kernel.
    [[nodiscard]] bool
    brush_trigger_intersects( ::xash::abi::edict_t *trigger,
                              ::xash::abi::edict_t *ent ) noexcept override;

private:
    const GameDll *game_ = nullptr; // @lifetime: engine
    const MoveEnv *env_  = nullptr; // @lifetime: engine
};

} // namespace xash::server
