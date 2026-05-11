#ifndef XASH_ENGINE_SERVER_OPERATOR_COMMAND_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_OPERATOR_COMMAND_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_operator_info_action_e
{
	SV_OPERATOR_INFO_PRINT_CURRENT = 0,
	SV_OPERATOR_INFO_PRINT_USAGE = 1,
	SV_OPERATOR_INFO_REJECT_STAR_KEY = 2,
	SV_OPERATOR_INFO_SET_VALUE = 3
};

enum sv_operator_kick_action_e
{
	SV_OPERATOR_KICK_PRINT_USAGE = 0,
	SV_OPERATOR_KICK_FIND_BY_USERID = 1,
	SV_OPERATOR_KICK_FIND_BY_NAME = 2
};

typedef struct sv_operator_info_plan_s
{
	enum sv_operator_info_action_e action;
	const char *key;
	const char *value;
} sv_operator_info_plan_t;

typedef struct sv_operator_kick_plan_s
{
	enum sv_operator_kick_action_e action;
	int user_id;
	const char *target;
	const char *reason;
} sv_operator_kick_plan_t;

sv_operator_info_plan_t SV_Operator_BuildInfoCommandPlan(
	int argument_count,
	const char *key_argument,
	const char *value_argument);

sv_operator_kick_plan_t SV_Operator_BuildKickCommandPlan(
	int argument_count,
	const char *target_argument,
	const char *reason_argument);

#ifdef __cplusplus
}
#endif

#endif
