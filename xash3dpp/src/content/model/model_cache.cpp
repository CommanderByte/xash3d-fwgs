// xash3dpp — content pipeline (model handle cache) implementation
// Legacy reference: engine/common/model.c (the mod_known cache + Mod_FindName /
//   Mod_ForName / Mod_FreeModel / Mod_LoadWorld / Mod_FreeUnused).
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (create_pool / destroy_pool)
//   xash3dpp_core       — thread-role assertions (main-thread lifecycle)
//   xash3dpp_filesystem — model / seqgroup file loads (injected via InitParams)
//
// The registry (O-1): a slot vector with the 4-state needload FSM, the
// slot-0-is-world invariant, and generation-checked opaque handles (OQ-2).
// The file load / format dispatch attaches its payload on top (loaders, O-3).

#include <xash3dpp/content/content.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/utilities/hash.hpp>
#include <xash3dpp/utilities/swap.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xash::content {

namespace {

// Advance a slot generation, skipping 0 (the null-handle sentinel).
[[nodiscard]] std::uint16_t bump_generation( std::uint16_t g ) noexcept
{
    ++g;
    return g == 0 ? std::uint16_t{ 1 } : g;
}

} // namespace

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct ModelCache::Impl
{
    struct Slot
    {
        Model         model;
        std::uint16_t generation = 0;      // 0 until first use
        bool          occupied   = false;
    };

    xash::memory::PoolHandle pool_;

    // @lifetime: non-owning; the injected Filesystem is owned by EngineContext
    //   and outlives this ModelCache. Set in init(), cleared in shutdown().
    xash::filesystem::Filesystem *fs_ = nullptr;

    // @lifetime: non-owning; the optional post-load hook is owned by the
    //   renderer/server and outlives this cache. Set in init(), cleared in shutdown().
    IModelPostProcess *post_process_ = nullptr;

    // Slot 0 is reserved for the world; regular models occupy 1..N.
    std::vector<Slot> slots_;   // @pre-reserved: content_max_models (reserve at init; cold registry)
    ContentStats      stats_ {};
};

ModelCache::ModelCache() : impl_{std::make_unique<Impl>()} {}
ModelCache::~ModelCache() = default;
ModelCache::ModelCache(ModelCache&&) noexcept            = default;
ModelCache& ModelCache::operator=(ModelCache&&) noexcept = default;

bool ModelCache::init(const InitParams& params)
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    impl_->fs_           = &params.filesystem;
    impl_->post_process_ = params.post_process;
    impl_->pool_         = xash::memory::create_pool("content");
    if (!impl_->pool_)
        return false;

    impl_->slots_.reserve(xash::limits::content_max_models);
    impl_->slots_.resize(1);  // slot 0 exists (world placeholder, unoccupied)
    return true;
}

void ModelCache::shutdown()
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    impl_->slots_.clear();
    if (impl_->pool_) {
        xash::memory::destroy_pool(impl_->pool_);
        impl_->pool_ = {};
    }
    impl_->fs_           = nullptr;
    impl_->post_process_ = nullptr;
}

const ContentStats& ModelCache::stats() const noexcept { return impl_->stats_; }

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

ModelHandle ModelCache::find( std::string_view name ) noexcept
{
    const auto &slots = impl_->slots_;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].occupied && slots[i].model.name() == name)
            return ModelHandle{ static_cast<std::uint16_t>(i), slots[i].generation };
    }
    return {};
}

ModelHandle ModelCache::find_or_alloc( std::string_view name )
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);

    if (const ModelHandle existing = find(name)) {
        ++impl_->stats_.cache_hits;
        return existing;
    }

    auto &slots = impl_->slots_;

    // Reuse a freed slot (index >= 1; slot 0 is the world) before growing.
    std::size_t idx = 0;
    for (std::size_t i = 1; i < slots.size(); ++i) {
        if (!slots[i].occupied) { idx = i; break; }
    }
    if (idx == 0) {
        if (slots.size() >= xash::limits::content_max_models)
            return {};  // cache full (legacy Host_Error)
        idx = slots.size();
        slots.emplace_back();
    }

    Impl::Slot &slot = slots[idx];
    slot.generation  = bump_generation(slot.generation);
    slot.occupied    = true;
    slot.model       = Model{ std::string(name) };
    slot.model.set_needload(NeedLoad::NeedsLoaded);
    return ModelHandle{ static_cast<std::uint16_t>(idx), slot.generation };
}

ModelHandle ModelCache::register_world( std::string_view name )
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);

    Impl::Slot &slot = impl_->slots_[0];  // world is pinned to slot 0
    slot.generation  = bump_generation(slot.generation);
    slot.occupied    = true;
    slot.model       = Model{ std::string(name) };
    slot.model.set_type(ModelType::Brush);
    slot.model.set_needload(NeedLoad::Present);
    return ModelHandle{ 0, slot.generation };
}

