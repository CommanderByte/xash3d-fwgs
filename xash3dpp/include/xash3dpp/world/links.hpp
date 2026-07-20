#pragma once
// xash3dpp — areanode spatial index: link/unlink edicts, trigger touching
// Legacy reference: engine/server/sv_world.c — SV_CreateAreaNode (:422),
// SV_ClearWorld (:466, areanode part), SV_UnlinkEdict (:491),
// SV_TouchLinks (:506), SV_FindTouchedLeafs (:593), SV_LinkEdict (:640);
// engine/common/world.h :32-34 (AREA_NODES/AREA_DEPTH).
// Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md §2.
//
// Three lists per node (Xash extension over Quake's two): triggers,
// solids, portals.  Split axis is the longer of X/Y — never Z.  Entity
// membership is the intrusive edict_t::area link (edict header field —
// the linking contract this module owns; entvars access goes through
// EntityView per Q-20).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <cstddef>

namespace xash::server {

// ---------------------------------------------------------------------------
// Intrusive link helpers (legacy ClearLink/RemoveLink/InsertLinkBefore)
// ---------------------------------------------------------------------------

inline void clear_link( ::xash::abi::link_t &l ) noexcept
{
    l.prev = l.next = &l;
}

inline void remove_link( ::xash::abi::link_t &l ) noexcept
{
    l.next->prev = l.prev;
    l.prev->next = l.next;
}

inline void insert_link_before( ::xash::abi::link_t &l,
                                ::xash::abi::link_t &before ) noexcept
{
    l.next         = &before;
    l.prev         = before.prev;
    l.prev->next   = &l;
    before.prev    = &l;
}

// EDICT_FROM_AREA: container-of on the intrusive area link.
[[nodiscard]] inline ::xash::abi::edict_t *
edict_from_area( ::xash::abi::link_t *l ) noexcept
{
    return reinterpret_cast<::xash::abi::edict_t *>(
        reinterpret_cast<char *>( l ) -
        offsetof( ::xash::abi::edict_t, area ));
}

// ---------------------------------------------------------------------------
// Hooks — installed by later slices (the game-DLL bridge / lifecycle)
// ---------------------------------------------------------------------------

struct IWorldLinkHooks
{
    virtual ~IWorldLinkHooks() = default;

    // pfnSetAbsBox: absmin/absmax expansion is GAME-DLL responsibility
    // (HLSDK SetObjectCollisionBox); the engine has no fallback.
    virtual void set_abs_box( ::xash::abi::edict_t *ent ) noexcept = 0;

    // pfnTouch dispatch (the caller has already applied the playersonly
    // gate and updated globals->time belongs to the dispatcher).
    virtual void dispatch_touch( ::xash::abi::edict_t *trigger,
                                 ::xash::abi::edict_t *other ) noexcept = 0;

    // Exact brush-trigger refinement after the AABB pass (legacy: force
    // BSP hull + PM_HullPointContents, with rotated-trigger support,
    // sv_world.c:546-567).  The real test is
    // server::brush_trigger_intersects (world_trace.hpp).
    // TODO(chunk6-S7): lifecycle's hooks implementation wires it (needs
    // the MoveEnv it owns); this default accepts the AABB hit.
    [[nodiscard]] virtual bool
    brush_trigger_intersects( ::xash::abi::edict_t * /*trigger*/,
                              ::xash::abi::edict_t * /*ent*/ ) noexcept
    {
        return true;
    }
};

// ---------------------------------------------------------------------------
// WorldLinks
// ---------------------------------------------------------------------------

// server.h:53-54 — pfnSetGroupMask policy for groupinfo filtering.
enum class GroupOp : int
{
    And  = 0,
    Nand = 1,
};

struct AreaNode
{
    int   axis = -1; // -1 = leaf
    float dist = 0.0f;
    AreaNode           *children[2]{};
    ::xash::abi::link_t trigger_edicts{};
    ::xash::abi::link_t solid_edicts{};
    ::xash::abi::link_t portal_edicts{};
};

// Per-link environment (server state the walk needs; owned by lifecycle).
struct LinkEnv
{
    const ::xash::map_loader::WorldData *world = nullptr; // @lifetime: engine
    ::xash::abi::edict_t *worldspawn = nullptr;           // @lifetime: engine
    bool playersonly = false; // sv.playersonly — suppresses pfnTouch
};

class WorldLinks
{
public:
    WorldLinks() = default;

    WorldLinks( const WorldLinks & )            = delete;
    WorldLinks &operator=( const WorldLinks & ) = delete;

    // SV_ClearWorld (areanode part): reset and rebuild the tree over the
    // world bounds.  Lightstyle/box-hull resets live with their owners.
    void clear_world( const ::xash::utilities::Vec3 &world_mins,
                      const ::xash::utilities::Vec3 &world_maxs );

    void set_hooks( IWorldLinkHooks *hooks ) noexcept { hooks_ = hooks; } // @lifetime: engine
    void set_group_op( GroupOp op ) noexcept { group_op_ = op; }

    // SV_LinkEdict.  touch_triggers additionally walks the trigger lists
    // (recursion-guarded by the legacy iTouchLinkSemaphore).
    void link_edict( ::xash::abi::edict_t *ent, bool touch_triggers,
                     const LinkEnv &env );

    // SV_UnlinkEdict: no-op when not linked; NULLs both link pointers.
    static void unlink_edict( ::xash::abi::edict_t *ent ) noexcept;

    [[nodiscard]] bool linked( const ::xash::abi::edict_t *ent ) const noexcept
    {
        return ent->area.prev != nullptr;
    }

    // Root of the areanode tree (S5b clip walks traverse it).
    [[nodiscard]] const AreaNode *root() const noexcept { return &nodes_[0]; }
    [[nodiscard]] AreaNode *root() noexcept { return &nodes_[0]; }
    [[nodiscard]] std::size_t node_count() const noexcept { return num_nodes_; }

private:
    AreaNode *create_node( int depth, const ::xash::utilities::Vec3 &mins,
                           const ::xash::utilities::Vec3 &maxs );
    void touch_links( ::xash::abi::edict_t *ent, AreaNode *node,
                      const LinkEnv &env );
    void find_touched_leafs( ::xash::abi::edict_t *ent, const LinkEnv &env );

    AreaNode    nodes_[::xash::limits::server_area_nodes]{};
    std::size_t num_nodes_       = 0;
    bool        touch_semaphore_ = false; // legacy iTouchLinkSemaphore
    GroupOp     group_op_        = GroupOp::And;
    IWorldLinkHooks *hooks_      = nullptr; // @lifetime: engine
};

} // namespace xash::server
