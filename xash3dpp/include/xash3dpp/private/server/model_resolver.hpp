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
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/precache.hpp>
#include <xash3dpp/private/server/world_trace.hpp>

#include <cstddef>
#include <span>
#include <unordered_map>

namespace xash::filesystem { class Filesystem; }

namespace xash::server {

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
    }

    [[nodiscard]] std::optional<BrushModel>
    brush_model( int modelindex ) noexcept override;

    [[nodiscard]] bool is_studio( int modelindex ) noexcept override;

    // Lazily load (and cache) the studio model for `modelindex`, returning its
    // studiohdr byte image (empty if absent / not a studio model). The cache
    // dedups by precache name; a negative result is cached too.
    [[nodiscard]] std::span<const std::byte> studio_bytes( int modelindex ) noexcept override;

private:
    [[nodiscard]] bool ensure_cache() noexcept;

    const ::xash::map_loader::WorldData *world_    = nullptr; // @lifetime: engine — borrowed map_loader WorldData (outlives the resolver)
    const PrecacheTables                *precache_ = nullptr; // @lifetime: runtime — borrowed PrecacheTables (owned by ServerRuntime, outlives the resolver)
    ::xash::filesystem::Filesystem      *fs_       = nullptr; // @lifetime: engine — borrowed Filesystem (injected dep, Q-4)

    // Studio model cache (lazily init) + the per-spawn modelindex -> handle map.
    ::xash::content::ModelCache                            studio_cache_;
    bool                                                   cache_ready_ = false;
    std::unordered_map<int, ::xash::content::ModelHandle>  studio_handles_;
};

} // namespace xash::server
