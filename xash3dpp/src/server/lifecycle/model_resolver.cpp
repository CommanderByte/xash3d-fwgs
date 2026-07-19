// xash3dpp — production IModelResolver (Chunk 6 S7b).
// See model_resolver.hpp for the design note.

#include <xash3dpp/private/server/model_resolver.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/content/bone_solver.hpp>
#include <xash3dpp/content/studio.hpp>
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

int ModelResolver::studio_hulls( int modelindex, const StudioHullPose &pose,
                                 std::span<::xash::content::StudioHitboxHull> out ) noexcept
{
    namespace ct = ::xash::content;

    const auto bytes = studio_bytes( modelindex );
    if ( bytes.empty() )
        return 0; // not a studio model → bbox fallback (legacy NULL hull)

    const ct::StudioView hdr( bytes );

    // The legacy OR-gate: STUDIO_TRACE_HITBOX forces the hitbox path even
    // for sized boxes; otherwise only the caller's useComplexHull opens it.
    if (( hdr.flags() & ct::k_studio_trace_hitbox ) == 0 && !pose.force_complex )
        return 0;

    StudioHullCache::Key key;
    key.modelindex = modelindex;
    key.frame      = pose.frame;
    key.sequence   = pose.sequence;
    key.angles     = pose.angles;
    key.origin     = pose.origin;
    key.size       = pose.size;
    for ( int i = 0; i < 4; ++i )
        key.controllers[i] = pose.controllers[i];
    key.blending[0] = pose.blending[0];
    key.blending[1] = pose.blending[1];

    if ( pose.use_cache )
    {
        const auto cached = hull_cache_.find( key );
        if ( !cached.empty() )
        {
            const std::size_t n =
                cached.size() < out.size() ? cached.size() : out.size();
            for ( std::size_t i = 0; i < n; ++i )
                out[i] = cached[i];
            return static_cast<int>( n );
        }
    }

    // Quake-bug pitch flip (Mod_HullForStudio): host.features
    // ENGINE_COMPENSATE_QUAKE_BUG is unwired (== 0, recorded Chunk 7
    // deferral) so the flip is unconditional, matching the legacy default.
    ct::BoneSetupInput in;
    in.frame       = pose.frame;
    in.sequence    = pose.sequence;
    in.angles      = pose.angles;
    in.angles.x    = -in.angles.x;
    in.origin      = pose.origin;
    in.controllers = std::span<const std::uint8_t>( pose.controllers, 4 );
    in.blending    = std::span<const std::uint8_t>( pose.blending, 2 );

    ct::BuiltinBoneSolver solver;
    ct::StudioHitboxHull dense[::xash::limits::studio_max_bones];
    const int n = ct::studio_hitbox_hulls(
        hdr, in, pose.size, solver,
        std::span<ct::StudioHitboxHull>( dense ));
    if ( n <= 0 )
        return 0;

    // CS shield skip (gamestate == 1): legacy skips WRITING slot 21 and
    // reports numhitboxes-1, so its trace loop runs slots 0..n-2 — i.e. the
    // DEFINED hitboxes {0..20, 22..n-2} plus one STALE slot (21, leftover
    // planes from a previous call), and the final real hitbox is never
    // traced. The stale slot has no defined semantics to reproduce, so we
    // COMPACT to exactly the defined set: iterate hitboxes 0..n-2 skipping
    // index 21 (for n <= 21 that is simply "all but the last" — the one
    // fully-defined legacy outcome). Hitgroups travel WITH each hull, so
    // the index shift cannot misattribute them. Recorded in
    // server-boundary.md (OQ-2 quirks).
    ct::StudioHitboxHull compact[::xash::limits::studio_max_bones];
    int count = 0;
    if ( pose.skip_shield )
    {
        for ( int i = 0; i < n - 1; ++i )
        {
            if ( i == 21 )
                continue;
            compact[count++] = dense[i];
        }
    }
    else
    {
        for ( int i = 0; i < n; ++i )
            compact[count++] = dense[i];
    }

    if ( pose.use_cache )
        hull_cache_.add( key, std::span<const ct::StudioHitboxHull>(
                                  compact, static_cast<std::size_t>( count )));

    const int wrote =
        static_cast<int>( out.size()) < count
            ? static_cast<int>( out.size()) : count;
    for ( int i = 0; i < wrote; ++i )
        out[static_cast<std::size_t>( i )] = compact[i];
    return wrote;
}

} // namespace xash::server
