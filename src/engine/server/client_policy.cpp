#include "engine/server/client_policy.hpp"

#include "engine/server/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

double Clamp(double value, double minimum, double maximum)
{
	if (value < minimum)
		return minimum;

	if (value > maximum)
		return maximum;

	return value;
}

}

ClientFlagSnapshot BuildClientFlagSnapshot(unsigned int flags)
{
	ClientFlagSnapshot snapshot = {};
	snapshot.resendUserinfo =
		(flags & kServerClientFlagResendUserinfo) != 0u;
	snapshot.resendMovevars =
		(flags & kServerClientFlagResendMovevars) != 0u;
	snapshot.skipNetMessage =
		(flags & kServerClientFlagSkipNetMessage) != 0u;
	snapshot.sendNetMessage =
		(flags & kServerClientFlagSendNetMessage) != 0u;
	snapshot.predictMovement =
		(flags & kServerClientFlagPredictMovement) != 0u;
	snapshot.localWeapons =
		(flags & kServerClientFlagLocalWeapons) != 0u;
	snapshot.lagCompensation =
		(flags & kServerClientFlagLagCompensation) != 0u;
	snapshot.fakeClient =
		(flags & kServerClientFlagFakeClient) != 0u;
	snapshot.hltvProxy =
		(flags & kServerClientFlagHltvProxy) != 0u;
	snapshot.sendResources =
		(flags & kServerClientFlagSendResources) != 0u;
	snapshot.forceUnmodified =
		(flags & kServerClientFlagForceUnmodified) != 0u;
	return snapshot;
}

bool ClientIsFakeClient(unsigned int flags)
{
	return BuildClientFlagSnapshot(flags).fakeClient;
}

bool ClientIsHltvProxy(unsigned int flags)
{
	return BuildClientFlagSnapshot(flags).hltvProxy;
}

bool ClientUsesLocalWeapons(unsigned int flags)
{
	return BuildClientFlagSnapshot(flags).localWeapons;
}

bool ClientPredictsMovement(unsigned int flags)
{
	return BuildClientFlagSnapshot(flags).predictMovement;
}

bool ClientUsesLagCompensation(unsigned int flags)
{
	return BuildClientFlagSnapshot(flags).lagCompensation;
}

bool ClientShouldAppearInHumanQueries(unsigned int flags)
{
	return !ClientIsFakeClient(flags);
}

UserinfoPenaltyPlan BuildUserinfoPenaltyPlan(
	const UserinfoPenaltyInput &input)
{
	UserinfoPenaltyPlan plan = {};
	plan.allowUpdate = true;
	plan.nextChangeTime = input.nextChangeTime;
	plan.penalty = input.penalty;
	plan.changeAttempts = input.changeAttempts;

	if (!input.penaltyEnabled || input.fakeClient || input.singlePlayer)
		return plan;

	if (plan.penalty == 0.0)
		plan.penalty = input.basePenalty;

	if (input.realtime <
		input.nextChangeTime + plan.penalty * input.penaltyMultiplier)
	{
		if (input.realtime < input.nextChangeTime &&
			plan.changeAttempts > 0)
		{
			plan.allowUpdate = false;
			plan.reportIgnoredUpdate = true;
		}

		plan.changeAttempts++;
	}

	if (plan.changeAttempts >= input.maxAttempts)
	{
		plan.penalty *= input.penaltyMultiplier;
		plan.changeAttempts = 0;
		plan.reportPenaltyChanged = true;
	}

	plan.nextChangeTime =
		input.realtime + plan.penalty * input.penaltyMultiplier;
	return plan;
}

double ResolveRequestedClientRate(
	int requestedRate,
	double defaultRate,
	double minimumRate,
	double maximumRate)
{
	double rate = requestedRate <= 0
		? defaultRate
		: static_cast<double>(requestedRate);

	return Clamp(rate, minimumRate, maximumRate);
}

double ResolveRequestedUpdateInterval(
	int requestedUpdateRate,
	int defaultUpdateRate)
{
	const int updateRate = requestedUpdateRate <= 0
		? defaultUpdateRate
		: requestedUpdateRate;

	return 1.0 / static_cast<double>(updateRate);
}

double ApplyUpdateIntervalLimits(
	double interval,
	double maximumUpdateRate,
	double minimumUpdateRate)
{
	if (maximumUpdateRate != 0.0)
	{
		const double minimumInterval = 1.0 / maximumUpdateRate;
		if (interval < minimumInterval)
			return minimumInterval;
	}

	if (minimumUpdateRate != 0.0)
	{
		const double maximumInterval = 1.0 / minimumUpdateRate;
		if (interval > maximumInterval)
			return maximumInterval;
	}

	return interval;
}

double ApplyLegacyServerRateLimits(
	double rate,
	double maximumRate,
	double minimumRate)
{
	if (maximumRate != 0.0)
	{
		if (rate > maximumRate)
			return rate;
	}

	if (minimumRate != 0.0)
	{
		if (rate < minimumRate)
			return rate;
	}

	return rate;
}

ClientUserinfoFlagPlan BuildClientUserinfoFlagPlan(
	int noPrediction,
	int lagCompensation,
	int localWeapons)
{
	ClientUserinfoFlagPlan plan = {};
	plan.predictMovement = noPrediction == 0;
	plan.lagCompensation = lagCompensation != 0;
	plan.localWeapons = localWeapons != 0;
	return plan;
}

}
}
}
