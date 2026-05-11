#include <cstdlib>

#include "engine/server/shared/server_group_filter.hpp"
#include "engine/server/shared/server_map_validation.hpp"
#include "engine/server/shared/server_visibility_constraints.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy_group
{

constexpr int kGroupOpAnd = 0;
constexpr int kGroupOpNand = 1;

}

namespace legacy_map
{

constexpr unsigned int kMapExists = 1u << 0;
constexpr unsigned int kMapHasLandmark = 1u << 2;
constexpr unsigned int kMapInvalidVersion = 1u << 3;

}

namespace legacy_visibility
{

constexpr int kExtendedLeafCapacity = 24;
constexpr int kClassicLeafCapacity = 48;
constexpr int kViewEntityCapacity = 128;

}

static bool TestGroupOperationMapping()
{
	return ToServerGroupOperation(legacy_group::kGroupOpAnd) ==
			ServerGroupOperation::And &&
		ToServerGroupOperation(legacy_group::kGroupOpNand) ==
			ServerGroupOperation::Nand &&
		ToServerGroupOperation(42) == ServerGroupOperation::Unknown;
}

static bool TestGroupRawOperationSemantics()
{
	return HasSharedGroupBits(0x01u, 0x03u) &&
		!HasSharedGroupBits(0x04u, 0x03u) &&
		ServerGroupOperationAllows(
			legacy_group::kGroupOpAnd,
			0x01u,
			0x03u) &&
		!ServerGroupOperationAllows(
			legacy_group::kGroupOpAnd,
			0x04u,
			0x03u) &&
		!ServerGroupOperationAllows(
			legacy_group::kGroupOpAnd,
			0x04u,
			0u) &&
		!ServerGroupOperationAllows(
			legacy_group::kGroupOpNand,
			0x01u,
			0x03u) &&
		ServerGroupOperationAllows(
			legacy_group::kGroupOpNand,
			0x04u,
			0x03u) &&
		ServerGroupOperationAllows(
			legacy_group::kGroupOpNand,
			0x04u,
			0u) &&
		ServerGroupOperationAllows(42, 0x01u, 0x01u) &&
		ServerGroupOperationAllows(42, 0x01u, 0u);
}

