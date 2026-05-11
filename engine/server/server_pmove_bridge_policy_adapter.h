#ifndef XASH_ENGINE_SERVER_PMOVE_BRIDGE_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_PMOVE_BRIDGE_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_pmove_unlag_admission_facts_s
{
	int max_clients;
	int game_allows_lag_compensation;
	int server_unlag_enabled;
	int client_lag_compensation_enabled;
	int client_spawned;
} sv_pmove_unlag_admission_facts_t;

typedef struct sv_pmove_unlag_latency_plan_s
{
	float latency;
	float normalized_max_unlag;
	int clamp_max_unlag_cvar_to_zero;
} sv_pmove_unlag_latency_plan_t;

int SV_PMoveBridge_ShouldEnableUnlag(
	const sv_pmove_unlag_admission_facts_t *facts);
int SV_PMoveBridge_IsPlayerEntityIndex(int edict_index, int max_clients);
int SV_PMoveBridge_ShouldUseInterpolatedPlayer(
	int edict_index,
	int max_clients,
	int interpolant_active,
	int interpolant_moving);
int SV_PMoveBridge_IsUnlagTeleport(
	const float *old_position,
	const float *new_position);
sv_pmove_unlag_latency_plan_t SV_PMoveBridge_BuildUnlagLatencyPlan(
	float client_latency,
	float max_unlag);
float SV_PMoveBridge_BuildLerpSeconds(
	int lerp_milliseconds,
	float next_message_interval);
float SV_PMoveBridge_BuildUnlagTargetTime(
	float realtime,
	float latency,
	float lerp_seconds,
	float push_seconds);
float SV_PMoveBridge_BuildInterpolationFraction(
	float target_time,
	float frame_time,
	float next_frame_time);

#ifdef __cplusplus
}
#endif

#endif
