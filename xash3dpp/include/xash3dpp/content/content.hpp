#pragma once
// xash3dpp — content pipeline (model handle cache) public API
// Legacy reference: engine/common/model.c (the mod_known cache) + the non-brush
//   loaders mod_studio.c / mod_sprite.c / mod_alias.c. The brush path dispatches
//   to xash3dpp_map_loader; image codecs live in xash3dpp_imagelib (Q-11).
// Boundary: docs/boundaries/content-boundary.md
// Modernization: docs/modernization-opportunities/content-modernization.md O-1.
//
// @thread-safety: init()/shutdown() and every load/lookup entry are main-thread
//   only (assert Main). The registry replaces the legacy `mod_known[]` +
//   `mod_studiohdr` globals (boundary H-1 / O-1); off-main readers get a
//   published snapshot (P-2) once a consumer schedules it — never a live ref.

#include <xash3dpp/content/errors.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace xash::filesystem { class Filesystem; }

namespace xash::content {

// ---------------------------------------------------------------------------
// Injected dependencies (P-3 context-first — no file-scope globals).
// ---------------------------------------------------------------------------

struct InitParams {
    // Model / seqgroup / external-texture file loads.
    xash::filesystem::Filesystem& filesystem;
    // TODO(Chunk 7): imagelib::ImageDecoder& for skin/miptex decode (O-2);
    //   the map_loader brush-dispatch seam (boundary OQ-3); an IModelPostProcess
    //   for the renderer/physics DLL callback (boundary OQ-4).
};

// ---------------------------------------------------------------------------
// Instrumentation (three-tier model — docs/design/debug-stats-design.md)
// ---------------------------------------------------------------------------

struct ContentStats {
    // Tier 1 — always-on: cold load-path counters (load is not per-frame hot).
    std::uint64_t models_loaded = 0;  // successful loads (legacy Mod_LoadModel)
    std::uint64_t cache_hits    = 0;  // find_or_load resolved to a live slot
};

// ---------------------------------------------------------------------------
// ModelCache — the typed model handle registry (boundary O-1). Encapsulates the
// legacy `mod_known[]` array, the 4-state `needload` FSM, the slot-0-is-world
// invariant, and the level-transition purge as class invariants (Q-22).
// ---------------------------------------------------------------------------

class ModelCache {
public:
    ModelCache();
    ~ModelCache();

    ModelCache(const ModelCache&)            = delete;
    ModelCache& operator=(const ModelCache&) = delete;
    ModelCache(ModelCache&&) noexcept;
    ModelCache& operator=(ModelCache&&) noexcept;

    // Lifecycle — main-thread only.
    [[nodiscard]] bool init(const InitParams& params);
    void shutdown();

    [[nodiscard]] const ContentStats& stats() const noexcept;

    // TODO(Chunk 7, O-1): the model-registry surface —
    //   find_or_load / extradata / purge_for_level_change / free_unused /
    //   validate_crc, plus the typed ModelHandle lookup (boundary OQ-2) and the
    //   server-side studio collision queries (hull_for_studio / bone_position /
    //   attachment). Dispatch is a std::variant over the four formats (O-3).

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::content
