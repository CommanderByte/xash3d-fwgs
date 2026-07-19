// xash3dpp — MapLoader implementation (Chunk 3 scaffold)
// Legacy reference: engine/common/host_state.c
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (no allocations yet in this stub)
//   xash3dpp_utilities  — (none yet)
//   xash3dpp_core       — core::log for transition diagnostics

#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <array>
#include <cstring>
#include <optional>
#include <string>

namespace xash {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct MapLoader::Impl
{
    xash::memory::PoolHandle pool;

    ::xash::filesystem::Filesystem *filesystem = nullptr; // @lifetime: engine

    // Active world — immutable after activation (Q-6); readers borrow via
    // MapLoader::world().
    std::optional<::xash::map_loader::WorldData> world;

    MapLoadState state = MapLoadState::RunFrame;
    MapLoadState next  = MapLoadState::RunFrame;

    // Fixed buffers — legacy MAX_QPATH.  Avoids heap traffic on transition.
    std::array<char, ::xash::limits::map_qpath_max> level_name    {};
    std::array<char, ::xash::limits::map_qpath_max> landmark_name {};

    bool background = false;
    bool load_game  = false;
    bool new_game   = false;
    bool initialised = false;

    // Observers — fixed-size slot table to avoid heap during attach/detach.
    static constexpr std::size_t k_max_observers = 4;
    std::array<IMapLoaderObserver *, k_max_observers> observers{};

    // Server-registered level-change executor (nullptr → inline load_world).
    ILevelChangeExecutor *executor = nullptr;

    // Always-on counters (CHECK-STATS); value-snapshot via stats().
    MapLoaderStats stats;

    void copy_name( std::array<char, ::xash::limits::map_qpath_max> &dst,
                    std::string_view src ) noexcept
    {
        const std::size_t n = src.size() < dst.size() - 1 ? src.size() : dst.size() - 1;
        std::memcpy( dst.data(), src.data(), n );
        dst[n] = '\0';
    }

    void notify_begin( std::string_view map, MapLoadState reason ) noexcept
    {
        for (auto *o : observers)
            if (o) o->on_load_begin( map, reason );
    }

    void notify_end( std::string_view map, bool success ) noexcept
    {
        for (auto *o : observers)
            if (o) o->on_load_end( map, success );
    }
};

MapLoader::MapLoader() noexcept : impl_{ std::make_unique<Impl>() } {}
MapLoader::~MapLoader() = default;

MapLoader::MapLoader(MapLoader &&) noexcept            = default;
MapLoader &MapLoader::operator=(MapLoader &&) noexcept = default;

bool MapLoader::init( const MapLoaderInitParams &p ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    if (s.initialised) return true;
    s.pool = xash::memory::create_pool("map_loader");
    if (!s.pool) return false;
    s.filesystem = p.filesystem;
    s.state = MapLoadState::RunFrame;
    s.next  = MapLoadState::RunFrame;
    s.initialised = true;
    return true;
}

void MapLoader::shutdown() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    if (!s.initialised) return;
    s.world.reset();
    if (s.pool) {
        xash::memory::destroy_pool(s.pool);
        s.pool = {};
    }
    s.observers.fill(nullptr);
    s.filesystem = nullptr;
    s.initialised = false;
}

void MapLoader::new_game( std::string_view map ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    s.copy_name( s.level_name, map );
    s.new_game = true;
    s.next     = MapLoadState::LoadLevel;
    ++s.stats.transitions_queued;
    core::log( core::LogLevel::Info, "map_loader", "new_game queued" );
}

void MapLoader::load_level( std::string_view map, bool background ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    s.copy_name( s.level_name, map );
    s.background = background;
    s.next       = MapLoadState::LoadLevel;
    ++s.stats.transitions_queued;
}

void MapLoader::load_game( std::string_view map ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    s.copy_name( s.level_name, map );
    s.load_game = true;
    s.next      = MapLoadState::LoadGame;
    ++s.stats.transitions_queued;
}

void MapLoader::change_level( std::string_view map, std::string_view landmark,
                              bool background ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    s.copy_name( s.level_name,    map );
    s.copy_name( s.landmark_name, landmark );
    s.background = background;
    s.next       = MapLoadState::ChangeLevel;
    ++s.stats.transitions_queued;
}

