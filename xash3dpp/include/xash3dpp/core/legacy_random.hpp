#pragma once
// GoldSrc's process-wide COM_RandomLong / COM_RandomFloat algorithm as an
// instance-owned component.  EngineContext owns the sole production instance;
// this type deliberately owns no file-scope state.

#include <array>
#include <cstdint>

namespace xash::core {

using LegacyRandomTimeFn = std::int64_t ( * )() noexcept;

class LegacyRandom
{
public:
    explicit LegacyRandom( LegacyRandomTimeFn wall_seconds = nullptr ) noexcept;

    void set_seed( int seed ) noexcept;
    [[nodiscard]] int random_long( int low, int high ) noexcept;
    [[nodiscard]] float random_float( float low, float high ) noexcept;

private:
    [[nodiscard]] static std::int64_t default_wall_seconds() noexcept;
    [[nodiscard]] std::int32_t next_raw() noexcept;

    LegacyRandomTimeFn              wall_seconds_ = nullptr;
    std::int32_t                    idum_ = 0;
    std::int32_t                    iy_   = 0;
    std::array<std::int32_t, 32>    iv_{};
};

} // namespace xash::core
