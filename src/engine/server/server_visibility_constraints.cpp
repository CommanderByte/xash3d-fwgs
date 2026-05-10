#include "engine/server/server_visibility_constraints.hpp"

namespace xash
{
namespace engine
{
namespace server
{

int ServerVisibilityEntityLeafCapacity(bool extendedLeafs)
{
	return ServerEntityLeafCapacity(extendedLeafs);
}

bool ServerVisibilityCanStoreEntityLeaf(
	int currentLeafCount,
	bool extendedLeafs)
{
	return currentLeafCount >= 0 &&
		currentLeafCount < ServerVisibilityEntityLeafCapacity(extendedLeafs);
}

int ServerVisibilityEntityLeafOverflowMarker(bool extendedLeafs)
{
	return ServerVisibilityEntityLeafCapacity(extendedLeafs) + 1;
}

bool ServerVisibilityEntityLeafOverflowed(
	int currentLeafCount,
	bool extendedLeafs)
{
	return currentLeafCount >
		ServerVisibilityEntityLeafCapacity(extendedLeafs);
}

int ServerVisibilityNextCachedLeafIndex(
	int currentLeafCount,
	bool extendedLeafs)
{
	const int capacity = ServerVisibilityEntityLeafCapacity(extendedLeafs);

	if (capacity <= 0)
		return 0;

	if (currentLeafCount < 0)
		return 0;

	return (currentLeafCount + 1) % capacity;
}

bool ServerVisibilityCanAddViewEntity(int currentViewEntityCount)
{
	return currentViewEntityCount >= 0 &&
		currentViewEntityCount < kServerMaxViewEntities;
}

ServerVisibilityConstraintSnapshot BuildServerVisibilityConstraintSnapshot()
{
	ServerVisibilityConstraintSnapshot snapshot = {};
	snapshot.classicEntityLeafCapacity = kServerMaxEntLeafs16;
	snapshot.extendedEntityLeafCapacity = kServerMaxEntLeafs32;
	snapshot.viewEntityCapacity = kServerMaxViewEntities;
	return snapshot;
}

}
}
}
