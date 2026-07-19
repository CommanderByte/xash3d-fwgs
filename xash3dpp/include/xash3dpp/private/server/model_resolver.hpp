#pragma once
// xash3dpp — production IModelResolver: precache-index → brush submodel.
// Legacy reference: engine/server/sv_init.c — SV_SpawnServer submodel
// precache (:1046-1051), sv.models[] population via Mod_ForName; the world
// model is always precache slot 1 (mod_local.h:34 WORLD_INDEX).
// Deep dive: docs/legacy-survey/deep-dive-server-lifecycle.md §6.
//
// The legacy engine keeps a parallel sv.models[] cache filled at precache
// time by Mod_ForName.  Brush models are the world (slot 1 → submodel 0)
// and the "*N" inline submodels; everything else (studio/sprite) needs the
// Chunk 7 content pipeline.  This resolver derives brush identity LAZILY
// from the precache name + the loaded WorldData — no separate cache array —
// which is exactly the information SV_SetModel and the trace hull selector
// consume.  Non-brush names resolve to nullopt (is_studio → true, the
// bbox-fallback path) until Chunk 7 lands real model loading.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/content/content.hpp>
#include <xash3dpp/content/model.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/precache.hpp>
#include <xash3dpp/private/server/world_trace.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>

namespace xash::filesystem { class Filesystem; }

namespace xash::server {

// ---------------------------------------------------------------------------
// StudioHullCache — the legacy 16-entry pose→hull ring (mod_studio.c
// STUDIO_CACHESIZE / Mod_CheckStudioCache / Mod_AddToStudioCache).
//
// Keys compare EXACTLY like legacy: float `==` on frame, VectorCompare
// (exact) on angles/origin/size, memcmp on controller[4]/blending[2].
// Layout is the legacy POOLED one: 16 ring entries share ONE 128-hull pool
// (cache_hull[MAXSTUDIOBONES]) and the whole cache clears wholesale when
// an add would exhaust the pool (Mod_AddToStudioCache's
// `numhitboxes + cache_current_hull >= MAXSTUDIOBONES` reset) — so hit/miss
// behaviour, capacity, AND the exhaustion reset all match. Two deliberate
// legacy quirks preserved: (a) the CS shield state is NOT part of the key —
// a same-pose lookup returns whatever skip state was cached
// (Mod_HullForStudio probes the cache before computing bSkipShield);
// (b) entries hold POST-skip hull sets. ~14 KB total — ServerRuntime lives
// on test stacks, so no per-entry max-size arrays. Cleared on level change
// (ModelResolver::bind).
// ---------------------------------------------------------------------------

class StudioHullCache
{
public:
    struct Key
    {
        int                     modelindex = 0;
        float                   frame      = 0.0f;
        int                     sequence   = 0;
        ::xash::utilities::Vec3 angles{}, origin{}, size{};
        std::uint8_t            controllers[4]{};
        std::uint8_t            blending[2]{};
    };

    // Cached hulls for an exactly-matching key, or an empty span (miss).
    // @lifetime: cache entry — the span aliases the shared pool and is
    // invalidated by the next add() (wholesale clear on exhaustion) or
    // clear(); consume or copy before mutating the cache.
    [[nodiscard]] std::span<const ::xash::content::StudioHitboxHull>
    find( const Key &k ) const noexcept
    {
        for ( unsigned i = 0; i < k_entries; ++i )
        {
            const Entry &e = entries_[( current_ - i ) & k_mask];
            if ( e.count < 0 || !same_key( e.key, k ))
                continue;
            return { pool_.data() + e.start,
                     static_cast<std::size_t>( e.count ) };
        }
        return {};
    }

    void add( const Key &k,
              std::span<const ::xash::content::StudioHitboxHull> hulls ) noexcept
    {
        if ( hulls.size() > pool_.size() )
            return; // cannot fit even an empty pool (defensive; n <= 128)
        // Legacy Mod_AddToStudioCache: pool exhaustion clears EVERYTHING.
        if ( pool_used_ + hulls.size() >= pool_.size() )
            clear();

        ++current_;
        Entry &e = entries_[current_ & k_mask];
        e.key    = k;
        e.start  = pool_used_;
        e.count  = static_cast<int>( hulls.size() );
        for ( std::size_t i = 0; i < hulls.size(); ++i )
            pool_[pool_used_ + i] = hulls[i];
        pool_used_ += hulls.size();
    }

