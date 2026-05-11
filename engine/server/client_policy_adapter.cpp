#include "client_policy_adapter.h"

#include "engine/server/client/client_policy.hpp"

namespace
{

sv_client_userinfo_penalty_plan_t ToLegacyPenaltyPlan(
	const xash::engine::server::UserinfoPenaltyPlan &plan)
{
	sv_client_userinfo_penalty_plan_t legacy = {};
	legacy.allow_update = plan.allowUpdate ? 1 : 0;
	legacy.report_ignored_update = plan.reportIgnoredUpdate ? 1 : 0;
	legacy.report_penalty_changed = plan.reportPenaltyChanged ? 1 : 0;
	legacy.next_change_time = plan.nextChangeTime;
	legacy.penalty = plan.penalty;
	legacy.change_attempts = plan.changeAttempts;
	return legacy;
}

sv_client_flag_snapshot_t ToLegacyFlagSnapshot(
	const xash::engine::server::ClientFlagSnapshot &snapshot)
{
	sv_client_flag_snapshot_t legacy = {};
	legacy.resend_userinfo = snapshot.resendUserinfo ? 1 : 0;
	legacy.resend_movevars = snapshot.resendMovevars ? 1 : 0;
	legacy.skip_net_message = snapshot.skipNetMessage ? 1 : 0;
	legacy.send_net_message = snapshot.sendNetMessage ? 1 : 0;
	legacy.predict_movement = snapshot.predictMovement ? 1 : 0;
	legacy.local_weapons = snapshot.localWeapons ? 1 : 0;
	legacy.lag_compensation = snapshot.lagCompensation ? 1 : 0;
	legacy.fake_client = snapshot.fakeClient ? 1 : 0;
	legacy.hltv_proxy = snapshot.hltvProxy ? 1 : 0;
	legacy.send_resources = snapshot.sendResources ? 1 : 0;
	legacy.force_unmodified = snapshot.forceUnmodified ? 1 : 0;
	return legacy;
}

}

extern "C" sv_client_userinfo_penalty_plan_t
SV_ClientPolicy_BuildUserinfoPenaltyPlan(
	const sv_client_userinfo_penalty_input_t *input)
{
	if (!input)
		return {};

	xash::engine::server::UserinfoPenaltyInput modern = {};
	modern.penaltyEnabled = input->penalty_enabled != 0;
	modern.fakeClient = input->fake_client != 0;
	modern.singlePlayer = input->single_player != 0;
	modern.realtime = input->realtime;
	modern.nextChangeTime = input->next_change_time;
	modern.penalty = input->penalty;
	modern.basePenalty = input->base_penalty;
	modern.penaltyMultiplier = input->penalty_multiplier;
	modern.changeAttempts = input->change_attempts;
	modern.maxAttempts = input->max_attempts;

	return ToLegacyPenaltyPlan(
		xash::engine::server::BuildUserinfoPenaltyPlan(modern));
}

extern "C" double SV_ClientPolicy_ResolveRequestedClientRate(
	int requested_rate,
	double default_rate,
	double minimum_rate,
	double maximum_rate)
{
	return xash::engine::server::ResolveRequestedClientRate(
		requested_rate,
		default_rate,
		minimum_rate,
		maximum_rate);
}

extern "C" double SV_ClientPolicy_ResolveRequestedUpdateInterval(
	int requested_update_rate,
	int default_update_rate)
{
	return xash::engine::server::ResolveRequestedUpdateInterval(
		requested_update_rate,
		default_update_rate);
}

extern "C" double SV_ClientPolicy_ApplyUpdateIntervalLimits(
	double interval,
	double maximum_update_rate,
	double minimum_update_rate)
{
	return xash::engine::server::ApplyUpdateIntervalLimits(
		interval,
		maximum_update_rate,
		minimum_update_rate);
}

extern "C" double SV_ClientPolicy_ApplyLegacyServerRateLimits(
	double rate,
	double maximum_rate,
	double minimum_rate)
{
	return xash::engine::server::ApplyLegacyServerRateLimits(
		rate,
		maximum_rate,
		minimum_rate);
}

extern "C" sv_client_userinfo_flag_plan_t
SV_ClientPolicy_BuildUserinfoFlagPlan(
	int no_prediction,
	int lag_compensation,
	int local_weapons)
{
	const xash::engine::server::ClientUserinfoFlagPlan modern =
		xash::engine::server::BuildClientUserinfoFlagPlan(
			no_prediction,
			lag_compensation,
			local_weapons);

	sv_client_userinfo_flag_plan_t legacy = {};
	legacy.predict_movement = modern.predictMovement ? 1 : 0;
	legacy.lag_compensation = modern.lagCompensation ? 1 : 0;
	legacy.local_weapons = modern.localWeapons ? 1 : 0;
	return legacy;
}

extern "C" sv_client_flag_snapshot_t SV_ClientPolicy_BuildFlagSnapshot(
	unsigned int flags)
{
	return ToLegacyFlagSnapshot(
		xash::engine::server::BuildClientFlagSnapshot(flags));
}

extern "C" int SV_ClientPolicy_IsFakeClient(unsigned int flags)
{
	return xash::engine::server::ClientIsFakeClient(flags) ? 1 : 0;
}

extern "C" int SV_ClientPolicy_IsHltvProxy(unsigned int flags)
{
	return xash::engine::server::ClientIsHltvProxy(flags) ? 1 : 0;
}

extern "C" int SV_ClientPolicy_UsesLocalWeapons(unsigned int flags)
{
	return xash::engine::server::ClientUsesLocalWeapons(flags) ? 1 : 0;
}

extern "C" int SV_ClientPolicy_PredictsMovement(unsigned int flags)
{
	return xash::engine::server::ClientPredictsMovement(flags) ? 1 : 0;
}

extern "C" int SV_ClientPolicy_UsesLagCompensation(unsigned int flags)
{
	return xash::engine::server::ClientUsesLagCompensation(flags) ? 1 : 0;
}

extern "C" int SV_ClientPolicy_ShouldAppearInHumanQueries(unsigned int flags)
{
	return xash::engine::server::ClientShouldAppearInHumanQueries(flags) ? 1 : 0;
}
