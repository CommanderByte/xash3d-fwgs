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

#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/private/server/precache.hpp>
#include <xash3dpp/private/server/world_trace.hpp>

namespace xash::server {

class ModelResolver final : public IModelResolver
{
public:
    // Bound each SV_SpawnServer to the freshly loaded world + its precache
    // tables; cleared (both null) on deactivate.  @lifetime: engine
    void bind( const ::xash::map_loader::WorldData *world,
               const PrecacheTables *precache ) noexcept
    {
        world_    = world;
        precache_ = precache;
    }

    [[nodiscard]] std::optional<BrushModel>
    brush_model( int modelindex ) noexcept override;

    [[nodiscard]] bool is_studio( int modelindex ) noexcept override;

private:
    const ::xash::map_loader::WorldData *world_    = nullptr; // @lifetime: engine — borrowed map_loader WorldData (outlives the resolver)
    const PrecacheTables                *precache_ = nullptr; // @lifetime: runtime — borrowed PrecacheTables (owned by ServerRuntime, outlives the resolver)
};

} // namespace xash::server
