#include "engine/server/server_pmove_bridge_policy.hpp"

#include <cmath>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

float MinFloat(float left, float right)
{
	return left < right ? left : right;
}

float Clamp01(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 1.0f)
		return 1.0f;
	return value;
}

}

bool ShouldEnablePmoveUnlag(const PmoveUnlagAdmissionFacts &facts)
{
	if (facts.maxClients <= 1)
		return false;
	if (!facts.gameAllowsLagCompensation)
		return false;
	if (!facts.serverUnlagEnabled)
		return false;
	if (!facts.clientLagCompensationEnabled)
		return false;
	return facts.clientSpawned;
}

bool IsPmovePlayerEntityIndex(int edictIndex, int maxClients)
{
	return edictIndex >= 1 && edictIndex <= maxClients;
}

bool ShouldUsePmoveInterpolatedPlayer(
	int edictIndex,
	int maxClients,
	bool interpolantActive,
	bool interpolantMoving)
{
	return IsPmovePlayerEntityIndex(edictIndex, maxClients) &&
		interpolantActive &&
		interpolantMoving;
}

bool IsPmoveUnlagTeleport(
	const PmoveVector3 &oldPosition,
	const PmoveVector3 &newPosition)
{
	return std::fabs(oldPosition.x - newPosition.x) >
			kPmoveUnlagTeleportDistance ||
		std::fabs(oldPosition.y - newPosition.y) >
			kPmoveUnlagTeleportDistance ||
		std::fabs(oldPosition.z - newPosition.z) >
			kPmoveUnlagTeleportDistance;
}

PmoveUnlagLatencyPlan BuildPmoveUnlagLatencyPlan(
	float clientLatency,
	float maxUnlag)
{
	PmoveUnlagLatencyPlan plan = {};
	plan.normalizedMaxUnlag = maxUnlag;
	plan.latency = MinFloat(clientLatency, kPmoveUnlagMaxLatencySeconds);

	if (maxUnlag != 0.0f)
	{
		if (maxUnlag < 0.0f)
		{
			plan.normalizedMaxUnlag = 0.0f;
			plan.clampMaxUnlagCvarToZero = true;
		}

		plan.latency = MinFloat(plan.latency, plan.normalizedMaxUnlag);
	}

	return plan;
}

float BuildPmoveLerpSeconds(int lerpMilliseconds, float nextMessageInterval)
{
	float lerpSeconds = static_cast<float>(lerpMilliseconds) * 0.001f;

	if (lerpSeconds > kPmoveUnlagMaxLerpSeconds)
		lerpSeconds = kPmoveUnlagMaxLerpSeconds;
	if (lerpSeconds < nextMessageInterval)
		lerpSeconds = nextMessageInterval;

	return lerpSeconds;
}

float BuildPmoveUnlagTargetTime(
	float realtime,
	float latency,
	float lerpSeconds,
	float pushSeconds)
{
	float targetTime = (realtime - latency - lerpSeconds) + pushSeconds;
	if (targetTime > realtime)
		targetTime = realtime;
	return targetTime;
}

float BuildPmoveInterpolationFraction(
	float targetTime,
	float frameTime,
	float nextFrameTime)
{
	const float span = nextFrameTime - frameTime;
	if (span == 0.0f)
		return 0.0f;
	return Clamp01((targetTime - frameTime) / span);
}

}
}
}
