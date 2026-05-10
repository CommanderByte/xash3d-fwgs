#ifndef XASH_ENGINE_SERVER_CLIENT_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_CLIENT_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_client_userinfo_penalty_input_s
{
	int penalty_enabled;
	int fake_client;
	int single_player;
	double realtime;
	double next_change_time;
	double penalty;
	double base_penalty;
	double penalty_multiplier;
	int change_attempts;
	int max_attempts;
} sv_client_userinfo_penalty_input_t;

typedef struct sv_client_userinfo_penalty_plan_s
{
	int allow_update;
	int report_ignored_update;
	int report_penalty_changed;
	double next_change_time;
	double penalty;
	int change_attempts;
} sv_client_userinfo_penalty_plan_t;

typedef struct sv_client_userinfo_flag_plan_s
{
	int predict_movement;
	int lag_compensation;
	int local_weapons;
} sv_client_userinfo_flag_plan_t;

sv_client_userinfo_penalty_plan_t SV_ClientPolicy_BuildUserinfoPenaltyPlan(
	const sv_client_userinfo_penalty_input_t *input);

double SV_ClientPolicy_ResolveRequestedClientRate(
	int requested_rate,
	double default_rate,
	double minimum_rate,
	double maximum_rate);

double SV_ClientPolicy_ResolveRequestedUpdateInterval(
	int requested_update_rate,
	int default_update_rate);

double SV_ClientPolicy_ApplyUpdateIntervalLimits(
	double interval,
	double maximum_update_rate,
	double minimum_update_rate);

double SV_ClientPolicy_ApplyLegacyServerRateLimits(
	double rate,
	double maximum_rate,
	double minimum_rate);

sv_client_userinfo_flag_plan_t SV_ClientPolicy_BuildUserinfoFlagPlan(
	int no_prediction,
	int lag_compensation,
	int local_weapons);

#ifdef __cplusplus
}
#endif

#endif
