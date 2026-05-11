#ifndef XASH_ENGINE_SERVER_WORLD_LINK_POLICY_HPP
#define XASH_ENGINE_SERVER_WORLD_LINK_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kWorldAreaChildNone = -1;
constexpr int kWorldAreaChildPositive = 0;
constexpr int kWorldAreaChildNegative = 1;
constexpr int kWorldAreaChildPositiveMask = 1 << 0;
constexpr int kWorldAreaChildNegativeMask = 1 << 1;

int SelectWorldAreaSplitAxis(float sizeX, float sizeY);
float BuildWorldAreaSplitDistance(float min, float max);
int SelectWorldAreaLinkChild(float min, float max, float splitDistance);
int BuildWorldAreaTraversalMask(float min, float max, float splitDistance);

}
}
}

#endif
