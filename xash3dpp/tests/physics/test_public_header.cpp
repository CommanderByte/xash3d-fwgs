// Compile/link gate: the public physics header is self-contained. Deliberately
// include no world header; IModelResolver remains an incomplete pointer type.
#include <xash3dpp/physics/pm_trace.hpp>

#include <type_traits>

static_assert( std::is_aggregate_v<::xash::physics::PmTraceEnv> );
static_assert( std::is_aggregate_v<::xash::physics::PmPhysentView> );

int main()
{
    ::xash::physics::PmTraceEnv env {};
    return env.models == nullptr ? 0 : 1;
}
