#ifndef XASH_ENGINE_SERVER_PMOVE_BRIDGE_POLICY_HPP
#define XASH_ENGINE_SERVER_PMOVE_BRIDGE_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr float kPmoveUnlagMaxLatencySeconds = 1.5f;
constexpr float kPmoveUnlagMaxLerpSeconds = 0.1f;
constexpr float kPmoveUnlagTeleportDistance = 64.0f;

struct PmoveUnlagAdmissionFacts
{
	int maxClients;
	bool gameAllowsLagCompensation;
	bool serverUnlagEnabled;
	bool clientLagCompensationEnabled;
	bool clientSpawned;
};

struct PmoveVector3
{
	float x;
	float y;
	float z;
};

struct PmoveUnlagLatencyPlan
{
	float latency;
	float normalizedMaxUnlag;
	bool clampMaxUnlagCvarToZero;
};

bool ShouldEnablePmoveUnlag(const PmoveUnlagAdmissionFacts &facts);
bool IsPmovePlayerEntityIndex(int edictIndex, int maxClients);
bool ShouldUsePmoveInterpolatedPlayer(
	int edictIndex,
	int maxClients,
	bool interpolantActive,
	bool interpolantMoving);
bool IsPmoveUnlagTeleport(
	const PmoveVector3 &oldPosition,
	const PmoveVector3 &newPosition);
PmoveUnlagLatencyPlan BuildPmoveUnlagLatencyPlan(
	float clientLatency,
	float maxUnlag);
float BuildPmoveLerpSeconds(int lerpMilliseconds, float nextMessageInterval);
float BuildPmoveUnlagTargetTime(
	float realtime,
	float latency,
	float lerpSeconds,
	float pushSeconds);
float BuildPmoveInterpolationFraction(
	float targetTime,
	float frameTime,
	float nextFrameTime);

}
}
}

#endif
