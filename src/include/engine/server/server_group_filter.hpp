#ifndef XASH_ENGINE_SERVER_SERVER_GROUP_FILTER_HPP
#define XASH_ENGINE_SERVER_SERVER_GROUP_FILTER_HPP

#include "engine/server/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerGroupOperation
{
	And = kServerGroupOpAnd,
	Nand = kServerGroupOpNand,
	Unknown,
};

struct ServerGroupFilterDecision
{
	bool active;
	bool sharedBits;
	bool passes;
};

ServerGroupOperation ToServerGroupOperation(int legacyOperation);
bool HasSharedGroupBits(unsigned int leftGroup, unsigned int rightGroup);
bool ServerGroupOperationAllows(
	int legacyOperation,
	unsigned int leftGroup,
	unsigned int rightGroup);
ServerGroupFilterDecision BuildEntityPairGroupFilterDecision(
	int legacyOperation,
	unsigned int leftGroup,
	unsigned int rightGroup);
ServerGroupFilterDecision BuildMaskedGroupFilterDecision(
	int legacyOperation,
	unsigned int entityGroup,
	unsigned int groupMask);
bool EntityPairPassesGroupFilter(
	int legacyOperation,
	unsigned int leftGroup,
	unsigned int rightGroup);
bool EntityPassesGroupMask(
	int legacyOperation,
	unsigned int entityGroup,
	unsigned int groupMask);

}
}
}

#endif
