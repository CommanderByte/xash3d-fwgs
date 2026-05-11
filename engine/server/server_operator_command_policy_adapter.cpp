#include "server_operator_command_policy_adapter.h"

#include "engine/server/server_operator_command_policy.hpp"

namespace
{

const char *TextOrEmpty(const char *text)
{
	return text ? text : "";
}

}

static_assert(SV_OPERATOR_INFO_PRINT_CURRENT ==
	static_cast<int>(xash::engine::server::OperatorInfoCommandAction::PrintCurrent),
	"operator info print-current action changed");
static_assert(SV_OPERATOR_INFO_PRINT_USAGE ==
	static_cast<int>(xash::engine::server::OperatorInfoCommandAction::PrintUsage),
	"operator info print-usage action changed");
static_assert(SV_OPERATOR_INFO_REJECT_STAR_KEY ==
	static_cast<int>(xash::engine::server::OperatorInfoCommandAction::RejectStarKey),
	"operator info reject-star action changed");
static_assert(SV_OPERATOR_INFO_SET_VALUE ==
	static_cast<int>(xash::engine::server::OperatorInfoCommandAction::SetValue),
	"operator info set-value action changed");
static_assert(SV_OPERATOR_KICK_PRINT_USAGE ==
	static_cast<int>(xash::engine::server::OperatorKickCommandAction::PrintUsage),
	"operator kick print-usage action changed");
static_assert(SV_OPERATOR_KICK_FIND_BY_USERID ==
	static_cast<int>(xash::engine::server::OperatorKickCommandAction::FindByUserId),
	"operator kick find-by-userid action changed");
static_assert(SV_OPERATOR_KICK_FIND_BY_NAME ==
	static_cast<int>(xash::engine::server::OperatorKickCommandAction::FindByName),
	"operator kick find-by-name action changed");

extern "C" sv_operator_info_plan_t SV_Operator_BuildInfoCommandPlan(
	int argument_count,
	const char *key_argument,
	const char *value_argument)
{
	const xash::engine::server::OperatorInfoCommandPlan plan =
		xash::engine::server::BuildOperatorInfoCommandPlan(
			argument_count,
			key_argument,
			value_argument);

	sv_operator_info_plan_t legacy = {};
	legacy.action = static_cast<sv_operator_info_action_e>(plan.action);
	legacy.key = TextOrEmpty(key_argument);
	legacy.value = TextOrEmpty(value_argument);
	return legacy;
}

extern "C" sv_operator_kick_plan_t SV_Operator_BuildKickCommandPlan(
	int argument_count,
	const char *target_argument,
	const char *reason_argument)
{
	const xash::engine::server::OperatorKickCommandPlan plan =
		xash::engine::server::BuildOperatorKickCommandPlan(
			argument_count,
			target_argument,
			reason_argument);

	sv_operator_kick_plan_t legacy = {};
	legacy.action = static_cast<sv_operator_kick_action_e>(plan.action);
	legacy.user_id = plan.userId;
	legacy.target = TextOrEmpty(target_argument);
	legacy.reason = TextOrEmpty(reason_argument);
	return legacy;
}
