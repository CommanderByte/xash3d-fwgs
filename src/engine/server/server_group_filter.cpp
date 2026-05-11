#include "engine/server/server_group_filter.hpp"

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

}
}
}
