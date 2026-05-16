// xash3dpp — MapLoader implementation (Chunk 3 scaffold)
// Legacy reference: engine/common/host_state.c
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations (no allocations yet in this stub)
//   xash3dpp_utilities  — (none yet)
//   xash3dpp_core       — core::log for transition diagnostics

#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <array>
#include <cstring>

namespace xash {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct MapLoader::Impl
{
    xash::memory::PoolHandle pool;

    MapLoadState state = MapLoadState::RunFrame;
    MapLoadState next  = MapLoadState::RunFrame;

    // Fixed buffers — see legacy MAX_QPATH (64).  Avoids heap traffic on
    // transition.  Real value lives in xash3dpp/limits.hpp when wired.
    std::array<char, 64> level_name    {};
    std::array<char, 64> landmark_name {};

    bool background = false;
    bool load_game  = false;
    bool new_game   = false;
    bool initialised = false;

    // Observers — fixed-size slot table to avoid heap during attach/detach.
    static constexpr std::size_t k_max_observers = 4;
    std::array<IMapLoaderObserver *, k_max_observers> observers{};

    void copy_name( std::array<char, 64> &dst, std::string_view src ) noexcept
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

bool MapLoader::init( const MapLoaderInitParams & ) noexcept
{
    Impl &s = *impl_;
    if (s.initialised) return true;
    s.pool = xash::memory::create_pool("map_loader");
    if (!s.pool) return false;
    s.state = MapLoadState::RunFrame;
    s.next  = MapLoadState::RunFrame;
    s.initialised = true;
    return true;
}

void MapLoader::shutdown() noexcept
{
    Impl &s = *impl_;
    if (!s.initialised) return;
    if (s.pool) {
        xash::memory::destroy_pool(s.pool);
        s.pool = {};
    }
    s.observers.fill(nullptr);
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

    // TODO Chunk 5/9 implementation prompt: dispatch to server / client.
    // For now, just transition + notify observers so client code can hook in.
    const std::string_view map{ s.level_name.data() };
    s.notify_begin( map, s.next );
    s.state = s.next;
    s.next  = MapLoadState::RunFrame;
    s.notify_end( map, /*success=*/true );
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

} // namespace xash
