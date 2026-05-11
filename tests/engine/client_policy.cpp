#include <cmath>
#include <cstdlib>

#include "engine/server/client/client_policy.hpp"
#include "engine/server/server_limits.hpp"

using namespace xash::engine::server;

namespace
{

bool CloseEnough(double lhs, double rhs)
{
	return std::fabs(lhs - rhs) < 0.000001;
}

UserinfoPenaltyInput DefaultPenaltyInput()
{
	UserinfoPenaltyInput input = {};
	input.penaltyEnabled = true;
	input.realtime = 10.0;
	input.nextChangeTime = 0.0;
	input.penalty = 0.0;
	input.basePenalty = 1.0;
	input.penaltyMultiplier = 2.0;
	input.changeAttempts = 0;
	input.maxAttempts = 3;
	return input;
}

bool TestPenaltyDisabledDoesNotMutateState()
{
	UserinfoPenaltyInput input = DefaultPenaltyInput();
	input.penaltyEnabled = false;
	input.fakeClient = true;
	input.singlePlayer = true;
	input.nextChangeTime = 5.0;
	input.penalty = 7.0;
	input.changeAttempts = 2;

	const UserinfoPenaltyPlan plan = BuildUserinfoPenaltyPlan(input);

	return plan.allowUpdate &&
		!plan.reportIgnoredUpdate &&
		!plan.reportPenaltyChanged &&
		CloseEnough(plan.nextChangeTime, 5.0) &&
		CloseEnough(plan.penalty, 7.0) &&
		plan.changeAttempts == 2;
}

bool TestFirstPenalizedUpdateStartsWindow()
{
	const UserinfoPenaltyPlan plan =
		BuildUserinfoPenaltyPlan(DefaultPenaltyInput());

	return plan.allowUpdate &&
		!plan.reportIgnoredUpdate &&
		!plan.reportPenaltyChanged &&
		CloseEnough(plan.penalty, 1.0) &&
		plan.changeAttempts == 0 &&
		CloseEnough(plan.nextChangeTime, 12.0);
}

bool TestQuickFirstRetryIsAllowedButCountsAttempt()
{
	UserinfoPenaltyInput input = DefaultPenaltyInput();
	input.realtime = 11.0;
	input.nextChangeTime = 12.0;
	input.penalty = 1.0;

	const UserinfoPenaltyPlan plan = BuildUserinfoPenaltyPlan(input);

	return plan.allowUpdate &&
		!plan.reportIgnoredUpdate &&
		plan.changeAttempts == 1 &&
		CloseEnough(plan.nextChangeTime, 13.0);
}

bool TestRepeatedQuickRetryIsIgnoredAndCanIncreasePenalty()
{
	UserinfoPenaltyInput input = DefaultPenaltyInput();
	input.realtime = 11.5;
	input.nextChangeTime = 13.0;
	input.penalty = 1.0;
	input.changeAttempts = 2;

	const UserinfoPenaltyPlan plan = BuildUserinfoPenaltyPlan(input);

	return !plan.allowUpdate &&
		plan.reportIgnoredUpdate &&
		plan.reportPenaltyChanged &&
		plan.changeAttempts == 0 &&
		CloseEnough(plan.penalty, 2.0) &&
		CloseEnough(plan.nextChangeTime, 15.5);
}

bool TestRequestedRateUsesDefaultAndHardClamp()
{
	return CloseEnough(
			ResolveRequestedClientRate(0, 9999.0, 1000.0, 100000.0),
			9999.0) &&
		CloseEnough(
			ResolveRequestedClientRate(250, 9999.0, 1000.0, 100000.0),
			1000.0) &&
		CloseEnough(
			ResolveRequestedClientRate(250000, 9999.0, 1000.0, 100000.0),
			100000.0);
}

bool TestUpdateIntervalDefaultsAndCvarLimits()
{
	const double defaultInterval = ResolveRequestedUpdateInterval(0, 20);
	const double fastInterval = ResolveRequestedUpdateInterval(100, 20);
	const double slowInterval = ResolveRequestedUpdateInterval(5, 20);

	return CloseEnough(defaultInterval, 0.05) &&
		CloseEnough(
			ApplyUpdateIntervalLimits(fastInterval, 50.0, 0.0),
			0.02) &&
		CloseEnough(
			ApplyUpdateIntervalLimits(slowInterval, 0.0, 10.0),
			0.1) &&
		CloseEnough(
			ApplyUpdateIntervalLimits(defaultInterval, 0.0, 0.0),
			defaultInterval);
}

bool TestLegacyServerRateLimitsDoNotClamp()
{
	return CloseEnough(
			ApplyLegacyServerRateLimits(5000.0, 2000.0, 0.0),
			5000.0) &&
		CloseEnough(
			ApplyLegacyServerRateLimits(1500.0, 0.0, 3000.0),
			1500.0) &&
		CloseEnough(
			ApplyLegacyServerRateLimits(4000.0, 5000.0, 1000.0),
			4000.0);
}

bool TestClientUserinfoFlags()
{
	const ClientUserinfoFlagPlan defaults =
		BuildClientUserinfoFlagPlan(0, 0, 0);
	const ClientUserinfoFlagPlan enabled =
		BuildClientUserinfoFlagPlan(1, 1, 1);

	return defaults.predictMovement &&
		!defaults.lagCompensation &&
		!defaults.localWeapons &&
		!enabled.predictMovement &&
		enabled.lagCompensation &&
		enabled.localWeapons;
}

bool TestClientFlagSnapshot()
{
	const unsigned int flags =
		kServerClientFlagResendUserinfo |
		kServerClientFlagPredictMovement |
		kServerClientFlagLocalWeapons |
		kServerClientFlagFakeClient |
		kServerClientFlagHltvProxy;
	const ClientFlagSnapshot snapshot = BuildClientFlagSnapshot(flags);

	return snapshot.resendUserinfo &&
		!snapshot.resendMovevars &&
		!snapshot.skipNetMessage &&
		!snapshot.sendNetMessage &&
		snapshot.predictMovement &&
		snapshot.localWeapons &&
		!snapshot.lagCompensation &&
		snapshot.fakeClient &&
		snapshot.hltvProxy &&
		!snapshot.sendResources &&
		!snapshot.forceUnmodified;
}

bool TestClientFlagPredicates()
{
	const unsigned int fake = kServerClientFlagFakeClient;
	const unsigned int hltv = kServerClientFlagHltvProxy;
	const unsigned int prediction =
		kServerClientFlagPredictMovement |
		kServerClientFlagLocalWeapons |
		kServerClientFlagLagCompensation;

	return ClientIsFakeClient(fake) &&
		!ClientIsFakeClient(0u) &&
		ClientIsHltvProxy(hltv) &&
		!ClientIsHltvProxy(0u) &&
		ClientPredictsMovement(prediction) &&
		ClientUsesLocalWeapons(prediction) &&
		ClientUsesLagCompensation(prediction) &&
		ClientShouldAppearInHumanQueries(0u) &&
		!ClientShouldAppearInHumanQueries(fake);
}

}

int main()
{
	if (!TestPenaltyDisabledDoesNotMutateState() ||
		!TestFirstPenalizedUpdateStartsWindow() ||
		!TestQuickFirstRetryIsAllowedButCountsAttempt() ||
		!TestRepeatedQuickRetryIsIgnoredAndCanIncreasePenalty() ||
		!TestRequestedRateUsesDefaultAndHardClamp() ||
		!TestUpdateIntervalDefaultsAndCvarLimits() ||
		!TestLegacyServerRateLimitsDoNotClamp() ||
		!TestClientUserinfoFlags() ||
		!TestClientFlagSnapshot() ||
		!TestClientFlagPredicates())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
