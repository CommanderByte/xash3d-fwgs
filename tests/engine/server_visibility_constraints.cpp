#include <cstdlib>

#include "engine/server/server_visibility_constraints.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy
{

constexpr int kExtendedLeafCapacity = 24;
constexpr int kClassicLeafCapacity = 48;
constexpr int kViewEntityCapacity = 128;

}

static bool TestLeafCapacityMirrors()
{
	return ServerVisibilityEntityLeafCapacity(true) ==
			legacy::kExtendedLeafCapacity &&
		ServerVisibilityEntityLeafCapacity(false) ==
			legacy::kClassicLeafCapacity &&
		BuildServerVisibilityConstraintSnapshot().extendedEntityLeafCapacity ==
			legacy::kExtendedLeafCapacity &&
		BuildServerVisibilityConstraintSnapshot().classicEntityLeafCapacity ==
			legacy::kClassicLeafCapacity;
}

static bool TestCanStoreLeaf()
{
	return !ServerVisibilityCanStoreEntityLeaf(-1, true) &&
		ServerVisibilityCanStoreEntityLeaf(0, true) &&
		ServerVisibilityCanStoreEntityLeaf(
			legacy::kExtendedLeafCapacity - 1,
			true) &&
		!ServerVisibilityCanStoreEntityLeaf(
			legacy::kExtendedLeafCapacity,
			true) &&
		ServerVisibilityCanStoreEntityLeaf(
			legacy::kClassicLeafCapacity - 1,
			false) &&
		!ServerVisibilityCanStoreEntityLeaf(
			legacy::kClassicLeafCapacity,
			false);
}

static bool TestOverflowMarkerAndCheck()
{
	return ServerVisibilityEntityLeafOverflowMarker(true) ==
			legacy::kExtendedLeafCapacity + 1 &&
		ServerVisibilityEntityLeafOverflowMarker(false) ==
			legacy::kClassicLeafCapacity + 1 &&
		!ServerVisibilityEntityLeafOverflowed(
			legacy::kExtendedLeafCapacity,
			true) &&
		ServerVisibilityEntityLeafOverflowed(
			legacy::kExtendedLeafCapacity + 1,
			true) &&
		!ServerVisibilityEntityLeafOverflowed(
			legacy::kClassicLeafCapacity,
			false) &&
		ServerVisibilityEntityLeafOverflowed(
			legacy::kClassicLeafCapacity + 1,
			false);
}

static bool TestCachedLeafIndex()
{
	return ServerVisibilityNextCachedLeafIndex(-1, true) == 0 &&
		ServerVisibilityNextCachedLeafIndex(0, true) == 1 &&
		ServerVisibilityNextCachedLeafIndex(
			legacy::kExtendedLeafCapacity - 1,
			true) == 0 &&
		ServerVisibilityNextCachedLeafIndex(
			legacy::kClassicLeafCapacity - 1,
			false) == 0;
}

static bool TestViewEntityCapacity()
{
	const ServerVisibilityConstraintSnapshot snapshot =
		BuildServerVisibilityConstraintSnapshot();

	return snapshot.viewEntityCapacity == legacy::kViewEntityCapacity &&
		!ServerVisibilityCanAddViewEntity(-1) &&
		ServerVisibilityCanAddViewEntity(0) &&
		ServerVisibilityCanAddViewEntity(legacy::kViewEntityCapacity - 1) &&
		!ServerVisibilityCanAddViewEntity(legacy::kViewEntityCapacity);
}

}

int main()
{
	if (!TestLeafCapacityMirrors() ||
		!TestCanStoreLeaf() ||
		!TestOverflowMarkerAndCheck() ||
		!TestCachedLeafIndex() ||
		!TestViewEntityCapacity())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
