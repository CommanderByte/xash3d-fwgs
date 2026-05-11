#include "engine/server/shared/server_group_filter.hpp"
#include "engine/server/shared/server_map_validation.hpp"
#include "engine/server/shared/server_visibility_constraints.hpp"

namespace xash
{
namespace engine
{
namespace server
{

ServerGroupOperation ToServerGroupOperation(int legacyOperation)
{
	switch (legacyOperation)
	{
	case kServerGroupOpAnd:
		return ServerGroupOperation::And;
	case kServerGroupOpNand:
		return ServerGroupOperation::Nand;
	default:
		return ServerGroupOperation::Unknown;
	}
}

bool HasSharedGroupBits(unsigned int leftGroup, unsigned int rightGroup)
{
	return (leftGroup & rightGroup) != 0u;
}

bool ServerGroupOperationAllows(
	int legacyOperation,
	unsigned int leftGroup,
	unsigned int rightGroup)
{
	const bool sharedBits = HasSharedGroupBits(leftGroup, rightGroup);

	switch (ToServerGroupOperation(legacyOperation))
	{
	case ServerGroupOperation::And:
		return sharedBits;
	case ServerGroupOperation::Nand:
		return !sharedBits;
	case ServerGroupOperation::Unknown:
		return true;
	}

	return true;
}

ServerGroupFilterDecision BuildEntityPairGroupFilterDecision(
	int legacyOperation,
	unsigned int leftGroup,
	unsigned int rightGroup)
{
	ServerGroupFilterDecision decision = {};
	decision.active = leftGroup != 0u && rightGroup != 0u;
	decision.sharedBits = HasSharedGroupBits(leftGroup, rightGroup);
	decision.passes = !decision.active ||
		ServerGroupOperationAllows(legacyOperation, leftGroup, rightGroup);
	return decision;
}

ServerGroupFilterDecision BuildMaskedGroupFilterDecision(
	int legacyOperation,
	unsigned int entityGroup,
	unsigned int groupMask)
{
	ServerGroupFilterDecision decision = {};
	decision.active = entityGroup != 0u;
	decision.sharedBits = HasSharedGroupBits(entityGroup, groupMask);
	decision.passes = !decision.active ||
		ServerGroupOperationAllows(legacyOperation, entityGroup, groupMask);
	return decision;
}

bool EntityPairPassesGroupFilter(
	int legacyOperation,
	unsigned int leftGroup,
	unsigned int rightGroup)
{
	return BuildEntityPairGroupFilterDecision(
		legacyOperation,
		leftGroup,
		rightGroup).passes;
}

bool EntityPassesGroupMask(
	int legacyOperation,
	unsigned int entityGroup,
	unsigned int groupMask)
{
	return BuildMaskedGroupFilterDecision(
		legacyOperation,
		entityGroup,
		groupMask).passes;
}

ServerMapValidationFlags DecodeServerMapValidationFlags(unsigned int mapFlags)
{
	ServerMapValidationFlags flags = {};
	flags.exists = (mapFlags & kServerMapExists) != 0u;
	flags.hasLandmark = (mapFlags & kServerMapHasLandmark) != 0u;
	flags.invalidVersion = (mapFlags & kServerMapInvalidVersion) != 0u;
	return flags;
}

bool ServerMapExists(unsigned int mapFlags)
{
	return DecodeServerMapValidationFlags(mapFlags).exists;
}

bool ServerMapHasLandmark(unsigned int mapFlags)
{
	return DecodeServerMapValidationFlags(mapFlags).hasLandmark;
}

bool ServerMapHasInvalidVersion(unsigned int mapFlags)
{
	return DecodeServerMapValidationFlags(mapFlags).invalidVersion;
}

ServerMapValidationResult ClassifyServerMapValidation(unsigned int mapFlags)
{
	const ServerMapValidationFlags flags =
		DecodeServerMapValidationFlags(mapFlags);

	if (flags.invalidVersion)
		return ServerMapValidationResult::InvalidVersion;

	if (!flags.exists)
		return ServerMapValidationResult::Missing;

	return ServerMapValidationResult::Valid;
}

bool ServerMapCanLoad(unsigned int mapFlags)
{
	return ClassifyServerMapValidation(mapFlags) ==
		ServerMapValidationResult::Valid;
}

bool ServerMapExistsForGameDll(unsigned int mapFlags)
{
	return ServerMapExists(mapFlags);
}

ServerChangeLevelMapValidationDecision BuildServerChangeLevelMapValidationDecision(
	unsigned int mapFlags,
	bool smoothRequested,
	bool validateChangeLevel)
{
	ServerChangeLevelMapValidationDecision decision = {};
	decision.flags = DecodeServerMapValidationFlags(mapFlags);
	decision.result = ClassifyServerMapValidation(mapFlags);
	decision.canContinue = decision.result == ServerMapValidationResult::Valid;
	decision.missingLandmarkForSmooth =
		decision.canContinue &&
		smoothRequested &&
		!decision.flags.hasLandmark;
	decision.disableSmooth =
		decision.missingLandmarkForSmooth && validateChangeLevel;
	return decision;
}

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