    void clear() noexcept
    {
        for ( Entry &e : entries_ )
            e.count = -1;
        current_   = 0;
        pool_used_ = 0;
    }

private:
    static constexpr unsigned k_entries = 16; // legacy STUDIO_CACHESIZE
    static constexpr unsigned k_mask    = k_entries - 1;

    struct Entry
    {
        Key         key{};
        std::size_t start = 0;  // offset into the shared pool
        int         count = -1; // -1 = empty slot
    };

    [[nodiscard]] static bool
    same_key( const Key &a, const Key &b ) noexcept
    {
        return a.modelindex == b.modelindex && a.frame == b.frame &&
               a.sequence == b.sequence &&
               exact( a.angles, b.angles ) && exact( a.origin, b.origin ) &&
               exact( a.size, b.size ) &&
               std::memcmp( a.controllers, b.controllers, 4 ) == 0 &&
               std::memcmp( a.blending, b.blending, 2 ) == 0;
    }

    [[nodiscard]] static bool
    exact( const ::xash::utilities::Vec3 &a,
           const ::xash::utilities::Vec3 &b ) noexcept
    {
        return a.x == b.x && a.y == b.y && a.z == b.z; // legacy VectorCompare
    }

    std::array<Entry, k_entries> entries_{};
    // Legacy cache_hull[MAXSTUDIOBONES]: ONE shared pool for all entries.
    std::array<::xash::content::StudioHitboxHull,
               ::xash::limits::studio_max_bones> pool_{};
    std::size_t pool_used_ = 0;
    unsigned    current_   = 0;
};

class ModelResolver final : public IModelResolver
{
public:
    // Bound each SV_SpawnServer to the freshly loaded world + its precache
    // tables + the filesystem (studio loads); cleared (all null) on deactivate.
    // The index -> studio-handle map is reset per spawn (fresh precache table).
    // @lifetime: engine
    void bind( const ::xash::map_loader::WorldData *world,
               const PrecacheTables *precache,
               ::xash::filesystem::Filesystem *fs ) noexcept
    {
        world_    = world;
        precache_ = precache;
        fs_       = fs;
        // Level transition: reap the previous level's studio models (OQ-8) so
        // the cache does not grow unbounded across maps, then drop the stale
        // modelindex map. The next level lazily reloads what it references.
        if ( cache_ready_ )
        {
            studio_cache_.purge_for_level_change();
            studio_cache_.free_unused();
        }
        studio_handles_.clear();
        // OQ-2: pose→hull entries reference the outgoing level's models
        // (and modelindex keys are per-precache-table) — drop them with it.
        hull_cache_.clear();
    }

    [[nodiscard]] std::optional<BrushModel>
    brush_model( int modelindex ) noexcept override;

    [[nodiscard]] bool is_studio( int modelindex ) noexcept override;

    // Lazily load (and cache) the studio model for `modelindex`, returning its
    // studiohdr byte image (empty if absent / not a studio model). The cache
    // dedups by precache name; a negative result is cached too.
    [[nodiscard]] std::span<const std::byte> studio_bytes( int modelindex ) noexcept override;

    // OQ-2: Mod_HullForStudio's geometry + cache half — STUDIO_TRACE_HITBOX
    // gate, quake-bug pitch flip, bone solve → oriented hitbox hulls, the
    // CS shield skip, and the 16-entry pose cache. See world_trace.hpp for
    // the caller-side gating contract.
    [[nodiscard]] int
    studio_hulls( int modelindex, const StudioHullPose &pose,
                  std::span<::xash::content::StudioHitboxHull> out ) noexcept override;

private:
    [[nodiscard]] bool ensure_cache() noexcept;

    const ::xash::map_loader::WorldData *world_    = nullptr; // @lifetime: engine — borrowed map_loader WorldData (outlives the resolver)
    const PrecacheTables                *precache_ = nullptr; // @lifetime: runtime — borrowed PrecacheTables (owned by ServerRuntime, outlives the resolver)
    ::xash::filesystem::Filesystem      *fs_       = nullptr; // @lifetime: engine — borrowed Filesystem (injected dep, Q-4)

    // Studio model cache (lazily init) + the per-spawn modelindex -> handle map.
    ::xash::content::ModelCache                            studio_cache_;
    bool                                                   cache_ready_ = false;
    std::unordered_map<int, ::xash::content::ModelHandle>  studio_handles_;

    // OQ-2 pose→hull ring (legacy mod_studio statics, instance-owned per P-3).
    StudioHullCache hull_cache_;
};

} // namespace xash::server
