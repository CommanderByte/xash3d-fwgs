// xash3dpp — production IModelResolver (Chunk 6 S7b).
// See model_resolver.hpp for the design note.

#include <xash3dpp/private/server/model_resolver.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/utilities/string.hpp>

namespace xash::server {

std::optional<BrushModel> ModelResolver::brush_model( int modelindex ) noexcept
{
    if ( world_ == nullptr || modelindex <= 0 )
        return std::nullopt;

    std::size_t submodel = 0;
    if ( modelindex == ::xash::abi::k_world_index )
    {
        // The world model is submodel 0; its precache name is
        // "maps/<name>.bsp", not "*0", so it is special-cased.
        submodel = 0;
    }
    else
    {
        const char *name =
            precache_ != nullptr
                ? precache_->model_name( static_cast<std::size_t>( modelindex ))
                : "";
        if ( name[0] != '*' )
            return std::nullopt; // studio/sprite/unknown → not a brush model
        submodel =
            static_cast<std::size_t>( ::xash::utilities::atoi( name + 1 ));
    }

    const auto subs = world_->submodels();
    if ( submodel >= subs.size() )
        return std::nullopt;

    BrushModel bm;
    bm.submodel   = submodel;
    bm.has_origin =
        ( subs[submodel].flags & ::xash::map_loader::k_model_has_origin ) != 0;
    return bm;
}

bool ModelResolver::is_studio( int modelindex ) noexcept
{
    // Brush models take the BSP hull path; anything else falls back to the
    // bbox hull exactly like the legacy no-hitbox-data studio case (Chunk 7
    // supplies real studio hulls via the OQ-2 IStudioHullProvider).
    return !brush_model( modelindex ).has_value();
}

} // namespace xash::server
