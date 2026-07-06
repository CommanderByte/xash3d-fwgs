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
#include <xash3dpp/content/model.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem { class Filesystem; }

namespace xash::content {

// ---------------------------------------------------------------------------
// IModelPostProcess (OQ-4) — the post-load hook the renderer (legacy
// Mod_ProcessRenderData) or dedicated-server physics (Mod_ProcessUserData)
// implements. content calls out after a model's payload is attached; it depends
// on this abstraction and never links toward the renderer. Returning false
// fails the load (legacy frees the model). Optional — absent for headless loads.
// ---------------------------------------------------------------------------

struct IModelPostProcess
{
    virtual ~IModelPostProcess() = default;

    [[nodiscard]] virtual bool on_model_loaded( ModelHandle handle, Model& model,
                                                std::span<const std::byte> file ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// Injected dependencies (P-3 context-first — no file-scope globals).
// ---------------------------------------------------------------------------

struct InitParams {
    // Model / seqgroup / external-texture file loads.
    xash::filesystem::Filesystem& filesystem;
    // Renderer / dedicated-server-physics post-load callback (OQ-4). Null on a
    // headless / test load — the parse still runs, the hook is simply skipped.
    IModelPostProcess* post_process = nullptr;
    // TODO(Chunk 7): imagelib::ImageDecoder& for skin/miptex decode (O-2);
    //   the map_loader brush-dispatch seam (boundary OQ-3).
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

    // ---- model registry (O-1; the typed handle-lookup deliverable) -------
    // These manage slots + the needload FSM; the file load/format dispatch
    // (studio/sprite/alias/brush) attaches its payload on top (loaders, O-3).

    // Find an already-registered model by name, or the null handle if absent.
    [[nodiscard]] ModelHandle find( std::string_view name ) noexcept;

    // Find-or-allocate a slot for `name` (legacy Mod_FindName). Never returns
    // the world slot; returns the null handle only if the cache is full.
    [[nodiscard]] ModelHandle find_or_alloc( std::string_view name );

    // Reserve slot 0 for the world model (legacy Mod_LoadWorld; slot-0-world
    // invariant). Re-registering the world reuses slot 0.
    [[nodiscard]] ModelHandle register_world( std::string_view name );

    // Resolve a handle to its model, or nullptr if the handle is null or stale
    // (generation mismatch — use-after-free safe).
    [[nodiscard]] Model*       resolve( ModelHandle h ) noexcept;
    [[nodiscard]] const Model* resolve( ModelHandle h ) const noexcept;

    // Free a model slot; bumps its generation so outstanding handles go stale.
    void free_model( ModelHandle h ) noexcept;

    // Number of currently-occupied slots (world included).
    [[nodiscard]] std::size_t live_count() const noexcept;

    // Load a model into `h` from an in-memory file image, dispatching on the
    // magic (O-3): IDST/IDSP/IDPO. Attaches the parsed payload, computes the
    // file CRC, and marks the slot Present. FS-decoupled for testability
    // (OQ-1); the production path loads the bytes via the injected filesystem.
    // A reload with a changed CRC on a checksum-required model fails
    // (LoadError::CrcMismatch — cheat detection).
    [[nodiscard]] Result<void> load_from_bytes( ModelHandle h, std::span<const std::byte> file );

    // ---- CRC cheat-detection surface (OQ-6; server consumes it) ----------
    // Flag a model (by name, allocating a slot if needed) as checksum-required
    // (legacy Mod_NeedCRC). Cleared by passing false.
    void need_crc( std::string_view name, bool need );

    // True iff a loaded model of that name has the given CRC (Mod_ValidateCRC).
    [[nodiscard]] bool validate_crc( std::string_view name, std::uint32_t crc ) const noexcept;

    // ---- P-4 typed introspection (debug / MCP / stats consumers) ---------
    // An owned snapshot of one registered model — safe to hold past a load.
    struct ModelInfo
    {
        std::string   name;
        ModelType     type     = ModelType::Bad;
        NeedLoad      needload = NeedLoad::Unreferenced;
        std::uint32_t crc      = 0;
    };

    // Snapshot every occupied slot (cold introspection path; the typed surface
    // debug/MCP/stats consumers read instead of poking registry internals).
    [[nodiscard]] std::vector<ModelInfo> model_infos() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::content
