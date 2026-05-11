#ifndef XASH_ENGINE_SERVER_OPERATOR_COMMAND_POLICY_HPP
#define XASH_ENGINE_SERVER_OPERATOR_COMMAND_POLICY_HPP

#include <string>

namespace xash
{
namespace engine
{
namespace server
{

enum class OperatorInfoCommandAction
{
	PrintCurrent = 0,
	PrintUsage = 1,
	RejectStarKey = 2,
	SetValue = 3
};

struct OperatorInfoCommandPlan
{
	OperatorInfoCommandAction action;
	std::string key;
	std::string value;
};

enum class OperatorKickCommandAction
{
	PrintUsage = 0,
	FindByUserId = 1,
	FindByName = 2
};

struct OperatorKickCommandPlan
{
	OperatorKickCommandAction action;
	int userId;
	std::string target;
	std::string reason;
};

OperatorInfoCommandPlan BuildOperatorInfoCommandPlan(
	int argumentCount,
	const char *keyArgument,
	const char *valueArgument);

OperatorKickCommandPlan BuildOperatorKickCommandPlan(
	int argumentCount,
	const char *targetArgument,
	const char *reasonArgument);

}
}
}

#endif
