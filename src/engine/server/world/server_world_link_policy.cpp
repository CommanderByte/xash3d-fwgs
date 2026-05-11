#include "engine/server/world/server_world_link_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{

int SelectWorldAreaSplitAxis(float sizeX, float sizeY)
{
	return sizeX > sizeY ? 0 : 1;
}

float BuildWorldAreaSplitDistance(float min, float max)
{
	return 0.5f * (min + max);
}

int SelectWorldAreaLinkChild(float min, float max, float splitDistance)
{
	if (min > splitDistance)
		return kWorldAreaChildPositive;

	if (max < splitDistance)
		return kWorldAreaChildNegative;

	return kWorldAreaChildNone;
}

int BuildWorldAreaTraversalMask(float min, float max, float splitDistance)
{
	int mask = 0;

	if (max > splitDistance)
		mask |= kWorldAreaChildPositiveMask;

	if (min < splitDistance)
		mask |= kWorldAreaChildNegativeMask;

	return mask;
}

}
}
}
