#pragma once
// xash3dpp — MapLoader: server-driven, client/demo-observed map-load FSM
// Legacy reference: engine/common/host_state.c
//   (host_state_t, game_status_t, COM_InitHostState, COM_NewGame,
//    COM_LoadLevel, COM_LoadGame, COM_ChangeLevel, COM_Frame outer-loop)
//
// Decision ref: docs/boundaries/host-boundary.md  Resolved-decision OQ-2
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

    [[nodiscard]] MapLoadState     state() const noexcept;
    [[nodiscard]] std::string_view current_map() const noexcept;

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
