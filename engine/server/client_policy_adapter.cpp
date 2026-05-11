#include "client_policy_adapter.h"

#include "client_adapter_shared.hpp"
#include "engine/server/client/client_policy.hpp"

using xash::engine::server::adapter::client::FromLegacyBool;
using xash::engine::server::adapter::client::ToLegacyBool;

namespace
{

sv_client_userinfo_penalty_plan_t ToLegacyPenaltyPlan(
	const xash::engine::server::UserinfoPenaltyPlan &plan)
{
	sv_client_userinfo_penalty_plan_t legacy = {};
	legacy.allow_update = ToLegacyBool(plan.allowUpdate);
	legacy.report_ignored_update = ToLegacyBool(plan.reportIgnoredUpdate);
	legacy.report_penalty_changed = ToLegacyBool(plan.reportPenaltyChanged);
	legacy.next_change_time = plan.nextChangeTime;
	legacy.penalty = plan.penalty;
	legacy.change_attempts = plan.changeAttempts;
	return legacy;
}

sv_client_flag_snapshot_t ToLegacyFlagSnapshot(
	const xash::engine::server::ClientFlagSnapshot &snapshot)
{
	sv_client_flag_snapshot_t legacy = {};
	legacy.resend_userinfo = ToLegacyBool(snapshot.resendUserinfo);
	legacy.resend_movevars = ToLegacyBool(snapshot.resendMovevars);
	legacy.skip_net_message = ToLegacyBool(snapshot.skipNetMessage);
	legacy.send_net_message = ToLegacyBool(snapshot.sendNetMessage);
	legacy.predict_movement = ToLegacyBool(snapshot.predictMovement);
	legacy.local_weapons = ToLegacyBool(snapshot.localWeapons);
	legacy.lag_compensation = ToLegacyBool(snapshot.lagCompensation);
	legacy.fake_client = ToLegacyBool(snapshot.fakeClient);
	legacy.hltv_proxy = ToLegacyBool(snapshot.hltvProxy);
	legacy.send_resources = ToLegacyBool(snapshot.sendResources);
	legacy.force_unmodified = ToLegacyBool(snapshot.forceUnmodified);
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
	modern.penaltyEnabled = FromLegacyBool(input->penalty_enabled);
	modern.fakeClient = FromLegacyBool(input->fake_client);
	modern.singlePlayer = FromLegacyBool(input->single_player);
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
	legacy.predict_movement = ToLegacyBool(modern.predictMovement);
	legacy.lag_compensation = ToLegacyBool(modern.lagCompensation);
	legacy.local_weapons = ToLegacyBool(modern.localWeapons);
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
	return ToLegacyBool(xash::engine::server::ClientIsFakeClient(flags));
}

extern "C" int SV_ClientPolicy_IsHltvProxy(unsigned int flags)
{
	return ToLegacyBool(xash::engine::server::ClientIsHltvProxy(flags));
}

extern "C" int SV_ClientPolicy_UsesLocalWeapons(unsigned int flags)
{
	return ToLegacyBool(xash::engine::server::ClientUsesLocalWeapons(flags));
}

extern "C" int SV_ClientPolicy_PredictsMovement(unsigned int flags)
{
	return ToLegacyBool(xash::engine::server::ClientPredictsMovement(flags));
}

extern "C" int SV_ClientPolicy_UsesLagCompensation(unsigned int flags)
{
	return ToLegacyBool(xash::engine::server::ClientUsesLagCompensation(flags));
}

extern "C" int SV_ClientPolicy_ShouldAppearInHumanQueries(unsigned int flags)
{
	return ToLegacyBool(
		xash::engine::server::ClientShouldAppearInHumanQueries(flags));
}
