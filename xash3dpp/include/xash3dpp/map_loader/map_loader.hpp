#pragma once
// xash3dpp — MapLoader: server-driven, client/demo-observed map-load FSM
// Legacy reference: engine/common/host_state.c
//   (host_state_t, game_status_t, COM_InitHostState, COM_NewGame,
//    COM_LoadLevel, COM_LoadGame, COM_ChangeLevel, COM_Frame outer-loop)
//
// Decision ref: docs/boundaries/host-boundary.md  Resolved-decision OQ-2
//
// @thread-safety: every MapLoader mutating entry point (init/shutdown/
// new_game/load_level/load_game/change_level/run_frame_step/
// attach_observer/detach_observer/set_level_executor/load_world/
// clear_world) is main-thread only (assert_thread_role Main — HB-3 gap
// closed 2026-07-19); the const accessors state()/current_map()/world()/
// stats() are read on the main thread and a borrowed world() pointer must
// NOT be held across a load/clear (Q-6, threading-analysis).
//
// MapLoader is a sibling subsystem of Host inside EngineContext.  It is
// driven by:
//   • console commands  (map, changelevel, load, demos)
//   • client            (background map at UI startup)
//   • server            (game-DLL changelevel)
// and is observed by:
//   • client    (loading plaque, demo bookkeeping)
//   • server    (savegame staging)

#include <xash3dpp/map_loader/world.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace xash::filesystem { class Filesystem; }

namespace xash {

// ---------------------------------------------------------------------------
// Map-load state machine
// ---------------------------------------------------------------------------

enum class MapLoadState : std::uint8_t
{
    RunFrame,     // normal frame processing
    LoadLevel,    // a new level is being loaded
    LoadGame,     // savegame is being restored
    ChangeLevel,  // landmark-based level transition in progress
    GameShutdown, // in-progress shutdown of the current game session
};

// Observer interface: client and server attach to receive load events.
// Implementations are expected to be cheap and non-blocking.
struct IMapLoaderObserver
{
    virtual ~IMapLoaderObserver() = default;

    virtual void on_load_begin( std::string_view map, MapLoadState reason ) noexcept = 0;
    virtual void on_load_end  ( std::string_view map, bool success ) noexcept       = 0;
};

// Level-change executor: the server registers one so the FSM can delegate the
// actual bring-up (SV_SpawnServer → entity spawn → SV_ActivateServer) instead
// of loading a bare world.  When ABSENT (client background-map path, the
// map_loader's own tests) the FSM falls back to the inline load_world — so
// map_loader keeps running standalone.  Legacy: host_state.c COM_LoadLevel /
// COM_LoadGame / COM_ChangeLevel dispatch into the server.
struct ILevelChangeExecutor
{
    virtual ~ILevelChangeExecutor() = default;

    // COM_LoadLevel: full spawn → entities → activate.  Returns success.
    [[nodiscard]] virtual bool exec_load_level( std::string_view map,
                                                bool background ) noexcept = 0;

    // COM_LoadGame: savegame restore (Chunk 8 save body behind this seam).
    [[nodiscard]] virtual bool exec_load_game( std::string_view map ) noexcept = 0;

    // COM_ChangeLevel: landmark transition (Chunk 8 save staging).
    [[nodiscard]] virtual bool exec_change_level( std::string_view map,
                                                  std::string_view landmark,
                                                  bool background ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// MapLoaderInitParams
// ---------------------------------------------------------------------------

struct MapLoaderInitParams
{
    // Injected dependency (Q-4): required for world loading; a MapLoader
    // without a filesystem still runs the FSM but every load fails.
    ::xash::filesystem::Filesystem *filesystem = nullptr; // @lifetime: engine
};

// ---------------------------------------------------------------------------
// MapLoaderStats — always-on transition/load counters (CHECK-STATS).
// Added 2026-07-19 (consolidation audit): the former call-frequency
// exemption's recorded revisit trigger — "when the server chunk lands" —
// had fired. Value snapshot via MapLoader::stats(); Main-read like every
// other accessor on this class.
// ---------------------------------------------------------------------------

struct MapLoaderStats
{
    std::uint32_t transitions_queued = 0; // new_game/load_level/load_game/change_level requests
    std::uint32_t worlds_loaded      = 0; // successful load_world publications
    std::uint32_t worlds_cleared     = 0; // clear_world calls that dropped a live world
    std::uint32_t load_failures      = 0; // load_world attempts that failed
};

// ---------------------------------------------------------------------------
// MapLoader
// ---------------------------------------------------------------------------

class MapLoader
{
public:
    MapLoader() noexcept;
    ~MapLoader();

    MapLoader(const MapLoader &)            = delete;
    MapLoader &operator=(const MapLoader &) = delete;

    MapLoader(MapLoader &&) noexcept;
    MapLoader &operator=(MapLoader &&) noexcept;

    [[nodiscard]] bool init( const MapLoaderInitParams &p ) noexcept;
    void               shutdown() noexcept;

    // ---- Transitions (legacy COM_* entry points) --------------------------

    void new_game    ( std::string_view map ) noexcept;
    void load_level  ( std::string_view map, bool background ) noexcept;
    void load_game   ( std::string_view map ) noexcept;
    void change_level( std::string_view map, std::string_view landmark,
                       bool background ) noexcept;

    // ---- Per-frame step ---------------------------------------------------
    // Called by Host::RunFrame() each frame.  Drives the FSM through one
    // step (legacy COM_Frame outer-loop).
    void run_frame_step() noexcept;

    // ---- Observers --------------------------------------------------------
    // Observer lifetime is the caller's responsibility.
    void attach_observer( IMapLoaderObserver *obs ) noexcept;
    void detach_observer( IMapLoaderObserver *obs ) noexcept;

    // ---- Level-change executor (the server) -------------------------------
    // At most one; nullptr → the inline load_world fallback.  @lifetime: engine
    void set_level_executor( ILevelChangeExecutor *exec ) noexcept;

    [[nodiscard]] MapLoadState     state() const noexcept;
    [[nodiscard]] std::string_view current_map() const noexcept;
    [[nodiscard]] MapLoaderStats   stats() const noexcept;

    // ---- World ownership ----------------------------------------------
    // Loads "maps/<name>.bsp" (a name containing '/' is used as-is; the
    // .bsp extension is appended when missing) and activates it as the
    // current world.  Q-6: the returned WorldData is immutable and
    // concurrent-read-safe; the pointer stays valid until the next
    // load_world/clear_world/shutdown.
    [[nodiscard]] bool load_world( std::string_view mapname,
                                   const map_loader::WorldLoadOptions &opts ) noexcept;
    void clear_world() noexcept;
    [[nodiscard]] const map_loader::WorldData *world() const noexcept; // nullptr when none

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash
