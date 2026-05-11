#include <cstdlib>

#include "engine/server/server_group_filter.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy
{

constexpr int kGroupOpAnd = 0;
constexpr int kGroupOpNand = 1;

}

static bool TestOperationMapping()
{
	return ToServerGroupOperation(legacy::kGroupOpAnd) ==
			ServerGroupOperation::And &&
		ToServerGroupOperation(legacy::kGroupOpNand) ==
			ServerGroupOperation::Nand &&
		ToServerGroupOperation(42) == ServerGroupOperation::Unknown;
}

static bool TestRawOperationSemantics()
{
	return HasSharedGroupBits(0x01u, 0x03u) &&
		!HasSharedGroupBits(0x04u, 0x03u) &&
		ServerGroupOperationAllows(legacy::kGroupOpAnd, 0x01u, 0x03u) &&
		!ServerGroupOperationAllows(legacy::kGroupOpAnd, 0x04u, 0x03u) &&
		!ServerGroupOperationAllows(legacy::kGroupOpAnd, 0x04u, 0u) &&
		!ServerGroupOperationAllows(legacy::kGroupOpNand, 0x01u, 0x03u) &&
		ServerGroupOperationAllows(legacy::kGroupOpNand, 0x04u, 0x03u) &&
		ServerGroupOperationAllows(legacy::kGroupOpNand, 0x04u, 0u) &&
		ServerGroupOperationAllows(42, 0x01u, 0x01u) &&
		ServerGroupOperationAllows(42, 0x01u, 0u);
}

static bool TestEntityPairFilter()
{
	const ServerGroupFilterDecision inactiveLeft =
		BuildEntityPairGroupFilterDecision(legacy::kGroupOpAnd, 0u, 0x01u);
	const ServerGroupFilterDecision inactiveRight =
		BuildEntityPairGroupFilterDecision(legacy::kGroupOpAnd, 0x01u, 0u);
	const ServerGroupFilterDecision andMatch =
		BuildEntityPairGroupFilterDecision(legacy::kGroupOpAnd, 0x01u, 0x03u);
	const ServerGroupFilterDecision andMiss =
		BuildEntityPairGroupFilterDecision(legacy::kGroupOpAnd, 0x04u, 0x03u);
	const ServerGroupFilterDecision nandMatch =
		BuildEntityPairGroupFilterDecision(legacy::kGroupOpNand, 0x01u, 0x03u);
	const ServerGroupFilterDecision nandMiss =
		BuildEntityPairGroupFilterDecision(legacy::kGroupOpNand, 0x04u, 0x03u);

	return !inactiveLeft.active &&
		inactiveLeft.passes &&
		!inactiveRight.active &&
		inactiveRight.passes &&
		andMatch.active &&
		andMatch.sharedBits &&
		andMatch.passes &&
		andMiss.active &&
		!andMiss.sharedBits &&
		!andMiss.passes &&
		nandMatch.active &&
		nandMatch.sharedBits &&
		!nandMatch.passes &&
		nandMiss.active &&
		!nandMiss.sharedBits &&
		nandMiss.passes &&
		EntityPairPassesGroupFilter(42, 0x01u, 0x01u);
}

static bool TestMaskedFilter()
{
	const ServerGroupFilterDecision inactiveEntity =
		BuildMaskedGroupFilterDecision(legacy::kGroupOpAnd, 0u, 0u);
	const ServerGroupFilterDecision andZeroMask =
		BuildMaskedGroupFilterDecision(legacy::kGroupOpAnd, 0x01u, 0u);
	const ServerGroupFilterDecision nandZeroMask =
		BuildMaskedGroupFilterDecision(legacy::kGroupOpNand, 0x01u, 0u);
	const ServerGroupFilterDecision andMatch =
		BuildMaskedGroupFilterDecision(legacy::kGroupOpAnd, 0x02u, 0x03u);
	const ServerGroupFilterDecision nandMatch =
		BuildMaskedGroupFilterDecision(legacy::kGroupOpNand, 0x02u, 0x03u);

	return !inactiveEntity.active &&
		inactiveEntity.passes &&
		andZeroMask.active &&
		!andZeroMask.sharedBits &&
		!andZeroMask.passes &&
		nandZeroMask.active &&
		!nandZeroMask.sharedBits &&
		nandZeroMask.passes &&
		andMatch.active &&
		andMatch.sharedBits &&
		andMatch.passes &&
		nandMatch.active &&
		nandMatch.sharedBits &&
		!nandMatch.passes &&
		EntityPassesGroupMask(42, 0x01u, 0u);
}

}

int main()
{
	if (!TestOperationMapping() ||
		!TestRawOperationSemantics() ||
		!TestEntityPairFilter() ||
		!TestMaskedFilter())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
