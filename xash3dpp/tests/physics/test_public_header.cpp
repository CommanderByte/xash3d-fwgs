// Compile/link gate: the public physics header is self-contained. Deliberately
// include no world header; IModelResolver remains an incomplete pointer type.
#include <xash3dpp/physics/pm_trace.hpp>

#include "../test_helpers.hpp"

#include <cstdio>
#include <type_traits>

static int g_pass = 0, g_fail = 0;

static_assert( std::is_aggregate_v<::xash::physics::PmTraceEnv> );
static_assert( std::is_aggregate_v<::xash::physics::PmPhysentView> );

int main()
{
    ::xash::physics::PmTraceEnv env {};
    CHECK( env.models == nullptr );
    CHECK( env.model_indices == nullptr );
    std::printf( "physics-public-header: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