static bool TestEntityPairFilter()
{
	const ServerGroupFilterDecision inactiveLeft =
		BuildEntityPairGroupFilterDecision(
			legacy_group::kGroupOpAnd,
			0u,
			0x01u);
	const ServerGroupFilterDecision inactiveRight =
		BuildEntityPairGroupFilterDecision(
			legacy_group::kGroupOpAnd,
			0x01u,
			0u);
	const ServerGroupFilterDecision andMatch =
		BuildEntityPairGroupFilterDecision(
			legacy_group::kGroupOpAnd,
			0x01u,
			0x03u);
	const ServerGroupFilterDecision andMiss =
		BuildEntityPairGroupFilterDecision(
			legacy_group::kGroupOpAnd,
			0x04u,
			0x03u);
	const ServerGroupFilterDecision nandMatch =
		BuildEntityPairGroupFilterDecision(
			legacy_group::kGroupOpNand,
			0x01u,
			0x03u);
	const ServerGroupFilterDecision nandMiss =
		BuildEntityPairGroupFilterDecision(
			legacy_group::kGroupOpNand,
			0x04u,
			0x03u);

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
		BuildMaskedGroupFilterDecision(legacy_group::kGroupOpAnd, 0u, 0u);
	const ServerGroupFilterDecision andZeroMask =
		BuildMaskedGroupFilterDecision(legacy_group::kGroupOpAnd, 0x01u, 0u);
	const ServerGroupFilterDecision nandZeroMask =
		BuildMaskedGroupFilterDecision(legacy_group::kGroupOpNand, 0x01u, 0u);
	const ServerGroupFilterDecision andMatch =
		BuildMaskedGroupFilterDecision(legacy_group::kGroupOpAnd, 0x02u, 0x03u);
	const ServerGroupFilterDecision nandMatch =
		BuildMaskedGroupFilterDecision(
			legacy_group::kGroupOpNand,
			0x02u,
			0x03u);

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

static bool TestMapFlagDecoding()
{
	const ServerMapValidationFlags empty = DecodeServerMapValidationFlags(0u);
	const ServerMapValidationFlags all = DecodeServerMapValidationFlags(
		legacy_map::kMapExists |
		legacy_map::kMapHasLandmark |
		legacy_map::kMapInvalidVersion);

	return !empty.exists &&
		!empty.hasLandmark &&
		!empty.invalidVersion &&
		all.exists &&
		all.hasLandmark &&
		all.invalidVersion &&
		ServerMapExists(legacy_map::kMapExists) &&
		ServerMapHasLandmark(legacy_map::kMapHasLandmark) &&
		ServerMapHasInvalidVersion(legacy_map::kMapInvalidVersion);
}

static bool TestMapLoadClassification()
{
	return ClassifyServerMapValidation(legacy_map::kMapExists) ==
			ServerMapValidationResult::Valid &&
		ClassifyServerMapValidation(0u) ==
			ServerMapValidationResult::Missing &&
		ClassifyServerMapValidation(legacy_map::kMapInvalidVersion) ==
			ServerMapValidationResult::InvalidVersion &&
		ClassifyServerMapValidation(
			legacy_map::kMapExists | legacy_map::kMapInvalidVersion) ==
			ServerMapValidationResult::InvalidVersion &&
		ServerMapCanLoad(legacy_map::kMapExists) &&
		!ServerMapCanLoad(0u) &&
		!ServerMapCanLoad(
			legacy_map::kMapExists | legacy_map::kMapInvalidVersion);
}

static bool TestGameDllMapExistsCompatibility()
{
	return ServerMapExistsForGameDll(legacy_map::kMapExists) &&
		ServerMapExistsForGameDll(
			legacy_map::kMapExists | legacy_map::kMapInvalidVersion) &&
		!ServerMapExistsForGameDll(0u);
}

static bool TestChangeLevelInvalidAndMissing()
{
	const ServerChangeLevelMapValidationDecision invalid =
		BuildServerChangeLevelMapValidationDecision(
			legacy_map::kMapExists | legacy_map::kMapInvalidVersion,
			true,
			true);
	const ServerChangeLevelMapValidationDecision missing =
		BuildServerChangeLevelMapValidationDecision(0u, true, true);

	return invalid.result == ServerMapValidationResult::InvalidVersion &&
		!invalid.canContinue &&
		!invalid.missingLandmarkForSmooth &&
		!invalid.disableSmooth &&
		missing.result == ServerMapValidationResult::Missing &&
		!missing.canContinue &&
		!missing.missingLandmarkForSmooth &&
		!missing.disableSmooth;
}

static bool TestChangeLevelLandmarkPolicy()
{
	const ServerChangeLevelMapValidationDecision classic =
		BuildServerChangeLevelMapValidationDecision(
			legacy_map::kMapExists,
			false,
			true);
	const ServerChangeLevelMapValidationDecision smoothWithoutValidation =
		BuildServerChangeLevelMapValidationDecision(
			legacy_map::kMapExists,
			true,
			false);
	const ServerChangeLevelMapValidationDecision smoothWithValidation =
		BuildServerChangeLevelMapValidationDecision(
			legacy_map::kMapExists,
			true,
			true);
	const ServerChangeLevelMapValidationDecision smoothWithLandmark =
		BuildServerChangeLevelMapValidationDecision(
			legacy_map::kMapExists | legacy_map::kMapHasLandmark,
			true,
			true);

	return classic.canContinue &&
		!classic.missingLandmarkForSmooth &&
		!classic.disableSmooth &&
		smoothWithoutValidation.canContinue &&
		smoothWithoutValidation.missingLandmarkForSmooth &&
		!smoothWithoutValidation.disableSmooth &&
		smoothWithValidation.canContinue &&
		smoothWithValidation.missingLandmarkForSmooth &&
		smoothWithValidation.disableSmooth &&
		smoothWithLandmark.canContinue &&
		!smoothWithLandmark.missingLandmarkForSmooth &&
		!smoothWithLandmark.disableSmooth;
}

static bool TestLeafCapacityMirrors()
{
	return ServerVisibilityEntityLeafCapacity(true) ==
			legacy_visibility::kExtendedLeafCapacity &&
		ServerVisibilityEntityLeafCapacity(false) ==
			legacy_visibility::kClassicLeafCapacity &&
		BuildServerVisibilityConstraintSnapshot().extendedEntityLeafCapacity ==
			legacy_visibility::kExtendedLeafCapacity &&
		BuildServerVisibilityConstraintSnapshot().classicEntityLeafCapacity ==
			legacy_visibility::kClassicLeafCapacity;
}

static bool TestCanStoreLeaf()
{
	return !ServerVisibilityCanStoreEntityLeaf(-1, true) &&
		ServerVisibilityCanStoreEntityLeaf(0, true) &&
		ServerVisibilityCanStoreEntityLeaf(
			legacy_visibility::kExtendedLeafCapacity - 1,
			true) &&
		!ServerVisibilityCanStoreEntityLeaf(
			legacy_visibility::kExtendedLeafCapacity,
			true) &&
		ServerVisibilityCanStoreEntityLeaf(
			legacy_visibility::kClassicLeafCapacity - 1,
			false) &&
		!ServerVisibilityCanStoreEntityLeaf(
			legacy_visibility::kClassicLeafCapacity,
			false);
}

static bool TestOverflowMarkerAndCheck()
{
	return ServerVisibilityEntityLeafOverflowMarker(true) ==
			legacy_visibility::kExtendedLeafCapacity + 1 &&
		ServerVisibilityEntityLeafOverflowMarker(false) ==
			legacy_visibility::kClassicLeafCapacity + 1 &&
		!ServerVisibilityEntityLeafOverflowed(
			legacy_visibility::kExtendedLeafCapacity,
			true) &&
		ServerVisibilityEntityLeafOverflowed(
			legacy_visibility::kExtendedLeafCapacity + 1,
			true) &&
		!ServerVisibilityEntityLeafOverflowed(
			legacy_visibility::kClassicLeafCapacity,
			false) &&
		ServerVisibilityEntityLeafOverflowed(
			legacy_visibility::kClassicLeafCapacity + 1,
			false);
}

static bool TestCachedLeafIndex()
{
	return ServerVisibilityNextCachedLeafIndex(-1, true) == 0 &&
		ServerVisibilityNextCachedLeafIndex(0, true) == 1 &&
		ServerVisibilityNextCachedLeafIndex(
			legacy_visibility::kExtendedLeafCapacity - 1,
			true) == 0 &&
		ServerVisibilityNextCachedLeafIndex(
			legacy_visibility::kClassicLeafCapacity - 1,
			false) == 0;
}

static bool TestViewEntityCapacity()
{
	const ServerVisibilityConstraintSnapshot snapshot =
		BuildServerVisibilityConstraintSnapshot();

	return snapshot.viewEntityCapacity ==
			legacy_visibility::kViewEntityCapacity &&
		!ServerVisibilityCanAddViewEntity(-1) &&
		ServerVisibilityCanAddViewEntity(0) &&
		ServerVisibilityCanAddViewEntity(
			legacy_visibility::kViewEntityCapacity - 1) &&
		!ServerVisibilityCanAddViewEntity(
			legacy_visibility::kViewEntityCapacity);
}

}

int main()
{
	if (!TestGroupOperationMapping() ||
		!TestGroupRawOperationSemantics() ||
		!TestEntityPairFilter() ||
		!TestMaskedFilter() ||
		!TestMapFlagDecoding() ||
		!TestMapLoadClassification() ||
		!TestGameDllMapExistsCompatibility() ||
		!TestChangeLevelInvalidAndMissing() ||
		!TestChangeLevelLandmarkPolicy() ||
		!TestLeafCapacityMirrors() ||
		!TestCanStoreLeaf() ||
		!TestOverflowMarkerAndCheck() ||
		!TestCachedLeafIndex() ||
		!TestViewEntityCapacity())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
