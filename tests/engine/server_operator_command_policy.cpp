#include <cstdlib>

#include "engine/server/server_operator_command_policy.hpp"

using namespace xash::engine::server;

namespace
{

bool TestInfoCommandPrintsCurrentWithoutArguments()
{
	const OperatorInfoCommandPlan plan =
		BuildOperatorInfoCommandPlan(1, nullptr, nullptr);

	return plan.action == OperatorInfoCommandAction::PrintCurrent &&
		plan.key.empty() &&
		plan.value.empty();
}

bool TestInfoCommandRejectsWrongArgumentCountsBeforeStarKeys()
{
	const OperatorInfoCommandPlan twoArgs =
		BuildOperatorInfoCommandPlan(2, "*protected", nullptr);
	const OperatorInfoCommandPlan fourArgs =
		BuildOperatorInfoCommandPlan(4, "hostname", "value");

	return twoArgs.action == OperatorInfoCommandAction::PrintUsage &&
		fourArgs.action == OperatorInfoCommandAction::PrintUsage;
}

bool TestInfoCommandRejectsStarKeysOnlyWhenSetting()
{
	const OperatorInfoCommandPlan plan =
		BuildOperatorInfoCommandPlan(3, "*protected", "value");

	return plan.action == OperatorInfoCommandAction::RejectStarKey &&
		plan.key == "*protected" &&
		plan.value == "value";
}

bool TestInfoCommandAllowsRegularKeyMutation()
{
	const OperatorInfoCommandPlan plan =
		BuildOperatorInfoCommandPlan(3, "hostname", "lambda");

	return plan.action == OperatorInfoCommandAction::SetValue &&
		plan.key == "hostname" &&
		plan.value == "lambda";
}

bool TestKickCommandRequiresTargetArgument()
{
	return BuildOperatorKickCommandPlan(1, nullptr, nullptr).action ==
		OperatorKickCommandAction::PrintUsage;
}

bool TestKickCommandClassifiesHashUserId()
{
	const OperatorKickCommandPlan plan =
		BuildOperatorKickCommandPlan(3, "#42", "reason");

	return plan.action == OperatorKickCommandAction::FindByUserId &&
		plan.userId == 42 &&
		plan.target == "#42" &&
		plan.reason == "reason";
}

bool TestKickCommandRequiresAllDigitsAfterHash()
{
	const OperatorKickCommandPlan plan =
		BuildOperatorKickCommandPlan(2, "#12abc", nullptr);

	return plan.action == OperatorKickCommandAction::FindByName &&
		plan.target == "#12abc" &&
		plan.reason.empty();
}

bool TestKickCommandKeepsNamesAndEmptyReason()
{
	const OperatorKickCommandPlan plan =
		BuildOperatorKickCommandPlan(2, "Gordon", nullptr);

	return plan.action == OperatorKickCommandAction::FindByName &&
		plan.target == "Gordon" &&
		plan.reason.empty();
}

}

int main()
{
	if (!TestInfoCommandPrintsCurrentWithoutArguments() ||
		!TestInfoCommandRejectsWrongArgumentCountsBeforeStarKeys() ||
		!TestInfoCommandRejectsStarKeysOnlyWhenSetting() ||
		!TestInfoCommandAllowsRegularKeyMutation() ||
		!TestKickCommandRequiresTargetArgument() ||
		!TestKickCommandClassifiesHashUserId() ||
		!TestKickCommandRequiresAllDigitsAfterHash() ||
		!TestKickCommandKeepsNamesAndEmptyReason())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
