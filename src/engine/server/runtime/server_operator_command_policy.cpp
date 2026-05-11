#include "engine/server/runtime/server_operator_command_policy.hpp"

#include <cstdlib>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

std::string StringOrEmpty(const char *value)
{
	return value ? value : "";
}

bool IsAllDigits(const char *value)
{
	if (!value || !*value)
		return false;

	for (const char *cursor = value; *cursor; ++cursor)
	{
		if (*cursor < '0' || *cursor > '9')
			return false;
	}

	return true;
}

}

OperatorInfoCommandPlan BuildOperatorInfoCommandPlan(
	int argumentCount,
	const char *keyArgument,
	const char *valueArgument)
{
	OperatorInfoCommandPlan plan = {};
	plan.key = StringOrEmpty(keyArgument);
	plan.value = StringOrEmpty(valueArgument);

	if (argumentCount == 1)
	{
		plan.action = OperatorInfoCommandAction::PrintCurrent;
		return plan;
	}

	if (argumentCount != 3)
	{
		plan.action = OperatorInfoCommandAction::PrintUsage;
		return plan;
	}

	if (!plan.key.empty() && plan.key[0] == '*')
	{
		plan.action = OperatorInfoCommandAction::RejectStarKey;
		return plan;
	}

	plan.action = OperatorInfoCommandAction::SetValue;
	return plan;
}

OperatorKickCommandPlan BuildOperatorKickCommandPlan(
	int argumentCount,
	const char *targetArgument,
	const char *reasonArgument)
{
	OperatorKickCommandPlan plan = {};
	plan.target = StringOrEmpty(targetArgument);
	plan.reason = StringOrEmpty(reasonArgument);

	if (argumentCount < 2)
	{
		plan.action = OperatorKickCommandAction::PrintUsage;
		return plan;
	}

	if (plan.target.size() > 1 &&
		plan.target[0] == '#' &&
		IsAllDigits(plan.target.c_str() + 1))
	{
		plan.action = OperatorKickCommandAction::FindByUserId;
		plan.userId = std::atoi(plan.target.c_str() + 1);
		return plan;
	}

	plan.action = OperatorKickCommandAction::FindByName;
	return plan;
}

}
}
}
