#ifndef XASH_ENGINE_SERVER_SERVER_VISIBILITY_CONSTRAINTS_HPP
#define XASH_ENGINE_SERVER_SERVER_VISIBILITY_CONSTRAINTS_HPP

#include "engine/server/shared/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{

struct ServerVisibilityConstraintSnapshot
{
	int classicEntityLeafCapacity;
	int extendedEntityLeafCapacity;
	int viewEntityCapacity;
};

int ServerVisibilityEntityLeafCapacity(bool extendedLeafs);
bool ServerVisibilityCanStoreEntityLeaf(
	int currentLeafCount,
	bool extendedLeafs);
int ServerVisibilityEntityLeafOverflowMarker(bool extendedLeafs);
bool ServerVisibilityEntityLeafOverflowed(
	int currentLeafCount,
	bool extendedLeafs);
int ServerVisibilityNextCachedLeafIndex(
	int currentLeafCount,
	bool extendedLeafs);
bool ServerVisibilityCanAddViewEntity(int currentViewEntityCount);
ServerVisibilityConstraintSnapshot BuildServerVisibilityConstraintSnapshot();

}
}
}

#endif