Model* ModelCache::resolve( ModelHandle h ) noexcept
{
    if (!h.valid() || h.index >= impl_->slots_.size())
        return nullptr;
    Impl::Slot &slot = impl_->slots_[h.index];
    if (!slot.occupied || slot.generation != h.generation)
        return nullptr;
    return &slot.model;
}

const Model* ModelCache::resolve( ModelHandle h ) const noexcept
{
    if (!h.valid() || h.index >= impl_->slots_.size())
        return nullptr;
    const Impl::Slot &slot = impl_->slots_[h.index];
    if (!slot.occupied || slot.generation != h.generation)
        return nullptr;
    return &slot.model;
}

void ModelCache::free_model( ModelHandle h ) noexcept
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    if (!h.valid() || h.index >= impl_->slots_.size())
        return;
    Impl::Slot &slot = impl_->slots_[h.index];
    if (!slot.occupied || slot.generation != h.generation)
        return;
    slot.occupied   = false;
    slot.generation = bump_generation(slot.generation);  // invalidate stale handles
    slot.model      = Model{};
}

std::size_t ModelCache::live_count() const noexcept
{
    std::size_t n = 0;
    for (const auto &slot : impl_->slots_)
        if (slot.occupied) ++n;
    return n;
}

Result<void> ModelCache::load_from_bytes( ModelHandle h, std::span<const std::byte> file )
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);

    Model *m = resolve(h);
    if (!m)
        return std::unexpected(LoadError::NotFound);
    if (file.size() < sizeof(std::int32_t))
        return std::unexpected(LoadError::Truncated);

    // Cheat-detection: a checksum-required model that was already loaded must
    // keep the same CRC on reload (legacy Mod_LoadModel CRC guard, OQ-6).
    const std::uint32_t crc = ::xash::utilities::crc32(file.data(), file.size());
    if (any(m->crc_flags() & CrcFlags::ChecksumDone)
        && any(m->crc_flags() & CrcFlags::ShouldChecksum)
        && m->crc() != crc)
        return std::unexpected(LoadError::CrcMismatch);

    // Dispatch on the file magic (legacy Mod_LoadModel switch; O-3).
    const auto magic = ::xash::utilities::read_le<std::int32_t>(file.data());
    switch (magic)
    {
    case k_studio_ident:
    {
        Result<StudioModel> sm = parse_studio(file);
        if (!sm)
            return std::unexpected(sm.error());
        m->set_studio(std::move(*sm));
        break;
    }
    case k_sprite_ident:
    {
        Result<SpriteModel> sp = parse_sprite(file);
        if (!sp)
            return std::unexpected(sp.error());
        m->set_sprite(std::move(*sp));
        break;
    }
    case k_alias_ident:
    {
        Result<AliasModel> al = parse_alias(file);
        if (!al)
            return std::unexpected(al.error());
        m->set_alias(std::move(*al));
        break;
    }
    // TODO(O-3): 29/30/BSP2 -> brush (dispatch to map_loader, OQ-3).
    default:
        return std::unexpected(LoadError::BadMagic);
    }

    // Common post-load state: mark present, record the CRC.
    m->set_needload(NeedLoad::Present);
    m->set_crc(crc);
    m->set_crc_flags(m->crc_flags() | CrcFlags::ChecksumDone);

    // Renderer / physics post-load hook (OQ-4); a rejection frees the model
    // (legacy Mod_ProcessRenderData / Mod_ProcessUserData returning 0).
    if (impl_->post_process_ && !impl_->post_process_->on_model_loaded(h, *m, file))
    {
        free_model(h);
        return std::unexpected(LoadError::UnsupportedFeature);
    }

    ++impl_->stats_.models_loaded;
    return {};
}

void ModelCache::need_crc( std::string_view name, bool need )
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    const ModelHandle h = find_or_alloc(name);
    Model *m = resolve(h);
    if (!m)
        return;
    if (need)
        m->set_crc_flags(m->crc_flags() | CrcFlags::ShouldChecksum);
    else
        m->set_crc_flags(m->crc_flags() & ~CrcFlags::ShouldChecksum);
}

bool ModelCache::validate_crc( std::string_view name, std::uint32_t crc ) const noexcept
{
    for (const auto &slot : impl_->slots_)
    {
        if (slot.occupied && slot.model.name() == name)
            return any(slot.model.crc_flags() & CrcFlags::ChecksumDone)
                   && slot.model.crc() == crc;
    }
    return false;
}

std::vector<ModelCache::ModelInfo> ModelCache::model_infos() const
{
    std::vector<ModelInfo> out;
    out.reserve(live_count());
    for (const auto &slot : impl_->slots_)
    {
        if (slot.occupied)
            out.push_back(ModelInfo{ std::string(slot.model.name()),
                                     slot.model.type(),
                                     slot.model.needload(),
                                     slot.model.crc() });
    }
    return out;
}

} // namespace xash::content
