// xash3dpp — MapLoader implementation (Chunk 3 scaffold)
// Legacy reference: engine/common/host_state.c
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (no allocations yet in this stub)
//   xash3dpp_utilities  — (none yet)
//   xash3dpp_core       — core::log for transition diagnostics

#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/core/log.hpp>
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
    Impl &s = *impl_;
    s.copy_name( s.level_name, map );
    s.new_game = true;
    s.next     = MapLoadState::LoadLevel;
    core::log( core::LogLevel::Info, "map_loader", "new_game queued" );
}

void MapLoader::load_level( std::string_view map, bool background ) noexcept
{
    Impl &s = *impl_;
    s.copy_name( s.level_name, map );
    s.background = background;
    s.next       = MapLoadState::LoadLevel;
}

void MapLoader::load_game( std::string_view map ) noexcept
{
    Impl &s = *impl_;
    s.copy_name( s.level_name, map );
    s.load_game = true;
    s.next      = MapLoadState::LoadGame;
}

void MapLoader::change_level( std::string_view map, std::string_view landmark,
                              bool background ) noexcept
{
    Impl &s = *impl_;
    s.copy_name( s.level_name,    map );
    s.copy_name( s.landmark_name, landmark );
    s.background = background;
    s.next       = MapLoadState::ChangeLevel;
}

void MapLoader::run_frame_step() noexcept
{
    Impl &s = *impl_;
    if (s.next == s.state) return;

    const std::string_view map{ s.level_name.data() };
    const MapLoadState pending = s.next;

    switch (pending)
    {
    case MapLoadState::LoadLevel:
    {
        // Synchronous world load (Chunk 5 scope): observers see the real
        // outcome and the FSM returns to frame processing in one step.
        s.notify_begin( map, pending );
        const bool ok = load_world( map, ::xash::map_loader::WorldLoadOptions{} );
        s.state = MapLoadState::RunFrame;
        s.next  = MapLoadState::RunFrame;
        s.notify_end( map, ok );
        break;
    }

    case MapLoadState::GameShutdown:
        s.notify_begin( map, pending );
        clear_world();
        s.state = pending;
        s.next  = MapLoadState::RunFrame;
        s.notify_end( map, /*success=*/true );
        break;

    default:
        // LoadGame (Chunk 8 save/restore) and ChangeLevel (Chunk 6 server
        // landmark handling) keep the scaffold transition-only behaviour.
        s.notify_begin( map, pending );
        s.state = pending;
        s.next  = MapLoadState::RunFrame;
        s.notify_end( map, /*success=*/true );
        break;
    }
}

void MapLoader::attach_observer( IMapLoaderObserver *obs ) noexcept
{
    if (!obs) return;
    Impl &s = *impl_;
    for (auto *&slot : s.observers)
        if (!slot) { slot = obs; return; }
}

void MapLoader::detach_observer( IMapLoaderObserver *obs ) noexcept
{
    Impl &s = *impl_;
    for (auto *&slot : s.observers)
        if (slot == obs) { slot = nullptr; return; }
}

MapLoadState     MapLoader::state()       const noexcept { return impl_->state; }
std::string_view MapLoader::current_map() const noexcept { return impl_->level_name.data(); }

bool MapLoader::load_world( std::string_view mapname,
                            const map_loader::WorldLoadOptions &opts ) noexcept
{
    Impl &s = *impl_;
    s.world.reset();

    if (!s.initialised || !s.filesystem) {
        core::log( core::LogLevel::Error, "map_loader",
                   "load_world: no filesystem available" );
        return false;
    }
    if (mapname.empty()) {
        core::log( core::LogLevel::Error, "map_loader", "load_world: empty map name" );
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
    if (!loaded) // load_world_data already logged the specific failure
        return false;

    s.world.emplace( std::move( *loaded ));
    return true;
}

void MapLoader::clear_world() noexcept
{
    impl_->world.reset();
}

const map_loader::WorldData *MapLoader::world() const noexcept
{
    return impl_->world.has_value() ? &*impl_->world : nullptr;
}

} // namespace xash
