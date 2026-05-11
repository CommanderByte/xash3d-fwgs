#include "server_pmove_bridge_policy_adapter.h"

#include "engine/server/world/server_pmove_bridge_policy.hpp"

namespace
{

xash::engine::server::PmoveVector3 BuildVector3(const float *value)
{
	xash::engine::server::PmoveVector3 result = {};

	if (!value)
		return result;

	result.x = value[0];
	result.y = value[1];
	result.z = value[2];
	return result;
}

}

extern "C" int SV_PMoveBridge_ShouldEnableUnlag(
	const sv_pmove_unlag_admission_facts_t *facts)
{
	if (!facts)
		return 0;

	xash::engine::server::PmoveUnlagAdmissionFacts cpp_facts = {};
	cpp_facts.maxClients = facts->max_clients;
	cpp_facts.gameAllowsLagCompensation =
		facts->game_allows_lag_compensation != 0;
	cpp_facts.serverUnlagEnabled = facts->server_unlag_enabled != 0;
	cpp_facts.clientLagCompensationEnabled =
		facts->client_lag_compensation_enabled != 0;
	cpp_facts.clientSpawned = facts->client_spawned != 0;

	return xash::engine::server::ShouldEnablePmoveUnlag(cpp_facts) ? 1 : 0;
}

extern "C" int SV_PMoveBridge_IsPlayerEntityIndex(
	int edict_index,
	int max_clients)
{
	return xash::engine::server::IsPmovePlayerEntityIndex(
		edict_index,
		max_clients) ? 1 : 0;
}

extern "C" int SV_PMoveBridge_ShouldUseInterpolatedPlayer(
	int edict_index,
	int max_clients,
	int interpolant_active,
	int interpolant_moving)
{
	return xash::engine::server::ShouldUsePmoveInterpolatedPlayer(
		edict_index,
		max_clients,
		interpolant_active != 0,
		interpolant_moving != 0) ? 1 : 0;
}

extern "C" int SV_PMoveBridge_IsUnlagTeleport(
	const float *old_position,
	const float *new_position)
{
	return xash::engine::server::IsPmoveUnlagTeleport(
		BuildVector3(old_position),
		BuildVector3(new_position)) ? 1 : 0;
}

extern "C" sv_pmove_unlag_latency_plan_t
SV_PMoveBridge_BuildUnlagLatencyPlan(
	float client_latency,
	float max_unlag)
{
	const xash::engine::server::PmoveUnlagLatencyPlan cpp_plan =
		xash::engine::server::BuildPmoveUnlagLatencyPlan(
			client_latency,
			max_unlag);

	sv_pmove_unlag_latency_plan_t result = {};
	result.latency = cpp_plan.latency;
	result.normalized_max_unlag = cpp_plan.normalizedMaxUnlag;
	result.clamp_max_unlag_cvar_to_zero =
		cpp_plan.clampMaxUnlagCvarToZero ? 1 : 0;
	return result;
}

extern "C" float SV_PMoveBridge_BuildLerpSeconds(
	int lerp_milliseconds,
	float next_message_interval)
{
	return xash::engine::server::BuildPmoveLerpSeconds(
		lerp_milliseconds,
		next_message_interval);
}

extern "C" float SV_PMoveBridge_BuildUnlagTargetTime(
	float realtime,
	float latency,
	float lerp_seconds,
	float push_seconds)
{
	return xash::engine::server::BuildPmoveUnlagTargetTime(
		realtime,
		latency,
		lerp_seconds,
		push_seconds);
}

extern "C" float SV_PMoveBridge_BuildInterpolationFraction(
	float target_time,
	float frame_time,
	float next_frame_time)
{
	return xash::engine::server::BuildPmoveInterpolationFraction(
		target_time,
		frame_time,
		next_frame_time);
}
