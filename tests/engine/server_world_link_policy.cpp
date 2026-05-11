#include <cstdlib>

#include "engine/server/server_world_link_policy.hpp"

using namespace xash::engine::server;

namespace
{

bool TestSplitAxisChoosesWiderDimension()
{
	return SelectWorldAreaSplitAxis(128.0f, 64.0f) == 0 &&
		SelectWorldAreaSplitAxis(64.0f, 128.0f) == 1 &&
		SelectWorldAreaSplitAxis(64.0f, 64.0f) == 1;
}

bool TestSplitDistanceUsesMidpoint()
{
	return BuildWorldAreaSplitDistance(-32.0f, 96.0f) == 32.0f &&
		BuildWorldAreaSplitDistance(8.0f, 10.0f) == 9.0f;
}

bool TestLinkChildSelectsStrictPositiveAndNegativeSides()
{
	return SelectWorldAreaLinkChild(10.1f, 12.0f, 10.0f) ==
			kWorldAreaChildPositive &&
		SelectWorldAreaLinkChild(2.0f, 9.9f, 10.0f) ==
			kWorldAreaChildNegative;
}

bool TestLinkChildStopsWhenBoundsCrossOrTouchSplit()
{
	return SelectWorldAreaLinkChild(9.0f, 11.0f, 10.0f) ==
			kWorldAreaChildNone &&
		SelectWorldAreaLinkChild(10.0f, 12.0f, 10.0f) ==
			kWorldAreaChildNone &&
		SelectWorldAreaLinkChild(8.0f, 10.0f, 10.0f) ==
			kWorldAreaChildNone &&
		SelectWorldAreaLinkChild(10.0f, 10.0f, 10.0f) ==
			kWorldAreaChildNone;
}

bool TestTraversalMaskUsesStrictPositiveAndNegativeTests()
{
	return BuildWorldAreaTraversalMask(11.0f, 12.0f, 10.0f) ==
			kWorldAreaChildPositiveMask &&
		BuildWorldAreaTraversalMask(8.0f, 9.0f, 10.0f) ==
			kWorldAreaChildNegativeMask &&
		BuildWorldAreaTraversalMask(8.0f, 12.0f, 10.0f) ==
			(kWorldAreaChildPositiveMask | kWorldAreaChildNegativeMask);
}

bool TestTraversalMaskPreservesSplitPlaneEdgeBehavior()
{
	return BuildWorldAreaTraversalMask(10.0f, 12.0f, 10.0f) ==
			kWorldAreaChildPositiveMask &&
		BuildWorldAreaTraversalMask(8.0f, 10.0f, 10.0f) ==
			kWorldAreaChildNegativeMask &&
		BuildWorldAreaTraversalMask(10.0f, 10.0f, 10.0f) == 0;
}

}

int main()
{
	if (!TestSplitAxisChoosesWiderDimension() ||
		!TestSplitDistanceUsesMidpoint() ||
		!TestLinkChildSelectsStrictPositiveAndNegativeSides() ||
		!TestLinkChildStopsWhenBoundsCrossOrTouchSplit() ||
		!TestTraversalMaskUsesStrictPositiveAndNegativeTests() ||
		!TestTraversalMaskPreservesSplitPlaneEdgeBehavior())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
