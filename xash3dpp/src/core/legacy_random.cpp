// GoldSrc COM_RandomLong / COM_RandomFloat, independently ported from
// engine/common/common.c:54-153.  tests/core/legacy_random_oracle.c compiles a
// byte-verified copy of that C implementation as the differential oracle.

#include <xash3dpp/core/legacy_random.hpp>

#include <bit>
#include <cstdint>
#include <ctime>
#include <limits>

namespace xash::core {

namespace {

constexpr std::int32_t k_ia   = 16807;
constexpr std::int32_t k_im   = 2147483647;
constexpr std::int32_t k_iq   = 127773;
constexpr std::int32_t k_ir   = 2836;
constexpr std::int32_t k_ntab = 32;
constexpr std::int32_t k_ndiv = 1 + ( k_im - 1 ) / k_ntab;
constexpr double       k_am   = 1.0 / k_im;
constexpr double       k_eps  = 1.2e-7;
constexpr double       k_rnmx = 1.0 - k_eps;
constexpr std::uint32_t k_max_random_range = 0x7fffffffu;

static_assert( sizeof( int ) == sizeof( std::int32_t ) );

[[nodiscard]] std::int32_t wrapping_negate( std::int32_t value ) noexcept
{
    const std::uint32_t bits = 0u - static_cast<std::uint32_t>( value );
    return std::bit_cast<std::int32_t>( bits );
}

} // namespace

LegacyRandom::LegacyRandom( LegacyRandomTimeFn wall_seconds ) noexcept
    : wall_seconds_( wall_seconds != nullptr ? wall_seconds
                                             : &default_wall_seconds )
{
}

std::int64_t LegacyRandom::default_wall_seconds() noexcept
{
    return static_cast<std::int64_t>( std::time( nullptr ));
}

void LegacyRandom::set_seed( int seed ) noexcept
{
    if ( seed != 0 )
    {
        idum_ = static_cast<std::int32_t>( seed );
    }
    else
    {
        const std::uint32_t seconds =
            static_cast<std::uint32_t>( wall_seconds_() );
        idum_ = std::bit_cast<std::int32_t>( 0u - seconds );
    }

    if ( idum_ > 1000 )
        idum_ = wrapping_negate( idum_ );
    else if ( idum_ > -1000 )
        idum_ -= 22261048;
}

std::int32_t LegacyRandom::next_raw() noexcept
{
    if ( idum_ <= 0 || iy_ == 0 )
    {
        const std::int32_t negated = wrapping_negate( idum_ );
        idum_ = negated < 1 ? 1 : negated;

        for ( int j = k_ntab + 7; j >= 0; --j )
        {
            const std::int32_t k = idum_ / k_iq;
            idum_ = k_ia * ( idum_ - k * k_iq ) - k_ir * k;
            if ( idum_ < 0 )
                idum_ += k_im;
            if ( j < k_ntab )
                iv_[static_cast<std::size_t>( j )] = idum_;
        }
        iy_ = iv_[0];
    }

    const std::int32_t k = idum_ / k_iq;
    idum_ = k_ia * ( idum_ - k * k_iq ) - k_ir * k;
    if ( idum_ < 0 )
        idum_ += k_im;
    const std::int32_t j = iy_ / k_ndiv;
    iy_ = iv_[static_cast<std::size_t>( j )];
    iv_[static_cast<std::size_t>( j )] = idum_;
    return iy_;
}

float LegacyRandom::random_float( float low, float high ) noexcept
{
    if ( idum_ == 0 )
        set_seed( 0 );

    float value = static_cast<float>( k_am ) *
                  static_cast<float>( next_raw() );
    if ( value > static_cast<float>( k_rnmx ))
        value = static_cast<float>( k_rnmx );
    return ( value * ( high - low )) + low;
}

int LegacyRandom::random_long( int low, int high ) noexcept
{
    if ( idum_ == 0 )
        set_seed( 0 );

    const std::uint32_t x = static_cast<std::uint32_t>( high ) -
                            static_cast<std::uint32_t>( low ) + 1u;
    if ( x == 0u || k_max_random_range < x - 1u )
        return low;

    const std::uint32_t max_acceptable =
        k_max_random_range - static_cast<std::uint32_t>(
            ( static_cast<std::uint64_t>( k_max_random_range ) + 1u ) % x );

    std::uint32_t n;
    do
    {
        n = static_cast<std::uint32_t>( next_raw() );
    } while ( n > max_acceptable );

    const std::uint32_t result =
        static_cast<std::uint32_t>( low ) + ( n % x );
    return std::bit_cast<std::int32_t>( result );
}

} // namespace xash::core
