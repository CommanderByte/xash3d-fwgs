// xash3dpp — content pipeline (model handle cache) implementation
// Legacy reference: engine/common/model.c (+ mod_studio.c / mod_sprite.c /
//   mod_alias.c non-brush loaders; brush dispatches to xash3dpp_map_loader)
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (create_pool / destroy_pool)
//   xash3dpp_core       — thread-role assertions (main-thread lifecycle)
//   xash3dpp_filesystem — model / seqgroup file loads (injected via InitParams)
//
// Skeleton: lifecycle only. The mod_known registry, the needload FSM, the
// format dispatch, and the studio collision surface are TODO (boundary O-1..O-5).
// The cache array is sized by the protocol MAX_MODELS cap when O-1 lands —
// reconcile with limits::sv_max_models (the shared 12-bit precache constant).

#include <xash3dpp/content/content.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

namespace xash::content {

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct ModelCache::Impl {
    xash::memory::PoolHandle pool_;

    // @lifetime: non-owning; the injected Filesystem is owned by EngineContext
    //   and outlives this ModelCache. Set in init(), cleared in shutdown().
    xash::filesystem::Filesystem* fs_ = nullptr;

    ContentStats stats_ {};
    // TODO(O-1): the `mod_known` model array + the 4-state needload FSM +
    //   slot-0-is-world invariant (the former model.c file-scope globals).
};

ModelCache::ModelCache() : impl_{std::make_unique<Impl>()} {}
ModelCache::~ModelCache() = default;
ModelCache::ModelCache(ModelCache&&) noexcept            = default;
ModelCache& ModelCache::operator=(ModelCache&&) noexcept = default;

bool ModelCache::init(const InitParams& params)
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    impl_->fs_   = &params.filesystem;
    impl_->pool_ = xash::memory::create_pool("content");
    return static_cast<bool>(impl_->pool_);
}

void ModelCache::shutdown()
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    // Release all pool-owned resources before destroying the pool.
    if (impl_->pool_) {
        xash::memory::destroy_pool(impl_->pool_);
        impl_->pool_ = {};
    }
    impl_->fs_ = nullptr;
}

const ContentStats& ModelCache::stats() const noexcept { return impl_->stats_; }

} // namespace xash::content