void MapLoader::run_frame_step() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    if (s.next == s.state) return;

    const std::string_view map{ s.level_name.data() };
    const MapLoadState pending = s.next;

    switch (pending)
    {
    case MapLoadState::LoadLevel:
    {
        // With a server executor: full SV_SpawnServer → entity spawn →
        // SV_ActivateServer.  Without one (client background map, map_loader's
        // own tests): the inline Chunk-5 world load.  Either way the outcome
        // reaches observers and the FSM returns to frame processing in a step.
        s.notify_begin( map, pending );
        const bool ok = s.executor
            ? s.executor->exec_load_level( map, s.background )
            // compliance-allow(thread-assert): call site inside run_frame_step (asserted Main above), not a mutator definition — the scanner's def matcher hit the ':' ternary branch.
            : load_world( map, ::xash::map_loader::WorldLoadOptions{} );
        s.state = MapLoadState::RunFrame;
        s.next  = MapLoadState::RunFrame;
        s.notify_end( map, ok );
        break;
    }

    case MapLoadState::LoadGame:
        // With a server: savegame restore (Chunk 8 body behind the seam),
        // then back to frame processing.  Without one: the scaffold parks at
        // the transition state (no world to load standalone).
        s.notify_begin( map, pending );
        if ( s.executor )
        {
            const bool ok = s.executor->exec_load_game( map );
            s.state = MapLoadState::RunFrame;
            s.next  = MapLoadState::RunFrame;
            s.notify_end( map, ok );
        }
        else
        {
            s.state = pending;
            s.next  = MapLoadState::RunFrame;
            s.notify_end( map, /*success=*/true );
        }
        break;

    case MapLoadState::ChangeLevel:
        // Landmark transition through the executor (Chunk 8 save staging);
        // without a server the scaffold parks at the transition state.
        s.notify_begin( map, pending );
        if ( s.executor )
        {
            const bool ok = s.executor->exec_change_level(
                map, std::string_view{ s.landmark_name.data() }, s.background );
            s.state = MapLoadState::RunFrame;
            s.next  = MapLoadState::RunFrame;
            s.notify_end( map, ok );
        }
        else
        {
            s.state = pending;
            s.next  = MapLoadState::RunFrame;
            s.notify_end( map, /*success=*/true );
        }
        break;

    case MapLoadState::GameShutdown:
        s.notify_begin( map, pending );
        clear_world();
        s.state = pending;
        s.next  = MapLoadState::RunFrame;
        s.notify_end( map, /*success=*/true );
        break;

    default:
        s.notify_begin( map, pending );
        s.state = pending;
        s.next  = MapLoadState::RunFrame;
        s.notify_end( map, /*success=*/true );
        break;
    }
}

void MapLoader::set_level_executor( ILevelChangeExecutor *exec ) noexcept
{
    // Main-thread mutator (OQ-9/TH-Role): the executor pointer is read by
    // run_frame_step on the main thread, so its write must be main-thread too.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    impl_->executor = exec;
}

void MapLoader::attach_observer( IMapLoaderObserver *obs ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    if (!obs) return;
    Impl &s = *impl_;
    for (auto *&slot : s.observers)
        if (!slot) { slot = obs; return; }
}

void MapLoader::detach_observer( IMapLoaderObserver *obs ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    for (auto *&slot : s.observers)
        if (slot == obs) { slot = nullptr; return; }
}

MapLoadState     MapLoader::state()       const noexcept { return impl_->state; }
std::string_view MapLoader::current_map() const noexcept { return impl_->level_name.data(); }
MapLoaderStats   MapLoader::stats()       const noexcept { return impl_->stats; }

bool MapLoader::load_world( std::string_view mapname,
                            const map_loader::WorldLoadOptions &opts ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    Impl &s = *impl_;
    s.world.reset();

    if (!s.initialised || !s.filesystem) {
        core::log( core::LogLevel::Error, "map_loader",
                   "load_world: no filesystem available" );
        ++s.stats.load_failures;
        return false;
    }
    if (mapname.empty()) {
        core::log( core::LogLevel::Error, "map_loader", "load_world: empty map name" );
        ++s.stats.load_failures;
        return false;
    }

    // "maps/<name>.bsp" unless the caller already provided a path/extension.
    std::string path;
    if (mapname.find('/') == std::string_view::npos)
        path = "maps/";
    path += mapname;
    if (path.size() < 4 || path.compare(path.size() - 4, 4, ".bsp") != 0)
        path += ".bsp";

    auto loaded = map_loader::load_world_data( *s.filesystem, path, opts );
    if (!loaded) { // load_world_data already logged the specific failure
        ++s.stats.load_failures;
        return false;
    }

    s.world.emplace( std::move( *loaded ));
    ++s.stats.worlds_loaded;
    return true;
}

void MapLoader::clear_world() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    if (impl_->world)
        ++impl_->stats.worlds_cleared;
    impl_->world.reset();
}

const map_loader::WorldData *MapLoader::world() const noexcept
{
    return impl_->world.has_value() ? &*impl_->world : nullptr;
}

} // namespace xash
