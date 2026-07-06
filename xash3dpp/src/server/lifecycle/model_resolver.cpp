// xash3dpp — production IModelResolver (Chunk 6 S7b).
// See model_resolver.hpp for the design note.

#include <xash3dpp/private/server/model_resolver.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <vector>

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

bool ModelResolver::ensure_cache() noexcept
{
    if ( cache_ready_ )
        return true;
    if ( fs_ == nullptr )
        return false;
    if ( !studio_cache_.init( ::xash::content::InitParams{ *fs_ } ))
        return false;
    cache_ready_ = true;
    return true;
}

std::span<const std::byte> ModelResolver::studio_bytes( int modelindex ) noexcept
{
    if ( modelindex <= 0 || precache_ == nullptr )
        return {};

    ::xash::content::ModelHandle h{};
    const auto it = studio_handles_.find( modelindex );
    if ( it != studio_handles_.end() )
    {
        h = it->second; // may be the null handle (a cached negative)
    }
    else
    {
        const char *name =
            precache_->model_name( static_cast<std::size_t>( modelindex ));
        if ( name == nullptr || name[0] == '\0' || name[0] == '*' )
        {
            studio_handles_.emplace( modelindex, ::xash::content::ModelHandle{} );
            return {}; // brush / inline submodel / unset — not a studio model
        }
        if ( !ensure_cache() )
            return {};

        std::vector<std::byte> bytes = fs_->load_file( name );
        h = studio_cache_.find_or_alloc( name );
        if ( bytes.empty() || !h.valid()
             || !studio_cache_.load_from_bytes( h, bytes ).has_value() )
        {
            studio_handles_.emplace( modelindex, ::xash::content::ModelHandle{} );
            return {};
        }
        studio_handles_.emplace( modelindex, h );
    }

    const ::xash::content::Model *m = studio_cache_.resolve( h );
    if ( m == nullptr )
        return {};
    const ::xash::content::StudioModel *sm = m->studio();
    if ( sm == nullptr )
        return {}; // loaded but not a studio format (sprite/alias)
    return sm->bytes();
}

} // namespace xash::server
