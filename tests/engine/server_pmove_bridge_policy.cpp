#include <cmath>
#include <cstdlib>

#include "engine/server/world/server_pmove_bridge_policy.hpp"

using namespace xash::engine::server;

namespace
{

bool NearlyEqual(float left, float right, float tolerance = 0.00001f)
{
	return std::fabs(left - right) <= tolerance;
}

PmoveUnlagAdmissionFacts BuildAllowedUnlagFacts()
{
	PmoveUnlagAdmissionFacts facts = {};
	facts.maxClients = 2;
	facts.gameAllowsLagCompensation = true;
	facts.serverUnlagEnabled = true;
	facts.clientLagCompensationEnabled = true;
	facts.clientSpawned = true;
	return facts;
}

bool TestUnlagAdmission()
{
	PmoveUnlagAdmissionFacts facts = BuildAllowedUnlagFacts();
	if (!ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.maxClients = 1;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.gameAllowsLagCompensation = false;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.serverUnlagEnabled = false;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.clientLagCompensationEnabled = false;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.clientSpawned = false;
	return !ShouldEnablePmoveUnlag(facts);
}

bool TestPlayerEntityIndexAndInterpolantAdmission()
{
	return !IsPmovePlayerEntityIndex(0, 4) &&
		IsPmovePlayerEntityIndex(1, 4) &&
		IsPmovePlayerEntityIndex(4, 4) &&
		!IsPmovePlayerEntityIndex(5, 4) &&
		!ShouldUsePmoveInterpolatedPlayer(0, 4, true, true) &&
		!ShouldUsePmoveInterpolatedPlayer(1, 4, false, true) &&
		!ShouldUsePmoveInterpolatedPlayer(1, 4, true, false) &&
		ShouldUsePmoveInterpolatedPlayer(1, 4, true, true);
}

bool TestTeleportThresholdIsStrict()
{
	const PmoveVector3 origin = { 0.0f, 0.0f, 0.0f };
	const PmoveVector3 exactlyThreshold = {
		kPmoveUnlagTeleportDistance,
		0.0f,
		0.0f,
	};
	const PmoveVector3 beyondThreshold = {
		0.0f,
		0.0f,
		-kPmoveUnlagTeleportDistance - 0.001f,
	};

	return !IsPmoveUnlagTeleport(origin, exactlyThreshold) &&
		IsPmoveUnlagTeleport(origin, beyondThreshold);
}

bool TestLatencyPlan()
{
	const PmoveUnlagLatencyPlan noMaxUnlag =
		BuildPmoveUnlagLatencyPlan(2.0f, 0.0f);
	const PmoveUnlagLatencyPlan positiveMaxUnlag =
		BuildPmoveUnlagLatencyPlan(1.0f, 0.25f);
	const PmoveUnlagLatencyPlan negativeMaxUnlag =
		BuildPmoveUnlagLatencyPlan(1.0f, -1.0f);

	return NearlyEqual(noMaxUnlag.latency, kPmoveUnlagMaxLatencySeconds) &&
		NearlyEqual(noMaxUnlag.normalizedMaxUnlag, 0.0f) &&
		!noMaxUnlag.clampMaxUnlagCvarToZero &&
		NearlyEqual(positiveMaxUnlag.latency, 0.25f) &&
		NearlyEqual(positiveMaxUnlag.normalizedMaxUnlag, 0.25f) &&
		!positiveMaxUnlag.clampMaxUnlagCvarToZero &&
		NearlyEqual(negativeMaxUnlag.latency, 0.0f) &&
		NearlyEqual(negativeMaxUnlag.normalizedMaxUnlag, 0.0f) &&
		negativeMaxUnlag.clampMaxUnlagCvarToZero;
}

bool TestLerpAndTargetTime()
{
	return NearlyEqual(BuildPmoveLerpSeconds(250, 0.0f),
			kPmoveUnlagMaxLerpSeconds) &&
		NearlyEqual(BuildPmoveLerpSeconds(10, 0.05f), 0.05f) &&
		NearlyEqual(BuildPmoveLerpSeconds(50, 0.01f), 0.05f) &&
		NearlyEqual(BuildPmoveUnlagTargetTime(100.0f, 0.1f, 0.05f, 0.0f),
			99.85f) &&
		NearlyEqual(BuildPmoveUnlagTargetTime(100.0f, 0.1f, 0.05f, 1.0f),
			100.0f);
}

bool TestInterpolationFraction()
{
	return NearlyEqual(BuildPmoveInterpolationFraction(10.0f, 5.0f, 5.0f),
			0.0f) &&
		NearlyEqual(BuildPmoveInterpolationFraction(4.0f, 5.0f, 10.0f),
			0.0f) &&
		NearlyEqual(BuildPmoveInterpolationFraction(12.0f, 5.0f, 10.0f),
			1.0f) &&
		NearlyEqual(BuildPmoveInterpolationFraction(7.5f, 5.0f, 10.0f),
			0.5f);
}

}

int main()
{
	if (!TestUnlagAdmission() ||
		!TestPlayerEntityIndexAndInterpolantAdmission() ||
		!TestTeleportThresholdIsStrict() ||
		!TestLatencyPlan() ||
		!TestLerpAndTargetTime() ||
		!TestInterpolationFraction())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
