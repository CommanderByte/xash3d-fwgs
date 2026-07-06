#pragma once
// xash3dpp — EdictArena: the Q-20 single authoritative entity store.
// Legacy reference: engine/server/sv_game.c — SV_InitEdict (:983),
// SV_FreeEdict (:1004), SV_AllocEdict (:1041), SV_FreePrivateData (:961),
// pfnPvAllocEntPrivateData / pfnPEntityOfEntOffset byte-offset contract.
// Deep dive: docs/legacy-survey/deep-dive-server-game-dll-bridge.md.
//
// The ABI-exact edict_t array IS the entity state — no shadow copies, no
// projection (Q-20).  The array base is allocated once and never moves
// (game DLLs hold raw pointers and do byte-offset arithmetic against it).
// Load-bearing legacy semantics preserved here:
//   • free() scrubs only a specific entvars subset and leaves the rest
//     STALE while the edict is free (games read freed edicts through the
//     peoei bugcomp path);
//   • init_edict() (alloc/reuse) zeroes all entvars, then sets
//     pContainingEntity = self and controller[0..3] = 0x7F;
//   • alloc() reuse policy: a freed slot is reusable when its freetime is
//     inside the first-seconds relax window or older than the grace
//     period (sv_game.c:1049-1051 comment);
//   • serialnumber increments on free (EHANDLE invalidation).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::server {

// Installed by the game-DLL bridge (S6): NEW_DLL_FUNCTIONS::
// pfnOnFreeEntPrivateData must run before the engine releases the block.
using PrivateDataReleaser = void ( * )( void *ctx, ::xash::abi::edict_t *ed );

class EdictArena
{
public:
    EdictArena() = default;

    EdictArena( const EdictArena & )            = delete;
    EdictArena &operator=( const EdictArena & ) = delete;

    // Allocates the fixed edict_t array (zeroed) from `pool`.  The base
    // pointer never changes afterwards.  `reserved` = worldspawn + client
    // slots (legacy maxclients + 1): alloc() never scans below it, and
    // num_entities starts there.
    [[nodiscard]] bool init( ::xash::memory::PoolHandle pool,
                             std::size_t max_edicts, std::size_t reserved );
    void shutdown();

    // SV_AllocEdict: reuse the first eligible freed slot in
    // [reserved, num_entities), else grow.  Returns nullptr when the
    // arena is exhausted (legacy Host_Error's — the bridge maps this to
    // the host error policy, Q-5).
    [[nodiscard]] ::xash::abi::edict_t *alloc_edict( double sv_time );

    // SV_FreeEdict: idempotent; unlinking from the world areas is the
    // caller's job (S5 owns the area links — legacy calls SV_UnlinkEdict
    // first).  Scrubs the legacy subset, stamps freetime, bumps
    // serialnumber.
    void free_edict( ::xash::abi::edict_t *ed, double sv_time );

    // SV_InitEdict: release private data, zero entvars, self-link
    // pContainingEntity, controller[0..3] = 0x7F, free = false.
    void init_edict( ::xash::abi::edict_t *ed );

    // Legacy rounds every private-data request up to 16 bytes
    // (sv_game.c:2955, "(cb + 15) & ~15") — a deliberate over-allocation
    // because shipped binary mods (Poke646 et al.) write past the end of
    // their requested block ("this is trashed last sixteen bytes").
    [[nodiscard]] static constexpr std::size_t
    private_data_size( std::size_t cb ) noexcept
    {
        return ( cb + 15 ) & ~std::size_t{ 15 };
    }

    // pfnPvAllocEntPrivateData: (re)allocate the DLL private block from
    // the pool (zeroed, size rounded via private_data_size).  Releases
    // any existing block first.
    [[nodiscard]] void *alloc_private( ::xash::abi::edict_t *ed,
                                       std::size_t size );

    // SV_FreePrivateData.  Pre: pvPrivateData, when set, came from
    // alloc_private (Known Deviation: legacy tolerates foreign pointers
    // via Mem_IsAllocatedExt; the xash3dpp memory API has no ownership
    // probe — revisit if a real mod assigns its own block).
    void free_private( ::xash::abi::edict_t *ed );

    void set_private_releaser( PrivateDataReleaser fn, void *ctx ) noexcept
    {
        releaser_     = fn;
        releaser_ctx_ = ctx;
    }

    // Index / byte-offset contract (pfnPEntityOfEntOffset et al.).
    [[nodiscard]] ::xash::abi::edict_t *edict_num( std::size_t i ) const noexcept;
    [[nodiscard]] int  index_of( const ::xash::abi::edict_t *ed ) const noexcept;
    [[nodiscard]] std::ptrdiff_t offset_of( const ::xash::abi::edict_t *ed ) const noexcept;
    [[nodiscard]] ::xash::abi::edict_t *ent_of_offset( std::ptrdiff_t off ) const noexcept;

    [[nodiscard]] ::xash::abi::edict_t *base() const noexcept { return edicts_; }
    [[nodiscard]] std::size_t max_edicts() const noexcept { return max_edicts_; }
    [[nodiscard]] std::size_t num_entities() const noexcept { return num_entities_; }
    [[nodiscard]] std::size_t reserved() const noexcept { return reserved_; }

    // Legacy SV_SpawnServer resets numEntities each spawn.
    void set_num_entities( std::size_t n ) noexcept { num_entities_ = n; }

    // SV_SetupClients (S7b): maxclients can change between spawns while
    // the arena persists — the alloc floor tracks svs.maxclients + 1.
    void set_reserved( std::size_t n ) noexcept { reserved_ = n; }

private:
    ::xash::memory::PoolHandle pool_;
    ::xash::abi::edict_t      *edicts_       = nullptr;
    std::size_t                max_edicts_   = 0;
    std::size_t                num_entities_ = 0;
    std::size_t                reserved_     = 0;
    PrivateDataReleaser        releaser_     = nullptr;
    void                      *releaser_ctx_ = nullptr; // @lifetime: caller-owned — opaque context for releaser_ (installed at arena init, not copied)
};

} // namespace xash::server
